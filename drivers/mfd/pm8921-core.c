/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/msm_ssbi.h>
#include <linux/mfd/core.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/leds-pm8xxx.h>

#define REG_HWREV		0x002  /* PMIC4 revision */
#define REG_HWREV_2		0x0E8  /* PMIC4 revision 2 */

#define REG_MPP_BASE		0x050
#define REG_IRQ_BASE		0x1BB

#define REG_TEMP_ALARM_CTRL	0x1B
#define REG_TEMP_ALARM_PWM	0x9B

#define REG_BATT_ALARM_THRESH	0x023
#define REG_BATT_ALARM_CTRL1	0x024
#define REG_BATT_ALARM_CTRL2	0x021
#define REG_BATT_ALARM_PWM_CTRL	0x020

#define PM8921_VERSION_MASK	0xFFF0
#define PM8921_VERSION_VALUE	0x06F0
#define PM8922_VERSION_VALUE	0x0AF0
#define PM8921_REVISION_MASK	0x000F

#define REG_PM8921_PON_CNTRL_3	0x01D
#define PM8921_RESTART_REASON_MASK	0x07

#define SINGLE_IRQ_RESOURCE(_name, _irq) \
{ \
	.name	= _name, \
	.start	= _irq, \
	.end	= _irq, \
	.flags	= IORESOURCE_IRQ, \
}

struct pm8921 {
	struct device			*dev;
	struct pm_irq_chip		*irq_chip;
	struct mfd_cell                 *mfd_regulators;
	u32				rev_registers;
};

static int pm8921_readb(const struct device *dev, u16 addr, u8 *val)
{
	const struct pm8xxx_drvdata *pm8921_drvdata = dev_get_drvdata(dev);
	const struct pm8921 *pmic = pm8921_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, val, 1);
}

static int pm8921_writeb(const struct device *dev, u16 addr, u8 val)
{
	const struct pm8xxx_drvdata *pm8921_drvdata = dev_get_drvdata(dev);
	const struct pm8921 *pmic = pm8921_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, &val, 1);
}

static int pm8921_read_buf(const struct device *dev, u16 addr, u8 *buf,
									int cnt)
{
	const struct pm8xxx_drvdata *pm8921_drvdata = dev_get_drvdata(dev);
	const struct pm8921 *pmic = pm8921_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, buf, cnt);
}

static int pm8921_write_buf(const struct device *dev, u16 addr, u8 *buf,
									int cnt)
{
	const struct pm8xxx_drvdata *pm8921_drvdata = dev_get_drvdata(dev);
	const struct pm8921 *pmic = pm8921_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, buf, cnt);
}

static int pm8921_read_irq_stat(const struct device *dev, int irq)
{
	const struct pm8xxx_drvdata *pm8921_drvdata = dev_get_drvdata(dev);
	const struct pm8921 *pmic = pm8921_drvdata->pm_chip_data;

	return pm8xxx_get_irq_stat(pmic->irq_chip, irq);
}

static enum pm8xxx_version pm8921_get_version(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8921_drvdata = dev_get_drvdata(dev);
	const struct pm8921 *pmic = pm8921_drvdata->pm_chip_data;
	enum pm8xxx_version version = -ENODEV;

	if ((pmic->rev_registers & PM8921_VERSION_MASK) == PM8921_VERSION_VALUE)
		version = PM8XXX_VERSION_8921;
	else if ((pmic->rev_registers & PM8921_VERSION_MASK)
			== PM8922_VERSION_VALUE)
		version = PM8XXX_VERSION_8922;

	return version;
}

static int pm8921_get_revision(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8921_drvdata = dev_get_drvdata(dev);
	const struct pm8921 *pmic = pm8921_drvdata->pm_chip_data;

	return pmic->rev_registers & PM8921_REVISION_MASK;
}

static struct pm8xxx_drvdata pm8921_drvdata = {
	.pmic_readb		= pm8921_readb,
	.pmic_writeb		= pm8921_writeb,
	.pmic_read_buf		= pm8921_read_buf,
	.pmic_write_buf		= pm8921_write_buf,
	.pmic_read_irq_stat	= pm8921_read_irq_stat,
	.pmic_get_version	= pm8921_get_version,
	.pmic_get_revision	= pm8921_get_revision,
};

