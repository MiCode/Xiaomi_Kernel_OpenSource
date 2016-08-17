/*
 * arch/arm/mach-tegra/board-pluto.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <linux/spi/rm31080a_ts.h>
#include <linux/tegra_uart.h>
#include <linux/memblock.h>
#include <linux/spi-tegra.h>
#include <linux/nfc/pn544.h>
#include <linux/nfc/bcm2079x.h>
#include <linux/rfkill-gpio.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <linux/regulator/consumer.h>
#include <linux/smb349-charger.h>
#include <linux/max17048_battery.h>
#include <linux/leds.h>
#include <linux/i2c/at24.h>
#include <linux/mfd/max8831.h>
#include <linux/of_platform.h>
#include <linux/a2220.h>
#include <linux/mfd/tlv320aic3262-registers.h>
#include <linux/mfd/tlv320aic3xxx-core.h>

#include <asm/hardware/gic.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t11.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/io_dpd.h>
#include <mach/i2s.h>
#include <mach/tegra_asoc_pdata.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/usb_phy.h>
#include <mach/gpio-tegra.h>
#include <mach/tegra_fiq_debugger.h>
#include <mach/tegra-bb-power.h>
#include <linux/platform_data/tegra_usb_modem_power.h>
#include <mach/hardware.h>
#include <mach/xusb.h>

#include "board.h"
#include "board-common.h"
#include "board-touch-raydium.h"
#include "clock.h"
#include "board-pluto.h"
#include "baseband-xmm-power.h"
#include "tegra-board-id.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "pm.h"
#include "common.h"


#ifdef CONFIG_BT_BLUESLEEP
static struct rfkill_gpio_platform_data pluto_bt_rfkill_pdata = {
	.name           = "bt_rfkill",
	.shutdown_gpio  = TEGRA_GPIO_PQ7,
	.reset_gpio	= TEGRA_GPIO_PQ6,
	.type           = RFKILL_TYPE_BLUETOOTH,
};

static struct platform_device pluto_bt_rfkill_device = {
	.name = "rfkill_gpio",
	.id             = -1,
	.dev = {
		.platform_data = &pluto_bt_rfkill_pdata,
	},
};

static noinline void __init pluto_setup_bt_rfkill(void)
{
	platform_device_register(&pluto_bt_rfkill_device);
}

static struct resource pluto_bluesleep_resources[] = {
	[0] = {
		.name = "gpio_host_wake",
			.start  = TEGRA_GPIO_PU6,
			.end    = TEGRA_GPIO_PU6,
			.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "gpio_ext_wake",
			.start  = TEGRA_GPIO_PEE1,
			.end    = TEGRA_GPIO_PEE1,
			.flags  = IORESOURCE_IO,
	},
	[2] = {
		.name = "host_wake",
			.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct platform_device pluto_bluesleep_device = {
	.name           = "bluesleep",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(pluto_bluesleep_resources),
	.resource       = pluto_bluesleep_resources,
};

static noinline void __init pluto_setup_bluesleep(void)
{
	pluto_bluesleep_resources[2].start =
		pluto_bluesleep_resources[2].end =
			gpio_to_irq(TEGRA_GPIO_PU6);
	platform_device_register(&pluto_bluesleep_device);
	return;
}
#elif defined CONFIG_BLUEDROID_PM
static struct resource pluto_bluedroid_pm_resources[] = {
	[0] = {
		.name   = "shutdown_gpio",
		.start  = TEGRA_GPIO_PQ7,
		.end    = TEGRA_GPIO_PQ7,
		.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "host_wake",
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
	[2] = {
		.name = "gpio_ext_wake",
		.start  = TEGRA_GPIO_PEE1,
		.end    = TEGRA_GPIO_PEE1,
		.flags  = IORESOURCE_IO,
	},
	[3] = {
		.name = "gpio_host_wake",
		.start  = TEGRA_GPIO_PU6,
		.end    = TEGRA_GPIO_PU6,
		.flags  = IORESOURCE_IO,
	},
	[4] = {
		.name = "reset_gpio",
		.start  = TEGRA_GPIO_PQ6,
		.end    = TEGRA_GPIO_PQ6,
		.flags  = IORESOURCE_IO,
	},
	[5] = {
		.name = "min_cpu_freq",
		.start  = 102000,
		.end    = 102000,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device pluto_bluedroid_pm_device = {
	.name = "bluedroid_pm",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(pluto_bluedroid_pm_resources),
	.resource       = pluto_bluedroid_pm_resources,
};

static noinline void __init pluto_setup_bluedroid_pm(void)
{
	pluto_bluedroid_pm_resources[1].start =
		pluto_bluedroid_pm_resources[1].end =
					gpio_to_irq(TEGRA_GPIO_PU6);
	platform_device_register(&pluto_bluedroid_pm_device);
}
#endif

static __initdata struct tegra_clk_init_table pluto_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pll_m",	NULL,		0,		false},
	{ "hda",	"pll_p",	108000000,	false},
	{ "hda2codec_2x", "pll_p",	48000000,	false},
	{ "pwm",	"pll_p",	3187500,	false},
	{ "i2s1",	"pll_a_out0",	0,		false},
	{ "i2s2",	"pll_a_out0",	0,		false},
	{ "i2s3",	"pll_a_out0",	0,		false},
	{ "i2s4",	"pll_a_out0",	0,		false},
	{ "spdif_out",	"pll_a_out0",	0,		false},
	{ "d_audio",	"clk_m",	12000000,	false},
	{ "dam0",	"clk_m",	12000000,	false},
	{ "dam1",	"clk_m",	12000000,	false},
	{ "dam2",	"clk_m",	12000000,	false},
	{ "audio0",	"i2s0_sync",	0,		false},
	{ "audio1",	"i2s1_sync",	0,		false},
	{ "audio2",	"i2s2_sync",	0,		false},
	{ "audio3",	"i2s3_sync",	0,		false},
	{ "audio4",	"i2s4_sync",	0,		false},
	{ "vi_sensor",	"pll_p",	150000000,	false},
	{ "cilab",	"pll_p",	150000000,	false},
	{ "cilcd",	"pll_p",	150000000,	false},
	{ "cile",	"pll_p",	150000000,	false},
	{ "i2c1",	"pll_p",	3200000,	false},
	{ "i2c2",	"pll_p",	3200000,	false},
	{ "i2c3",	"pll_p",	3200000,	false},
	{ "i2c4",	"pll_p",	3200000,	false},
	{ "i2c5",	"pll_p",	3200000,	false},
	{ "extern3",	"clk_m",	12000000,	false},
	{ "dsia",	"pll_d2_out0",	0,		false},
	{ NULL,		NULL,		0,		0},
};

static struct bcm2079x_platform_data nfc_pdata = {
	.irq_gpio = TEGRA_GPIO_PW2,
	.en_gpio = TEGRA_GPIO_PU4,
	.wake_gpio = TEGRA_GPIO_PX7,
	};

static struct i2c_board_info __initdata pluto_i2c_bus3_board_info[] = {
	{
		I2C_BOARD_INFO("bcm2079x-i2c", 0x77),
		.platform_data = &nfc_pdata,
	},
};

static struct tegra_i2c_platform_data pluto_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_I2C1_SCL, 0},
	.sda_gpio		= {TEGRA_GPIO_I2C1_SDA, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data pluto_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.is_clkon_always = true,
	.scl_gpio		= {TEGRA_GPIO_I2C2_SCL, 0},
	.sda_gpio		= {TEGRA_GPIO_I2C2_SDA, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data pluto_i2c3_platform_data = {
	.adapter_nr	= 2,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.scl_gpio		= {TEGRA_GPIO_I2C3_SCL, 0},
	.sda_gpio		= {TEGRA_GPIO_I2C3_SDA, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data pluto_i2c4_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 10000, 0 },
	.scl_gpio		= {TEGRA_GPIO_I2C4_SCL, 0},
	.sda_gpio		= {TEGRA_GPIO_I2C4_SDA, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data pluto_i2c5_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.scl_gpio		= {TEGRA_GPIO_I2C5_SCL, 0},
	.sda_gpio		= {TEGRA_GPIO_I2C5_SDA, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct aic3262_gpio_setup aic3262_gpio[] = {
	/* GPIO 1*/
	{
		.used = 1,
		.in = 0,
		.value = AIC3262_GPIO1_FUNC_INT1_OUTPUT ,
	},
	/* GPIO 2*/
	{
		.used = 1,
		.in = 0,
		.value = AIC3262_GPIO2_FUNC_ADC_MOD_CLK_OUTPUT,
	},
	/* GPI1 */
	{
		.used = 1,
		.in = 1,
	},
	/* GPI2 */
	{
		.used = 1,
		.in = 1,
		.in_reg = AIC3262_DMIC_INPUT_CNTL,
		.in_reg_bitmask	= AIC3262_DMIC_CONFIGURE_MASK,
		.in_reg_shift = AIC3262_DMIC_CONFIGURE_SHIFT,
		.value = AIC3262_DMIC_GPI2_LEFT_GPI2_RIGHT,
	},
	/* GPO1 */
	{
		.used = 1,
		.in = 0,
		.value = AIC3262_GPO1_FUNC_MSO_OUTPUT_FOR_SPI,
	},
};
static struct aic3xxx_pdata aic3262_codec_pdata = {
	.gpio_irq	= 0,
	.gpio		= aic3262_gpio,
	.naudint_irq    = 0,
	.irq_base       = AIC3262_CODEC_IRQ_BASE,
};

