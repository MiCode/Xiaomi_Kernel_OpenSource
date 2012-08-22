/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/msm_ssbi.h>
#include <linux/mfd/pmic8058.h>
#include <linux/regulator/pmic8058-regulator.h>
#include <linux/i2c.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/regulator/pm8058-xo.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>

#include <mach/board.h>
#include <mach/memory.h>
#include <mach/msm_iomap.h>
#include <mach/dma.h>
#include <mach/sirc.h>
#include <mach/restart.h>

#include <mach/socinfo.h>
#include "devices.h"
#include "timer.h"
#include "pm.h"
#include "spm.h"
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/msm_adc.h>
#include <linux/m_adcproc.h>
#include <linux/platform_data/qcom_crypto_device.h>

#define PMIC_GPIO_INT		144
#define PMIC_VREG_WLAN_LEVEL	2900
#define PMIC_GPIO_SD_DET	165

#define GPIO_EPHY_RST_N		37
#define GPIO_MAC_TXD_3      119
#define GPIO_MAC_TXD_2      120
#define GPIO_MAC_TXD_1      121
#define GPIO_MAC_TXD_0      122
#define GPIO_MAC_TX_EN      123
#define GPIO_MAC_MDIO       127
#define GPIO_MAC_MDC        128
#define GPIO_MAC_TX_CLK     133
#define GPIO_GRFC_FTR0_0	136 /* GRFC 20 */
#define GPIO_GRFC_FTR0_1	137 /* GRFC 21 */
#define GPIO_GRFC_FTR1_0	145 /* GRFC 22 */
#define GPIO_GRFC_FTR1_1	93 /* GRFC 19 */
#define GPIO_GRFC_2		110
#define GPIO_GRFC_3		109
#define GPIO_GRFC_4		108
#define GPIO_GRFC_5		107
#define GPIO_GRFC_6		106
#define GPIO_GRFC_7		105
#define GPIO_GRFC_8		104
#define GPIO_GRFC_9		103
#define GPIO_GRFC_10		102
#define GPIO_GRFC_11		101
#define GPIO_GRFC_13		99
#define GPIO_GRFC_14		98
#define GPIO_GRFC_15		97
#define GPIO_GRFC_16		96
#define GPIO_GRFC_17		95
#define GPIO_GRFC_18		94
#define GPIO_GRFC_24		150
#define GPIO_GRFC_25		151
#define GPIO_GRFC_26		152
#define GPIO_GRFC_27		153
#define GPIO_GRFC_28		154
#define GPIO_GRFC_29		155

#define GPIO_USER_FIRST		58
#define GPIO_USER_LAST		63

#define GPIO_UIM_RESET		75
#define GPIO_UIM_DATA_IO	76
#define GPIO_UIM_CLOCK		77

#define GPIO_PM_UIM_M_RST	26	/* UIM_RST input */
#define GPIO_PM_UIM_RST		27	/* UIM_RST output */
#define GPIO_PM_UIM_M_CLK	28	/* UIM_CLK input */
#define GPIO_PM_UIM_CLK		29	/* UIM_CLK output */

#define FPGA_SDCC_STATUS        0x8E0001A8

/* Macros assume PMIC GPIOs start at 0 */
#define PM8058_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio + NR_MSM_GPIOS)
#define PM8058_GPIO_SYS_TO_PM(sys_gpio) (sys_gpio - NR_MSM_GPIOS)
#define PM8058_MPP_BASE			(NR_MSM_GPIOS + PM8058_GPIOS)
#define PM8058_MPP_PM_TO_SYS(pm_gpio)	(pm_gpio + PM8058_MPP_BASE)
#define PM8058_MPP_SYS_TO_PM(sys_gpio)	(sys_gpio - PM8058_MPP_BASE)

#define PMIC_GPIO_5V_PA_PWR	21	/* PMIC GPIO Number 22 */
#define PMIC_GPIO_4_2V_PA_PWR	22	/* PMIC GPIO Number 23 */
#define PMIC_MPP_UIM_M_DATA	0	/* UIM_DATA input */
#define PMIC_MPP_UIM_DATA	1	/* UIM_DATA output */
#define PMIC_MPP_3		2	/* PMIC MPP Number 3 */
#define PMIC_MPP_6		5	/* PMIC MPP Number 6 */
#define PMIC_MPP_7		6	/* PMIC MPP Number 7 */
#define PMIC_MPP_10		9	/* PMIC MPP Number 10 */

