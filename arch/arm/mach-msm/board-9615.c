/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/slimbus/slimbus.h>
#ifdef CONFIG_WCD9310_CODEC
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#endif
#include <linux/msm_ssbi.h>
#include <linux/memblock.h>
#include <linux/usb/android.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/leds.h>
#include <linux/leds-pm8xxx.h>
#include <linux/power/ltc4088-charger.h>
#include <linux/gpio.h>
#include <linux/msm_tsens.h>
#include <linux/ion.h>
#include <linux/memory.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <mach/msm_spi.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_xo.h>
#include <mach/dma.h>
#include <mach/ion.h>
#include <mach/msm_memtypes.h>
#include <mach/cpuidle.h>
#include <mach/usb_bam.h>
#include <mach/restart.h>
#include "timer.h"
#include "devices.h"
#include "board-9615.h"
#include "pm.h"
#include "pm-boot.h"
#include <mach/gpiomux.h>
#include "ci13xxx_udc.h"

#ifdef CONFIG_ION_MSM
#define MSM_ION_AUDIO_SIZE	0xAF000
#define MSM_ION_HEAP_NUM	3
#define MSM_KERNEL_EBI_SIZE	0x51000

static struct memtype_reserve msm9615_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm9615_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct ion_co_heap_pdata co_ion_pdata = {
	.adjacent_mem_id = INVALID_HEAP_ID,
	.align = PAGE_SIZE,
};

static struct ion_platform_data ion_pdata = {
	.nr = MSM_ION_HEAP_NUM,
	.heaps = {
		{
			.id	= ION_SYSTEM_HEAP_ID,
			.type	= ION_HEAP_TYPE_SYSTEM,
			.name	= ION_VMALLOC_HEAP_NAME,
		},
		{
			.id	= ION_IOMMU_HEAP_ID,
			.type	= ION_HEAP_TYPE_IOMMU,
			.name	= ION_IOMMU_HEAP_NAME,
		},
		{
			.id	= ION_AUDIO_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_AUDIO_HEAP_NAME,
			.size	= MSM_ION_AUDIO_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &co_ion_pdata,
		},
	}
};

static struct platform_device ion_dev = {
	.name = "ion-msm",
	.id = 1,
	.dev = { .platform_data = &ion_pdata },
};

static void __init reserve_ion_memory(void)
{
	msm9615_reserve_table[MEMTYPE_EBI1].size += MSM_ION_AUDIO_SIZE;
}

static void __init msm9615_calculate_reserve_sizes(void)
{
	reserve_ion_memory();
	msm9615_reserve_table[MEMTYPE_EBI1].size += MSM_KERNEL_EBI_SIZE;
}

static struct reserve_info msm9615_reserve_info __initdata = {
	.memtype_reserve_table = msm9615_reserve_table,
	.calculate_reserve_sizes = msm9615_calculate_reserve_sizes,
	.paddr_to_memtype = msm9615_paddr_to_memtype,
};
#endif

struct pm8xxx_gpio_init {
	unsigned		gpio;
	struct pm_gpio		config;
};

struct pm8xxx_mpp_init {
	unsigned			mpp;
	struct pm8xxx_mpp_config_data	config;
};

#define PM8018_GPIO_INIT(_gpio, _dir, _buf, _val, _pull, _vin, _out_strength, \
			_func, _inv, _disable) \
{ \
	.gpio	= PM8018_GPIO_PM_TO_SYS(_gpio), \
	.config	= { \
		.direction	= _dir, \
		.output_buffer	= _buf, \
		.output_value	= _val, \
		.pull		= _pull, \
		.vin_sel	= _vin, \
		.out_strength	= _out_strength, \
		.function	= _func, \
		.inv_int_pol	= _inv, \
		.disable_pin	= _disable, \
	} \
}

#define PM8018_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8018_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

#define PM8018_GPIO_DISABLE(_gpio) \
	PM8018_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, 0, 0, 0, PM8018_GPIO_VIN_S3, \
			 0, 0, 0, 1)

