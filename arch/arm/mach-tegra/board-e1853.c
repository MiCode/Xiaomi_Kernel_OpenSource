/*
 * arch/arm/mach-tegra/board-e1853.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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
 *
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
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/platform_data/tegra_nor.h>
#include <linux/spi/spi.h>
#include <linux/tegra_uart.h>
#include <linux/mtd/partitions.h>
#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT)
#include <linux/i2c/atmel_mxt_ts.h>
#endif
#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/io_dpd.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/pci.h>
#include <mach/audio.h>
#include <mach/tegra_asoc_vcm_pdata.h>
#include <mach/ioexpander.h>
#include <asm/mach/flash.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <mach/usb_phy.h>
#include <sound/wm8903.h>
#include <mach/tsensor.h>
#include "board.h"
#include "clock.h"
#include "board-e1853.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "common.h"
#include "pm.h"

#define IO_EXPANDER_ADDR	(0x75)
#define BT_RESET_BIT_POS	(IO_EXP_PIN_0)
#define BT_ENABLE_BIT_POS	(IO_EXP_PIN_5)
#define BT_WAKEUP_BIT_POS	(IO_EXP_PIN_7)

static __initdata struct tegra_clk_init_table e1853_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pll_m",		NULL,		0,		true},
	{ "hda",		"pll_p",	108000000,	false},
	{ "hda2codec_2x",	"pll_p",	48000000,	false},
	{ "pwm",		"clk_32k",	32768,		false},
	{ "blink",		"clk_32k",	32768,		true},
	{ "pll_a",		NULL,		552960000,	false},
	/* audio cif clock should be faster than i2s */
	{ "pll_a_out0",		NULL,		24576000,	false},
	{ "d_audio",		"pll_a_out0",	24576000,	false},
	{ "nor",		"pll_p",	86500000,	true},
	{ "uarta",		"pll_p",	480000000,	true},
	{ "uartb",		"pll_p",	480000000,	true},
	{ "uartc",		"pll_p",	480000000,	true},
	{ "uartd",		"pll_p",	480000000,	true},
	{ "sdmmc2",		"pll_p",	52000000,	true},
	{ "sbc2",		"pll_m",	100000000,	true},
	{ "sbc3",		"pll_m",	100000000,	true},
	{ "sbc4",		"pll_m",	100000000,	true},
	{ "sbc5",		"pll_m",	100000000,	true},
	{ "sbc6",		"pll_m",	100000000,	true},
	{ "cpu_g",		"cclk_g",	900000000,	true},
	{ "i2s0",		"pll_a_out0",	24576000,	false},
	{ "i2s1",		"pll_a_out0",	24576000,	false},
	{ "i2s2",		"pll_a_out0",	24576000,	false},
	{ "i2s3",		"pll_a_out0",	24576000,	false},
	{ "i2s4",		"pll_a_out0",	24576000,	false},
	{ "audio0",		"i2s0_sync",	12288000,	false},
	{ "audio1",		"i2s1_sync",	12288000,	false},
	{ "audio2",		"i2s2_sync",	12288000,	false},
	{ "audio3",		"i2s3_sync",	12288000,	false},
	{ "audio4",		"i2s4_sync",	12288000,	false},
	{ "apbif",		"clk_m",	12000000,	false},
	{ "dam0",		"clk_m",	12000000,	true},
	{ "dam1",		"clk_m",	12000000,	true},
	{ "dam2",		"clk_m",	12000000,	true},
	{ "vi",			"pll_p",	470000000,	false},
	{ "vi_sensor",		"pll_p",	150000000,	false},
	{ "vde",		"pll_c",	484000000,	true},
	{ "host1x",		"pll_c",	242000000,	true},
	{ "mpe",		"pll_c",	484000000,	true},
	{ "se",			"pll_m",	625000000,	true},
	{ "i2c1",		"pll_p",	3200000,	true},
	{ "i2c2",		"pll_p",	3200000,	true},
	{ "i2c3",		"pll_p",	3200000,	true},
	{ "i2c4",		"pll_p",	3200000,	true},
	{ "i2c5",		"pll_p",	3200000,	true},
	{ "sdmmc2",		"pll_p",	104000000,	false},
	{"wake.sclk",		NULL,		334000000,	true },
	{ NULL,			NULL,		0,		0},
};

