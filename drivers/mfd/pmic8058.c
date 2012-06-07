/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
/*
 * Qualcomm PMIC8058 driver
 *
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/msm_ssbi.h>
#include <linux/mfd/core.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/msm_adc.h>
#include <linux/module.h>

#define REG_MPP_BASE			0x50
#define REG_IRQ_BASE			0x1BB

/* PMIC8058 Revision */
#define PM8058_REG_REV			0x002  /* PMIC4 revision */
#define PM8058_VERSION_MASK		0xF0
#define PM8058_REVISION_MASK		0x0F
#define PM8058_VERSION_VALUE		0xE0

/* PMIC 8058 Battery Alarm SSBI registers */
#define REG_BATT_ALARM_THRESH		0x023
#define REG_BATT_ALARM_CTRL1		0x024
#define REG_BATT_ALARM_CTRL2		0x0AA
#define REG_BATT_ALARM_PWM_CTRL		0x0A3

#define REG_TEMP_ALRM_CTRL		0x1B
#define REG_TEMP_ALRM_PWM		0x9B

/* PON CNTL 4 register */
#define SSBI_REG_ADDR_PON_CNTL_4 0x98
#define PM8058_PON_RESET_EN_MASK 0x01

/* PON CNTL 5 register */
#define SSBI_REG_ADDR_PON_CNTL_5 0x7B
#define PM8058_HARD_RESET_EN_MASK 0x08

/* GP_TEST1 register */
#define SSBI_REG_ADDR_GP_TEST_1		0x07A

#define PM8058_RTC_BASE			0x1E8
#define PM8058_OTHC_CNTR_BASE0		0xA0
#define PM8058_OTHC_CNTR_BASE1		0x134
#define PM8058_OTHC_CNTR_BASE2		0x137

#define SINGLE_IRQ_RESOURCE(_name, _irq) \
{ \
	.name	= _name, \
	.start	= _irq, \
	.end	= _irq, \
	.flags	= IORESOURCE_IRQ, \
}

struct pm8058_chip {
	struct pm8058_platform_data	pdata;
	struct device		*dev;
	struct pm_irq_chip	*irq_chip;
	struct mfd_cell         *mfd_regulators, *mfd_xo_buffers;

	u8		revision;
};

static int pm8058_readb(const struct device *dev, u16 addr, u8 *val)
{
	const struct pm8xxx_drvdata *pm8058_drvdata = dev_get_drvdata(dev);
	const struct pm8058_chip *pmic = pm8058_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, val, 1);
}

static int pm8058_writeb(const struct device *dev, u16 addr, u8 val)
{
	const struct pm8xxx_drvdata *pm8058_drvdata = dev_get_drvdata(dev);
	const struct pm8058_chip *pmic = pm8058_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, &val, 1);
}

static int pm8058_read_buf(const struct device *dev, u16 addr, u8 *buf,
								int cnt)
{
	const struct pm8xxx_drvdata *pm8058_drvdata = dev_get_drvdata(dev);
	const struct pm8058_chip *pmic = pm8058_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, buf, cnt);
}

static int pm8058_write_buf(const struct device *dev, u16 addr, u8 *buf,
								int cnt)
{
	const struct pm8xxx_drvdata *pm8058_drvdata = dev_get_drvdata(dev);
	const struct pm8058_chip *pmic = pm8058_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, buf, cnt);
}

static int pm8058_read_irq_stat(const struct device *dev, int irq)
{
	const struct pm8xxx_drvdata *pm8058_drvdata = dev_get_drvdata(dev);
	const struct pm8058_chip *pmic = pm8058_drvdata->pm_chip_data;

	return pm8xxx_get_irq_stat(pmic->irq_chip, irq);

	return 0;
}

static enum pm8xxx_version pm8058_get_version(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8058_drvdata = dev_get_drvdata(dev);
	const struct pm8058_chip *pmic = pm8058_drvdata->pm_chip_data;
	enum pm8xxx_version version = -ENODEV;

	if ((pmic->revision & PM8058_VERSION_MASK) == PM8058_VERSION_VALUE)
		version = PM8XXX_VERSION_8058;

	return version;
}