#define PM8018_GPIO_OUTPUT(_gpio, _val, _strength) \
	PM8018_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM8018_GPIO_VIN_S3, \
			PM_GPIO_STRENGTH_##_strength, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8018_GPIO_INPUT(_gpio, _pull) \
	PM8018_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
			_pull, PM8018_GPIO_VIN_S3, \
			PM_GPIO_STRENGTH_NO, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8018_GPIO_OUTPUT_FUNC(_gpio, _val, _func) \
	PM8018_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM8018_GPIO_VIN_S3, \
			PM_GPIO_STRENGTH_HIGH, \
			_func, 0, 0)

#define PM8018_GPIO_OUTPUT_VIN(_gpio, _val, _vin) \
	PM8018_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, _vin, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

/* Initial PM8018 GPIO configurations */
static struct pm8xxx_gpio_init pm8018_gpios[] __initdata = {
	PM8018_GPIO_OUTPUT(2,	0,	HIGH), /* EXT_LDO_EN_WLAN */
	PM8018_GPIO_OUTPUT(6,	0,	LOW), /* WLAN_CLK_PWR_REQ */
};

/* Initial PM8018 MPP configurations */
static struct pm8xxx_mpp_init pm8018_mpps[] __initdata = {
};

void __init msm9615_pm8xxx_gpio_mpp_init(void)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(pm8018_gpios); i++) {
		rc = pm8xxx_gpio_config(pm8018_gpios[i].gpio,
					&pm8018_gpios[i].config);
		if (rc) {
			pr_err("%s: pm8018_gpio_config: rc=%d\n", __func__, rc);
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(pm8018_mpps); i++) {
		rc = pm8xxx_mpp_config(pm8018_mpps[i].mpp,
					&pm8018_mpps[i].config);
		if (rc) {
			pr_err("%s: pm8018_mpp_config: rc=%d\n", __func__, rc);
			break;
		}
	}
}

