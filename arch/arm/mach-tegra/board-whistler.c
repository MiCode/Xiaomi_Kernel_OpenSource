/*
 * arch/arm/mach-tegra/board-whistler.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/synaptics_i2c_rmi.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/gpio_scrollwheel.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/mfd/max8907c.h>
#include <linux/memblock.h>
#include <linux/tegra_uart.h>
#include <linux/rfkill-gpio.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/pinmux-tegra20.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/i2s.h>
#include <mach/tegra_asoc_pdata.h>
#include <mach/gpio-tegra.h>

#include <sound/tlv320aic326x.h>

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>

#include <mach/usb_phy.h>

#include "board.h"
#include "clock.h"
#include "board-whistler.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "pm.h"
#include "board-whistler-baseband.h"
#include "common.h"

#define SZ_3M (SZ_1M + SZ_2M)
#define SZ_152M (SZ_128M + SZ_16M + SZ_8M)
#define USB1_VBUS_GPIO TCA6416_GPIO_BASE

static struct platform_device *whistler_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
};

struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "pll_p"},
	[1] = {.name = "pll_m"},
	[2] = {.name = "clk_m"},
};

static struct tegra_uart_platform_data whistler_uart_pdata;

static void __init uart_debug_init(void)
{
	unsigned long rate;
	struct clk *debug_uart_clk;
	struct clk *c;
	int modem_id = tegra_get_modem_id();

	if (modem_id == 0x1) {
		/* UARTB is the debug port. */
		pr_info("Selecting UARTB as the debug console\n");
		whistler_uart_devices[1] = &debug_uartb_device;
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartb_device.dev.platform_data))->mapbase;
		debug_uart_clk = clk_get_sys("serial8250.0", "uartb");

		/* Clock enable for the debug channel */
		if (!IS_ERR_OR_NULL(debug_uart_clk)) {
			rate = ((struct plat_serial8250_port *)(
			debug_uartb_device.dev.platform_data))->uartclk;
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
	} else {
		/* UARTA is the debug port. */
		pr_info("Selecting UARTA as the debug console\n");
		whistler_uart_devices[0] = &debug_uarta_device;
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->mapbase;
		debug_uart_clk = clk_get_sys("serial8250.0", "uarta");

		/* Clock enable for the debug channel */
		if (!IS_ERR_OR_NULL(debug_uart_clk)) {
			rate = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->uartclk;
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
}

static void __init whistler_uart_init(void)
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
	whistler_uart_pdata.parent_clk_list = uart_parent_clk;
	whistler_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);

	tegra_uarta_device.dev.platform_data = &whistler_uart_pdata;
	tegra_uartb_device.dev.platform_data = &whistler_uart_pdata;
	tegra_uartc_device.dev.platform_data = &whistler_uart_pdata;

	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(whistler_uart_devices,
				ARRAY_SIZE(whistler_uart_devices));
}
static struct rfkill_gpio_platform_data whistler_bt_rfkill_pdata[] = {
	{
		.name		= "bt_rfkill",
		.shutdown_gpio  = TEGRA_GPIO_PU0,
		.reset_gpio     = TEGRA_GPIO_INVALID,
		.type		= RFKILL_TYPE_BLUETOOTH,
	},
};

static struct platform_device whistler_bt_rfkill_device = {
	.name = "rfkill_gpio",
	.id   = -1,
	.dev  = {
		.platform_data  = whistler_bt_rfkill_pdata,
	},
};

static struct resource whistler_bluesleep_resources[] = {
	[0] = {
		.name = "gpio_host_wake",
			.start  = TEGRA_GPIO_PU6,
			.end    = TEGRA_GPIO_PU6,
			.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "gpio_ext_wake",
			.start  = TEGRA_GPIO_PU1,
			.end    = TEGRA_GPIO_PU1,
			.flags  = IORESOURCE_IO,
	},
	[2] = {
		.name = "host_wake",
			.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct platform_device whistler_bluesleep_device = {
	.name           = "bluesleep",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(whistler_bluesleep_resources),
	.resource       = whistler_bluesleep_resources,
};

static void __init whistler_setup_bluesleep(void)
{
	whistler_bluesleep_resources[2].start =
		whistler_bluesleep_resources[2].end =
			gpio_to_irq(TEGRA_GPIO_PU6);
	platform_device_register(&whistler_bluesleep_device);
	return;
}

static __initdata struct tegra_clk_init_table whistler_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pwm",	"clk_32k",	32768,		false},
	{ "kbc",	"clk_32k",	32768,		true},
	{ "sdmmc2",	"pll_p",	25000000,	false},
	{ "i2s1",	"pll_a_out0",	0,		false},
	{ "i2s2",	"pll_a_out0",	0,		false},
	{ "spdif_out",	"pll_a_out0",	0,		false},
	{ NULL,		NULL,		0,		0},
};

static struct tegra_i2c_platform_data whistler_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PC4, 0},
	.sda_gpio		= {TEGRA_GPIO_PC5, 0},
	.arb_recovery = arb_lost_recovery,
	.slave_addr = 0xFC,
};