static int pm8058_get_revision(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8058_drvdata = dev_get_drvdata(dev);
	const struct pm8058_chip *pmic = pm8058_drvdata->pm_chip_data;

	return pmic->revision & PM8058_REVISION_MASK;
}

static struct pm8xxx_drvdata pm8058_drvdata = {
	.pmic_readb		= pm8058_readb,
	.pmic_writeb		= pm8058_writeb,
	.pmic_read_buf		= pm8058_read_buf,
	.pmic_write_buf		= pm8058_write_buf,
	.pmic_read_irq_stat	= pm8058_read_irq_stat,
	.pmic_get_version	= pm8058_get_version,
	.pmic_get_revision	= pm8058_get_revision,
};

static const struct resource pm8058_charger_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("CHGVAL",		PM8058_CHGVAL_IRQ),
	SINGLE_IRQ_RESOURCE("CHGINVAL",		PM8058_CHGINVAL_IRQ),
	SINGLE_IRQ_RESOURCE("CHGILIM",		PM8058_CHGILIM_IRQ),
	SINGLE_IRQ_RESOURCE("VCP",		PM8058_VCP_IRQ),
	SINGLE_IRQ_RESOURCE("ATC_DONE",		PM8058_ATC_DONE_IRQ),
	SINGLE_IRQ_RESOURCE("ATCFAIL",		PM8058_ATCFAIL_IRQ),
	SINGLE_IRQ_RESOURCE("AUTO_CHGDONE",	PM8058_AUTO_CHGDONE_IRQ),
	SINGLE_IRQ_RESOURCE("AUTO_CHGFAIL",	PM8058_AUTO_CHGFAIL_IRQ),
	SINGLE_IRQ_RESOURCE("CHGSTATE",		PM8058_CHGSTATE_IRQ),
	SINGLE_IRQ_RESOURCE("FASTCHG",		PM8058_FASTCHG_IRQ),
	SINGLE_IRQ_RESOURCE("CHG_END",		PM8058_CHG_END_IRQ),
	SINGLE_IRQ_RESOURCE("BATTTEMP",		PM8058_BATTTEMP_IRQ),
	SINGLE_IRQ_RESOURCE("CHGHOT",		PM8058_CHGHOT_IRQ),
	SINGLE_IRQ_RESOURCE("CHGTLIMIT",	PM8058_CHGTLIMIT_IRQ),
	SINGLE_IRQ_RESOURCE("CHG_GONE",		PM8058_CHG_GONE_IRQ),
	SINGLE_IRQ_RESOURCE("VCPMAJOR",		PM8058_VCPMAJOR_IRQ),
	SINGLE_IRQ_RESOURCE("VBATDET",		PM8058_VBATDET_IRQ),
	SINGLE_IRQ_RESOURCE("BATFET",		PM8058_BATFET_IRQ),
	SINGLE_IRQ_RESOURCE("BATT_REPLACE",	PM8058_BATT_REPLACE_IRQ),
	SINGLE_IRQ_RESOURCE("BATTCONNECT",	PM8058_BATTCONNECT_IRQ),
	SINGLE_IRQ_RESOURCE("VBATDET_LOW",	PM8058_VBATDET_LOW_IRQ),
};

static struct mfd_cell pm8058_charger_cell __devinitdata = {
	.name		= "pm8058-charger",
	.id		= -1,
	.resources	= pm8058_charger_resources,
	.num_resources	= ARRAY_SIZE(pm8058_charger_resources),
};

static const struct resource misc_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("pm8xxx_osc_halt_irq", PM8058_OSCHALT_IRQ),
};

static struct mfd_cell misc_cell __devinitdata = {
	.name		= PM8XXX_MISC_DEV_NAME,
	.id		= -1,
	.resources	= misc_cell_resources,
	.num_resources	= ARRAY_SIZE(misc_cell_resources),
};