static const struct resource gpio_cell_resources[] __devinitconst = {
	[0] = {
		.start = PM8921_IRQ_BLOCK_BIT(PM8921_GPIO_BLOCK_START, 0),
		.end   = PM8921_IRQ_BLOCK_BIT(PM8921_GPIO_BLOCK_START, 0)
			+ PM8921_NR_GPIOS - 1,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell gpio_cell __devinitdata = {
	.name		= PM8XXX_GPIO_DEV_NAME,
	.id		= -1,
	.resources	= gpio_cell_resources,
	.num_resources	= ARRAY_SIZE(gpio_cell_resources),
};

static const struct resource adc_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE(NULL, PM8921_ADC_EOC_USR_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8921_ADC_BATT_TEMP_WARM_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8921_ADC_BATT_TEMP_COLD_IRQ),
};

static struct mfd_cell adc_cell __devinitdata = {
	.name		= PM8XXX_ADC_DEV_NAME,
	.id		= -1,
	.resources	= adc_cell_resources,
	.num_resources	= ARRAY_SIZE(adc_cell_resources),
};

static const struct resource mpp_cell_resources[] __devinitconst = {
	{
		.start	= PM8921_IRQ_BLOCK_BIT(PM8921_MPP_BLOCK_START, 0),
		.end	= PM8921_IRQ_BLOCK_BIT(PM8921_MPP_BLOCK_START, 0)
			  + PM8921_NR_MPPS - 1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct mfd_cell mpp_cell __devinitdata = {
	.name		= PM8XXX_MPP_DEV_NAME,
	.id		= 0,
	.resources	= mpp_cell_resources,
	.num_resources	= ARRAY_SIZE(mpp_cell_resources),
};

static const struct resource rtc_cell_resources[] __devinitconst = {
	[0] = SINGLE_IRQ_RESOURCE(NULL, PM8921_RTC_ALARM_IRQ),
	[1] = {
		.name   = "pmic_rtc_base",
		.start  = PM8921_RTC_BASE,
		.end    = PM8921_RTC_BASE,
		.flags  = IORESOURCE_IO,
	},
};

static struct mfd_cell rtc_cell __devinitdata = {
	.name           = PM8XXX_RTC_DEV_NAME,
	.id             = -1,
	.resources      = rtc_cell_resources,
	.num_resources  = ARRAY_SIZE(rtc_cell_resources),
};

static const struct resource resources_pwrkey[] __devinitconst = {
	SINGLE_IRQ_RESOURCE(NULL, PM8921_PWRKEY_REL_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8921_PWRKEY_PRESS_IRQ),
};

static struct mfd_cell pwrkey_cell __devinitdata = {
	.name		= PM8XXX_PWRKEY_DEV_NAME,
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_pwrkey),
	.resources	= resources_pwrkey,
};

static const struct resource resources_keypad[] = {
	SINGLE_IRQ_RESOURCE(NULL, PM8921_KEYPAD_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8921_KEYSTUCK_IRQ),
};

static struct mfd_cell keypad_cell __devinitdata = {
	.name		= PM8XXX_KEYPAD_DEV_NAME,
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_keypad),
	.resources	= resources_keypad,
};

static struct mfd_cell debugfs_cell __devinitdata = {
	.name		= "pm8xxx-debug",
	.id		= 0,
	.platform_data	= "pm8921-dbg",
	.pdata_size	= sizeof("pm8921-dbg"),
};

static struct mfd_cell pwm_cell __devinitdata = {
	.name           = PM8XXX_PWM_DEV_NAME,
	.id             = -1,
};

static const struct resource charger_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("USBIN_VALID_IRQ", PM8921_USBIN_VALID_IRQ),
	SINGLE_IRQ_RESOURCE("USBIN_OV_IRQ", PM8921_USBIN_OV_IRQ),
	SINGLE_IRQ_RESOURCE("BATT_INSERTED_IRQ", PM8921_BATT_INSERTED_IRQ),
	SINGLE_IRQ_RESOURCE("VBATDET_LOW_IRQ", PM8921_VBATDET_LOW_IRQ),
	SINGLE_IRQ_RESOURCE("USBIN_UV_IRQ", PM8921_USBIN_UV_IRQ),
	SINGLE_IRQ_RESOURCE("VBAT_OV_IRQ", PM8921_VBAT_OV_IRQ),
	SINGLE_IRQ_RESOURCE("CHGWDOG_IRQ", PM8921_CHGWDOG_IRQ),
	SINGLE_IRQ_RESOURCE("VCP_IRQ", PM8921_VCP_IRQ),
	SINGLE_IRQ_RESOURCE("ATCDONE_IRQ", PM8921_ATCDONE_IRQ),
	SINGLE_IRQ_RESOURCE("ATCFAIL_IRQ", PM8921_ATCFAIL_IRQ),
	SINGLE_IRQ_RESOURCE("CHGDONE_IRQ", PM8921_CHGDONE_IRQ),
	SINGLE_IRQ_RESOURCE("CHGFAIL_IRQ", PM8921_CHGFAIL_IRQ),
	SINGLE_IRQ_RESOURCE("CHGSTATE_IRQ", PM8921_CHGSTATE_IRQ),
	SINGLE_IRQ_RESOURCE("LOOP_CHANGE_IRQ", PM8921_LOOP_CHANGE_IRQ),
	SINGLE_IRQ_RESOURCE("FASTCHG_IRQ", PM8921_FASTCHG_IRQ),
	SINGLE_IRQ_RESOURCE("TRKLCHG_IRQ", PM8921_TRKLCHG_IRQ),
	SINGLE_IRQ_RESOURCE("BATT_REMOVED_IRQ", PM8921_BATT_REMOVED_IRQ),
	SINGLE_IRQ_RESOURCE("BATTTEMP_HOT_IRQ", PM8921_BATTTEMP_HOT_IRQ),
	SINGLE_IRQ_RESOURCE("CHGHOT_IRQ", PM8921_CHGHOT_IRQ),
	SINGLE_IRQ_RESOURCE("BATTTEMP_COLD_IRQ", PM8921_BATTTEMP_COLD_IRQ),
	SINGLE_IRQ_RESOURCE("CHG_GONE_IRQ", PM8921_CHG_GONE_IRQ),
	SINGLE_IRQ_RESOURCE("BAT_TEMP_OK_IRQ", PM8921_BAT_TEMP_OK_IRQ),
	SINGLE_IRQ_RESOURCE("COARSE_DET_LOW_IRQ", PM8921_COARSE_DET_LOW_IRQ),
	SINGLE_IRQ_RESOURCE("VDD_LOOP_IRQ", PM8921_VDD_LOOP_IRQ),
	SINGLE_IRQ_RESOURCE("VREG_OV_IRQ", PM8921_VREG_OV_IRQ),
	SINGLE_IRQ_RESOURCE("VBATDET_IRQ", PM8921_VBATDET_IRQ),
	SINGLE_IRQ_RESOURCE("BATFET_IRQ", PM8921_BATFET_IRQ),
	SINGLE_IRQ_RESOURCE("PSI_IRQ", PM8921_PSI_IRQ),
	SINGLE_IRQ_RESOURCE("DCIN_VALID_IRQ", PM8921_DCIN_VALID_IRQ),
	SINGLE_IRQ_RESOURCE("DCIN_OV_IRQ", PM8921_DCIN_OV_IRQ),
	SINGLE_IRQ_RESOURCE("DCIN_UV_IRQ", PM8921_DCIN_UV_IRQ),
};

