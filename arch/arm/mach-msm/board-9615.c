/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slimbus/slimbus.h>
#include <linux/msm_ssbi.h>
#include <linux/memblock.h>
#include <linux/usb/android.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/leds.h>
#include <linux/leds-pm8xxx.h>
#include <linux/power/ltc4088-charger.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/gpio.h>
#include <mach/msm_spi.h>
#include <mach/msm_bus_board.h>
#include "timer.h"
#include "devices.h"
#include "board-9615.h"
#include "cpuidle.h"
#include "pm.h"
#include "acpuclock.h"
#include "pm-boot.h"

static struct pm8xxx_adc_amux pm8018_adc_channels_data[] = {
	{"vcoin", CHANNEL_VCOIN, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vbat", CHANNEL_VBAT, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vph_pwr", CHANNEL_VPH_PWR, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"batt_therm", CHANNEL_BATT_THERM, CHAN_PATH_SCALING1, AMUX_RSV2,
		ADC_DECIMATION_TYPE2, ADC_SCALE_BATT_THERM},
	{"batt_id", CHANNEL_BATT_ID, CHAN_PATH_SCALING1, AMUX_RSV2,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"pmic_therm", CHANNEL_DIE_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PMIC_THERM},
	{"625mv", CHANNEL_625MV, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"125v", CHANNEL_125V, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"pa_therm0", ADC_MPP_1_AMUX3, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PA_THERM},
};

static struct pm8xxx_adc_properties pm8018_adc_data = {
	.adc_vdd_reference	= 1800, /* milli-voltage for this adc */
	.bitresolution		= 15,
	.bipolar                = 0,
};

static struct pm8xxx_adc_platform_data pm8018_adc_pdata = {
	.adc_channel		= pm8018_adc_channels_data,
	.adc_num_board_channel	= ARRAY_SIZE(pm8018_adc_channels_data),
	.adc_prop		= &pm8018_adc_data,
};

static struct pm8xxx_irq_platform_data pm8xxx_irq_pdata __devinitdata = {
	.irq_base		= PM8018_IRQ_BASE,
	.devirq			= MSM_GPIO_TO_INT(87),
	.irq_trigger_flag	= IRQF_TRIGGER_LOW,
};

static struct pm8xxx_gpio_platform_data pm8xxx_gpio_pdata __devinitdata = {
	.gpio_base		= PM8018_GPIO_PM_TO_SYS(1),
};

