/*
 * arch/arm/mach-tegra/board-enterprise.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/spi/spi.h>
#include <linux/tegra_uart.h>
#include <linux/fsl_devices.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/memblock.h>
#include <linux/rfkill-gpio.h>
#include <linux/mfd/tlv320aic3262-registers.h>
#include <linux/mfd/tlv320aic3xxx-core.h>

#include <linux/nfc/pn544.h>
#include <linux/of_platform.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>

#include <sound/max98088.h>

#include <asm/hardware/gic.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/io_dpd.h>
#include <mach/usb_phy.h>
#include <mach/i2s.h>
#include <mach/tegra_asoc_pdata.h>
#include <mach/tegra-bb-power.h>
#include <mach/gpio-tegra.h>
#include <mach/tegra_fiq_debugger.h>

#include <asm/mach-types.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>

#include "board.h"
#include "board-common.h"
#include "clock.h"
#include "board-enterprise.h"
#include "baseband-xmm-power.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "pm.h"
#include "common.h"

/* wl128x BT, FM, GPS connectivity chip */
struct ti_st_plat_data enterprise_wilink_pdata = {
       .nshutdown_gpio = TEGRA_GPIO_PE6,
       .dev_name = BLUETOOTH_UART_DEV_NAME,
       .flow_cntrl = 1,
       .baud_rate = 3000000,
};

static struct platform_device wl128x_device = {
       .name           = "kim",
       .id             = -1,
       .dev.platform_data = &enterprise_wilink_pdata,
};

static struct platform_device btwilink_device = {
       .name = "btwilink",
       .id = -1,
};

static noinline void __init enterprise_bt_st(void)
{
       pr_info("enterprise_bt_st");

       platform_device_register(&wl128x_device);
       platform_device_register(&btwilink_device);
}

#ifdef CONFIG_BT_BLUESLEEP
static struct rfkill_gpio_platform_data enterprise_bt_rfkill_pdata[] = {
	{
		.name           = "bt_rfkill",
		.shutdown_gpio  = TEGRA_GPIO_PE6,
		.reset_gpio     = TEGRA_GPIO_INVALID,
		.type           = RFKILL_TYPE_BLUETOOTH,
	},
};