/*
 * PM8058
 */
struct pm8xxx_mpp_init_info {
	unsigned			mpp;
	struct pm8xxx_mpp_config_data	config;
};

#define PM8XXX_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8058_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

static int pm8058_gpios_init(void)
{
	int i;
	int rc;
	struct pm8058_gpio_cfg {
		int                gpio;
		struct pm_gpio	   cfg;
	};

	struct pm8058_gpio_cfg gpio_cfgs[] = {
		{				/* 5V PA Power */
			PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_5V_PA_PWR),
			{
				.vin_sel = 0,
				.direction = PM_GPIO_DIR_BOTH,
				.output_value = 1,
				.output_buffer = PM_GPIO_OUT_BUF_CMOS,
				.pull = PM_GPIO_PULL_DN,
				.out_strength = PM_GPIO_STRENGTH_HIGH,
				.function = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol = 0,
			},
		},
		{				/* 4.2V PA Power */
			PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_4_2V_PA_PWR),
			{
				.vin_sel = 0,
				.direction = PM_GPIO_DIR_BOTH,
				.output_value = 1,
				.output_buffer = PM_GPIO_OUT_BUF_CMOS,
				.pull = PM_GPIO_PULL_DN,
				.out_strength = PM_GPIO_STRENGTH_HIGH,
				.function = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol = 0,
			},
		},
	};

	for (i = 0; i < ARRAY_SIZE(gpio_cfgs); ++i) {
		rc = pm8xxx_gpio_config(gpio_cfgs[i].gpio, &gpio_cfgs[i].cfg);
		if (rc < 0) {
			pr_err("%s pmic gpio config failed\n", __func__);
			return rc;
		}
	}

	return 0;
}

static int pm8058_mpps_init(void)
{
	int rc, i;

	struct pm8xxx_mpp_init_info pm8058_mpps[] = {
		PM8XXX_MPP_INIT(PMIC_MPP_3, A_OUTPUT,
			PM8XXX_MPP_AOUT_LVL_1V25_2, AOUT_CTRL_ENABLE),
		PM8XXX_MPP_INIT(PMIC_MPP_6, A_OUTPUT,
			PM8XXX_MPP_AOUT_LVL_1V25_2, AOUT_CTRL_ENABLE),
		PM8XXX_MPP_INIT(PMIC_MPP_UIM_M_DATA, D_BI_DIR,
			PM8058_MPP_DIG_LEVEL_S3, BI_PULLUP_1KOHM),
		PM8XXX_MPP_INIT(PMIC_MPP_UIM_DATA, D_BI_DIR,
			PM8058_MPP_DIG_LEVEL_L3, BI_PULLUP_30KOHM),
	};

	for (i = 0; i < ARRAY_SIZE(pm8058_mpps); i++) {
		rc = pm8xxx_mpp_config(pm8058_mpps[i].mpp,
					&pm8058_mpps[i].config);
		if (rc) {
			pr_err("%s: Config %d mpp pm 8058 failed\n",
						__func__, pm8058_mpps[i].mpp);
			return rc;
		}
	}

	return 0;
}

static struct regulator_consumer_supply pm8058_vreg_supply[PM8058_VREG_MAX] = {
	[PM8058_VREG_ID_L3] = REGULATOR_SUPPLY("8058_l3", NULL),
	[PM8058_VREG_ID_L8] = REGULATOR_SUPPLY("8058_l8", NULL),
	[PM8058_VREG_ID_L9] = REGULATOR_SUPPLY("8058_l9", NULL),
	[PM8058_VREG_ID_L14] = REGULATOR_SUPPLY("8058_l14", NULL),
	[PM8058_VREG_ID_L15] = REGULATOR_SUPPLY("8058_l15", NULL),
	[PM8058_VREG_ID_L18] = REGULATOR_SUPPLY("8058_l18", NULL),
	[PM8058_VREG_ID_S4] = REGULATOR_SUPPLY("8058_s4", NULL),

	[PM8058_VREG_ID_LVS0] = REGULATOR_SUPPLY("8058_lvs0", NULL),
};