static const struct resource bms_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("PM8921_BMS_SBI_WRITE_OK", PM8921_BMS_SBI_WRITE_OK),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_CC_THR", PM8921_BMS_CC_THR),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_VSENSE_THR", PM8921_BMS_VSENSE_THR),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_VSENSE_FOR_R", PM8921_BMS_VSENSE_FOR_R),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_OCV_FOR_R", PM8921_BMS_OCV_FOR_R),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_GOOD_OCV", PM8921_BMS_GOOD_OCV),
	SINGLE_IRQ_RESOURCE("PM8921_BMS_VSENSE_AVG", PM8921_BMS_VSENSE_AVG),
};

static struct mfd_cell charger_cell __devinitdata = {
	.name		= PM8921_CHARGER_DEV_NAME,
	.id		= -1,
	.resources	= charger_cell_resources,
	.num_resources	= ARRAY_SIZE(charger_cell_resources),
};

static struct mfd_cell bms_cell __devinitdata = {
	.name		= PM8921_BMS_DEV_NAME,
	.id		= -1,
	.resources	= bms_cell_resources,
	.num_resources	= ARRAY_SIZE(bms_cell_resources),
};

static struct mfd_cell misc_cell __devinitdata = {
	.name           = PM8XXX_MISC_DEV_NAME,
	.id             = -1,
};