static struct platform_device enterprise_bt_rfkill_device = {
	.name = "rfkill_gpio",
	.id		= -1,
	.dev = {
		.platform_data = &enterprise_bt_rfkill_pdata,
	},
};
static struct resource enterprise_ti_bluesleep_resources[] = {
	[0] = {
		.name = "gpio_host_wake",
			.start  = TEGRA_GPIO_PS2,
			.end    = TEGRA_GPIO_PS2,
			.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "host_wake",
			.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};
static struct resource enterprise_brcm_bluesleep_resources[] = {
	[0] = {
		.name = "gpio_host_wake",
			.start  = TEGRA_GPIO_PS2,
			.end    = TEGRA_GPIO_PS2,
			.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "gpio_ext_wake",
			.start  = TEGRA_GPIO_PE7,
			.end    = TEGRA_GPIO_PE7,
			.flags  = IORESOURCE_IO,
	},
	[2] = {
		.name = "host_wake",
			.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct platform_device enterprise_ti_bluesleep_device = {
	.name           = "bluesleep",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(enterprise_ti_bluesleep_resources),
	.resource       = enterprise_ti_bluesleep_resources,
};

static struct platform_device enterprise_brcm_bluesleep_device = {
	.name           = "bluesleep",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(enterprise_brcm_bluesleep_resources),
	.resource       = enterprise_brcm_bluesleep_resources,
};
static void __init enterprise_bt_rfkill(void)
{
	platform_device_register(&enterprise_bt_rfkill_device);
	return;
}
static void __init enterprise_setup_bluesleep(void)
{
	if (tegra_get_commchip_id() == COMMCHIP_TI_WL18XX) {
		enterprise_ti_bluesleep_resources[1].start =
		enterprise_ti_bluesleep_resources[1].end =
			gpio_to_irq(TEGRA_GPIO_PS2);
		platform_device_register(&enterprise_ti_bluesleep_device);
	}
	else {
		enterprise_brcm_bluesleep_resources[2].start =
		enterprise_brcm_bluesleep_resources[2].end =
			gpio_to_irq(TEGRA_GPIO_PS2);
		platform_device_register(&enterprise_brcm_bluesleep_device);
	}
	return;
}
#elif defined CONFIG_BLUEDROID_PM
static struct resource enterprise_bluedroid_pm_resources[] = {
	[0] = {
		.name   = "shutdown_gpio",
		.start  = TEGRA_GPIO_PE6,
		.end    = TEGRA_GPIO_PE6,
		.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "host_wake",
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
	[2] = {
		.name = "gpio_ext_wake",
		.start  = TEGRA_GPIO_PE7,
		.end    = TEGRA_GPIO_PE7,
		.flags  = IORESOURCE_IO,
	},
	[3] = {
		.name = "gpio_host_wake",
		.start  = TEGRA_GPIO_PS2,
		.end    = TEGRA_GPIO_PS2,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device enterprise_bluedroid_pm_device = {
	.name = "bluedroid_pm",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(enterprise_bluedroid_pm_resources),
	.resource       = enterprise_bluedroid_pm_resources,
};

static void __init enterprise_bluedroid_pm(void)
{
	struct board_info board_info;
	tegra_get_board_info(&board_info);

	enterprise_bluedroid_pm_resources[1].start =
		enterprise_bluedroid_pm_resources[1].end =
				gpio_to_irq(TEGRA_GPIO_PS2);
	if (board_info.board_id == BOARD_E1239)
		enterprise_bluedroid_pm_resources[1].start =
			enterprise_bluedroid_pm_resources[1].end =
							TEGRA_GPIO_PF4;
	platform_device_register(&enterprise_bluedroid_pm_device);
	return;
}
#endif
static __initdata struct tegra_clk_init_table enterprise_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pll_m",	NULL,		0,		false},
	{ "hda",	"pll_p",	108000000,	false},
	{ "hda2codec_2x","pll_p",	48000000,	false},
	{ "pwm",	"pll_p",	3187500,	false},
	{ "blink",	"clk_32k",	32768,		true},
	{ "i2s0",	"pll_a_out0",	0,		false},
	{ "i2s1",	"pll_a_out0",	0,		false},
	{ "i2s3",	"pll_a_out0",	0,		false},
	{ "spdif_out",	"pll_a_out0",	0,		false},
	{ "d_audio",	"clk_m",	13000000,	false},
	{ "dam0",	"clk_m",	13000000,	false},
	{ "dam1",	"clk_m",	13000000,	false},
	{ "dam2",	"clk_m",	13000000,	false},
	{ "audio0",	"i2s0_sync",	0,		false},
	{ "audio1",	"i2s1_sync",	0,		false},
	{ "audio2",	"i2s2_sync",	0,		false},
	{ "audio3",	"i2s3_sync",	0,		false},
	{ "audio4",	"i2s4_sync",	0,		false},
	{ "vi",		"pll_p",	0,		false},
	{ "vi_sensor",	"pll_p",	0,		false},
	{ "i2c5",	"pll_p",	3200000,	false},
	{ NULL,		NULL,		0,		0},
};

static __initdata struct tegra_clk_init_table enterprise_clk_i2s2_table[] = {
	/* name		parent		rate		enabled */
	{ "i2s2",	"pll_a_out0",	0,		false},
	{ NULL,		NULL,		0,		0},
};

static __initdata struct tegra_clk_init_table enterprise_clk_i2s4_table[] = {
	/* name		parent		rate		enabled */
	{ "i2s4",	"pll_a_out0",	0,		false},
	{ NULL,		NULL,		0,		0},
};

static struct aic3262_gpio_setup aic3262_gpio[] = {
	/* GPIO 1*/
	{
		.used		= 1,
		.in		= 0,
		.value		= AIC3262_GPIO1_FUNC_INT1_OUTPUT ,
	},
	/* GPIO 2*/
	{
		.used		= 1,
		.in		= 0,
		.value		= AIC3262_GPIO2_FUNC_ADC_MOD_CLK_OUTPUT,
	},
	/* GPIO 1 */
	{
		.used		= 0,
	},
	{// GPI2
		.used		= 1,
		.in		= 1,
		.in_reg         = AIC3262_DMIC_INPUT_CNTL,
		.in_reg_bitmask	= AIC3262_DMIC_CONFIGURE_MASK,
		.in_reg_shift	= AIC3262_DMIC_CONFIGURE_SHIFT,
		.value		= AIC3262_DMIC_GPI2_LEFT_GPI2_RIGHT,
	},
	{// GPO1
		.used		= 0,
		.value		= AIC3262_GPO1_FUNC_DISABLED,
	},
};

static struct aic3xxx_pdata aic3262_codec_pdata = {
	.gpio_irq	= 1,
	.gpio_reset	= TEGRA_GPIO_CODEC_RST,
	.gpio		= aic3262_gpio,
	.naudint_irq	= TEGRA_GPIO_HP_DET,
	.irq_base	= AIC3262_CODEC_IRQ_BASE,
};

static struct tegra_i2c_platform_data enterprise_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PC4, 0},
	.sda_gpio		= {TEGRA_GPIO_PC5, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data enterprise_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.is_clkon_always = true,
	.scl_gpio		= {TEGRA_GPIO_PT5, 0},
	.sda_gpio		= {TEGRA_GPIO_PT6, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data enterprise_i2c3_platform_data = {
	.adapter_nr	= 2,
	.bus_count	= 1,
	.bus_clk_rate	= { 271000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PBB1, 0},
	.sda_gpio		= {TEGRA_GPIO_PBB2, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data enterprise_i2c4_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 10000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PV4, 0},
	.sda_gpio		= {TEGRA_GPIO_PV5, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data enterprise_i2c5_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 390000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PZ6, 0},
	.sda_gpio		= {TEGRA_GPIO_PZ7, 0},
	.arb_recovery = arb_lost_recovery,
};

/* Equalizer filter coefs generated from the MAXIM MAX98088
 * evkit software tool */
static struct max98088_eq_cfg max98088_eq_cfg[] = {
	{
		.name = "FLAT",
		.rate = 44100,
		.band1 = {0x2000, 0xC002, 0x4000, 0x00E9, 0x0000},
		.band2 = {0x2000, 0xC00F, 0x4000, 0x02BC, 0x0000},
		.band3 = {0x2000, 0xC0A7, 0x4000, 0x0916, 0x0000},
		.band4 = {0x2000, 0xC5C2, 0x4000, 0x1A87, 0x0000},
		.band5 = {0x2000, 0xF6B0, 0x4000, 0x3F51, 0x0000},
	},
	{
		.name = "LOWPASS1K",
		.rate = 44100,
		.band1 = {0x205D, 0xC001, 0x3FEF, 0x002E, 0x02E0},
		.band2 = {0x5B9A, 0xC093, 0x3AB2, 0x088B, 0x1981},
		.band3 = {0x0D22, 0xC170, 0x26EA, 0x0D79, 0x32CF},
		.band4 = {0x0894, 0xC612, 0x01B3, 0x1B34, 0x3FFA},
		.band5 = {0x0815, 0x3FFF, 0xCF78, 0x0000, 0x29B7},
	},
	{ /* BASS=-12dB, TREBLE=+9dB, Fc=5KHz */
		.name = "HIBOOST",
		.rate = 44100,
		.band1 = {0x0815, 0xC001, 0x3AA4, 0x0003, 0x19A2},
		.band2 = {0x0815, 0xC103, 0x092F, 0x0B55, 0x3F56},
		.band3 = {0x0E0A, 0xC306, 0x1E5C, 0x136E, 0x3856},
		.band4 = {0x2459, 0xF665, 0x0CAA, 0x3F46, 0x3EBB},
		.band5 = {0x5BBB, 0x3FFF, 0xCEB0, 0x0000, 0x28CA},
	},
	{ /* BASS=12dB, TREBLE=+12dB */
		.name = "LOUD12DB",
		.rate = 44100,
		.band1 = {0x7FC1, 0xC001, 0x3EE8, 0x0020, 0x0BC7},
		.band2 = {0x51E9, 0xC016, 0x3C7C, 0x033F, 0x14E9},
		.band3 = {0x1745, 0xC12C, 0x1680, 0x0C2F, 0x3BE9},
		.band4 = {0x4536, 0xD7E2, 0x0ED4, 0x31DD, 0x3E42},
		.band5 = {0x7FEF, 0x3FFF, 0x0BAB, 0x0000, 0x3EED},
	},
	{
		.name = "FLAT",
		.rate = 16000,
		.band1 = {0x2000, 0xC004, 0x4000, 0x0141, 0x0000},
		.band2 = {0x2000, 0xC033, 0x4000, 0x0505, 0x0000},
		.band3 = {0x2000, 0xC268, 0x4000, 0x115F, 0x0000},
		.band4 = {0x2000, 0xDA62, 0x4000, 0x33C6, 0x0000},
		.band5 = {0x2000, 0x4000, 0x4000, 0x0000, 0x0000},
	},
	{
		.name = "LOWPASS1K",
		.rate = 16000,
		.band1 = {0x2000, 0xC004, 0x4000, 0x0141, 0x0000},
		.band2 = {0x5BE8, 0xC3E0, 0x3307, 0x15ED, 0x26A0},
		.band3 = {0x0F71, 0xD15A, 0x08B3, 0x2BD0, 0x3F67},
		.band4 = {0x0815, 0x3FFF, 0xCF78, 0x0000, 0x29B7},
		.band5 = {0x0815, 0x3FFF, 0xCF78, 0x0000, 0x29B7},
	},
	{ /* BASS=-12dB, TREBLE=+9dB, Fc=2KHz */
		.name = "HIBOOST",
		.rate = 16000,
		.band1 = {0x0815, 0xC001, 0x3BD2, 0x0009, 0x16BF},
		.band2 = {0x080E, 0xC17E, 0xF653, 0x0DBD, 0x3F43},
		.band3 = {0x0F80, 0xDF45, 0xEE33, 0x36FE, 0x3D79},
		.band4 = {0x590B, 0x3FF0, 0xE882, 0x02BD, 0x3B87},
		.band5 = {0x4C87, 0xF3D0, 0x063F, 0x3ED4, 0x3FB1},
	},
	{ /* BASS=12dB, TREBLE=+12dB */
		.name = "LOUD12DB",
		.rate = 16000,
		.band1 = {0x7FC1, 0xC001, 0x3D07, 0x0058, 0x1344},
		.band2 = {0x2DA6, 0xC013, 0x3CF1, 0x02FF, 0x138B},
		.band3 = {0x18F1, 0xC08E, 0x244D, 0x0863, 0x34B5},
		.band4 = {0x2BE0, 0xF385, 0x04FD, 0x3EC5, 0x3FCE},
		.band5 = {0x7FEF, 0x4000, 0x0BAB, 0x0000, 0x3EED},
	},
};


static struct max98088_pdata enterprise_max98088_pdata = {
	/* equalizer configuration */
	.eq_cfg = max98088_eq_cfg,
	.eq_cfgcnt = ARRAY_SIZE(max98088_eq_cfg),

	/* debounce time */
	.debounce_time_ms = 200,

	/* microphone configuration */
	.digmic_left_mode = 1,
	.digmic_right_mode = 1,

	/* receiver output configuration */
	.receiver_mode = 0,	/* 0 = amplifier, 1 = line output */
};

static struct pn544_i2c_platform_data nfc_pdata = {
		.irq_gpio = TEGRA_GPIO_PS4,
		.ven_gpio = TEGRA_GPIO_PM6,
		.firm_gpio = 0,
};


static struct i2c_board_info __initdata max98088_board_info = {
	I2C_BOARD_INFO("max98088", 0x10),
	.platform_data = &enterprise_max98088_pdata,
	.irq = TEGRA_GPIO_HP_DET,
};

static struct i2c_board_info __initdata enterprise_codec_aic326x_info = {
	I2C_BOARD_INFO("tlv320aic3262", 0x18),
	.platform_data = &aic3262_codec_pdata,
	.irq = TEGRA_GPIO_HP_DET,
};

static struct i2c_board_info __initdata nfc_board_info = {
	I2C_BOARD_INFO("pn544", 0x28),
	.platform_data = &nfc_pdata,
};

static void enterprise_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &enterprise_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &enterprise_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &enterprise_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &enterprise_i2c4_platform_data;
	tegra_i2c_device5.dev.platform_data = &enterprise_i2c5_platform_data;

	platform_device_register(&tegra_i2c_device5);
	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device1);

	i2c_register_board_info(0, &max98088_board_info, 1);
	i2c_register_board_info(0, &enterprise_codec_aic326x_info, 1);
	nfc_board_info.irq = gpio_to_irq(TEGRA_GPIO_PS4);
	i2c_register_board_info(0, &nfc_board_info, 1);
}

static struct platform_device *enterprise_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
};

static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "clk_m"},
	[1] = {.name = "pll_p"},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	[2] = {.name = "pll_m"},
#endif
};
static struct tegra_uart_platform_data enterprise_uart_pdata;
static struct tegra_uart_platform_data enterprise_loopback_uart_pdata;

static void __init uart_debug_init(void)
{
	int debug_port_id;

	/* UARTD is the debug port. */
	pr_info("Selecting UARTD as the debug console\n");
	debug_port_id = uart_console_debug_init(3);
	if (debug_port_id < 0)
		return;

	enterprise_uart_devices[debug_port_id] = uart_console_debug_device;
}

static void __init enterprise_uart_init(void)
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
	enterprise_uart_pdata.parent_clk_list = uart_parent_clk;
	enterprise_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	enterprise_loopback_uart_pdata.parent_clk_list = uart_parent_clk;
	enterprise_loopback_uart_pdata.parent_clk_count =
						ARRAY_SIZE(uart_parent_clk);
	enterprise_loopback_uart_pdata.is_loopback = true;
	tegra_uarta_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartb_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartc_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartd_device.dev.platform_data = &enterprise_uart_pdata;
	/* UARTE is used for loopback test purpose */
	tegra_uarte_device.dev.platform_data = &enterprise_loopback_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(enterprise_uart_devices,
				ARRAY_SIZE(enterprise_uart_devices));
}
/* add vibrator for enterprise */
static struct platform_device vibrator_device = {
	.name = "tegra-vibrator",
	.id = -1,
};