#define PM8058_VREG_INIT(_id, _min_uV, _max_uV, _modes, _ops, _apply_uV, \
			_always_on, _pull_down) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask = _modes, \
				.valid_ops_mask = _ops, \
				.min_uV = _min_uV, \
				.max_uV = _max_uV, \
				.apply_uV = _apply_uV, \
				.always_on = _always_on, \
			}, \
			.num_consumer_supplies = 1, \
			.consumer_supplies = &pm8058_vreg_supply[_id], \
		}, \
		.id = _id, \
		.pull_down_enable = _pull_down, \
		.pin_ctrl = 0, \
		.pin_fn = PM8058_VREG_PIN_FN_ENABLE, \
	}

#define PM8058_VREG_INIT_LDO(_id, _min_uV, _max_uV) \
	PM8058_VREG_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL | \
			REGULATOR_MODE_IDLE | REGULATOR_MODE_STANDBY, \
			REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS | \
			REGULATOR_CHANGE_MODE, 1, 1, 1)

#define PM8058_VREG_INIT_SMPS(_id, _min_uV, _max_uV) \
	PM8058_VREG_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL | \
			REGULATOR_MODE_IDLE | REGULATOR_MODE_STANDBY, \
			REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS | \
			REGULATOR_CHANGE_MODE, 1, 1, 1)

#define PM8058_VREG_INIT_LVS(_id, _min_uV, _max_uV) \
	PM8058_VREG_INIT(_id, _min_uV, _min_uV, REGULATOR_MODE_NORMAL, \
			REGULATOR_CHANGE_STATUS, 0, 0, 1)

static struct pm8058_vreg_pdata pm8058_vreg_init[] = {
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L3, 3000000, 3000000),
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L8, 2200000, 2200000),
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L9, 2050000, 2050000),
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L14, 2850000, 2850000),
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L15, 2200000, 2200000),
	PM8058_VREG_INIT_LDO(PM8058_VREG_ID_L18, 2200000, 2200000),
	PM8058_VREG_INIT_LVS(PM8058_VREG_ID_LVS0, 1800000, 1800000),
	PM8058_VREG_INIT_SMPS(PM8058_VREG_ID_S4, 1300000, 1300000),
};

#ifdef CONFIG_SENSORS_MSM_ADC
static struct adc_access_fn xoadc_fn = {
	pm8058_xoadc_select_chan_and_start_conv,
	pm8058_xoadc_read_adc_code,
	pm8058_xoadc_get_properties,
	pm8058_xoadc_slot_request,
	pm8058_xoadc_restore_slot,
	pm8058_xoadc_calibrate,
};