static struct pm8xxx_mpp_platform_data pm8xxx_mpp_pdata __devinitdata = {
	.mpp_base		= PM8018_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_rtc_platform_data pm8xxx_rtc_pdata __devinitdata = {
	.rtc_write_enable	= false,
	.rtc_alarm_powerup	= false,
};

static struct pm8xxx_pwrkey_platform_data pm8xxx_pwrkey_pdata = {
	.pull_up		= 1,
	.kpd_trigger_delay_us	= 15625,
	.wakeup			= 1,
};

static struct pm8xxx_misc_platform_data pm8xxx_misc_pdata = {
	.priority		= 0,
};

#define PM8018_LED_KB_MAX_CURRENT	20	/* I = 20mA */
#define PM8XXX_LED_PWM_PERIOD_US	1000

/**
 * PM8XXX_PWM_CHANNEL_NONE shall be used when LED shall not be
 * driven using PWM feature.
 */
#define PM8XXX_PWM_CHANNEL_NONE		-1

static struct led_info pm8018_led_info[] = {
	[0] = {
		.name	= "led:kb",
	},
};

static struct led_platform_data pm8018_led_core_pdata = {
	.num_leds = ARRAY_SIZE(pm8018_led_info),
	.leds = pm8018_led_info,
};

static struct pm8xxx_led_config pm8018_led_configs[] = {
	[0] = {
		.id = PM8XXX_ID_LED_KB_LIGHT,
		.mode = PM8XXX_LED_MODE_PWM3,
		.max_current = PM8018_LED_KB_MAX_CURRENT,
		.pwm_channel = 2,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD_US,
	},
};

static struct pm8xxx_led_platform_data pm8xxx_leds_pdata = {
		.led_core = &pm8018_led_core_pdata,
		.configs = pm8018_led_configs,
		.num_configs = ARRAY_SIZE(pm8018_led_configs),
};

#ifdef CONFIG_LTC4088_CHARGER
static struct ltc4088_charger_platform_data ltc4088_chg_pdata = {
		.gpio_mode_select_d0 = 7,
		.gpio_mode_select_d1 = 6,
		.gpio_mode_select_d2 = 4,
};
#endif

static struct pm8018_platform_data pm8018_platform_data __devinitdata = {
	.irq_pdata		= &pm8xxx_irq_pdata,
	.gpio_pdata		= &pm8xxx_gpio_pdata,
	.mpp_pdata		= &pm8xxx_mpp_pdata,
	.rtc_pdata		= &pm8xxx_rtc_pdata,
	.pwrkey_pdata		= &pm8xxx_pwrkey_pdata,
	.misc_pdata		= &pm8xxx_misc_pdata,
	.regulator_pdatas	= msm_pm8018_regulator_pdata,
	.adc_pdata		= &pm8018_adc_pdata,
	.leds_pdata		= &pm8xxx_leds_pdata,
};

static struct msm_ssbi_platform_data msm9615_ssbi_pm8018_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name		= PM8018_CORE_DEV_NAME,
		.platform_data	= &pm8018_platform_data,
	},
};

static struct platform_device msm9615_device_rpm_regulator __devinitdata = {
	.name	= "rpm-regulator",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_rpm_regulator_9615_pdata,
	},
};

static struct platform_device msm9615_device_ext_2p95v_vreg = {
	.name	= GPIO_REGULATOR_DEV_NAME,
	.id	= 18,
	.dev	= {
		.platform_data =
			&msm_gpio_regulator_pdata[GPIO_VREG_ID_EXT_2P95V],
	},
};

static  struct msm_cpuidle_state msm_cstates[] __initdata = {
	{0, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{0, 1, "C1", "STANDALONE_POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE},

	{0, 2, "C2", "POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE},
};
static struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_POWER_COLLAPSE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},
	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},
	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 1,
		.suspend_enabled = 1,
	},
};

static struct msm_pm_boot_platform_data msm_pm_boot_pdata __initdata = {
	.mode = MSM_PM_BOOT_CONFIG_REMAP_BOOT_ADDR,
	.v_addr = MSM_APCS_GLB_BASE +  0x24,
};

static void __init msm9615_init_buses(void)
{
#ifdef CONFIG_MSM_BUS_SCALING
	msm_bus_rpm_set_mt_mask();
	msm_bus_9615_sys_fabric_pdata.rpm_enabled = 1;
	msm_bus_9615_sys_fabric.dev.platform_data =
		&msm_bus_9615_sys_fabric_pdata;
	msm_bus_def_fab.dev.platform_data = &msm_bus_9615_def_fab_pdata;
#endif
}

static struct slim_boardinfo msm_slim_devices[] = {
	/* add slimbus slaves as needed */
};

static struct msm_spi_platform_data msm9615_qup_spi_gsbi3_pdata = {
	.max_clock_speed = 24000000,
};

static struct msm_i2c_platform_data msm9615_i2c_qup_gsbi5_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
};

#define USB_5V_EN		3
#define PM_USB_5V_EN	PM8018_GPIO_PM_TO_SYS(USB_5V_EN)