static struct i2c_board_info __initdata cs42l73_board_info = {
	I2C_BOARD_INFO("cs42l73", 0x4a),
};

static struct i2c_board_info __initdata pluto_codec_a2220_info = {
	I2C_BOARD_INFO("audience_a2220", 0x3E),
};

static struct i2c_board_info __initdata pluto_codec_aic326x_info = {
	I2C_BOARD_INFO("tlv320aic3262", 0x18),
	.platform_data = &aic3262_codec_pdata,
};

static void pluto_i2c_init(void)
{
	tegra11_i2c_device1.dev.platform_data = &pluto_i2c1_platform_data;
	tegra11_i2c_device2.dev.platform_data = &pluto_i2c2_platform_data;
	tegra11_i2c_device3.dev.platform_data = &pluto_i2c3_platform_data;
	tegra11_i2c_device4.dev.platform_data = &pluto_i2c4_platform_data;
	tegra11_i2c_device5.dev.platform_data = &pluto_i2c5_platform_data;

	platform_device_register(&tegra11_i2c_device5);
	platform_device_register(&tegra11_i2c_device4);
	platform_device_register(&tegra11_i2c_device3);
	platform_device_register(&tegra11_i2c_device2);
	platform_device_register(&tegra11_i2c_device1);

	i2c_register_board_info(0, &pluto_codec_a2220_info, 1);
	i2c_register_board_info(0, &cs42l73_board_info, 1);
	pluto_i2c_bus3_board_info[0].irq = gpio_to_irq(TEGRA_GPIO_PW2);
	i2c_register_board_info(0, pluto_i2c_bus3_board_info, 1);
	i2c_register_board_info(0, &pluto_codec_aic326x_info, 1);
}