static struct pm8xxx_adc_amux pm8018_adc_channels_data[] = {
	{"vcoin", CHANNEL_VCOIN, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vbat", CHANNEL_VBAT, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vph_pwr", CHANNEL_VPH_PWR, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	/* AMUX8 is used to read either Batt_id/Batt_therm.
	 * Current configuration is to support Batt_id. If clients
	 * want to read the Batt_therm, the scaling function needs to be
	 * updated to use ADC_SCALE_BATT_THERM instead of ADC_SCALE_DEFAULT.
	 * E.g.
	 * {"batt_therm", CHANNEL_BATT_ID_THERM, CHAN_PATH_SCALING1,
	 * AMUX_RSV2, ADC_DECIMATION_TYPE2, ADC_SCALE_BATT_THERM},
	 */
	{"batt_id", CHANNEL_BATT_ID_THERM, CHAN_PATH_SCALING1,
		AMUX_RSV2, ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
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

#ifdef CONFIG_WCD9310_CODEC

#define TABLA_INTERRUPT_BASE (NR_MSM_IRQS + NR_GPIO_IRQS)

/*
 * MDM9x15 I2S.
 */
static struct wcd9xxx_pdata wcd9xxx_i2c_platform_data = {
	.irq = MSM_GPIO_TO_INT(85),
	.irq_base = TABLA_INTERRUPT_BASE,
	.num_irqs = NR_TABLA_IRQS,
	.reset_gpio = 84,
	.micbias = {
		.ldoh_v = TABLA_LDOH_2P85_V,
		.cfilt1_mv = 1800,
		.cfilt2_mv = 1800,
		.cfilt3_mv = 1800,
		.bias1_cfilt_sel = TABLA_CFILT1_SEL,
		.bias2_cfilt_sel = TABLA_CFILT2_SEL,
		.bias3_cfilt_sel = TABLA_CFILT3_SEL,
		.bias4_cfilt_sel = TABLA_CFILT3_SEL,
	},
	.regulator = {
	{
		.name = "CDC_VDD_CP",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_CP_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_RX",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_RX_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_TX",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_TX_CUR_MAX,
	},
	{
		.name = "VDDIO_CDC",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_VDDIO_CDC_CUR_MAX,
	},
	{
		.name = "VDDD_CDC_D",
		.min_uV = 1225000,
		.max_uV = 1225000,
		.optimum_uA = WCD9XXX_VDDD_CDC_D_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_A_1P2V",
		.min_uV = 1225000,
		.max_uV = 1225000,
		.optimum_uA = WCD9XXX_VDDD_CDC_A_CUR_MAX,
	}
	},
};

static struct i2c_board_info wcd9xxx_device_info[] __initdata = {
	{
		I2C_BOARD_INFO("tabla top level", TABLA_I2C_SLAVE_ADDR),
		.platform_data = &wcd9xxx_i2c_platform_data,
	},
	{
		I2C_BOARD_INFO("tabla analog", TABLA_ANALOG_I2C_SLAVE_ADDR),
		.platform_data = &wcd9xxx_i2c_platform_data,
	},
	{
		I2C_BOARD_INFO("tabla digital1", TABLA_DIGITAL1_I2C_SLAVE_ADDR),
		.platform_data = &wcd9xxx_i2c_platform_data,
	},
	{
		I2C_BOARD_INFO("tabla digital2", TABLA_DIGITAL2_I2C_SLAVE_ADDR),
		.platform_data = &wcd9xxx_i2c_platform_data,
	},
};

/*
 * MDM9x15 I2S.
 */

/* Micbias setting is based on 8660 CDP/MTP/FLUID requirement
 * 4 micbiases are used to power various analog and digital
 * microphones operating at 1800 mV. Technically, all micbiases
 * can source from single cfilter since all microphones operate
 * at the same voltage level. The arrangement below is to make
 * sure all cfilters are exercised. LDO_H regulator ouput level
 * does not need to be as high as 2.85V. It is choosen for
 * microphone sensitivity purpose.
 */

static struct wcd9xxx_pdata tabla20_platform_data = {
	.slimbus_slave_device = {
		.name = "tabla-slave",
		.e_addr = {0, 0, 0x60, 0, 0x17, 2},
	},
	.irq = MSM_GPIO_TO_INT(85),
	.irq_base = TABLA_INTERRUPT_BASE,
	.num_irqs = NR_WCD9XXX_IRQS,
	.reset_gpio = 84,
	.micbias = {
		.ldoh_v = TABLA_LDOH_2P85_V,
		.cfilt1_mv = 1800,
		.cfilt2_mv = 1800,
		.cfilt3_mv = 1800,
		.bias1_cfilt_sel = TABLA_CFILT1_SEL,
		.bias2_cfilt_sel = TABLA_CFILT2_SEL,
		.bias3_cfilt_sel = TABLA_CFILT3_SEL,
		.bias4_cfilt_sel = TABLA_CFILT3_SEL,
	},
	.regulator = {
	{
		.name = "CDC_VDD_CP",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_CP_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_RX",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_RX_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_TX",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_CDC_VDDA_TX_CUR_MAX,
	},
	{
		.name = "VDDIO_CDC",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.optimum_uA = WCD9XXX_VDDIO_CDC_CUR_MAX,
	},
	{
		.name = "VDDD_CDC_D",
		.min_uV = 1225000,
		.max_uV = 1225000,
		.optimum_uA = WCD9XXX_VDDD_CDC_D_CUR_MAX,
	},
	{
		.name = "CDC_VDDA_A_1P2V",
		.min_uV = 1225000,
		.max_uV = 1225000,
		.optimum_uA = WCD9XXX_VDDD_CDC_A_CUR_MAX,
	},
	},
};

static struct slim_device msm_slim_tabla20 = {
	.name = "tabla2x-slim",
	.e_addr = {0, 1, 0x60, 0, 0x17, 2},
	.dev = {
		.platform_data = &tabla20_platform_data,
	},
};
#endif

static struct i2c_registry msm9615_i2c_devices[] __initdata = {
#ifdef CONFIG_WCD9310_CODEC
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_9615_GSBI5_QUP_I2C_BUS_ID,
		wcd9xxx_device_info,
		ARRAY_SIZE(wcd9xxx_device_info),
	},
#endif
};

static struct slim_boardinfo msm_slim_devices[] = {
	/* add slimbus slaves as needed */
#ifdef CONFIG_WCD9310_CODEC
	{
		.bus_num = 1,
		.slim_slave = &msm_slim_tabla20,
	},
#endif
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

static int msm_hsusb_vbus_power(bool on)
{
	int rc;
	struct pm_gpio usb_vbus = {
			.direction      = PM_GPIO_DIR_OUT,
			.pull           = PM_GPIO_PULL_NO,
			.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
			.vin_sel        = 2,
			.out_strength   = PM_GPIO_STRENGTH_HIGH,
			.function       = PM_GPIO_FUNC_NORMAL,
			.inv_int_pol    = 0,
	};

	usb_vbus.output_value = on;

	rc = pm8xxx_gpio_config(PM_USB_5V_EN, &usb_vbus);
	if (rc)
		pr_err("failed to config usb_5v_en gpio\n");

	return rc;
}

static int shelby_phy_init_seq[] = {
	0x44, 0x80,/* set VBUS valid threshold and
			disconnect valid threshold */
	0x38, 0x81, /* update DC voltage level */
	0x24, 0x82,/* set preemphasis and rise/fall time */
	0x13, 0x83,/* set source impedance adjustment */
	-1};

#define USB_BAM_PHY_BASE	0x12502000
#define HSIC_BAM_PHY_BASE	0x12542000
#define A2_BAM_PHY_BASE		0x124C2000
static struct usb_bam_pipe_connect msm_usb_bam_connections[2][4][2] = {
	[0][0][USB_TO_PEER_PERIPHERAL] = {
		.src_phy_addr = USB_BAM_PHY_BASE,
		.src_pipe_index = 11,
		.dst_phy_addr = A2_BAM_PHY_BASE,
		.dst_pipe_index = 0,
		.data_fifo_base_offset = 0x1100,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x1700,
		.desc_fifo_size = 0x300,
	},
	[0][0][PEER_PERIPHERAL_TO_USB] = {
		.src_phy_addr = A2_BAM_PHY_BASE,
		.src_pipe_index = 1,
		.dst_phy_addr = USB_BAM_PHY_BASE,
		.dst_pipe_index = 10,
		.data_fifo_base_offset = 0xa00,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x1000,
		.desc_fifo_size = 0x100,
	},
	[0][1][USB_TO_PEER_PERIPHERAL] = {
		.src_phy_addr = USB_BAM_PHY_BASE,
		.src_pipe_index = 13,
		.dst_phy_addr = A2_BAM_PHY_BASE,
		.dst_pipe_index = 2,
		.data_fifo_base_offset = 0x2100,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x2700,
		.desc_fifo_size = 0x300,
	},
	[0][1][PEER_PERIPHERAL_TO_USB] = {
		.src_phy_addr = A2_BAM_PHY_BASE,
		.src_pipe_index = 3,
		.dst_phy_addr = USB_BAM_PHY_BASE,
		.dst_pipe_index = 12,
		.data_fifo_base_offset = 0x1a00,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x2000,
		.desc_fifo_size = 0x100,
	},
	[0][2][USB_TO_PEER_PERIPHERAL] = {
		.src_phy_addr = USB_BAM_PHY_BASE,
		.src_pipe_index = 15,
		.dst_phy_addr = A2_BAM_PHY_BASE,
		.dst_pipe_index = 4,
		.data_fifo_base_offset = 0x3100,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x3700,
		.desc_fifo_size = 0x300,
	},
	[0][2][PEER_PERIPHERAL_TO_USB] = {
		.src_phy_addr = A2_BAM_PHY_BASE,
		.src_pipe_index = 5,
		.dst_phy_addr = USB_BAM_PHY_BASE,
		.dst_pipe_index = 14,
		.data_fifo_base_offset = 0x2a00,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x3000,
		.desc_fifo_size = 0x100,
	},
	[1][0][USB_TO_PEER_PERIPHERAL] = {
		.src_phy_addr = HSIC_BAM_PHY_BASE,
		.src_pipe_index = 1,
		.dst_phy_addr = A2_BAM_PHY_BASE,
		.dst_pipe_index = 0,
		.data_fifo_base_offset = 0x1100,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x1700,
		.desc_fifo_size = 0x300,
	},
	[1][0][PEER_PERIPHERAL_TO_USB] = {
		.src_phy_addr = A2_BAM_PHY_BASE,
		.src_pipe_index = 1,
		.dst_phy_addr = HSIC_BAM_PHY_BASE,
		.dst_pipe_index = 0,
		.data_fifo_base_offset = 0xa00,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x1000,
		.desc_fifo_size = 0x100,
	},
	[1][1][USB_TO_PEER_PERIPHERAL] = {
		.src_phy_addr = HSIC_BAM_PHY_BASE,
		.src_pipe_index = 3,
		.dst_phy_addr = A2_BAM_PHY_BASE,
		.dst_pipe_index = 2,
		.data_fifo_base_offset = 0x2100,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x2700,
		.desc_fifo_size = 0x300,
	},
	[1][1][PEER_PERIPHERAL_TO_USB] = {
		.src_phy_addr = A2_BAM_PHY_BASE,
		.src_pipe_index = 3,
		.dst_phy_addr = HSIC_BAM_PHY_BASE,
		.dst_pipe_index = 2,
		.data_fifo_base_offset = 0x1a00,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x2000,
		.desc_fifo_size = 0x100,
	},
	[1][2][USB_TO_PEER_PERIPHERAL] = {
		.src_phy_addr = HSIC_BAM_PHY_BASE,
		.src_pipe_index = 5,
		.dst_phy_addr = A2_BAM_PHY_BASE,
		.dst_pipe_index = 4,
		.data_fifo_base_offset = 0x3100,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x3700,
		.desc_fifo_size = 0x300,
	},
	[1][2][PEER_PERIPHERAL_TO_USB] = {
		.src_phy_addr = A2_BAM_PHY_BASE,
		.src_pipe_index = 5,
		.dst_phy_addr = HSIC_BAM_PHY_BASE,
		.dst_pipe_index = 4,
		.data_fifo_base_offset = 0x2a00,
		.data_fifo_size = 0x600,
		.desc_fifo_base_offset = 0x3000,
		.desc_fifo_size = 0x100,
	}
};

static struct msm_usb_bam_platform_data msm_usb_bam_pdata = {
	.connections = &msm_usb_bam_connections[0][0][0],
#ifndef CONFIG_USB_CI13XXX_MSM_HSIC
	.usb_active_bam = HSUSB_BAM,
#else
	.usb_active_bam = HSIC_BAM,
#endif
	.usb_bam_num_pipes = 16,
};

static struct msm_otg_platform_data msm_otg_pdata = {
	.mode			= USB_OTG,
	.otg_control	= OTG_PHY_CONTROL,
	.phy_type		= SNPS_28NM_INTEGRATED_PHY,
	.vbus_power		= msm_hsusb_vbus_power,
	.disable_reset_on_disconnect	= true,
	.enable_lpm_on_dev_suspend	= true,
	.core_clk_always_on_workaround = true,
};


static struct ci13xxx_platform_data msm_peripheral_pdata = {
	.usb_core_id = 0,
};

static struct msm_hsic_peripheral_platform_data
			msm_hsic_peripheral_pdata_private = {
	.core_clk_always_on_workaround = true,
};

static struct ci13xxx_platform_data msm_hsic_peripheral_pdata = {
	.usb_core_id = 1,
	.prv_data = &msm_hsic_peripheral_pdata_private,
};

#define PID_MAGIC_ID		0x71432909
#define SERIAL_NUM_MAGIC_ID	0x61945374
#define SERIAL_NUMBER_LENGTH	127
#define DLOAD_USB_BASE_ADD	0x2B0000C8

struct magic_num_struct {
	uint32_t pid;
	uint32_t serial_num;
};

struct dload_struct {
	uint32_t	reserved1;
	uint32_t	reserved2;
	uint32_t	reserved3;
	uint16_t	reserved4;
	uint16_t	pid;
	char		serial_number[SERIAL_NUMBER_LENGTH];
	uint16_t	reserved5;
	struct magic_num_struct magic_struct;
};

static int usb_diag_update_pid_and_serial_num(uint32_t pid, const char *snum)
{
	struct dload_struct __iomem *dload = 0;

	dload = ioremap(DLOAD_USB_BASE_ADD, sizeof(*dload));
	if (!dload) {
		pr_err("%s: cannot remap I/O memory region: %08x\n",
					__func__, DLOAD_USB_BASE_ADD);
		return -ENXIO;
	}

	pr_debug("%s: dload:%p pid:%x serial_num:%s\n",
				__func__, dload, pid, snum);
	/* update pid */
	dload->magic_struct.pid = PID_MAGIC_ID;
	dload->pid = pid;

	/* update serial number */
	dload->magic_struct.serial_num = 0;
	if (!snum) {
		memset(dload->serial_number, 0, SERIAL_NUMBER_LENGTH);
		goto out;
	}

	dload->magic_struct.serial_num = SERIAL_NUM_MAGIC_ID;
	strlcpy(dload->serial_number, snum, SERIAL_NUMBER_LENGTH);
out:
	iounmap(dload);
	return 0;
}

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

static struct tsens_platform_data msm_tsens_pdata  = {
	.tsens_factor		= 1000,
	.hw_type		= MDM_9615,
	.tsens_num_sensor	= 5,
	.slope = {1176, 1162, 1162, 1149, 1176},
};

static struct platform_device msm_tsens_device = {
	.name   = "tsens8960-tm",
	.id = -1,
};

static struct platform_device *common_devices[] = {
	&msm9615_device_acpuclk,
	&msm9615_device_dmov,
	&msm_device_smd,
#ifdef CONFIG_LTC4088_CHARGER
	&msm_device_charger,
#endif
	&msm_device_otg,
	&msm_device_hsic_peripheral,
	&msm_device_gadget_peripheral,
	&msm_device_hsusb_host,
	&msm_device_hsic_host,
	&msm_device_usb_bam,
	&msm_android_usb_device,
	&msm_android_usb_hsic_device,
	&msm9615_device_uart_gsbi4,
	&msm9615_device_ext_2p95v_vreg,
	&msm9615_device_ssbi_pmic1,
	&msm9615_device_qup_i2c_gsbi5,
	&msm9615_device_qup_spi_gsbi3,
	&msm_device_sps,
	&msm9615_slim_ctrl,
	&msm_device_nand,
	&msm_device_bam_dmux,
	&msm9615_rpm_device,
#ifdef CONFIG_HW_RANDOM_MSM
	&msm_device_rng,
#endif
#ifdef CONFIG_ION_MSM
	&ion_dev,
#endif