static struct msm_adc_channels msm_adc_channels_data[] = {
	{"pmic_therm", CHANNEL_ADC_DIE_TEMP, 0, &xoadc_fn, CHAN_PATH_TYPE12,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE1, scale_pmic_therm},
	{"ref_1250mv", CHANNEL_ADC_1250_REF, 0, &xoadc_fn, CHAN_PATH_TYPE13,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
	{"xo_therm", CHANNEL_ADC_XOTHERM, 0, &xoadc_fn, CHAN_PATH_TYPE_NONE,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE5, tdkntcgtherm},
	{"fsm_therm", CHANNEL_ADC_FSM_THERM, 0, &xoadc_fn, CHAN_PATH_TYPE6,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE5, tdkntcgtherm},
	{"pa_therm", CHANNEL_ADC_PA_THERM, 0, &xoadc_fn, CHAN_PATH_TYPE7,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE5, tdkntcgtherm},
};

static struct msm_adc_platform_data msm_adc_pdata = {
	.channel = msm_adc_channels_data,
	.num_chan_supported = ARRAY_SIZE(msm_adc_channels_data),
	.target_hw = FSM_9xxx,
};

static struct platform_device msm_adc_device = {
	.name   = "msm_adc",
	.id = -1,
	.dev = {
		.platform_data = &msm_adc_pdata,
	},
};

static void pmic8058_xoadc_mpp_config(void)
{
	int rc, i;
	struct pm8xxx_mpp_init_info xoadc_mpps[] = {
		PM8XXX_MPP_INIT(PMIC_MPP_7, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH5,
							AOUT_CTRL_DISABLE),
		PM8XXX_MPP_INIT(PMIC_MPP_10, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH6,
							AOUT_CTRL_DISABLE),
	};
	for (i = 0; i < ARRAY_SIZE(xoadc_mpps); i++) {
		rc = pm8xxx_mpp_config(xoadc_mpps[i].mpp,
					&xoadc_mpps[i].config);
		if (rc) {
			pr_err("%s: Config MPP %d of PM8058 failed\n",
					__func__, xoadc_mpps[i].mpp);
		}
	}
}

static struct regulator *vreg_ldo18_adc;

static int pmic8058_xoadc_vreg_config(int on)
{
	int rc;

	if (on) {
		rc = regulator_enable(vreg_ldo18_adc);
		if (rc)
			pr_err("%s: Enable of regulator ldo18_adc "
						"failed\n", __func__);
	} else {
		rc = regulator_disable(vreg_ldo18_adc);
		if (rc)
			pr_err("%s: Disable of regulator ldo18_adc "
						"failed\n", __func__);
	}

	return rc;
}

static int pmic8058_xoadc_vreg_setup(void)
{
	int rc;

	vreg_ldo18_adc = regulator_get(NULL, "8058_l18");
	if (IS_ERR(vreg_ldo18_adc)) {
		pr_err("%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(vreg_ldo18_adc));
		rc = PTR_ERR(vreg_ldo18_adc);
		goto fail;
	}

	rc = regulator_set_voltage(vreg_ldo18_adc, 2200000, 2200000);
	if (rc) {
		pr_err("%s: unable to set ldo18 voltage to 2.2V\n", __func__);
		goto fail;
	}

	return rc;
fail:
	regulator_put(vreg_ldo18_adc);
	return rc;
}

static void pmic8058_xoadc_vreg_shutdown(void)
{
	regulator_put(vreg_ldo18_adc);
}

/* usec. For this ADC,
 * this time represents clk rate @ txco w/ 1024 decimation ratio.
 * Each channel has different configuration, thus at the time of starting
 * the conversion, xoadc will return actual conversion time
 * */
static struct adc_properties pm8058_xoadc_data = {
	.adc_reference          = 2200, /* milli-voltage for this adc */
	.bitresolution         = 15,
	.bipolar                = 0,
	.conversiontime         = 54,
};

static struct xoadc_platform_data pm8058_xoadc_pdata = {
	.xoadc_prop = &pm8058_xoadc_data,
	.xoadc_mpp_config = pmic8058_xoadc_mpp_config,
	.xoadc_vreg_set = pmic8058_xoadc_vreg_config,
	.xoadc_num = XOADC_PMIC_0,
	.xoadc_vreg_setup = pmic8058_xoadc_vreg_setup,
	.xoadc_vreg_shutdown = pmic8058_xoadc_vreg_shutdown,
};
#endif

#define XO_CONSUMERS(_id) \
	static struct regulator_consumer_supply xo_consumers_##_id[]

/*
 * Consumer specific regulator names:
 *                       regulator name         consumer dev_name
 */
XO_CONSUMERS(A0) = {
	REGULATOR_SUPPLY("8058_xo_a0", NULL),
	REGULATOR_SUPPLY("a0_clk_buffer", "fsm_xo_driver"),
};
XO_CONSUMERS(A1) = {
	REGULATOR_SUPPLY("8058_xo_a1", NULL),
	REGULATOR_SUPPLY("a1_clk_buffer", "fsm_xo_driver"),
};

#define PM8058_XO_INIT(_id, _modes, _ops, _always_on) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask = _modes, \
				.valid_ops_mask = _ops, \
				.boot_on = 1, \
				.always_on = _always_on, \
			}, \
			.num_consumer_supplies = \
				ARRAY_SIZE(xo_consumers_##_id),\
			.consumer_supplies = xo_consumers_##_id, \
		}, \
		.id = PM8058_XO_ID_##_id, \
	}

#define PM8058_XO_INIT_AX(_id) \
	PM8058_XO_INIT(_id, REGULATOR_MODE_NORMAL, REGULATOR_CHANGE_STATUS, 0)

static struct pm8058_xo_pdata pm8058_xo_init_pdata[] = {
	PM8058_XO_INIT_AX(A0),
	PM8058_XO_INIT_AX(A1),
};

#define PM8058_GPIO_INT		47