static noinline void __init enterprise_vibrator_init(void)
{
	platform_device_register(&vibrator_device);
}


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

static struct tegra_asoc_platform_data enterprise_audio_pdata = {
	.gpio_spkr_en		= -1,
	.gpio_hp_det		= TEGRA_GPIO_HP_DET,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= -1,
	.gpio_ext_mic_en	= -1,
	.debounce_time_hp	= -1,
	/*defaults for Enterprise board*/
	.i2s_param[HIFI_CODEC]	= {
		.audio_port_id	= 0,
		.is_i2s_master	= 0,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
		.channels	    = 2,
	},
	.i2s_param[BASEBAND]	= {
		.audio_port_id	= 2,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
		.sample_size	= 16,
		.rate		= 8000,
		.channels	= 1,
		.bit_clk    = 2048000,
	},
	.i2s_param[BT_SCO]	= {
		.audio_port_id	= 3,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
		.sample_size	= 16,
	},
};

static struct platform_device enterprise_audio_device = {
	.name	= "tegra-snd-max98088",
	.id	= 0,
	.dev	= {
		.platform_data  = &enterprise_audio_pdata,
	},
};

static struct tegra_asoc_platform_data enterprise_audio_aic326x_pdata = {
	.gpio_spkr_en		= -1,
	.gpio_hp_det		= TEGRA_GPIO_HP_DET,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= -1,
	.gpio_ext_mic_en	= -1,
	/*defaults for Verbier-Enterprise (E1197) board with TI AIC326X codec*/
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
	.i2s_param[VOICE_CODEC]	= {
		.audio_port_id	= 1,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
		.rate		= 8000,
		.channels	= 2,
	},
};