static struct platform_device *pluto_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
};
static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "clk_m"},
	[1] = {.name = "pll_p"},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	[2] = {.name = "pll_m"},
#endif
};

static struct tegra_uart_platform_data pluto_uart_pdata;
static struct tegra_uart_platform_data pluto_loopback_uart_pdata;

static void __init uart_debug_init(void)
{
	int debug_port_id;

	debug_port_id = uart_console_debug_init(3);
	if (debug_port_id < 0)
		return;
	pluto_uart_devices[debug_port_id] = uart_console_debug_device;
}

static void __init pluto_uart_init(void)
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
	pluto_uart_pdata.parent_clk_list = uart_parent_clk;
	pluto_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	pluto_loopback_uart_pdata.parent_clk_list = uart_parent_clk;
	pluto_loopback_uart_pdata.parent_clk_count =
						ARRAY_SIZE(uart_parent_clk);
	pluto_loopback_uart_pdata.is_loopback = true;
	tegra_uarta_device.dev.platform_data = &pluto_uart_pdata;
	tegra_uartb_device.dev.platform_data = &pluto_uart_pdata;
	tegra_uartc_device.dev.platform_data = &pluto_uart_pdata;
	tegra_uartd_device.dev.platform_data = &pluto_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(pluto_uart_devices,
				ARRAY_SIZE(pluto_uart_devices));
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

static struct tegra_asoc_platform_data pluto_audio_pdata = {
	.gpio_spkr_en		= TEGRA_GPIO_SPKR_EN,
	.gpio_hp_det		= TEGRA_GPIO_HP_DET,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= TEGRA_GPIO_INT_MIC_EN,
	.gpio_ext_mic_en	= TEGRA_GPIO_EXT_MIC_EN,
	.gpio_ldo1_en		= TEGRA_GPIO_LDO1_EN,
	.edp_support		=  true,
	.edp_states		= {1776, 888, 0},
	.i2s_param[HIFI_CODEC]	= {
		.audio_port_id	= 1,
		.is_i2s_master	= 0,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
		.channels       = 2,
	},
	.i2s_param[BASEBAND]	= {
		.audio_port_id	= 2,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
		.rate		= 16000,
		.channels	= 2,
		.bit_clk	= 1024000,
	},
	.i2s_param[BT_SCO]	= {
		.audio_port_id	= 3,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
		.sample_size	= 16,
		.channels	= 1,
		.bit_clk	= 512000,
	},
	.i2s_param[VOICE_CODEC]	= {
		.audio_port_id	= 0,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
		.rate		= 16000,
		.channels	= 2,
	},
};