static struct pm8xxx_irq_platform_data pm8xxx_irq_pdata = {
	.irq_base		= PMIC8058_IRQ_BASE,
	.devirq			= MSM_GPIO_TO_INT(PM8058_GPIO_INT),
	.irq_trigger_flag	= IRQF_TRIGGER_LOW,
};

static struct pm8xxx_gpio_platform_data pm8xxx_gpio_pdata = {
	.gpio_base	= PM8058_GPIO_PM_TO_SYS(0),
};

static struct pm8xxx_mpp_platform_data pm8xxx_mpp_pdata = {
	.mpp_base	= PM8058_MPP_PM_TO_SYS(0),
};

static struct pm8058_platform_data pm8058_fsm9xxx_data = {
	.irq_pdata		= &pm8xxx_irq_pdata,
	.gpio_pdata		= &pm8xxx_gpio_pdata,
	.mpp_pdata		= &pm8xxx_mpp_pdata,
	.regulator_pdatas	= pm8058_vreg_init,
	.num_regulators		= ARRAY_SIZE(pm8058_vreg_init),
	.xo_buffer_pdata	= pm8058_xo_init_pdata,
	.num_xo_buffers		= ARRAY_SIZE(pm8058_xo_init_pdata),
#ifdef CONFIG_SENSORS_MSM_ADC
	.xoadc_pdata		= &pm8058_xoadc_pdata,
#endif
};

#ifdef CONFIG_MSM_SSBI
static struct msm_ssbi_platform_data fsm9xxx_ssbi_pm8058_pdata = {
	.controller_type = FSM_SBI_CTRL_SSBI,
	.slave  = {
		.name                   = "pm8058-core",
		.platform_data          = &pm8058_fsm9xxx_data,
	},
};
#endif