static struct mfd_cell leds_cell __devinitdata = {
	.name		= PM8XXX_LEDS_DEV_NAME,
	.id		= -1,
};

static const struct resource thermal_alarm_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("pm8921_tempstat_irq", PM8921_TEMPSTAT_IRQ),
	SINGLE_IRQ_RESOURCE("pm8921_overtemp_irq", PM8921_OVERTEMP_IRQ),
};

static struct pm8xxx_tm_core_data thermal_alarm_cdata = {
	.adc_channel =			CHANNEL_DIE_TEMP,
	.adc_type =			PM8XXX_TM_ADC_PM8XXX_ADC,
	.reg_addr_temp_alarm_ctrl =	REG_TEMP_ALARM_CTRL,
	.reg_addr_temp_alarm_pwm =	REG_TEMP_ALARM_PWM,
	.tm_name =			"pm8921_tz",
	.irq_name_temp_stat =		"pm8921_tempstat_irq",
	.irq_name_over_temp =		"pm8921_overtemp_irq",
};

static struct mfd_cell thermal_alarm_cell __devinitdata = {
	.name		= PM8XXX_TM_DEV_NAME,
	.id		= -1,
	.resources	= thermal_alarm_cell_resources,
	.num_resources	= ARRAY_SIZE(thermal_alarm_cell_resources),
	.platform_data	= &thermal_alarm_cdata,
	.pdata_size	= sizeof(struct pm8xxx_tm_core_data),
};

static const struct resource batt_alarm_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("pm8921_batt_alarm_irq", PM8921_BATT_ALARM_IRQ),
};

static struct pm8xxx_batt_alarm_core_data batt_alarm_cdata = {
	.irq_name		= "pm8921_batt_alarm_irq",
	.reg_addr_threshold	= REG_BATT_ALARM_THRESH,
	.reg_addr_ctrl1		= REG_BATT_ALARM_CTRL1,
	.reg_addr_ctrl2		= REG_BATT_ALARM_CTRL2,
	.reg_addr_pwm_ctrl	= REG_BATT_ALARM_PWM_CTRL,
};

static struct mfd_cell batt_alarm_cell __devinitdata = {
	.name		= PM8XXX_BATT_ALARM_DEV_NAME,
	.id		= -1,
	.resources	= batt_alarm_cell_resources,
	.num_resources	= ARRAY_SIZE(batt_alarm_cell_resources),
	.platform_data	= &batt_alarm_cdata,
	.pdata_size	= sizeof(struct pm8xxx_batt_alarm_core_data),
};

static const struct resource ccadc_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("PM8921_BMS_CCADC_EOC", PM8921_BMS_CCADC_EOC),
};

static struct mfd_cell ccadc_cell __devinitdata = {
	.name		= PM8XXX_CCADC_DEV_NAME,
	.id		= -1,
	.resources	= ccadc_cell_resources,
	.num_resources	= ARRAY_SIZE(ccadc_cell_resources),
};

static struct mfd_cell vibrator_cell __devinitdata = {
	.name           = PM8XXX_VIBRATOR_DEV_NAME,
	.id             = -1,
};

static int __devinit
pm8921_add_subdevices(const struct pm8921_platform_data *pdata,
		      struct pm8921 *pmic)
{
	int ret = 0, irq_base = 0;
	struct pm_irq_chip *irq_chip;
	static struct mfd_cell *mfd_regulators;
	enum pm8xxx_version version;
	int i;

	version = pm8xxx_get_version(pmic->dev);

	if (pdata->irq_pdata) {
		pdata->irq_pdata->irq_cdata.nirqs = PM8921_NR_IRQS;
		pdata->irq_pdata->irq_cdata.base_addr = REG_IRQ_BASE;
		irq_base = pdata->irq_pdata->irq_base;
		irq_chip = pm8xxx_irq_init(pmic->dev, pdata->irq_pdata);

		if (IS_ERR(irq_chip)) {
			pr_err("Failed to init interrupts ret=%ld\n",
					PTR_ERR(irq_chip));
			return PTR_ERR(irq_chip);
		}
		pmic->irq_chip = irq_chip;
	}