static struct tegra_asoc_platform_data pluto_aic3262_pdata = {
	.gpio_spkr_en		= TEGRA_GPIO_SPKR_EN,
	.gpio_hp_det		= TEGRA_GPIO_HP_DET,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= TEGRA_GPIO_INT_MIC_EN,
	.gpio_ext_mic_en	= TEGRA_GPIO_EXT_MIC_EN,
	.gpio_ldo1_en		= TEGRA_GPIO_LDO1_EN,
	.edp_support		= true,
	.edp_states		= {1776, 888, 0},
	.i2s_param[HIFI_CODEC]	= {
		.audio_port_id	= 1,
		.is_i2s_master	= 0,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
		.rate		= 48000,
		.channels	= 2,
	},
	.i2s_param[BASEBAND]	= {
		.audio_port_id	= 2,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
		.rate		= 16000,
		.channels	= 2,
		.bit_clk	= 1024000,
	},
	.i2s_param[BT_SCO]	= {
		.audio_port_id	= 3,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
		.sample_size	= 16,
		.channels	= 1,
		.bit_clk	= 512000,
	},
	.i2s_param[VOICE_CODEC]	= {
		.audio_port_id	= 0,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
		.sample_size	= 16,
		.rate		= 16000,
		.channels	= 2,
	},
};

static struct platform_device pluto_audio_device = {
	.name	= "tegra-snd-cs42l73",
	.id	= 2,
	.dev	= {
		.platform_data = &pluto_audio_pdata,
	},
};

static struct platform_device pluto_audio_aic326x_device = {
	.name	= "tegra-snd-aic326x",
	.id	= 2,
	.dev	= {
		.platform_data  = &pluto_aic3262_pdata,
	},
};


static struct tegra_spi_device_controller_data dev_bdata = {
	.rx_clk_tap_delay = 0,
	.tx_clk_tap_delay = 0,
};
static struct spi_board_info aic326x_spi_board_info[] = {
	{
		.modalias = "tlv320aic3xxx",
		.bus_num = 3,
		.chip_select = 0,
		.max_speed_hz = 4*1000*1000,
		.mode = SPI_MODE_1,
		.controller_data = &dev_bdata,
		.platform_data = &aic3262_codec_pdata,
	},
};



#ifdef CONFIG_MHI_NETDEV
struct platform_device mhi_netdevice0 = {
	.name = "mhi_net_device",
	.id = 0,
};
#endif /* CONFIG_MHI_NETDEV */

static struct platform_device *pluto_devices[] __initdata = {
	&tegra_pmu_device,
	&tegra_rtc_device,
	&tegra_udc_device,
#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
	&tegra_smmu_device,
#endif
#if defined(CONFIG_TEGRA_WATCHDOG)
	&tegra_wdt0_device,
#endif
#if defined(CONFIG_TEGRA_AVP)
	&tegra_avp_device,
#endif
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	&tegra11_se_device,
#endif
	&tegra_ahub_device,
	&tegra_pcm_device,
	&tegra_dam_device0,
	&tegra_dam_device1,
	&tegra_dam_device2,
	&tegra_i2s_device0,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_i2s_device3,
	&tegra_i2s_device4,
	&tegra_spdif_device,
	&spdif_dit_device,
	&bluetooth_dit_device,
	&baseband_dit_device,
	&pluto_audio_device,
	&pluto_audio_aic326x_device,
	&tegra_hda_device,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_AES)
	&tegra_aes_device,
#endif
#ifdef CONFIG_MHI_NETDEV
	&mhi_netdevice0,  /* MHI netdevice */
#endif /* CONFIG_MHI_NETDEV */
};

#ifdef CONFIG_USB_SUPPORT

static void pluto_usb_hsic_postsupend(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L2);
#endif
}

static void pluto_usb_hsic_preresume(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L2TOL0);
#endif
}

static void pluto_usb_hsic_post_resume(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L0);
#endif
}

static void pluto_usb_hsic_phy_power(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L0);
#endif
}

static void pluto_usb_hsic_post_phy_off(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L2);
#endif
}

static struct tegra_usb_phy_platform_ops oem2_plat_ops = {
	.post_suspend = pluto_usb_hsic_postsupend,
	.pre_resume = pluto_usb_hsic_preresume,
	.port_power = pluto_usb_hsic_phy_power,
	.post_resume = pluto_usb_hsic_post_resume,
	.post_phy_off = pluto_usb_hsic_post_phy_off,
};

static struct tegra_usb_platform_data tegra_ehci3_hsic_smsc_hub_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_HSIC,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
};

static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.support_pmu_vbus = true,
	.id_det_type = TEGRA_USB_PMU_ID,
	.unaligned_dma_buf_supported = false,
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
		.xcvr_lsfslew = 0,
		.xcvr_lsrslew = 3,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.support_pmu_vbus = true,
	.id_det_type = TEGRA_USB_PMU_ID,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 0,
		.xcvr_lsrslew = 3,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
		.vbus_oc_map = 0x7,
	},
};

static struct tegra_xusb_board_data xusb_bdata = {
	.portmap = TEGRA_XUSB_SS_P0 | TEGRA_XUSB_USB2_P0,
	/* ss_portmap[0:3] = SS0 map, ss_portmap[4:7] = SS1 map */
	.ss_portmap = (TEGRA_XUSB_SS_PORT_MAP_USB2_P0 << 0),
};

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
	.id_extcon_dev_name = "MAX77665_MUIC_ID",
	.vbus_extcon_dev_name = "palmas-extcon",
};