static int __init buses_init(void)
{
	if (gpio_tlmm_config(GPIO_CFG(PMIC_GPIO_INT, 5, GPIO_CFG_INPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE))
		pr_err("%s: gpio_tlmm_config (gpio=%d) failed\n",
			__func__, PMIC_GPIO_INT);

	return 0;
}

/*
 * EPHY
 */

static struct msm_gpio phy_config_data[] = {
	{ GPIO_CFG(GPIO_EPHY_RST_N, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "MAC_RST_N" },
	{ GPIO_CFG(GPIO_MAC_TXD_3, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "MAC_TXD_3"},
	{ GPIO_CFG(GPIO_MAC_TXD_2, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "MAC_TXD_2"},
	{ GPIO_CFG(GPIO_MAC_TXD_1, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "MAC_TXD_1"},
	{ GPIO_CFG(GPIO_MAC_TXD_0, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "MAC_TXD_0"},
	{ GPIO_CFG(GPIO_MAC_TX_EN, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "MAC_TX_EN"},
	{ GPIO_CFG(GPIO_MAC_TX_CLK, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_10MA), "MAC_TX_CLK"},
	{ GPIO_CFG(GPIO_MAC_MDIO, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_6MA), "MDIO_MAC_MDIO"},
	{ GPIO_CFG(GPIO_MAC_MDC, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_6MA), "MDC_MAC_MDC"},
};

static int __init phy_init(void)
{
	msm_gpios_request_enable(phy_config_data, ARRAY_SIZE(phy_config_data));
	gpio_direction_output(GPIO_EPHY_RST_N, 0);
	udelay(100);
	gpio_set_value(GPIO_EPHY_RST_N, 1);

	return 0;
}

/*
 * RF
 */

static struct msm_gpio grfc_config_data[] = {
	{ GPIO_CFG(GPIO_GRFC_FTR0_0, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "HH_RFMODE1_0" },
	{ GPIO_CFG(GPIO_GRFC_FTR0_1, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "HH_RFMODE1_1" },
	{ GPIO_CFG(GPIO_GRFC_FTR1_0, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "HH_RFMODE2_0" },
	{ GPIO_CFG(GPIO_GRFC_FTR1_1, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "HH_RFMODE2_1" },
	{ GPIO_CFG(GPIO_GRFC_2, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_2" },
	{ GPIO_CFG(GPIO_GRFC_3, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_3" },
	{ GPIO_CFG(GPIO_GRFC_4, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_4" },
	{ GPIO_CFG(GPIO_GRFC_5, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_5" },
	{ GPIO_CFG(GPIO_GRFC_6, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_6" },
	{ GPIO_CFG(GPIO_GRFC_7, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_7" },
	{ GPIO_CFG(GPIO_GRFC_8, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_8" },
	{ GPIO_CFG(GPIO_GRFC_9, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_9" },
	{ GPIO_CFG(GPIO_GRFC_10, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_10" },
	{ GPIO_CFG(GPIO_GRFC_11, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_11" },
	{ GPIO_CFG(GPIO_GRFC_13, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_13" },
	{ GPIO_CFG(GPIO_GRFC_14, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_14" },
	{ GPIO_CFG(GPIO_GRFC_15, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_15" },
	{ GPIO_CFG(GPIO_GRFC_16, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_16" },
	{ GPIO_CFG(GPIO_GRFC_17, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_17" },
	{ GPIO_CFG(GPIO_GRFC_18, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_18" },
	{ GPIO_CFG(GPIO_GRFC_24, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_24" },
	{ GPIO_CFG(GPIO_GRFC_25, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_25" },
	{ GPIO_CFG(GPIO_GRFC_26, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_26" },
	{ GPIO_CFG(GPIO_GRFC_27, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_27" },
	{ GPIO_CFG(GPIO_GRFC_28, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_28" },
	{ GPIO_CFG(GPIO_GRFC_29, 7, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), "GPIO_GRFC_29" },
	{ GPIO_CFG(39, 1, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "PP2S_EXT_SYNC" },
};

static int __init grfc_init(void)
{
	msm_gpios_request_enable(grfc_config_data,
		ARRAY_SIZE(grfc_config_data));

	return 0;
}

/*
 * UART
 */

#ifdef CONFIG_SERIAL_MSM_CONSOLE
static struct msm_gpio uart1_config_data[] = {
	{ GPIO_CFG(138, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"UART1_Rx" },
	{ GPIO_CFG(139, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"UART1_Tx" },
};

static void fsm9xxx_init_uart1(void)
{
	msm_gpios_request_enable(uart1_config_data,
			ARRAY_SIZE(uart1_config_data));

}
#endif

static struct msm_gpio uart3_uim_config_data[] = {
	{ GPIO_CFG(GPIO_UIM_RESET, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), "UIM_Reset" },
	{ GPIO_CFG(GPIO_UIM_DATA_IO, 2, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), "UIM_Data" },
	{ GPIO_CFG(GPIO_UIM_CLOCK, 2, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), "UIM_Clock" },
};

static void fsm9xxx_init_uart3_uim(void)
{
	struct pm_gpio pmic_uim_gpio_in = {
		.direction	= PM_GPIO_DIR_IN,
		.pull		= PM_GPIO_PULL_NO,
		.out_strength	= PM_GPIO_STRENGTH_HIGH,
		.function	= PM_GPIO_FUNC_PAIRED,
		.vin_sel	= PM8058_GPIO_VIN_L3,
	};
	struct pm_gpio pmic_uim_gpio_out = {
		.direction	= PM_GPIO_DIR_OUT,
		.pull		= PM_GPIO_PULL_NO,
		.out_strength	= PM_GPIO_STRENGTH_HIGH,
		.function	= PM_GPIO_FUNC_PAIRED,
		.vin_sel	= PM8058_GPIO_VIN_L3,
	};

	/* TLMM */
	msm_gpios_request_enable(uart3_uim_config_data,
			ARRAY_SIZE(uart3_uim_config_data));

	/* Put UIM to reset state */
	gpio_direction_output(GPIO_UIM_RESET, 0);
	gpio_set_value(GPIO_UIM_RESET, 0);
	gpio_export(GPIO_UIM_RESET, false);

	/* PMIC */
	pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(GPIO_PM_UIM_M_RST),
		&pmic_uim_gpio_in);
	pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(GPIO_PM_UIM_RST),
		&pmic_uim_gpio_out);
	pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(GPIO_PM_UIM_M_CLK),
		&pmic_uim_gpio_in);
	pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(GPIO_PM_UIM_CLK),
		&pmic_uim_gpio_out);
}

/*
 * SSBI
 */

#ifdef CONFIG_I2C_SSBI
static struct msm_i2c_ssbi_platform_data msm_i2c_ssbi2_pdata = {
	.controller_type = FSM_SBI_CTRL_SSBI,
};

static struct msm_i2c_ssbi_platform_data msm_i2c_ssbi3_pdata = {
	.controller_type = FSM_SBI_CTRL_SSBI,
};
#endif

#if defined(CONFIG_I2C_SSBI) || defined(CONFIG_MSM_SSBI)
/* Intialize GPIO configuration for SSBI */
static struct msm_gpio ssbi_gpio_config_data[] = {
	{ GPIO_CFG(140, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
		"SSBI_1" },
	{ GPIO_CFG(141, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
		"SSBI_2" },
	{ GPIO_CFG(92, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
		"SSBI_3" },
};

static void
fsm9xxx_init_ssbi_gpio(void)
{
	msm_gpios_request_enable(ssbi_gpio_config_data,
		ARRAY_SIZE(ssbi_gpio_config_data));

}
#endif

/*
 * User GPIOs
 */

static void user_gpios_init(void)
{
	unsigned int gpio;

	for (gpio = GPIO_USER_FIRST; gpio <= GPIO_USER_LAST; ++gpio)
		gpio_tlmm_config(GPIO_CFG(gpio, 0, GPIO_CFG_INPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
}

/*
 * Crypto
 */

#define QCE_SIZE		0x10000

#define QCE_0_BASE		0x80C00000
#define QCE_1_BASE		0x80E00000
#define QCE_2_BASE		0x81000000

#define QCE_NO_HW_KEY_SUPPORT		0 /* No shared HW key with external */
#define QCE_NO_SHARE_CE_RESOURCE	0 /* No CE resource shared with TZ */
#define QCE_NO_CE_SHARED		0 /* CE not shared with TZ */
#define QCE_NO_SHA_HMAC_SUPPORT		0 /* No SHA-HMAC by SHA operation */

static struct resource qcrypto_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE1_IN_CHAN,
		.end = DMOV_CE1_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE1_IN_CRCI,
		.end = DMOV_CE1_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE1_OUT_CRCI,
		.end = DMOV_CE1_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[4] = {
		.name = "crypto_crci_hash",
		.start = DMOV_CE1_HASH_CRCI,
		.end = DMOV_CE1_HASH_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

static struct msm_ce_hw_support qcrypto_ce_hw_suppport = {
	.ce_shared = QCE_NO_CE_SHARED,
	.shared_ce_resource = QCE_NO_SHARE_CE_RESOURCE,
	.hw_key_support = QCE_NO_HW_KEY_SUPPORT,
	.sha_hmac = QCE_NO_SHA_HMAC_SUPPORT,
	.bus_scale_table = NULL,
};

struct platform_device qcrypto_device = {
	.name		= "qcrypto",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcrypto_resources),
	.resource	= qcrypto_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &qcrypto_ce_hw_suppport,
	},
};

static struct resource qcedev_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE1_IN_CHAN,
		.end = DMOV_CE1_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE1_IN_CRCI,
		.end = DMOV_CE1_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE1_OUT_CRCI,
		.end = DMOV_CE1_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[4] = {
		.name = "crypto_crci_hash",
		.start = DMOV_CE1_HASH_CRCI,
		.end = DMOV_CE1_HASH_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

static struct msm_ce_hw_support qcedev_ce_hw_suppport = {
	.ce_shared = QCE_NO_CE_SHARED,
	.shared_ce_resource = QCE_NO_SHARE_CE_RESOURCE,
	.hw_key_support = QCE_NO_HW_KEY_SUPPORT,
	.sha_hmac = QCE_NO_SHA_HMAC_SUPPORT,
	.bus_scale_table = NULL,
};

static struct platform_device qcedev_device = {
	.name		= "qce",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcedev_resources),
	.resource	= qcedev_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &qcedev_ce_hw_suppport,
	},
};

static struct resource ota_qcrypto_resources[] = {
	[0] = {
		.start = QCE_1_BASE,
		.end = QCE_1_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE2_IN_CHAN,
		.end = DMOV_CE2_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE2_IN_CRCI,
		.end = DMOV_CE2_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE2_OUT_CRCI,
		.end = DMOV_CE2_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[4] = {
		.name = "crypto_crci_hash",
		.start = DMOV_CE2_HASH_CRCI,
		.end = DMOV_CE2_HASH_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device ota_qcrypto_device = {
	.name		= "qcota",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(ota_qcrypto_resources),
	.resource	= ota_qcrypto_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct platform_device fsm9xxx_device_acpuclk = {
	.name		= "acpuclk-9xxx",
	.id		= -1,
};

/*
 * Devices
 */

static struct platform_device *devices[] __initdata = {
	&fsm9xxx_device_acpuclk,
	&msm_device_smd,
	&msm_device_dmov,
	&msm_device_nand,
#ifdef CONFIG_MSM_SSBI
	&msm_device_ssbi_pmic1,
#endif
#ifdef CONFIG_I2C_SSBI
	&msm_device_ssbi2,
	&msm_device_ssbi3,
#endif
#ifdef CONFIG_SENSORS_MSM_ADC
	&msm_adc_device,
#endif
#ifdef CONFIG_I2C_QUP
	&msm_gsbi1_qup_i2c_device,
#endif
#if defined(CONFIG_SERIAL_MSM) || defined(CONFIG_MSM_SERIAL_DEBUGGER)
	&msm_device_uart1,
#endif
	&msm_device_uart3,
#if defined(CONFIG_QFP_FUSE)
	&fsm_qfp_fuse_device,
#endif
	&qfec_device,
	&qcrypto_device,
	&qcedev_device,
	&ota_qcrypto_device,
	&fsm_xo_device,
	&fsm9xxx_device_watchdog,
};

static void __init fsm9xxx_init_irq(void)
{
	msm_init_irq();
	msm_init_sirc();
}

#ifdef CONFIG_MSM_SPM
static struct msm_spm_platform_data msm_spm_data __initdata = {
	.reg_base_addr = MSM_SAW_BASE,

	.reg_init_values[MSM_SPM_REG_SAW_CFG] = 0x05,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_CTL] = 0x18,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_SLP_TMR_DLY] = 0x00006666,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_WAKE_TMR_DLY] = 0xFF000666,

	.reg_init_values[MSM_SPM_REG_SAW_SPM_PMIC_CTL] = 0xE0F272,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_CLK_EN] = 0x01,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_PRECLMP_EN] = 0x03,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_POSTCLMP_EN] = 0x00,

	.reg_init_values[MSM_SPM_REG_SAW_SLP_CLMP_EN] = 0x01,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_RST_EN] = 0x00,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_MPM_CFG] = 0x00,

	.awake_vlevel = 0xF2,
	.retention_vlevel = 0xE0,
	.collapse_vlevel = 0x72,
	.retention_mid_vlevel = 0xE0,
	.collapse_mid_vlevel = 0xE0,
};
#endif

static void __init fsm9xxx_init(void)
{
	regulator_has_full_constraints();

#if defined(CONFIG_I2C_SSBI) || defined(CONFIG_MSM_SSBI)
	fsm9xxx_init_ssbi_gpio();
#endif
#ifdef CONFIG_MSM_SSBI
	msm_device_ssbi_pmic1.dev.platform_data =
			&fsm9xxx_ssbi_pm8058_pdata;
#endif
	buses_init();

	platform_add_devices(devices, ARRAY_SIZE(devices));

#ifdef CONFIG_MSM_SPM
	msm_spm_init(&msm_spm_data, 1);
#endif
	pm8058_gpios_init();
	pm8058_mpps_init();
	phy_init();
	grfc_init();
	user_gpios_init();

#ifdef CONFIG_SERIAL_MSM_CONSOLE
	fsm9xxx_init_uart1();
#endif
	fsm9xxx_init_uart3_uim();
#ifdef CONFIG_I2C_SSBI
	msm_device_ssbi2.dev.platform_data = &msm_i2c_ssbi2_pdata;
	msm_device_ssbi3.dev.platform_data = &msm_i2c_ssbi3_pdata;
#endif
}

static void __init fsm9xxx_map_io(void)
{
	msm_shared_ram_phys = 0x00100000;
	msm_map_fsm9xxx_io();
	msm_clock_init(&fsm9xxx_clock_init_data);
	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed!\n",
		       __func__);

}

MACHINE_START(FSM9XXX_SURF, "QCT FSM9XXX")
	.atag_offset = 0x100,
	.map_io = fsm9xxx_map_io,
	.init_irq = fsm9xxx_init_irq,
	.handle_irq = vic_handle_irq,
	.init_machine = fsm9xxx_init,
	.timer = &msm_timer,
	.restart = fsm_restart,
MACHINE_END