static struct tegra_i2c_platform_data e1853_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data e1853_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.is_clkon_always = true,
};

static struct tegra_i2c_platform_data e1853_i2c4_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data e1853_i2c5_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_pci_platform_data e1853_pci_platform_data = {
	.port_status[0] = 1,
	.port_status[1] = 1,
	.port_status[2] = 1,
	.use_dock_detect = 0,
	.gpio 		= 0,
};

static void e1853_pcie_init(void)
{
	tegra_pci_device.dev.platform_data = &e1853_pci_platform_data;
	platform_device_register(&tegra_pci_device);
}

static void e1853_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &e1853_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &e1853_i2c2_platform_data;
	tegra_i2c_device4.dev.platform_data = &e1853_i2c4_platform_data;
	tegra_i2c_device5.dev.platform_data = &e1853_i2c5_platform_data;

	platform_device_register(&tegra_i2c_device5);
	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device1);
}

static struct platform_device *e1853_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
};

static void __init uart_debug_init(void)
{
	/* UARTA is the debug port. */
	pr_info("Selecting UARTA as the debug console\n");
	e1853_uart_devices[0] = &debug_uarta_device;
	debug_uart_clk = clk_get_sys("serial8250.0", "uarta");
	debug_uart_port_base = ((struct plat_serial8250_port *)(
				debug_uarta_device.dev.platform_data))->mapbase;
}

static void __init e1853_uart_init(void)
{
	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs()) {
		uart_debug_init();
		/* Clock enable for the debug channel */
		if (!IS_ERR_OR_NULL(debug_uart_clk)) {
			pr_info("The debug console clock name is %s\n",
						debug_uart_clk->name);
			clk_enable(debug_uart_clk);
			clk_set_rate(debug_uart_clk, 408000000);
		} else {
			pr_err("Not getting the clock %s for debug console\n",
					debug_uart_clk->name);
		}
	}

	platform_add_devices(e1853_uart_devices,
				ARRAY_SIZE(e1853_uart_devices));
}

#if defined(CONFIG_RTC_DRV_TEGRA)
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
#endif

static struct tegra_asoc_vcm_platform_data jetson_audio_pdata = {
	.codec_info[0] = {
		.codec_dai_name = "dit-hifi",
		.cpu_dai_name = "tegra30-i2s.0",
		.codec_name = "spdif-dit.0",
		.name = "tegra-i2s-1",
		.pcm_driver = "tegra-pcm-audio",
		.i2s_format = format_i2s,
		/* Audio Codec is Master */
		.master = 1,
	},
	.codec_info[1] = {
		.codec_dai_name = "dit-hifi",
		.cpu_dai_name = "tegra30-i2s.3",
		.codec_name = "spdif-dit.1",
		.name = "tegra-i2s-2",
		.pcm_driver = "tegra-tdm-pcm-audio",
		.i2s_format = format_tdm,
		/* Audio Codec is Master */
		.master = 1,
		.num_slots = 8,
		.slot_width = 32,
		.tx_mask = 0xff,
		.rx_mask = 0xff,
	},
};

static struct platform_device generic_codec_1 = {
	.name		= "spdif-dit",
	.id			= 0,
};
static struct platform_device generic_codec_2 = {
	.name		= "spdif-dit",
	.id			= 1,
};

static struct platform_device tegra_snd_e1853 = {
	.name       = "tegra-snd-e1853",
	.id = 0,
	.dev    = {
		.platform_data = &jetson_audio_pdata,
	},
};

static void e1853_i2s_audio_init(void)
{
	platform_device_register(&tegra_pcm_device);
	platform_device_register(&tegra_tdm_pcm_device);
	platform_device_register(&generic_codec_1);
	platform_device_register(&generic_codec_2);
	platform_device_register(&tegra_i2s_device0);
	platform_device_register(&tegra_i2s_device3);
	platform_device_register(&tegra_ahub_device);
	platform_device_register(&tegra_snd_e1853);
}