static struct platform_device enterprise_audio_aic326x_device = {
	.name	= "tegra-snd-aic326x",
	.id	= 0,
	.dev	= {
		.platform_data  = &enterprise_audio_aic326x_pdata,
	},
};

static struct platform_device *enterprise_devices[] __initdata = {
	&tegra_pmu_device,
	&tegra_rtc_device,
	&tegra_udc_device,
	&tegra_wdt0_device,
#if defined(CONFIG_TEGRA_AVP)
	&tegra_avp_device,
#endif
	&tegra_spi_device4,
	&tegra_hda_device,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	&tegra_se_device,
#endif
#if defined(CONFIG_CRYPTO_DEV_TEGRA_AES)
	&tegra_aes_device,
#endif
};

#define MXT_CFG_NAME            "Android_Enterprise_2012-01-31.cfg"
static u8 read_chg(void)
{
	return gpio_get_value(TEGRA_GPIO_PH6);
}

static struct mxt_platform_data atmel_mxt_info = {
	.irqflags       = IRQF_TRIGGER_FALLING,
	.read_chg       = &read_chg,
	.mxt_cfg_name	= MXT_CFG_NAME,
};

static struct i2c_board_info __initdata atmel_i2c_info[] = {
	{
		I2C_BOARD_INFO("atmel_mxt_ts", MXT224_I2C_ADDR1),
		.flags = I2C_CLIENT_WAKE,
		.platform_data = &atmel_mxt_info,
	}
};