static struct mfd_cell pm8058_pwm_cell __devinitdata = {
	.name		= "pm8058-pwm",
	.id		= -1,
};

static struct resource xoadc_resources[] = {
	SINGLE_IRQ_RESOURCE(NULL, PM8058_ADC_IRQ),
};

static struct mfd_cell xoadc_cell __devinitdata = {
	.name		= "pm8058-xoadc",
	.id		= -1,
	.resources	= xoadc_resources,
	.num_resources	= ARRAY_SIZE(xoadc_resources),
};

static const struct resource thermal_alarm_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("pm8058_tempstat_irq", PM8058_TEMPSTAT_IRQ),
	SINGLE_IRQ_RESOURCE("pm8058_overtemp_irq", PM8058_OVERTEMP_IRQ),
};

static struct pm8xxx_tm_core_data thermal_alarm_cdata = {
	.adc_channel			= CHANNEL_ADC_DIE_TEMP,
	.adc_type			= PM8XXX_TM_ADC_PM8058_ADC,
	.reg_addr_temp_alarm_ctrl	= REG_TEMP_ALRM_CTRL,
	.reg_addr_temp_alarm_pwm	= REG_TEMP_ALRM_PWM,
	.tm_name			= "pm8058_tz",
	.irq_name_temp_stat		= "pm8058_tempstat_irq",
	.irq_name_over_temp		= "pm8058_overtemp_irq",
};

static struct mfd_cell thermal_alarm_cell __devinitdata = {
	.name		= PM8XXX_TM_DEV_NAME,
	.id		= -1,
	.resources	= thermal_alarm_cell_resources,
	.num_resources	= ARRAY_SIZE(thermal_alarm_cell_resources),
	.platform_data	= &thermal_alarm_cdata,
	.pdata_size	= sizeof(struct pm8xxx_tm_core_data),
};

static struct mfd_cell debugfs_cell __devinitdata = {
	.name		= "pm8xxx-debug",
	.id		= -1,
	.platform_data	= "pm8058-dbg",
	.pdata_size	= sizeof("pm8058-dbg"),
};

static const struct resource othc0_cell_resources[] __devinitconst = {
	{
		.name	= "othc_base",
		.start	= PM8058_OTHC_CNTR_BASE0,
		.end	= PM8058_OTHC_CNTR_BASE0,
		.flags	= IORESOURCE_IO,
	},
};

static const struct resource othc1_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE(NULL, PM8058_SW_1_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8058_IR_1_IRQ),
	{
		.name	= "othc_base",
		.start	= PM8058_OTHC_CNTR_BASE1,
		.end	= PM8058_OTHC_CNTR_BASE1,
		.flags	= IORESOURCE_IO,
	},
};

static const struct resource othc2_cell_resources[] __devinitconst = {
	{
		.name	= "othc_base",
		.start	= PM8058_OTHC_CNTR_BASE2,
		.end	= PM8058_OTHC_CNTR_BASE2,
		.flags	= IORESOURCE_IO,
	},
};

static const struct resource batt_alarm_cell_resources[] __devinitconst = {
	SINGLE_IRQ_RESOURCE("pm8058_batt_alarm_irq", PM8058_BATT_ALARM_IRQ),
};

static struct mfd_cell leds_cell __devinitdata = {
	.name		= "pm8058-led",
	.id		= -1,
};

static struct mfd_cell othc0_cell __devinitdata = {
	.name		= "pm8058-othc",
	.id		= 0,
	.resources	= othc0_cell_resources,
	.num_resources  = ARRAY_SIZE(othc0_cell_resources),
};

static struct mfd_cell othc1_cell __devinitdata = {
	.name		= "pm8058-othc",
	.id		= 1,
	.resources	= othc1_cell_resources,
	.num_resources  = ARRAY_SIZE(othc1_cell_resources),
};

static struct mfd_cell othc2_cell __devinitdata = {
	.name		= "pm8058-othc",
	.id		= 2,
	.resources	= othc2_cell_resources,
	.num_resources  = ARRAY_SIZE(othc2_cell_resources),
};