static struct regulator *baseband_reg;
static struct gpio modem_gpios[] = { /* i500 modem */
	{MDM_RST, GPIOF_OUT_INIT_LOW, "MODEM RESET"},
};

static struct gpio modem2_gpios[] = {
	{MDM2_PWR_ON, GPIOF_OUT_INIT_LOW, "MODEM2 PWR ON"},
	{MDM2_RST, GPIOF_OUT_INIT_LOW, "MODEM2 RESET"},
};

static struct tegra_usb_platform_data tegra_ehci2_hsic_baseband_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_HSIC,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
};

static struct tegra_usb_platform_data tegra_ehci3_hsic_baseband2_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_HSIC,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
};

static struct tegra_usb_platform_data tegra_hsic_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_HSIC,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
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

	val = platform_device_add_data(pdev, &tegra_hsic_pdata,
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

static void tegra_usb_hsic_host_unregister(struct platform_device **platdev)
{
	struct platform_device *pdev = *platdev;

	if (pdev && &pdev->dev) {
		platform_device_unregister(pdev);
		*platdev = NULL;
	} else
		pr_err("%s: no platform device\n", __func__);
}

static struct tegra_usb_phy_platform_ops oem1_hsic_pops;

static union tegra_bb_gpio_id bb_gpio_oem1 = {
	.oem1 = {
		.reset = BB_OEM1_GPIO_RST,
		.pwron = BB_OEM1_GPIO_ON,
		.awr = BB_OEM1_GPIO_AWR,
		.cwr = BB_OEM1_GPIO_CWR,
		.spare = BB_OEM1_GPIO_SPARE,
		.wdi = BB_OEM1_GPIO_WDI,
	},
};

static struct tegra_bb_pdata bb_pdata_oem1 = {
	.id = &bb_gpio_oem1,
	.device = &tegra_ehci3_device,
	.ehci_register = tegra_usb_hsic_host_register,
	.ehci_unregister = tegra_usb_hsic_host_unregister,
	.bb_id = TEGRA_BB_OEM1,
};

static struct platform_device tegra_bb_oem1 = {
	.name = "tegra_baseband_power",
	.id = -1,
	.dev = {
		.platform_data = &bb_pdata_oem1,
	},
};

static int baseband_init(void)
{
	int ret;

	ret = gpio_request_array(modem_gpios, ARRAY_SIZE(modem_gpios));
	if (ret) {
		pr_warn("%s:gpio request failed\n", __func__);
		return ret;
	}

	baseband_reg = regulator_get(NULL, "vdd_core_bb");
	if (IS_ERR_OR_NULL(baseband_reg))
		pr_warn("%s: baseband regulator get failed\n", __func__);
	else
		regulator_enable(baseband_reg);

	/* enable pull-up for MDM1 UART RX */
	tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_GPIO_PU1,
				    TEGRA_PUPD_PULL_UP);

	/* enable pull-down for MDM1_COLD_BOOT */
	tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_ULPI_DATA4,
				    TEGRA_PUPD_PULL_DOWN);

	/* export GPIO for user space access through sysfs */
	gpio_export(MDM_RST, false);

	return 0;
}

static const struct tegra_modem_operations baseband_operations = {
	.init = baseband_init,
};

#define MODEM_BOOT_EDP_MAX 0
/* FIXME: get accurate boot current value */
static unsigned int modem_boot_edp_states[] = { 1900, 0 };
static struct edp_client modem_boot_edp_client = {
	.name = "modem_boot",
	.states = modem_boot_edp_states,
	.num_states = ARRAY_SIZE(modem_boot_edp_states),
	.e0_index = MODEM_BOOT_EDP_MAX,
	.priority = EDP_MAX_PRIO,
};

static struct tegra_usb_modem_power_platform_data baseband_pdata = {
	.ops = &baseband_operations,
	.wake_gpio = -1,
	.boot_gpio = MDM_COLDBOOT,
	.boot_irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.autosuspend_delay = 2000,
	.short_autosuspend_delay = 50,
	.tegra_ehci_device = &tegra_ehci2_device,
	.tegra_ehci_pdata = &tegra_ehci2_hsic_baseband_pdata,
	.modem_boot_edp_client = &modem_boot_edp_client,
	.edp_manager_name = "battery",
	.i_breach_ppm = 500000,
	/* FIXME: get useful adjperiods */
	.i_thresh_3g_adjperiod = 10000,
	.i_thresh_lte_adjperiod = 10000,
};

static struct platform_device icera_baseband_device = {
	.name = "tegra_usb_modem_power",
	.id = -1,
	.dev = {
		.platform_data = &baseband_pdata,
	},
};