#if defined(CONFIG_SPI_TEGRA) && defined(CONFIG_SPI_SPIDEV)
static struct spi_board_info tegra_spi_devices[] __initdata = {
	{
		.modalias = "spidev",
		.bus_num = 1,
		.chip_select = 1,
		.mode = SPI_MODE_0,
		.max_speed_hz = 18000000,
		.platform_data = NULL,
		.irq = 0,
	},
	{
		.modalias = "spidev",
		.bus_num = 2,
		.chip_select = 1,
		.mode = SPI_MODE_0,
		.max_speed_hz = 18000000,
		.platform_data = NULL,
		.irq = 0,
	},
	{
		.modalias = "spidev",
		.bus_num = 4,
		.chip_select = 2,
		.mode = SPI_MODE_0,
		.max_speed_hz = 18000000,
		.platform_data = NULL,
		.irq = 0,
	},
};

static void __init e1853_register_spidev(void)
{
	spi_register_board_info(tegra_spi_devices,
			ARRAY_SIZE(tegra_spi_devices));
}
#else
#define e1853_register_spidev() do {} while (0)
#endif


static void e1853_spi_init(void)
{
	tegra_spi_device3.name = "spi_slave_tegra";
	platform_device_register(&tegra_spi_device2);
	platform_device_register(&tegra_spi_device3);
	platform_device_register(&tegra_spi_device5);
	e1853_register_spidev();
}

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

static struct platform_device *e1853_devices[] __initdata = {
#if defined(CONFIG_TEGRA_AVP)
	&tegra_avp_device,
#endif
#if defined(CONFIG_RTC_DRV_TEGRA)
	&tegra_rtc_device,
#endif
	&tegra_camera,
	&tegra_wdt0_device
};

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT

#define MXT_CONFIG_CRC  0xD62DE8
static const u8 config[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0x32, 0x0A, 0x00, 0x14, 0x14, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x00,
	0x1B, 0x2A, 0x00, 0x20, 0x3C, 0x04, 0x05, 0x00,
	0x02, 0x01, 0x00, 0x0A, 0x0A, 0x0A, 0x0A, 0xFF,
	0x02, 0x55, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x64, 0x02, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23,
	0x00, 0x00, 0x00, 0x05, 0x0A, 0x15, 0x1E, 0x00,
	0x00, 0x04, 0xFF, 0x03, 0x3F, 0x64, 0x64, 0x01,
	0x0A, 0x14, 0x28, 0x4B, 0x00, 0x02, 0x00, 0x64,
	0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x10, 0x3C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define MXT_CONFIG_CRC_SKU2000  0xA24D9A
static const u8 config_sku2000[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0x32, 0x0A, 0x00, 0x14, 0x14, 0x19,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x00,
	0x1B, 0x2A, 0x00, 0x20, 0x3A, 0x04, 0x05, 0x00, /* 23=thr 2 di */
	0x04, 0x04, 0x41, 0x0A, 0x0A, 0x0A, 0x0A, 0xFF,
	0x02, 0x55, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, /* 0A=limit */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23,
	0x00, 0x00, 0x00, 0x05, 0x0A, 0x15, 0x1E, 0x00,
	0x00, 0x04, 0x00, 0x03, 0x3F, 0x64, 0x64, 0x01,
	0x0A, 0x14, 0x28, 0x4B, 0x00, 0x02, 0x00, 0x64,
	0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x10, 0x3C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static struct mxt_platform_data atmel_mxt_info = {
	.x_line         = 27,
	.y_line         = 42,
	.x_size         = 768,
	.y_size         = 1366,
	.blen           = 0x20,
	.threshold      = 0x3C,
	.voltage        = 3300000,              /* 3.3V */
	.orient         = 5,
	.config         = config,
	.config_length  = 157,
	.config_crc     = MXT_CONFIG_CRC,
	.irqflags       = IRQF_TRIGGER_FALLING,
	.read_chg       = NULL,
};

static struct i2c_board_info __initdata atmel_i2c_info[] = {
	{
		I2C_BOARD_INFO("atmel_mxt_ts", 0x5A),
		.irq = TEGRA_GPIO_TO_IRQ(TOUCH_GPIO_IRQ_ATMEL_T9),
		.platform_data = &atmel_mxt_info,
	}
};

static __initdata struct tegra_clk_init_table spi_clk_init_table[] = {
	/* name         parent          rate            enabled */
	{ "sbc1",       "pll_p",        52000000,       true},
	{ NULL,         NULL,           0,              0},
};

static int __init e1853_touch_init(void)
{
	tegra_gpio_enable(TOUCH_GPIO_IRQ_ATMEL_T9);
	tegra_gpio_enable(TOUCH_GPIO_RST_ATMEL_T9);

	gpio_request(TOUCH_GPIO_IRQ_ATMEL_T9, "atmel-irq");
	gpio_direction_input(TOUCH_GPIO_IRQ_ATMEL_T9);

	gpio_request(TOUCH_GPIO_RST_ATMEL_T9, "atmel-reset");
	gpio_direction_output(TOUCH_GPIO_RST_ATMEL_T9, 0);
	usleep_range(1000, 2000);
	gpio_set_value(TOUCH_GPIO_RST_ATMEL_T9, 1);
	msleep(100);

	atmel_mxt_info.config = config_sku2000;
	atmel_mxt_info.config_crc = MXT_CONFIG_CRC_SKU2000;

	i2c_register_board_info(TOUCH_BUS_ATMEL_T9, atmel_i2c_info, 1);

	return 0;
}

#endif /* CONFIG_TOUCHSCREEN_ATMEL_MXT */

#if defined(CONFIG_USB_G_ANDROID)
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
		.idle_wait_delay = 17,
		.elastic_limit = 16,
		.term_range_adj = 6,
		.xcvr_setup = 63,
		.xcvr_setup_offset = 6,
		.xcvr_use_fuses = 1,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_use_lsb = 1,
	},
};
#else
static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
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
		.idle_wait_delay = 17,
		.elastic_limit = 16,
		.term_range_adj = 6,
		.xcvr_setup = 63,
		.xcvr_setup_offset = 6,
		.xcvr_use_fuses = 1,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_use_lsb = 1,
	},
};
#endif