static void msm_hsusb_vbus_power(bool on)
{
	int rc;
	static bool vbus_is_on;
	struct pm_gpio usb_vbus = {
			.direction      = PM_GPIO_DIR_OUT,
			.pull           = PM_GPIO_PULL_NO,
			.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
			.output_value   = 0,
			.vin_sel        = 2,
			.out_strength   = PM_GPIO_STRENGTH_HIGH,
			.function       = PM_GPIO_FUNC_NORMAL,
			.inv_int_pol    = 0,
	};

	if (vbus_is_on == on)
		return;

	if (on) {
		rc = pm8xxx_gpio_config(PM_USB_5V_EN, &usb_vbus);
		if (rc) {
			pr_err("failed to config usb_5v_en gpio\n");
			return;
		}

		rc = gpio_request(PM_USB_5V_EN,
						"usb_5v_en");
		if (rc < 0) {
			pr_err("failed to request usb_5v_en gpio\n");
			return;
		}

		rc = gpio_direction_output(PM_USB_5V_EN, 1);
		if (rc) {
			pr_err("%s: unable to set_direction for gpio [%d]\n",
				__func__, PM_USB_5V_EN);
			goto free_usb_5v_en;
		}

		vbus_is_on = true;
		return;
	}
	gpio_set_value(PM_USB_5V_EN, 0);
free_usb_5v_en:
	gpio_free(PM_USB_5V_EN);
	vbus_is_on = false;
}

static int shelby_phy_init_seq[] = {
	0x44, 0x80,/* set VBUS valid threshold and
			disconnect valid threshold */
	0x38, 0x81, /* update DC voltage level */
	0x14, 0x82,/* set preemphasis and rise/fall time */
	0x13, 0x83,/* set source impedance adjustment */
	-1};

#define USB_BAM_PHY_BASE	0x12502000
#define USB_BAM_PHY_SIZE	0x10000
#define A2_BAM_PHY_BASE		0x124C2000
static struct usb_bam_pipe_connect msm_usb_bam_connections[4][2] = {
	[0][USB_TO_PEER_PERIPHERAL] = {
		.src_phy_addr = USB_BAM_PHY_BASE,
		.src_pipe_index = 11,
		.dst_phy_addr = A2_BAM_PHY_BASE,
		.dst_pipe_index = 0,
		.data_fifo_base_offset = 0x1100,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x1700,
		.desc_fifo_size = 0x300,
	},
	[0][PEER_PERIPHERAL_TO_USB] = {
		.src_phy_addr = A2_BAM_PHY_BASE,
		.src_pipe_index = 1,
		.dst_phy_addr = USB_BAM_PHY_BASE,
		.dst_pipe_index = 10,
		.data_fifo_base_offset = 0xa00,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x1000,
		.desc_fifo_size = 0x100,
	},
};

static struct msm_usb_bam_platform_data msm_usb_bam_pdata = {
	.connections = &msm_usb_bam_connections[0][0],
	.usb_bam_phy_base = USB_BAM_PHY_BASE,
	.usb_bam_phy_size = USB_BAM_PHY_SIZE,
	.usb_bam_num_pipes = 32,
};

static struct msm_otg_platform_data msm_otg_pdata = {
	.mode			= USB_OTG,
	.otg_control	= OTG_PHY_CONTROL,
	.phy_type		= SNPS_28NM_INTEGRATED_PHY,
	.pclk_src_name	= "dfab_usb_hs_clk",
	.vbus_power		= msm_hsusb_vbus_power,
	.disable_reset_on_disconnect	= true,
};

static int usb_diag_update_pid_and_serial_num(uint32_t pid, const char *snum)
{
	return 0;
}

static struct android_usb_platform_data android_usb_pdata = {
	.update_pid_and_serial_num = usb_diag_update_pid_and_serial_num,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data = &android_usb_pdata,
	},
};

static struct platform_device msm_wlan_ar6000_pm_device = {
	.name           = "wlan_ar6000_pm_dev",
	.id             = -1,
};

static int __init msm9615_init_ar6000pm(void)
{
	return platform_device_register(&msm_wlan_ar6000_pm_device);
}