static struct pm8xxx_batt_alarm_core_data batt_alarm_cdata = {
	.irq_name		= "pm8058_batt_alarm_irq",
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

static struct mfd_cell upl_cell __devinitdata = {
	.name		= PM8XXX_UPL_DEV_NAME,
	.id		= -1,
};

static struct mfd_cell nfc_cell __devinitdata = {
	.name		= PM8XXX_NFC_DEV_NAME,
	.id		= -1,
};

static const struct resource rtc_cell_resources[] __devinitconst = {
	[0] = SINGLE_IRQ_RESOURCE(NULL, PM8058_RTC_ALARM_IRQ),
	[1] = {
		.name   = "pmic_rtc_base",
		.start  = PM8058_RTC_BASE,
		.end    = PM8058_RTC_BASE,
		.flags  = IORESOURCE_IO,
	},
};

static struct mfd_cell rtc_cell __devinitdata = {
	.name		= PM8XXX_RTC_DEV_NAME,
	.id		= -1,
	.resources	= rtc_cell_resources,
	.num_resources  = ARRAY_SIZE(rtc_cell_resources),
};

static const struct resource resources_pwrkey[] __devinitconst = {
	SINGLE_IRQ_RESOURCE(NULL, PM8058_PWRKEY_REL_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8058_PWRKEY_PRESS_IRQ),
};

static struct mfd_cell vibrator_cell __devinitdata = {
	.name		= PM8XXX_VIBRATOR_DEV_NAME,
	.id		= -1,
};

static struct mfd_cell pwrkey_cell __devinitdata = {
	.name		= PM8XXX_PWRKEY_DEV_NAME,
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_pwrkey),
	.resources	= resources_pwrkey,
};

static const struct resource resources_keypad[] = {
	SINGLE_IRQ_RESOURCE(NULL, PM8058_KEYPAD_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8058_KEYSTUCK_IRQ),
};

static struct mfd_cell keypad_cell __devinitdata = {
	.name		= PM8XXX_KEYPAD_DEV_NAME,
	.id		= -1,
	.num_resources  = ARRAY_SIZE(resources_keypad),
	.resources	= resources_keypad,
};