static struct tegra_usb_platform_data tegra_ehci2_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
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
		.idle_wait_delay = 17,
		.elastic_limit = 16,
		.term_range_adj = 6,
		.xcvr_setup = 63,
		.xcvr_setup_offset = 6,
		.xcvr_use_fuses = 1,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_use_lsb = 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
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
		.idle_wait_delay = 17,
		.elastic_limit = 16,
		.term_range_adj = 6,
		.xcvr_setup = 63,
		.xcvr_setup_offset = 6,
		.xcvr_use_fuses = 1,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_use_lsb = 1,
	},
};

static void e1853_usb_init(void)
{
	/* Need to parse sku info to decide host/device mode */

	/* G_ANDROID require device mode */
#if defined(CONFIG_USB_G_ANDROID)
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
	platform_device_register(&tegra_udc_device);
#else
	tegra_ehci1_device.dev.platform_data = &tegra_ehci1_utmi_pdata;
	platform_device_register(&tegra_ehci1_device);
#endif
	tegra_ehci2_device.dev.platform_data = &tegra_ehci2_utmi_pdata;
	platform_device_register(&tegra_ehci2_device);

	tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
	platform_device_register(&tegra_ehci3_device);
}

static struct tegra_nor_platform_data e1853_nor_data = {
	.flash = {
		.map_name = "cfi_probe",
		.width = 2,
	},
	.chip_parms = {
		.MuxMode = NorMuxMode_ADMux,
		.BurstLength = NorBurstLength_CntBurst,
		.ReadMode = NorReadMode_Async,
		.ReadyActive = NorReadyActive_BeforeData,
		/* FIXME: Need to use characterized value */
		.timing_default = {
			.timing0 = 0xA0A05585,
			.timing1 = 0x200A0406,
		},
		.timing_read = {
			.timing0 = 0xA0A05585,
			.timing1 = 0x00050406,
		},
	},
};

static void e1853_nor_init(void)
{
	tegra_nor_device.resource[2].end = TEGRA_NOR_FLASH_BASE + SZ_64M - 1;
	tegra_nor_device.dev.platform_data = &e1853_nor_data;
	platform_device_register(&tegra_nor_device);
}