static int __init enterprise_touch_init(void)
{
	int ret;
	ret = gpio_request(TEGRA_GPIO_PH6, "atmel-irq");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return ret;
	}
	ret = gpio_direction_input(TEGRA_GPIO_PH6);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n",
			__func__, ret);
		gpio_free(TEGRA_GPIO_PH6);
		return ret;
	}
	ret = gpio_request(TEGRA_GPIO_PF5, "atmel-reset");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return ret;
	}
	ret = gpio_direction_output(TEGRA_GPIO_PF5, 0);
	if (ret < 0) {
		pr_err("%s: gpio_direction_ouput failed %d\n",
			__func__, ret);
		gpio_free(TEGRA_GPIO_PF5);
		return ret;
	}
	msleep(1);
	gpio_set_value(TEGRA_GPIO_PF5, 1);
	msleep(100);

	atmel_i2c_info[0].irq = gpio_to_irq(TEGRA_GPIO_PH6);
	i2c_register_board_info(1, atmel_i2c_info, 1);

	return 0;
}

static void enterprise_usb_hsic_postsupend(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L2);
#endif
}

static void enterprise_usb_hsic_preresume(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L2TOL0);
#endif
}

static void enterprise_usb_hsic_post_resume(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L0);
#endif
}

static void enterprise_usb_hsic_phy_power(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L0);
#endif
}