	if (pdata->gpio_pdata) {
		pdata->gpio_pdata->gpio_cdata.ngpios = PM8921_NR_GPIOS;
		gpio_cell.platform_data = pdata->gpio_pdata;
		gpio_cell.pdata_size = sizeof(struct pm8xxx_gpio_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &gpio_cell, 1,
					NULL, irq_base);
		if (ret) {
			pr_err("Failed to add  gpio subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->mpp_pdata) {
		pdata->mpp_pdata->core_data.nmpps = PM8921_NR_MPPS;
		pdata->mpp_pdata->core_data.base_addr = REG_MPP_BASE;
		mpp_cell.platform_data = pdata->mpp_pdata;
		mpp_cell.pdata_size = sizeof(struct pm8xxx_mpp_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &mpp_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add mpp subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->rtc_pdata) {
		rtc_cell.platform_data = pdata->rtc_pdata;
		rtc_cell.pdata_size = sizeof(struct pm8xxx_rtc_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &rtc_cell, 1, NULL,
				irq_base);
		if (ret) {
			pr_err("Failed to add rtc subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->pwrkey_pdata) {
		pwrkey_cell.platform_data = pdata->pwrkey_pdata;
		pwrkey_cell.pdata_size =
			sizeof(struct pm8xxx_pwrkey_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &pwrkey_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add pwrkey subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->keypad_pdata) {
		keypad_cell.platform_data = pdata->keypad_pdata;
		keypad_cell.pdata_size =
			sizeof(struct pm8xxx_keypad_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &keypad_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add keypad subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->charger_pdata) {
		pdata->charger_pdata->charger_cdata.vbat_channel = CHANNEL_VBAT;
		pdata->charger_pdata->charger_cdata.batt_temp_channel
						= CHANNEL_BATT_THERM;
		pdata->charger_pdata->charger_cdata.batt_id_channel
						= CHANNEL_BATT_ID;
		charger_cell.platform_data = pdata->charger_pdata;
		charger_cell.pdata_size =
				sizeof(struct pm8921_charger_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &charger_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add charger subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->adc_pdata) {
		adc_cell.platform_data = pdata->adc_pdata;
		adc_cell.pdata_size =
			sizeof(struct pm8xxx_adc_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &adc_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add regulator subdevices ret=%d\n",
					ret);
		}
	}

	if (pdata->bms_pdata) {
		pdata->bms_pdata->bms_cdata.batt_temp_channel
						= CHANNEL_BATT_THERM;
		pdata->bms_pdata->bms_cdata.vbat_channel = CHANNEL_VBAT;
		pdata->bms_pdata->bms_cdata.ref625mv_channel = CHANNEL_625MV;
		pdata->bms_pdata->bms_cdata.ref1p25v_channel = CHANNEL_125V;
		pdata->bms_pdata->bms_cdata.batt_id_channel = CHANNEL_BATT_ID;
		bms_cell.platform_data = pdata->bms_pdata;
		bms_cell.pdata_size = sizeof(struct pm8921_bms_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &bms_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add bms subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	/* Add one device for each regulator used by the board. */
	if (pdata->num_regulators > 0 && pdata->regulator_pdatas) {
		mfd_regulators = kzalloc(sizeof(struct mfd_cell)
					 * (pdata->num_regulators), GFP_KERNEL);
		if (!mfd_regulators) {
			pr_err("Cannot allocate %d bytes for pm8921 regulator "
				"mfd cells\n", sizeof(struct mfd_cell)
						* (pdata->num_regulators));
			ret = -ENOMEM;
			goto bail;
		}
		for (i = 0; i < pdata->num_regulators; i++) {
			mfd_regulators[i].name = PM8921_REGULATOR_DEV_NAME;
			mfd_regulators[i].id = pdata->regulator_pdatas[i].id;
			mfd_regulators[i].platform_data =
				&(pdata->regulator_pdatas[i]);
			mfd_regulators[i].pdata_size =
				sizeof(struct pm8921_regulator_platform_data);
		}
		ret = mfd_add_devices(pmic->dev, 0, mfd_regulators,
				pdata->num_regulators, NULL, irq_base);
		if (ret) {
			pr_err("Failed to add regulator subdevices ret=%d\n",
				ret);
			kfree(mfd_regulators);
			goto bail;
		}
		pmic->mfd_regulators = mfd_regulators;
	}

	ret = mfd_add_devices(pmic->dev, 0, &debugfs_cell, 1, NULL, irq_base);
	if (ret) {
		pr_err("Failed to add debugfs subdevice ret=%d\n", ret);
		goto bail;
	}

	ret = mfd_add_devices(pmic->dev, 0, &pwm_cell, 1, NULL, 0);
	if (ret) {
		pr_err("Failed to add pwm subdevice ret=%d\n", ret);
		goto bail;
	}

	if (pdata->misc_pdata) {
		misc_cell.platform_data = pdata->misc_pdata;
		misc_cell.pdata_size = sizeof(struct pm8xxx_misc_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &misc_cell, 1, NULL,
				      irq_base);
		if (ret) {
			pr_err("Failed to add  misc subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	if (pdata->leds_pdata) {
		leds_cell.platform_data = pdata->leds_pdata;
		leds_cell.pdata_size = sizeof(struct pm8xxx_led_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &leds_cell, 1, NULL, 0);
		if (ret) {
			pr_err("Failed to add leds subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	ret = mfd_add_devices(pmic->dev, 0, &thermal_alarm_cell, 1, NULL,
				irq_base);
	if (ret) {
		pr_err("Failed to add thermal alarm subdevice ret=%d\n",
			ret);
		goto bail;
	}

	ret = mfd_add_devices(pmic->dev, 0, &batt_alarm_cell, 1, NULL,
				irq_base);
	if (ret) {
		pr_err("Failed to add battery alarm subdevice ret=%d\n",
			ret);
		goto bail;
	}

	if (pdata->vibrator_pdata) {
		vibrator_cell.platform_data = pdata->vibrator_pdata;
		vibrator_cell.pdata_size =
				sizeof(struct pm8xxx_vibrator_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &vibrator_cell, 1, NULL, 0);
		if (ret) {
			pr_err("Failed to add vibrator subdevice ret=%d\n",
									ret);
			goto bail;
		}
	}

	if (pdata->ccadc_pdata) {
		ccadc_cell.platform_data = pdata->ccadc_pdata;
		ccadc_cell.pdata_size =
				sizeof(struct pm8xxx_ccadc_platform_data);

		ret = mfd_add_devices(pmic->dev, 0, &ccadc_cell, 1, NULL,
					irq_base);
		if (ret) {
			pr_err("Failed to add ccadc subdevice ret=%d\n", ret);
			goto bail;
		}
	}

	return 0;
bail:
	if (pmic->irq_chip) {
		pm8xxx_irq_exit(pmic->irq_chip);
		pmic->irq_chip = NULL;
	}
	return ret;
}

static const char * const pm8921_restart_reason[] = {
	[0] = "Unknown",
	[1] = "Triggered from CBL (external charger)",
	[2] = "Triggered from KPD (power key press)",
	[3] = "Triggered from CHG (usb charger insertion)",
	[4] = "Triggered from SMPL (sudden momentary power loss)",
	[5] = "Triggered from RTC (real time clock)",
	[6] = "Triggered by Hard Reset",
	[7] = "Triggered by General Purpose Trigger",
};

static const char * const pm8921_rev_names[] = {
	[PM8XXX_REVISION_8921_TEST]	= "test",
	[PM8XXX_REVISION_8921_1p0]	= "1.0",
	[PM8XXX_REVISION_8921_1p1]	= "1.1",
	[PM8XXX_REVISION_8921_2p0]	= "2.0",
	[PM8XXX_REVISION_8921_3p0]	= "3.0",
};

static const char * const pm8922_rev_names[] = {
	[PM8XXX_REVISION_8922_TEST]	= "test",
	[PM8XXX_REVISION_8922_1p0]	= "1.0",
	[PM8XXX_REVISION_8922_1p1]	= "1.1",
	[PM8XXX_REVISION_8922_2p0]	= "2.0",
};

static int __devinit pm8921_probe(struct platform_device *pdev)
{
	const struct pm8921_platform_data *pdata = pdev->dev.platform_data;
	const char *revision_name = "unknown";
	struct pm8921 *pmic;
	enum pm8xxx_version version;
	int revision;
	int rc;
	u8 val;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	pmic = kzalloc(sizeof(struct pm8921), GFP_KERNEL);
	if (!pmic) {
		pr_err("Cannot alloc pm8921 struct\n");
		return -ENOMEM;
	}

	/* Read PMIC chip revision */
	rc = msm_ssbi_read(pdev->dev.parent, REG_HWREV, &val, sizeof(val));
	if (rc) {
		pr_err("Failed to read hw rev reg %d:rc=%d\n", REG_HWREV, rc);
		goto err_read_rev;
	}
	pr_info("PMIC revision 1: %02X\n", val);
	pmic->rev_registers = val;

	/* Read PMIC chip revision 2 */
	rc = msm_ssbi_read(pdev->dev.parent, REG_HWREV_2, &val, sizeof(val));
	if (rc) {
		pr_err("Failed to read hw rev 2 reg %d:rc=%d\n",
			REG_HWREV_2, rc);
		goto err_read_rev;
	}
	pr_info("PMIC revision 2: %02X\n", val);
	pmic->rev_registers |= val << BITS_PER_BYTE;

	pmic->dev = &pdev->dev;
	pm8921_drvdata.pm_chip_data = pmic;
	platform_set_drvdata(pdev, &pm8921_drvdata);

	/* Print out human readable version and revision names. */
	version = pm8xxx_get_version(pmic->dev);
	revision = pm8xxx_get_revision(pmic->dev);
	if (version == PM8XXX_VERSION_8921) {
		if (revision >= 0 && revision < ARRAY_SIZE(pm8921_rev_names))
			revision_name = pm8921_rev_names[revision];
		pr_info("PMIC version: PM8921 rev %s\n", revision_name);
	} else if (version == PM8XXX_VERSION_8922) {
		if (revision >= 0 && revision < ARRAY_SIZE(pm8922_rev_names))
			revision_name = pm8922_rev_names[revision];
		pr_info("PMIC version: PM8922 rev %s\n", revision_name);
	} else {
		WARN_ON(version != PM8XXX_VERSION_8921
			&& version != PM8XXX_VERSION_8922);
	}

	/* Log human readable restart reason */
	rc = msm_ssbi_read(pdev->dev.parent, REG_PM8921_PON_CNTRL_3, &val, 1);
	if (rc) {
		pr_err("Cannot read restart reason rc=%d\n", rc);
		goto err_read_rev;
	}
	val &= PM8921_RESTART_REASON_MASK;
	pr_info("PMIC Restart Reason: %s\n", pm8921_restart_reason[val]);

	rc = pm8921_add_subdevices(pdata, pmic);
	if (rc) {
		pr_err("Cannot add subdevices rc=%d\n", rc);
		goto err;
	}

	/* gpio might not work if no irq device is found */
	WARN_ON(pmic->irq_chip == NULL);

	return 0;

err:
	mfd_remove_devices(pmic->dev);
	platform_set_drvdata(pdev, NULL);
err_read_rev:
	kfree(pmic);
	return rc;
}

static int __devexit pm8921_remove(struct platform_device *pdev)
{
	struct pm8xxx_drvdata *drvdata;
	struct pm8921 *pmic = NULL;

	drvdata = platform_get_drvdata(pdev);
	if (drvdata)
		pmic = drvdata->pm_chip_data;
	if (pmic) {
		if (pmic->dev)
			mfd_remove_devices(pmic->dev);
		if (pmic->irq_chip)
			pm8xxx_irq_exit(pmic->irq_chip);
		kfree(pmic->mfd_regulators);
		kfree(pmic);
	}
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver pm8921_driver = {
	.probe		= pm8921_probe,
	.remove		= __devexit_p(pm8921_remove),
	.driver		= {
		.name	= "pm8921-core",
		.owner	= THIS_MODULE,
	},
};

static int __init pm8921_init(void)
{
	return platform_driver_register(&pm8921_driver);
}
postcore_initcall(pm8921_init);

static void __exit pm8921_exit(void)
{
	platform_driver_unregister(&pm8921_driver);
}
module_exit(pm8921_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC 8921 core driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8921-core");