static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup	= TEGRA_PINGROUP_DDC,
	.func		= TEGRA_MUX_I2C2,
};

static const struct tegra_pingroup_config i2c2_gen2 = {
	.pingroup	= TEGRA_PINGROUP_PTA,
	.func		= TEGRA_MUX_I2C2,
};

static struct tegra_i2c_platform_data whistler_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 2,
	.bus_clk_rate	= { 10000, 100000 },
	.bus_mux	= { &i2c2_ddc, &i2c2_gen2 },
	.bus_mux_len	= { 1, 1 },
	.scl_gpio		= {0, TEGRA_GPIO_PT5},
	.sda_gpio		= {0, TEGRA_GPIO_PT6},
	.arb_recovery = arb_lost_recovery,
	.slave_addr = 0xFC,
};

static struct tegra_i2c_platform_data whistler_i2c3_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PBB2, 0},
	.sda_gpio		= {TEGRA_GPIO_PBB3, 0},
	.arb_recovery = arb_lost_recovery,
	.slave_addr = 0xFC,
};

static struct tegra_i2c_platform_data whistler_dvc_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.is_dvc		= true,
	.scl_gpio		= {TEGRA_GPIO_PZ6, 0},
	.sda_gpio		= {TEGRA_GPIO_PZ7, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct aic326x_pdata whistler_aic3262_pdata = {
	/* debounce time */
	.debounce_time_ms = 512,
};

static struct i2c_board_info __initdata wm8753_board_info[] = {
	{
		I2C_BOARD_INFO("wm8753", 0x1a),
	},
	{
		I2C_BOARD_INFO("aic3262-codec", 0x18),
		.platform_data = &whistler_aic3262_pdata,
	},
};

static void whistler_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &whistler_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &whistler_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &whistler_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &whistler_dvc_platform_data;

	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device1);

	wm8753_board_info[0].irq = wm8753_board_info[1].irq =
		gpio_to_irq(TEGRA_GPIO_HP_DET);
	i2c_register_board_info(4, wm8753_board_info,
		ARRAY_SIZE(wm8753_board_info));
}

#define GPIO_SCROLL(_pinaction, _gpio, _desc)	\
{	\
	.pinaction = GPIO_SCROLLWHEEL_PIN_##_pinaction, \
	.gpio = TEGRA_GPIO_##_gpio,	\
	.desc = _desc,	\
	.active_low = 1,	\
	.debounce_interval = 2,	\
}

static struct gpio_scrollwheel_button scroll_keys[] = {
	[0] = GPIO_SCROLL(ONOFF, PR3, "sw_onoff"),
	[1] = GPIO_SCROLL(PRESS, PQ5, "sw_press"),
	[2] = GPIO_SCROLL(ROT1, PQ3, "sw_rot1"),
	[3] = GPIO_SCROLL(ROT2, PQ4, "sw_rot2"),
};

static struct gpio_scrollwheel_platform_data whistler_scroll_platform_data = {
	.buttons = scroll_keys,
	.nbuttons = ARRAY_SIZE(scroll_keys),
};

static struct platform_device whistler_scroll_device = {
	.name	= "alps-gpio-scrollwheel",
	.id	= 0,
	.dev	= {
		.platform_data	= &whistler_scroll_platform_data,
	},
};

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

static struct tegra_asoc_platform_data whistler_audio_pdata = {
	.gpio_spkr_en		= -1,
	.gpio_hp_det		= TEGRA_GPIO_HP_DET,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= -1,
	.gpio_ext_mic_en	= -1,
	.debounce_time_hp	= 200,
	.i2s_param[HIFI_CODEC]	= {
		.audio_port_id	= 0,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
	},
	.i2s_param[BASEBAND]	= {
		.audio_port_id	= 2,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
		.sample_size	= 16,
		.rate		= 8000,
		.channels	= 1,
	},
	.i2s_param[BT_SCO]	= {
		.sample_size	= 16,
		.audio_port_id	= 3,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
	},
};