static void enterprise_usb_hsic_post_phy_off(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L2);
#endif
}

static struct tegra_usb_phy_platform_ops hsic_xmm_plat_ops = {
	.post_suspend = enterprise_usb_hsic_postsupend,
	.pre_resume = enterprise_usb_hsic_preresume,
	.port_power = enterprise_usb_hsic_phy_power,
	.post_resume = enterprise_usb_hsic_post_resume,
	.post_phy_off = enterprise_usb_hsic_post_phy_off,
};

static struct tegra_usb_platform_data tegra_ehci2_hsic_xmm_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_HSIC,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = false,
	},
	.ops = &hsic_xmm_plat_ops,
};

static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.id_det_type = TEGRA_USB_VIRTUAL_ID,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_DEVICE,
	.u_data.dev = {
		.vbus_pmu_irq = ENT_TPS80031_IRQ_BASE +
				TPS80031_INT_VBUS_DET,
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
	.has_hostpc = true,
	.id_det_type = TEGRA_USB_VIRTUAL_ID,
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

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
};

static struct platform_device *
tegra_usb_hsic_host_register(struct platform_device *ehci_dev)
{
	struct platform_device *pdev;
	int val;

	pdev = platform_device_alloc(ehci_dev->name, ehci_dev->id);
	if (!pdev)
		return NULL;

	val = platform_device_add_resources(pdev, ehci_dev->resource,
						ehci_dev->num_resources);
	if (val)
		goto error;

	pdev->dev.dma_mask =  ehci_dev->dev.dma_mask;
	pdev->dev.coherent_dma_mask = ehci_dev->dev.coherent_dma_mask;

	val = platform_device_add_data(pdev, &tegra_ehci2_hsic_xmm_pdata,
			sizeof(struct tegra_usb_platform_data));
	if (val)
		goto error;

	val = platform_device_add(pdev);
	if (val)
		goto error;

	return pdev;

error:
	pr_err("%s: failed to add the host contoller device\n", __func__);
	platform_device_put(pdev);
	return NULL;
}

void tegra_usb_hsic_host_unregister(struct platform_device **platdev)
{
	struct platform_device *pdev = *platdev;

	if (pdev && &pdev->dev) {
		platform_device_unregister(pdev);
		*platdev = NULL;
	} else
		pr_err("%s: no platform device\n", __func__);
}

static void enterprise_usb_init(void)
{
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;

	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);
}

static struct platform_device *enterprise_audio_devices[] __initdata = {
	&tegra_ahub_device,
	&tegra_dam_device0,
	&tegra_dam_device1,
	&tegra_dam_device2,
	&tegra_i2s_device0,
	&tegra_i2s_device1,
	&tegra_i2s_device3,
	&tegra_spdif_device,
	&spdif_dit_device,
	&bluetooth_dit_device,
	&baseband_dit_device,
	&tegra_pcm_device,
	&enterprise_audio_device,
	&enterprise_audio_aic326x_device,
};

static void enterprise_audio_init(void)
{
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	if (board_info.board_id == BOARD_E1239) {
		enterprise_audio_aic326x_pdata.i2s_param[BASEBAND].
					audio_port_id = 4;
		enterprise_audio_aic326x_pdata.i2s_param[BASEBAND].
					i2s_mode	= TEGRA_DAIFMT_I2S;
		enterprise_audio_aic326x_pdata.i2s_param[BASEBAND].
					channels	= 2;
		platform_device_register(&tegra_i2s_device4);
	} else {
		if (board_info.board_id == BOARD_E1197)
			enterprise_audio_pdata.i2s_param[HIFI_CODEC].
					audio_port_id = 1;
		else if (board_info.fab == BOARD_FAB_A04) {
			enterprise_audio_pdata.i2s_param[BASEBAND].
					audio_port_id = 4;
			platform_device_register(&tegra_i2s_device4);
		} else {
			enterprise_audio_pdata.i2s_param[BASEBAND].
					audio_port_id = 2;
			platform_device_register(&tegra_i2s_device2);
		}

	}
	platform_add_devices(enterprise_audio_devices,
		ARRAY_SIZE(enterprise_audio_devices));
}