static void baseband2_start(void)
{
	pr_info("%s\n", __func__);
	gpio_set_value(MDM2_PWR_ON, 1);
}

static void baseband2_reset(void)
{
	/* Initiate power cycle on baseband sub system */
	pr_info("%s\n", __func__);
	gpio_set_value(MDM2_RST, 0);
	mdelay(200);
	gpio_set_value(MDM2_RST, 1);
}

static int baseband2_init(void)
{
	int ret;

	tegra_pinmux_set_tristate(TEGRA_PINGROUP_GPIO_X1_AUD, TEGRA_TRI_NORMAL);

	ret = gpio_request_array(modem2_gpios, ARRAY_SIZE(modem2_gpios));
	if (ret)
		return ret;

	/* enable pull-down for MDM2_COLD_BOOT */
	tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_KB_ROW4,
				    TEGRA_PUPD_PULL_DOWN);

	/* export GPIO for user space access through sysfs */
	gpio_export(MDM2_RST, false);

	return 0;
}

static const struct tegra_modem_operations baseband2_operations = {
	.init = baseband2_init,
	.start = baseband2_start,
	.reset = baseband2_reset,
};

static struct tegra_usb_modem_power_platform_data baseband2_pdata = {
	.ops = &baseband2_operations,
	.wake_gpio = -1,
	.boot_gpio = MDM2_COLDBOOT,
	.boot_irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.autosuspend_delay = 2000,
	.short_autosuspend_delay = 50,
	.tegra_ehci_device = &tegra_ehci3_device,
	.tegra_ehci_pdata = &tegra_ehci3_hsic_baseband2_pdata,
};

static struct platform_device icera_baseband2_device = {
	.name = "tegra_usb_modem_power",
	.id = -1,
	.dev = {
		.platform_data = &baseband2_pdata,
	},
};

static struct baseband_power_platform_data tegra_baseband_xmm_power_data = {
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

static struct platform_device tegra_baseband_xmm_power_device = {
	.name = "baseband_xmm_power",
	.id = -1,
	.dev = {
		.platform_data = &tegra_baseband_xmm_power_data,
	},
};

static struct platform_device tegra_baseband_xmm_power2_device = {
	.name = "baseband_xmm_power2",
	.id = -1,
	.dev = {
		.platform_data = &tegra_baseband_xmm_power_data,
	},
};

static void pluto_usb_init(void)
{
	int usb_port_owner_info = tegra_get_usb_port_owner_info();
	struct tegra_xusb_platform_data *xusb_pdata;

	if ((usb_port_owner_info & UTMI1_PORT_OWNER_XUSB)) {
		xusb_pdata = tegra_xusb_init(&xusb_bdata);
		tegra_otg_pdata.is_xhci = true;
		tegra_otg_pdata.xhci_device = &tegra_xhci_device;
		tegra_otg_pdata.xhci_pdata = xusb_pdata;
	} else {
		tegra_otg_pdata.is_xhci = false;
	}
	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);

	/* Setup the udc platform data */
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
}