static void __init tegra_e1853_init(void)
{
	tegra_init_board_info();
	tegra_clk_init_from_table(e1853_clk_init_table);
	tegra_enable_pinmux();
	tegra_smmu_init();
	tegra_soc_device_init("e1853");
	e1853_pinmux_init();
	e1853_i2c_init();
	e1853_gpio_init();
/*	e1853_regulator_init();
	e1853_suspend_init(); */
	e1853_i2s_audio_init();
	e1853_uart_init();
	e1853_usb_init();
	tegra_io_dpd_init();
	e1853_sdhci_init();
	e1853_spi_init();
	platform_add_devices(e1853_devices, ARRAY_SIZE(e1853_devices));
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT
	e1853_touch_init();
#endif
	e1853_panel_init();
	e1853_nor_init();
	e1853_pcie_init();
}

static void __init tegra_e1853_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	tegra_reserve(0, SZ_8M, SZ_8M);
#else
	tegra_reserve(SZ_128M, SZ_8M, SZ_8M);
#endif
}

static int __init e1853_bt_init(void)
{
	struct i2c_adapter *adapter;
	struct i2c_board_info info = { {0} };
	struct i2c_client *client = NULL;
	struct i2c_msg msg;
	u8 cmd_buf[2];
	int ret = 0;

	/* Program the IO Expander */
	adapter = i2c_get_adapter(1);
	if (!adapter) {
		printk(KERN_WARNING "%s: adapter is null\n", __func__);
		ret = -ENXIO;
		goto i2c_done;
	}

	info.addr = IO_EXPANDER_ADDR;
	client = i2c_new_device(adapter, &info);
	i2c_put_adapter(adapter);
	if (!client) {
		printk(KERN_WARNING "%s: client is null\n", __func__);
		ret = -ENXIO;
		goto i2c_done;
	}

	/* Set output state for BT_RST, BT_EN and BT_WAKEUP */
	/* Read register contents of OUTPUT_PORT_REG */
	cmd_buf[0] = IO_EXP_OUTPUT_PORT_REG_0;
	msg.addr = IO_EXPANDER_ADDR;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = &cmd_buf[0];
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto i2c_done;

	msg.addr = IO_EXPANDER_ADDR;
	msg.flags = I2C_M_RD;
	msg.len = 1;
	msg.buf = &cmd_buf[1];
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto i2c_done;

	/* Set required output values and write  */
	cmd_buf[0] = IO_EXP_OUTPUT_PORT_REG_0;
	cmd_buf[1] |= ((1 << BT_ENABLE_BIT_POS) |
			(1 << BT_WAKEUP_BIT_POS) |
			(1 << BT_RESET_BIT_POS));
	msg.addr = IO_EXPANDER_ADDR;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = &cmd_buf[0];
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto i2c_done;

	/* Set BT_RST, BT_EN and BT_WAKEUP as output pins */
	/* Read register contents of CONFIG_REG */
	cmd_buf[0] = IO_EXP_CONFIG_REG_0;
	msg.addr = IO_EXPANDER_ADDR;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = &cmd_buf[0];
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto i2c_done;

	msg.addr = IO_EXPANDER_ADDR;
	msg.flags = I2C_M_RD;
	msg.len = 1;
	msg.buf = &cmd_buf[1];
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto i2c_done;

	/* Set required direction bits */
	cmd_buf[0] = IO_EXP_CONFIG_REG_0;
	cmd_buf[1] &=  (~((1 << BT_ENABLE_BIT_POS) |
			(1 << BT_WAKEUP_BIT_POS) |
			(1 << BT_RESET_BIT_POS)));
	msg.addr = IO_EXPANDER_ADDR;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = &cmd_buf[0];
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto i2c_done;

	ret = 0;

i2c_done:
	if (ret < 0)
		printk(KERN_ERR "%s: I2C transaction failed\n", __func__);
	if (client)
		i2c_unregister_device(client);
	return ret;
}

late_initcall(e1853_bt_init);

MACHINE_START(E1853, "e1853")
	.atag_offset    = 0x100,
	.soc		= &tegra_soc_desc,
	.init_irq       = tegra_init_irq,
	.init_early     = tegra30_init_early,
	.init_machine   = tegra_e1853_init,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_e1853_reserve,
	.timer          = &tegra_timer,
	.handle_irq	= gic_handle_irq,
MACHINE_END