static struct baseband_power_platform_data tegra_baseband_power_data = {
	.baseband_type = BASEBAND_XMM,
	.modem = {
		.xmm = {
			.bb_rst = XMM_GPIO_BB_RST,
			.bb_on = XMM_GPIO_BB_ON,
			.ipc_bb_wake = XMM_GPIO_IPC_BB_WAKE,
			.ipc_ap_wake = XMM_GPIO_IPC_AP_WAKE,
			.ipc_hsic_active = XMM_GPIO_IPC_HSIC_ACTIVE,
			.ipc_hsic_sus_req = XMM_GPIO_IPC_HSIC_SUS_REQ,
		},
	},
};

static struct platform_device tegra_baseband_power_device = {
	.name = "baseband_xmm_power",
	.id = -1,
	.dev = {
		.platform_data = &tegra_baseband_power_data,
	},
};

static struct platform_device tegra_baseband_power2_device = {
	.name = "baseband_xmm_power2",
	.id = -1,
	.dev = {
		.platform_data = &tegra_baseband_power_data,
	},
};

#ifdef CONFIG_TEGRA_BB_M7400
static union tegra_bb_gpio_id m7400_gpio_id = {
	.m7400 = {
		.pwr_status = GPIO_BB_RESET,
		.pwr_on = GPIO_BB_PWRON,
		.uart_awr = GPIO_BB_APACK,
		.uart_cwr = GPIO_BB_CPACK,
		.usb_awr = GPIO_BB_APACK2,
		.usb_cwr = GPIO_BB_CPACK2,
		.service = GPIO_BB_RSVD2,
		.resout2 = GPIO_BB_RSVD1,
	},
};

static struct tegra_bb_pdata m7400_pdata = {
	.id = &m7400_gpio_id,
	.device = &tegra_ehci2_device,
	.ehci_register = tegra_usb_hsic_host_register,
	.ehci_unregister = tegra_usb_hsic_host_unregister,
	.bb_id = TEGRA_BB_M7400,
};

static struct platform_device tegra_baseband_m7400_device = {
	.name = "tegra_baseband_power",
	.id = -1,
	.dev = {
		.platform_data = &m7400_pdata,
	},
};
#endif

static void enterprise_baseband_init(void)
{
	struct board_info board_info;

	int modem_id = tegra_get_modem_id();

	tegra_get_board_info(&board_info);

	switch (modem_id) {
	case TEGRA_BB_PH450: /* PH450 ULPI */
		enterprise_modem_init();
		break;
	case TEGRA_BB_XMM6260: /* XMM6260 HSIC */
		/* baseband-power.ko will register ehci2 device */
		tegra_ehci2_device.dev.platform_data =
					&tegra_ehci2_hsic_xmm_pdata;
		tegra_baseband_power_data.hsic_register =
						&tegra_usb_hsic_host_register;
		tegra_baseband_power_data.hsic_unregister =
						&tegra_usb_hsic_host_unregister;
		tegra_baseband_power_data.ehci_device =
					&tegra_ehci2_device;
		if ((board_info.board_id == BOARD_E1239) &&
			(board_info.fab <= BOARD_FAB_A02)) {
			tegra_baseband_power_data.modem.
				xmm.ipc_hsic_active  = BB_GPIO_LCD_PWR2;
			tegra_baseband_power_data.modem.
				xmm.ipc_hsic_sus_req = BB_GPIO_LCD_PWR1;
		}
		platform_device_register(&tegra_baseband_power_device);
		platform_device_register(&tegra_baseband_power2_device);
		break;
#ifdef CONFIG_TEGRA_BB_M7400
	case TEGRA_BB_M7400: /* M7400 HSIC */
		tegra_ehci2_hsic_xmm_pdata.u_data.host.power_off_on_suspend = 0;
		tegra_ehci2_device.dev.platform_data
			= &tegra_ehci2_hsic_xmm_pdata;
		platform_device_register(&tegra_baseband_m7400_device);
		break;
#endif
	}
}
static void enterprise_nfc_init(void)
{
	struct board_info bi;

	/* Enable firmware GPIO PX7 for board E1205 */
	tegra_get_board_info(&bi);
	if (bi.board_id == BOARD_E1205 && bi.fab >= BOARD_FAB_A03) {
		nfc_pdata.firm_gpio = TEGRA_GPIO_PX7;
	} else if (bi.board_id == BOARD_E1239) {
		nfc_pdata.firm_gpio = TEGRA_GPIO_PN6;
	}
}