static void pluto_modem_init(void)
{
	int modem_id = tegra_get_modem_id();
	struct board_info board_info;
	int usb_port_owner_info = tegra_get_usb_port_owner_info();

	tegra_get_board_info(&board_info);
	pr_info("%s: modem_id = %d\n", __func__, modem_id);

	switch (modem_id) {
	case TEGRA_BB_I500: /* on board i500 HSIC */
		if (!(usb_port_owner_info & HSIC1_PORT_OWNER_XUSB)) {
			platform_device_register(&icera_baseband_device);
		}
		break;
	case TEGRA_BB_I500SWD: /* i500 SWD HSIC */
		if (!(usb_port_owner_info & HSIC2_PORT_OWNER_XUSB)) {
			platform_device_register(&icera_baseband2_device);
		}
		break;
	case TEGRA_BB_OEM1:	/* OEM1 HSIC */
		if ((board_info.board_id == BOARD_E1575) ||
			((board_info.board_id == BOARD_E1580) &&
				(board_info.fab >= BOARD_FAB_A03))) {
			tegra_pinmux_set_tristate(TEGRA_PINGROUP_GPIO_X1_AUD,
							TEGRA_TRI_NORMAL);
			bb_gpio_oem1.oem1.pwron = BB_OEM1_GPIO_ON_V;
		}
		if (!(usb_port_owner_info & HSIC2_PORT_OWNER_XUSB)) {
			tegra_hsic_pdata.ops = &oem1_hsic_pops;
			tegra_ehci3_device.dev.platform_data
				= &tegra_hsic_pdata;
			platform_device_register(&tegra_bb_oem1);
		}
		break;
	case TEGRA_BB_OEM2: /* XMM6260/XMM6360 HSIC */
		/* fix wrong wiring in Pluto A02 */
		if ((board_info.board_id == BOARD_E1580) &&
			(board_info.fab == BOARD_FAB_A02)) {
			pr_info(
"%s: Pluto A02: replace MDM2_PWR_ON with MDM2_PWR_ON_FOR_PLUTO_A02\n",
				__func__);
			if (tegra_baseband_xmm_power_data.modem.xmm.bb_on
				!= MDM2_PWR_ON)
				pr_err(
"%s: expected MDM2_PWR_ON default gpio for XMM bb_on\n",
					__func__);
			tegra_baseband_xmm_power_data.modem.xmm.bb_on
				= MDM2_PWR_ON_FOR_PLUTO_A02;
		}
		/* baseband-power.ko will register ehci3 device */
		tegra_hsic_pdata.ops = &oem2_plat_ops;
		tegra_hsic_pdata.u_data.host.remote_wakeup_supported = false;
		tegra_hsic_pdata.u_data.host.power_off_on_suspend = false;
		tegra_ehci3_device.dev.platform_data =
					&tegra_hsic_pdata;
		tegra_baseband_xmm_power_data.hsic_register =
						&tegra_usb_hsic_host_register;
		tegra_baseband_xmm_power_data.hsic_unregister =
						&tegra_usb_hsic_host_unregister;
		tegra_baseband_xmm_power_data.ehci_device =
					&tegra_ehci3_device;
		platform_device_register(&tegra_baseband_xmm_power_device);
		platform_device_register(&tegra_baseband_xmm_power2_device);
		/* override audio settings - use 8kHz */
		pluto_audio_pdata.i2s_param[BASEBAND].audio_port_id
			= 2;
		pluto_audio_pdata.i2s_param[BASEBAND].is_i2s_master
			= 1;
		pluto_audio_pdata.i2s_param[BASEBAND].i2s_mode
			= TEGRA_DAIFMT_I2S;
		pluto_audio_pdata.i2s_param[BASEBAND].sample_size
			= 16;
		pluto_audio_pdata.i2s_param[BASEBAND].rate
			= 8000;
		pluto_audio_pdata.i2s_param[BASEBAND].channels
			= 2;
		break;
	case TEGRA_BB_HSIC_HUB: /* HSIC hub */
		if (!(usb_port_owner_info & HSIC2_PORT_OWNER_XUSB)) {
			tegra_ehci3_device.dev.platform_data =
				&tegra_ehci3_hsic_smsc_hub_pdata;
			platform_device_register(&tegra_ehci3_device);
		}
		break;
	default:
		return;
	}
}

#else
static void pluto_usb_init(void) { }
static void pluto_modem_init(void) { }
#endif

static void pluto_audio_init(void)
{
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	spi_register_board_info(aic326x_spi_board_info,
					ARRAY_SIZE(aic326x_spi_board_info));
}

static struct platform_device *pluto_spi_devices[] __initdata = {
        &tegra11_spi_device4,
};

struct spi_clk_parent spi_parent_clk_pluto[] = {
        [0] = {.name = "pll_p"},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
        [1] = {.name = "pll_m"},
        [2] = {.name = "clk_m"},
#else
        [1] = {.name = "clk_m"},
#endif
};

static struct tegra_spi_platform_data pluto_spi_pdata = {
	.max_dma_buffer         = 16 * 1024,
        .is_clkon_always        = false,
        .max_rate               = 25000000,
};

static void __init pluto_spi_init(void)
{
        int i;
        struct clk *c;
        struct board_info board_info, display_board_info;

        tegra_get_board_info(&board_info);
        tegra_get_display_board_info(&display_board_info);

        for (i = 0; i < ARRAY_SIZE(spi_parent_clk_pluto); ++i) {
                c = tegra_get_clock_by_name(spi_parent_clk_pluto[i].name);
                if (IS_ERR_OR_NULL(c)) {
                        pr_err("Not able to get the clock for %s\n",
                                                spi_parent_clk_pluto[i].name);
                        continue;
                }
                spi_parent_clk_pluto[i].parent_clk = c;
                spi_parent_clk_pluto[i].fixed_clk_rate = clk_get_rate(c);
        }
        pluto_spi_pdata.parent_clk_list = spi_parent_clk_pluto;
        pluto_spi_pdata.parent_clk_count = ARRAY_SIZE(spi_parent_clk_pluto);
	pluto_spi_pdata.is_dma_based = (tegra_revision == TEGRA_REVISION_A01)
						? false : true ;
	tegra11_spi_device4.dev.platform_data = &pluto_spi_pdata;
        platform_add_devices(pluto_spi_devices,
                                ARRAY_SIZE(pluto_spi_devices));
}

static __initdata struct tegra_clk_init_table touch_clk_init_table[] = {
	/* name         parent          rate            enabled */
	{ "extern2",    "pll_p",        41000000,       false},
	{ "clk_out_2",  "extern2",      40800000,       false},
	{ NULL,         NULL,           0,              0},
};