static const struct resource mpp_cell_resources[] __devinitconst = {
	{
		.start	= PM8058_IRQ_BLOCK_BIT(PM8058_MPP_BLOCK_START, 0),
		.end	= PM8058_IRQ_BLOCK_BIT(PM8058_MPP_BLOCK_START, 0)
			  + PM8058_MPPS - 1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct mfd_cell mpp_cell __devinitdata = {
	.name		= PM8XXX_MPP_DEV_NAME,
	.id		= 0,
	.resources	= mpp_cell_resources,
	.num_resources	= ARRAY_SIZE(mpp_cell_resources),
};

static const struct resource gpio_cell_resources[] __devinitconst = {
	[0] = {
		.start = PM8058_IRQ_BLOCK_BIT(PM8058_GPIO_BLOCK_START, 0),
		.end   = PM8058_IRQ_BLOCK_BIT(PM8058_GPIO_BLOCK_START, 0)
			+ PM8058_GPIOS - 1,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell gpio_cell __devinitdata = {
	.name		= PM8XXX_GPIO_DEV_NAME,
	.id		= -1,
	.resources	= gpio_cell_resources,
	.num_resources	= ARRAY_SIZE(gpio_cell_resources),
};

static int __devinit
pm8058_add_subdevices(const struct pm8058_platform_data *pdata,
				struct pm8058_chip *pmic)
{
	int rc = 0, irq_base = 0, i;
	struct pm_irq_chip *irq_chip;
	static struct mfd_cell *mfd_regulators, *mfd_xo_buffers;

	if (pdata->irq_pdata) {
		pdata->irq_pdata->irq_cdata.nirqs = PM8058_NR_IRQS;
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
		pdata->gpio_pdata->gpio_cdata.ngpios = PM8058_GPIOS;
		gpio_cell.platform_data = pdata->gpio_pdata;
		gpio_cell.pdata_size = sizeof(struct pm8xxx_gpio_platform_data);
		rc = mfd_add_devices(pmic->dev, 0, &gpio_cell, 1,
					NULL, irq_base);
		if (rc) {
			pr_err("Failed to add  gpio subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->mpp_pdata) {
		pdata->mpp_pdata->core_data.nmpps = PM8058_MPPS;
		pdata->mpp_pdata->core_data.base_addr = REG_MPP_BASE;
		mpp_cell.platform_data = pdata->mpp_pdata;
		mpp_cell.pdata_size = sizeof(struct pm8xxx_mpp_platform_data);
		rc = mfd_add_devices(pmic->dev, 0, &mpp_cell, 1, NULL,
					irq_base);
		if (rc) {
			pr_err("Failed to add mpp subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->num_regulators > 0 && pdata->regulator_pdatas) {
		mfd_regulators = kzalloc(sizeof(struct mfd_cell)
					 * (pdata->num_regulators), GFP_KERNEL);
		if (!mfd_regulators) {
			pr_err("Cannot allocate %d bytes for pm8058 regulator "
				"mfd cells\n", sizeof(struct mfd_cell)
						* (pdata->num_regulators));
			rc = -ENOMEM;
			goto bail;
		}
		for (i = 0; i < pdata->num_regulators; i++) {
			mfd_regulators[i].name = "pm8058-regulator";
			mfd_regulators[i].id = pdata->regulator_pdatas[i].id;
			mfd_regulators[i].platform_data =
				&(pdata->regulator_pdatas[i]);
			mfd_regulators[i].pdata_size =
					sizeof(struct pm8058_vreg_pdata);
		}
		rc = mfd_add_devices(pmic->dev, 0, mfd_regulators,
				pdata->num_regulators, NULL, irq_base);
		if (rc) {
			pr_err("Failed to add regulator subdevices ret=%d\n",
				rc);
			kfree(mfd_regulators);
			goto bail;
		}
		pmic->mfd_regulators = mfd_regulators;
	}

	if (pdata->num_xo_buffers > 0 && pdata->xo_buffer_pdata) {
		mfd_xo_buffers = kzalloc(sizeof(struct mfd_cell)
					 * (pdata->num_xo_buffers), GFP_KERNEL);
		if (!mfd_xo_buffers) {
			pr_err("Cannot allocate %d bytes for pm8058 XO buffer "
				"mfd cells\n", sizeof(struct mfd_cell)
						* (pdata->num_xo_buffers));
			rc = -ENOMEM;
			goto bail;
		}
		for (i = 0; i < pdata->num_xo_buffers; i++) {
			mfd_xo_buffers[i].name = PM8058_XO_BUFFER_DEV_NAME;
			mfd_xo_buffers[i].id = pdata->xo_buffer_pdata[i].id;
			mfd_xo_buffers[i].platform_data =
				&(pdata->xo_buffer_pdata[i]);
			mfd_xo_buffers[i].pdata_size =
					sizeof(struct pm8058_xo_pdata);
		}
		rc = mfd_add_devices(pmic->dev, 0, mfd_xo_buffers,
				pdata->num_xo_buffers, NULL, irq_base);
		if (rc) {
			pr_err("Failed to add XO buffer subdevices ret=%d\n",
				rc);
			kfree(mfd_xo_buffers);
			goto bail;
		}
		pmic->mfd_xo_buffers = mfd_xo_buffers;
	}

	if (pdata->keypad_pdata) {
		keypad_cell.platform_data = pdata->keypad_pdata;
		keypad_cell.pdata_size =
			sizeof(struct pm8xxx_keypad_platform_data);
		rc = mfd_add_devices(pmic->dev, 0, &keypad_cell, 1, NULL,
					irq_base);
		if (rc) {
			pr_err("Failed to add keypad subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->rtc_pdata) {
		rtc_cell.platform_data = pdata->rtc_pdata;
		rtc_cell.pdata_size = sizeof(struct pm8xxx_rtc_platform_data);
		rc = mfd_add_devices(pmic->dev, 0, &rtc_cell, 1, NULL,
						irq_base);
		if (rc) {
			pr_err("Failed to add rtc subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->pwrkey_pdata) {
		pwrkey_cell.platform_data = pdata->pwrkey_pdata;
		pwrkey_cell.pdata_size =
			sizeof(struct pm8xxx_pwrkey_platform_data);
		rc = mfd_add_devices(pmic->dev, 0, &pwrkey_cell, 1, NULL,
							irq_base);
		if (rc) {
			pr_err("Failed to add pwrkey subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->vibrator_pdata) {
		vibrator_cell.platform_data = pdata->vibrator_pdata;
		vibrator_cell.pdata_size =
				sizeof(struct pm8xxx_vibrator_platform_data);
		rc = mfd_add_devices(pmic->dev, 0, &vibrator_cell, 1, NULL,
								irq_base);
		if (rc) {
			pr_err("Failed to add vibrator subdevice ret=%d\n",
									rc);
			goto bail;
		}
	}

	if (pdata->leds_pdata) {
		leds_cell.platform_data = pdata->leds_pdata;
		leds_cell.pdata_size =
			sizeof(struct pmic8058_leds_platform_data);
		rc = mfd_add_devices(pmic->dev, 0, &leds_cell, 1, NULL,
								irq_base);
		if (rc) {
			pr_err("Failed to add leds subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->xoadc_pdata) {
		xoadc_cell.platform_data = pdata->xoadc_pdata;
		xoadc_cell.pdata_size =
			sizeof(struct xoadc_platform_data);
		rc = mfd_add_devices(pmic->dev, 0, &xoadc_cell, 1, NULL,
								irq_base);
		if (rc) {
			pr_err("Failed to add leds subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->othc0_pdata) {
		othc0_cell.platform_data = pdata->othc0_pdata;
		othc0_cell.pdata_size =
			sizeof(struct pmic8058_othc_config_pdata);
		rc = mfd_add_devices(pmic->dev, 0, &othc0_cell, 1, NULL, 0);
		if (rc) {
			pr_err("Failed to add othc0 subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->othc1_pdata) {
		othc1_cell.platform_data = pdata->othc1_pdata;
		othc1_cell.pdata_size =
			sizeof(struct pmic8058_othc_config_pdata);
		rc = mfd_add_devices(pmic->dev, 0, &othc1_cell, 1, NULL,
								irq_base);
		if (rc) {
			pr_err("Failed to add othc1 subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->othc2_pdata) {
		othc2_cell.platform_data = pdata->othc2_pdata;
		othc2_cell.pdata_size =
			sizeof(struct pmic8058_othc_config_pdata);
		rc = mfd_add_devices(pmic->dev, 0, &othc2_cell, 1, NULL, 0);
		if (rc) {
			pr_err("Failed to add othc2 subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->pwm_pdata) {
		pm8058_pwm_cell.platform_data = pdata->pwm_pdata;
		pm8058_pwm_cell.pdata_size = sizeof(struct pm8058_pwm_pdata);
		rc = mfd_add_devices(pmic->dev, 0, &pm8058_pwm_cell, 1, NULL,
								irq_base);
		if (rc) {
			pr_err("Failed to add pwm subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	if (pdata->misc_pdata) {
		misc_cell.platform_data = pdata->misc_pdata;
		misc_cell.pdata_size = sizeof(struct pm8xxx_misc_platform_data);
		rc = mfd_add_devices(pmic->dev, 0, &misc_cell, 1, NULL,
				      irq_base);
		if (rc) {
			pr_err("Failed to add  misc subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	rc = mfd_add_devices(pmic->dev, 0, &thermal_alarm_cell, 1, NULL,
				irq_base);
	if (rc) {
		pr_err("Failed to add thermal alarm subdevice ret=%d\n",
			rc);
		goto bail;
	}

	rc = mfd_add_devices(pmic->dev, 0, &batt_alarm_cell, 1, NULL,
				irq_base);
	if (rc) {
		pr_err("Failed to add battery alarm subdevice ret=%d\n",
			rc);
		goto bail;
	}

	rc = mfd_add_devices(pmic->dev, 0, &upl_cell, 1, NULL, 0);
	if (rc) {
		pr_err("Failed to add upl subdevice ret=%d\n", rc);
		goto bail;
	}

	rc = mfd_add_devices(pmic->dev, 0, &nfc_cell, 1, NULL, 0);
	if (rc) {
		pr_err("Failed to add upl subdevice ret=%d\n", rc);
		goto bail;
	}

	if (pdata->charger_pdata) {
		pm8058_charger_cell.platform_data = pdata->charger_pdata;
		pm8058_charger_cell.pdata_size = sizeof(struct
						pmic8058_charger_data);
		rc = mfd_add_devices(pmic->dev, 0, &pm8058_charger_cell,
						1, NULL, irq_base);
		if (rc) {
			pr_err("Failed to add charger subdevice ret=%d\n", rc);
			goto bail;
		}
	}

	rc = mfd_add_devices(pmic->dev, 0, &debugfs_cell, 1, NULL, irq_base);
	if (rc) {
		pr_err("Failed to add debugfs subdevice ret=%d\n", rc);
		goto bail;
	}

	return rc;
bail:
	if (pmic->irq_chip) {
		pm8xxx_irq_exit(pmic->irq_chip);
		pmic->irq_chip = NULL;
	}
	return rc;
}

static int __devinit pm8058_probe(struct platform_device *pdev)
{
	int rc;
	struct pm8058_platform_data *pdata = pdev->dev.platform_data;
	struct pm8058_chip *pmic;

	if (pdata == NULL) {
		pr_err("%s: No platform_data or IRQ.\n", __func__);
		return -ENODEV;
	}

	pmic = kzalloc(sizeof *pmic, GFP_KERNEL);
	if (pmic == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	pmic->dev = &pdev->dev;

	pm8058_drvdata.pm_chip_data = pmic;
	platform_set_drvdata(pdev, &pm8058_drvdata);

	/* Read PMIC chip revision */
	rc = pm8058_readb(pmic->dev, PM8058_REG_REV, &pmic->revision);
	if (rc)
		pr_err("%s: Failed on pm8058_readb for revision: rc=%d.\n",
			__func__, rc);

	pr_info("%s: PMIC revision: %X\n", __func__, pmic->revision);

	(void) memcpy((void *)&pmic->pdata, (const void *)pdata,
		      sizeof(pmic->pdata));

	rc = pm8058_add_subdevices(pdata, pmic);
	if (rc) {
		pr_err("Cannot add subdevices rc=%d\n", rc);
		goto err;
	}

	rc = pm8xxx_hard_reset_config(PM8XXX_SHUTDOWN_ON_HARD_RESET);
	if (rc < 0)
		pr_err("%s: failed to config shutdown on hard reset: %d\n",
								__func__, rc);

	return 0;

err:
	mfd_remove_devices(pmic->dev);
	platform_set_drvdata(pdev, NULL);
	kfree(pmic);
	return rc;
}

static int __devexit pm8058_remove(struct platform_device *pdev)
{
	struct pm8xxx_drvdata *drvdata;
	struct pm8058_chip *pmic = NULL;

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

static struct platform_driver pm8058_driver = {
	.probe		= pm8058_probe,
	.remove		= __devexit_p(pm8058_remove),
	.driver		= {
		.name	= "pm8058-core",
		.owner	= THIS_MODULE,
	},
};

static int __init pm8058_init(void)
{
	return platform_driver_register(&pm8058_driver);
}
postcore_initcall(pm8058_init);

static void __exit pm8058_exit(void)
{
	platform_driver_unregister(&pm8058_driver);
}
module_exit(pm8058_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8058 core driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pmic8058-core");