static struct platform_device whistler_audio_aic326x_device = {
	.name	= "tegra-snd-aic326x",
	.id	= 0,
	.dev	= {
		.platform_data  = &whistler_audio_pdata,
	},
};

static struct platform_device whistler_audio_wm8753_device = {
	.name	= "tegra-snd-wm8753",
	.id	= 0,
	.dev	= {
		.platform_data  = &whistler_audio_pdata,
	},
};

static struct platform_device *whistler_devices[] __initdata = {
	&tegra_pmu_device,
	&tegra_udc_device,
	&tegra_gart_device,
	&tegra_aes_device,
	&tegra_wdt_device,
	&tegra_avp_device,
	&whistler_scroll_device,
	&tegra_camera,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_spdif_device,
	&tegra_das_device,
	&spdif_dit_device,
	&bluetooth_dit_device,
	&baseband_dit_device,
	&whistler_bt_rfkill_device,
	&tegra_pcm_device,
	&whistler_audio_aic326x_device,
	&whistler_audio_wm8753_device,
};

static struct synaptics_i2c_rmi_platform_data synaptics_pdata = {
	.flags		= SYNAPTICS_FLIP_X | SYNAPTICS_FLIP_Y | SYNAPTICS_SWAP_XY,
	.irqflags	= IRQF_TRIGGER_LOW,
};

static struct i2c_board_info whistler_i2c_touch_info[] = {
	{
		I2C_BOARD_INFO("synaptics-rmi-ts", 0x20),
		.platform_data	= &synaptics_pdata,
	},
};

static int __init whistler_touch_init(void)
{
	whistler_i2c_touch_info[0].irq = gpio_to_irq(TEGRA_GPIO_PC6);
	i2c_register_board_info(0, whistler_i2c_touch_info, 1);

	return 0;
}

static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
	.has_hostpc = false,
	.id_det_type = TEGRA_USB_VIRTUAL_ID,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_DEVICE,
	.u_data.dev = {
		.vbus_pmu_irq = MAX8907C_INT_BASE + MAX8907C_IRQ_VCHG_DC_R,
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
	.port_otg = true,
	.has_hostpc = false,
	.id_det_type = TEGRA_USB_VIRTUAL_ID,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = TEGRA_GPIO_PN6,
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

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
};

#define SERIAL_NUMBER_LENGTH 20
static void whistler_usb_init(void)
{
	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);

	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
}

static void __init tegra_whistler_init(void)
{
	int modem_id = tegra_get_modem_id();
	tegra_clk_init_from_table(whistler_clk_init_table);
	whistler_emc_init();
	tegra_soc_device_init("whistler");
	tegra_enable_pinmux();
	whistler_pinmux_init();
	whistler_i2c_init();
	whistler_uart_init();
	platform_add_devices(whistler_devices, ARRAY_SIZE(whistler_devices));
	tegra_ram_console_debug_init();
	whistler_sdhci_init();
	whistler_regulator_init();
	whistler_panel_init();
	whistler_sensors_init();
	whistler_touch_init();
	whistler_kbc_init();
	whistler_usb_init();
	whistler_emc_init();
	if (modem_id == 0x1)
		whistler_baseband_init();
	whistler_setup_bluesleep();
}

int __init tegra_whistler_protected_aperture_init(void)
{
	tegra_protected_aperture_init(tegra_grhost_aperture);
	return 0;
}

void __init tegra_whistler_reserve(void)
{
	if (memblock_reserve(0x0, 4096) < 0)
		pr_warn("Cannot reserve first 4K of memory for safety\n");

	tegra_reserve(SZ_152M, SZ_3M, SZ_1M);
	tegra_ram_console_debug_reserve(SZ_1M);
}

static const char *whistler_dt_board_compat[] = {
	"nvidia,whistler",
	NULL
};

MACHINE_START(WHISTLER, "whistler")
	.atag_offset	= 0x100,
	.soc		= &tegra_soc_desc,
	.map_io         = tegra_map_common_io,
	.init_early	= tegra20_init_early,
	.init_irq       = tegra_init_irq,
	.handle_irq	= gic_handle_irq,
	.reserve        = tegra_whistler_reserve,
	.timer          = &tegra_timer,
	.init_machine   = tegra_whistler_init,
	.restart	= tegra_assert_system_reset,
	.dt_compat	= whistler_dt_board_compat,
MACHINE_END