struct rm_spi_ts_platform_data rm31080ts_pluto_data = {
	.gpio_reset = TOUCH_GPIO_RST_RAYDIUM_SPI,
	.config = 0,
	.platform_id = RM_PLATFORM_P005,
	.name_of_clock = "clk_out_2",
	.name_of_clock_con = "extern2",
};

static struct tegra_spi_device_controller_data dev_cdata = {
	.rx_clk_tap_delay = 0,
	.tx_clk_tap_delay = 0,
};

struct spi_board_info rm31080a_pluto_spi_board[1] = {
	{
	 .modalias = "rm_ts_spidev",
	 .bus_num = 3,
	 .chip_select = 2,
	 .max_speed_hz = 12 * 1000 * 1000,
	 .mode = SPI_MODE_0,
	 .controller_data = &dev_cdata,
	 .platform_data = &rm31080ts_pluto_data,
	 },
};

static int __init pluto_touch_init(void)
{
	tegra_clk_init_from_table(touch_clk_init_table);
	rm31080a_pluto_spi_board[0].irq = gpio_to_irq(TOUCH_GPIO_IRQ_RAYDIUM_SPI);
	touch_init_raydium(TOUCH_GPIO_IRQ_RAYDIUM_SPI,
				TOUCH_GPIO_RST_RAYDIUM_SPI,
				&rm31080ts_pluto_data,
				&rm31080a_pluto_spi_board[0],
				ARRAY_SIZE(rm31080a_pluto_spi_board));
	return 0;
}

static void __init pluto_dtv_init(void)
{
	platform_device_register(&tegra_dtv_device);
}

static void __init tegra_pluto_init(void)
{
	pluto_sysedp_init();
	tegra_clk_init_from_table(pluto_clk_init_table);
	tegra_clk_verify_parents();
	tegra_soc_device_init("tegra_pluto");
	tegra_enable_pinmux();
	pluto_pinmux_init();
	pluto_i2c_init();
	pluto_spi_init();
	pluto_usb_init();
	pluto_uart_init();
	pluto_audio_init();
	platform_add_devices(pluto_devices, ARRAY_SIZE(pluto_devices));
	tegra_ram_console_debug_init();
	tegra_io_dpd_init();
	pluto_sdhci_init();
	pluto_regulator_init();
	pluto_dtv_init();
	pluto_suspend_init();
	pluto_touch_init();
	pluto_emc_init();
	pluto_edp_init();
	pluto_panel_init();
	pluto_pmon_init();
	pluto_kbc_init();
#ifdef CONFIG_BT_BLUESLEEP
	pluto_setup_bluesleep();
	pluto_setup_bt_rfkill();
#elif defined CONFIG_BLUEDROID_PM
	pluto_setup_bluedroid_pm();
#endif
	tegra_release_bootloader_fb();
	pluto_modem_init();
#ifdef CONFIG_TEGRA_WDT_RECOVERY
	tegra_wdt_recovery_init();
#endif
	pluto_sensors_init();
	tegra_serial_debug_init(TEGRA_UARTD_BASE, INT_WDT_CPU, NULL, -1, -1);
	pluto_soctherm_init();
	tegra_register_fuse();
	pluto_sysedp_core_init();
	pluto_sysedp_psydepl_init();
}

static void __init pluto_ramconsole_reserve(unsigned long size)
{
	tegra_ram_console_debug_reserve(SZ_1M);
}

static void __init tegra_pluto_dt_init(void)
{
#ifdef CONFIG_USE_OF
	of_platform_populate(NULL,
		of_default_bus_match_table, NULL, NULL);
#endif

	tegra_pluto_init();
}
static void __init tegra_pluto_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	/* for PANEL_5_SHARP_1080p: 1920*1080*4*2 = 16588800 bytes */
	tegra_reserve(0, SZ_16M, SZ_4M);
#else
	tegra_reserve(SZ_128M, SZ_16M, SZ_4M);
#endif
	pluto_ramconsole_reserve(SZ_1M);
}

static const char * const pluto_dt_board_compat[] = {
	"nvidia,pluto",
	NULL
};

MACHINE_START(TEGRA_PLUTO, "tegra_pluto")
	.atag_offset	= 0x100,
	.soc		= &tegra_soc_desc,
	.map_io		= tegra_map_common_io,
	.reserve	= tegra_pluto_reserve,
	.init_early     = tegra11x_init_early,
	.init_irq	= tegra_init_irq,
	.handle_irq	= gic_handle_irq,
	.timer		= &tegra_timer,
	.init_machine	= tegra_pluto_dt_init,
	.restart	= tegra_assert_system_reset,
	.dt_compat	= pluto_dt_board_compat,
MACHINE_END