#ifdef CONFIG_LTC4088_CHARGER
static struct platform_device msm_device_charger = {
	.name	= LTC4088_CHARGER_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data = &ltc4088_chg_pdata,
	},
};
#endif

static struct platform_device *common_devices[] = {
	&msm9615_device_dmov,
	&msm_device_smd,
#ifdef CONFIG_LTC4088_CHARGER
	&msm_device_charger,
#endif
	&msm_device_otg,
	&msm_device_gadget_peripheral,
	&msm_device_hsusb_host,
	&msm_device_usb_bam,
	&android_usb_device,
	&msm9615_device_uart_gsbi4,
	&msm9615_device_ext_2p95v_vreg,
	&msm9615_device_ssbi_pmic1,
	&msm9615_device_qup_i2c_gsbi5,
	&msm9615_device_qup_spi_gsbi3,
	&msm_device_sps,
	&msm9615_slim_ctrl,
	&msm9615_device_tsens,
	&msm_device_nand,
	&msm_device_bam_dmux,
	&msm_rpm_device,
#ifdef CONFIG_HW_RANDOM_MSM
	&msm_device_rng,
#endif

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)
	&msm9615_qcrypto_device,
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)
	&msm9615_qcedev_device,
#endif
	&msm9615_device_watchdog,
	&msm_bus_9615_sys_fabric,
	&msm_bus_def_fab,
};

static void __init msm9615_i2c_init(void)
{
	msm9615_device_qup_i2c_gsbi5.dev.platform_data =
					&msm9615_i2c_qup_gsbi5_pdata;
}

static void __init msm9615_reserve(void)
{
	msm_pm_boot_pdata.p_addr = memblock_alloc(SZ_8, SZ_64K);
}

static void __init msm9615_common_init(void)
{
	msm9615_device_init();
	msm9615_init_gpiomux();
	msm9615_i2c_init();
	regulator_suppress_info_printing();
	platform_device_register(&msm9615_device_rpm_regulator);
	msm_clock_init(&msm9615_clock_init_data);
	msm9615_init_buses();
	msm9615_device_qup_spi_gsbi3.dev.platform_data =
				&msm9615_qup_spi_gsbi3_pdata;
	msm9615_device_ssbi_pmic1.dev.platform_data =
						&msm9615_ssbi_pm8018_pdata;
	pm8018_platform_data.num_regulators = msm_pm8018_regulator_pdata_len;

	msm_device_otg.dev.platform_data = &msm_otg_pdata;
	msm_otg_pdata.phy_init_seq = shelby_phy_init_seq;
	msm_device_usb_bam.dev.platform_data = &msm_usb_bam_pdata;
	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));

	acpuclk_init(&acpuclk_9615_soc_data);

	/* Ensure ar6000pm device is registered before MMC/SDC */
	msm9615_init_ar6000pm();

	msm9615_init_mmc();
	slim_register_board_info(msm_slim_devices,
		ARRAY_SIZE(msm_slim_devices));
	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	msm_pm_set_rpm_wakeup_irq(RPM_APCC_CPU0_WAKE_UP_IRQ);
	msm_cpuidle_set_states(msm_cstates, ARRAY_SIZE(msm_cstates),
						msm_pm_data);
	BUG_ON(msm_pm_boot_init(&msm_pm_boot_pdata));
}

static void __init msm9615_cdp_init(void)
{
	msm9615_common_init();
}

static void __init msm9615_mtp_init(void)
{
	msm9615_common_init();
}

MACHINE_START(MSM9615_CDP, "QCT MSM9615 CDP")
	.map_io = msm9615_map_io,
	.init_irq = msm9615_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm9615_cdp_init,
	.reserve = msm9615_reserve,
MACHINE_END

MACHINE_START(MSM9615_MTP, "QCT MSM9615 MTP")
	.map_io = msm9615_map_io,
	.init_irq = msm9615_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm9615_mtp_init,
	.reserve = msm9615_reserve,
MACHINE_END