static void __init tegra_enterprise_init(void)
{
	struct board_info board_info;
	tegra_get_board_info(&board_info);
	if (board_info.fab == BOARD_FAB_A04)
		tegra_clk_init_from_table(enterprise_clk_i2s4_table);
	else
		tegra_clk_init_from_table(enterprise_clk_i2s2_table);

	tegra_clk_init_from_table(enterprise_clk_init_table);
	tegra_enable_pinmux();
	tegra_smmu_init();
	tegra_soc_device_init("tegra_enterprise");
	enterprise_pinmux_init();
	enterprise_i2c_init();
	enterprise_uart_init();
	enterprise_usb_init();
#ifdef CONFIG_BT_BLUESLEEP
	if (board_info.board_id == BOARD_E1239)
		enterprise_bt_rfkill_pdata[0].shutdown_gpio = TEGRA_GPIO_PF4;
#endif
	platform_add_devices(enterprise_devices, ARRAY_SIZE(enterprise_devices));
	tegra_ram_console_debug_init();
	enterprise_regulator_init();
	tegra_io_dpd_init();
	enterprise_sdhci_init();
#ifdef CONFIG_TEGRA_EDP_LIMITS
	enterprise_edp_init();
#endif
	enterprise_kbc_init();
	enterprise_nfc_init();
	enterprise_touch_init();
	enterprise_audio_init();
	enterprise_baseband_init();
	enterprise_panel_init();
	if (tegra_get_commchip_id() == COMMCHIP_TI_WL18XX)
		enterprise_bt_st();
	else
#ifdef CONFIG_BT_BLUESLEEP
		enterprise_bt_rfkill();
	enterprise_setup_bluesleep();
#elif defined CONFIG_BLUEDROID_PM
		enterprise_bluedroid_pm();
#endif
	enterprise_emc_init();
	enterprise_sensors_init();
	enterprise_suspend_init();
	enterprise_bpc_mgmt_init();
	tegra_release_bootloader_fb();
	tegra_serial_debug_init(TEGRA_UARTD_BASE, INT_WDT_CPU, NULL, -1, -1);
	enterprise_vibrator_init();
	tegra_register_fuse();
}

static void __init tegra_enterprise_dt_init(void)
{
	tegra_enterprise_init();

#ifdef CONFIG_USE_OF
	of_platform_populate(NULL,
		of_default_bus_match_table, NULL, NULL);
#endif
}

static void __init tegra_enterprise_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	tegra_reserve(0, SZ_4M, SZ_8M);
#else
	tegra_reserve(SZ_128M, SZ_4M, SZ_8M);
#endif
	tegra_ram_console_debug_reserve(SZ_1M);
}

static const char *enterprise_dt_board_compat[] = {
	"nvidia,enterprise",
	NULL
};

static const char *tai_dt_board_compat[] = {
	"nvidia,tai",
	NULL
};

MACHINE_START(TEGRA_ENTERPRISE, "tegra_enterprise")
	.atag_offset	= 0x100,
	.soc		= &tegra_soc_desc,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_enterprise_reserve,
	.init_early	= tegra30_init_early,
	.init_irq       = tegra_init_irq,
	.handle_irq	= gic_handle_irq,
	.timer          = &tegra_timer,
	.init_machine   = tegra_enterprise_dt_init,
	.restart	= tegra_assert_system_reset,
	.dt_compat	= enterprise_dt_board_compat,
MACHINE_END

MACHINE_START(TAI, "tai")
	.atag_offset	= 0x100,
	.soc		= &tegra_soc_desc,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_enterprise_reserve,
	.init_early	= tegra30_init_early,
	.init_irq       = tegra_init_irq,
	.handle_irq	= gic_handle_irq,
	.timer          = &tegra_timer,
	.init_machine   = tegra_enterprise_dt_init,
	.restart	= tegra_assert_system_reset,
	.dt_compat	= tai_dt_board_compat,
MACHINE_END