	&msm_pcm,
	&msm_multi_ch_pcm,
	&msm_pcm_routing,
	&msm_cpudai0,
	&msm_cpudai1,
	&msm_cpudai_bt_rx,
	&msm_cpudai_bt_tx,
	&msm_cpu_fe,
	&msm_stub_codec,
	&msm_voice,
	&msm_voip,
	&msm_i2s_cpudai0,
	&msm_i2s_cpudai1,
	&msm_pcm_hostless,
	&msm_cpudai_afe_01_rx,
	&msm_cpudai_afe_01_tx,
	&msm_cpudai_afe_02_rx,
	&msm_cpudai_afe_02_tx,
	&msm_pcm_afe,
	&msm_cpudai_auxpcm_rx,
	&msm_cpudai_auxpcm_tx,
	&msm_cpudai_sec_auxpcm_rx,
	&msm_cpudai_sec_auxpcm_tx,

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
	&msm9615_rpm_log_device,
	&msm9615_rpm_stat_device,
	&msm_tsens_device,
};

static void __init msm9615_i2c_init(void)
{
	u8 mach_mask = 0;
	int i;
	/* Mask is hardcoded to SURF (CDP).
	 * works on MTP with same configuration.
	 */
	mach_mask = I2C_SURF;
	if (machine_is_msm9615_cdp())
		mach_mask = I2C_SURF;
	else if (machine_is_msm9615_mtp())
		mach_mask = I2C_FFA;
	else
		pr_err("unmatched machine ID in register_i2c_devices\n");
	msm9615_device_qup_i2c_gsbi5.dev.platform_data =
					&msm9615_i2c_qup_gsbi5_pdata;
	for (i = 0; i < ARRAY_SIZE(msm9615_i2c_devices); ++i) {
		if (msm9615_i2c_devices[i].machs & mach_mask) {
			i2c_register_board_info(msm9615_i2c_devices[i].bus,
						msm9615_i2c_devices[i].info,
						msm9615_i2c_devices[i].len);
			}
	}
}

static void __init msm9615_reserve(void)
{
#ifdef CONFIG_ION_MSM
	reserve_info = &msm9615_reserve_info;
	msm_reserve();
#endif
}

static void __init msm9615_common_init(void)
{
	struct android_usb_platform_data *android_pdata =
				msm_android_usb_device.dev.platform_data;
	struct android_usb_platform_data *android_hsic_pdata =
				msm_android_usb_hsic_device.dev.platform_data;

	msm9615_device_init();
	msm9615_init_gpiomux();
	msm9615_i2c_init();
	regulator_suppress_info_printing();
	platform_device_register(&msm9615_device_rpm_regulator);
	msm_xo_init();
	msm_clock_init(&msm9615_clock_init_data);
	msm9615_init_buses();
	msm9615_device_qup_spi_gsbi3.dev.platform_data =
				&msm9615_qup_spi_gsbi3_pdata;
	msm9615_device_ssbi_pmic1.dev.platform_data =
						&msm9615_ssbi_pm8018_pdata;
	pm8018_platform_data.num_regulators = msm_pm8018_regulator_pdata_len;

	msm_device_otg.dev.platform_data = &msm_otg_pdata;
	msm_otg_pdata.phy_init_seq = shelby_phy_init_seq;
	msm_device_gadget_peripheral.dev.platform_data =
		&msm_peripheral_pdata;
	msm_device_hsic_peripheral.dev.platform_data =
		&msm_hsic_peripheral_pdata;
	msm_device_usb_bam.dev.platform_data = &msm_usb_bam_pdata;
	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));
	msm9615_pm8xxx_gpio_mpp_init();

	/* Ensure ar6000pm device is registered before MMC/SDC */
	msm9615_init_ar6000pm();

	msm9615_init_mmc();
	slim_register_board_info(msm_slim_devices,
		ARRAY_SIZE(msm_slim_devices));

	android_pdata->update_pid_and_serial_num =
					usb_diag_update_pid_and_serial_num;
	android_hsic_pdata->update_pid_and_serial_num =
					usb_diag_update_pid_and_serial_num;

	msm_pm_boot_pdata.p_addr = allocate_contiguous_ebi_nomap(SZ_8, SZ_64K);
	BUG_ON(msm_pm_boot_init(&msm_pm_boot_pdata));
	msm_tsens_early_init(&msm_tsens_pdata);
}

static void __init msm9615_cdp_init(void)
{
	msm9615_common_init();
#ifdef CONFIG_FB_MSM
	mdm9615_init_fb();
#endif
}

static void __init msm9615_mtp_init(void)
{
	msm9615_common_init();
}

#ifdef CONFIG_FB_MSM
static void __init mdm9615_allocate_memory_regions(void)
{
	mdm9615_allocate_fb_region();
}
#endif

MACHINE_START(MSM9615_CDP, "QCT MSM9615 CDP")
	.map_io = msm9615_map_io,
	.init_irq = msm9615_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm9615_cdp_init,
	.reserve = msm9615_reserve,
#ifdef CONFIG_FB_MSM
	.init_early = mdm9615_allocate_memory_regions,
#endif
	.restart = msm_restart,
MACHINE_END

MACHINE_START(MSM9615_MTP, "QCT MSM9615 MTP")
	.map_io = msm9615_map_io,
	.init_irq = msm9615_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm9615_mtp_init,
	.reserve = msm9615_reserve,
	.restart = msm_restart,
MACHINE_END
