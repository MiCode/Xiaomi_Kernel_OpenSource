/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/string.h>
#include <linux/err.h>
#include <linux/msm_ssbi.h>
#include <linux/mfd/core.h>
#include <linux/mfd/pm8xxx/pm8018.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/regulator.h>
#include <linux/leds-pm8xxx.h>


/* PMIC PM8018 SSBI Addresses */
#define REG_HWREV		0x002  /* PMIC4 revision */
#define REG_HWREV_2		0x0E8  /* PMIC4 revision 2 */

#define REG_MPP_BASE		0x050
#define REG_IRQ_BASE		0x1BB

#define REG_RTC_BASE		0x11D

#define REG_TEMP_ALARM_CTRL	0x01B
#define REG_TEMP_ALARM_PWM	0x09B


#define PM8018_VERSION_MASK	0xFFF0
#define PM8018_VERSION_VALUE	0x08F0
#define PM8018_REVISION_MASK	0x000F

#define REG_PM8018_PON_CNTRL_3	0x01D
#define PM8018_RESTART_REASON_MASK	0x07

#define SINGLE_IRQ_RESOURCE(_name, _irq) \
{ \
	.name	= _name, \
	.start	= _irq, \
	.end	= _irq, \
	.flags	= IORESOURCE_IRQ, \
}

struct pm8018 {
	struct device					*dev;
	struct pm_irq_chip				*irq_chip;
	struct mfd_cell					*mfd_regulators;
	struct pm8xxx_regulator_core_platform_data	*regulator_cdata;
	u32						rev_registers;
};

static int pm8018_readb(const struct device *dev, u16 addr, u8 *val)
{
	const struct pm8xxx_drvdata *pm8018_drvdata = dev_get_drvdata(dev);
	const struct pm8018 *pmic = pm8018_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, val, 1);
}

static int pm8018_writeb(const struct device *dev, u16 addr, u8 val)
{
	const struct pm8xxx_drvdata *pm8018_drvdata = dev_get_drvdata(dev);
	const struct pm8018 *pmic = pm8018_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, &val, 1);
}

static int pm8018_read_buf(const struct device *dev, u16 addr, u8 *buf,
									int cnt)
{
	const struct pm8xxx_drvdata *pm8018_drvdata = dev_get_drvdata(dev);
	const struct pm8018 *pmic = pm8018_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, buf, cnt);
}

static int pm8018_write_buf(const struct device *dev, u16 addr, u8 *buf,
									int cnt)
{
	const struct pm8xxx_drvdata *pm8018_drvdata = dev_get_drvdata(dev);
	const struct pm8018 *pmic = pm8018_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, buf, cnt);
}

static int pm8018_read_irq_stat(const struct device *dev, int irq)
{
	const struct pm8xxx_drvdata *pm8018_drvdata = dev_get_drvdata(dev);
	const struct pm8018 *pmic = pm8018_drvdata->pm_chip_data;

	return pm8xxx_get_irq_stat(pmic->irq_chip, irq);
}

static enum pm8xxx_version pm8018_get_version(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8018_drvdata = dev_get_drvdata(dev);
	const struct pm8018 *pmic = pm8018_drvdata->pm_chip_data;
	enum pm8xxx_version version = -ENODEV;

	if ((pmic->rev_registers & PM8018_VERSION_MASK) == PM8018_VERSION_VALUE)
		version = PM8XXX_VERSION_8018;

	return version;
}

static int pm8018_get_revision(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8018_drvdata = dev_get_drvdata(dev);
	const struct pm8018 *pmic = pm8018_drvdata->pm_chip_data;

	return pmic->rev_registers & PM8018_REVISION_MASK;
}

static struct pm8xxx_drvdata pm8018_drvdata = {
	.pmic_readb		= pm8018_readb,
	.pmic_writeb		= pm8018_writeb,
	.pmic_read_buf		= pm8018_read_buf,
	.pmic_write_buf		= pm8018_write_buf,
	.pmic_read_irq_stat	= pm8018_read_irq_stat,
	.pmic_get_version	= pm8018_get_version,
	.pmic_get_revision	= pm8018_get_revision,
};

static const struct resource gpio_cell_resources[] __devinitconst = {
	[0] = {
		.start = PM8018_IRQ_BLOCK_BIT(PM8018_GPIO_BLOCK_START, 0),
		.end   = PM8018_IRQ_BLOCK_BIT(PM8018_GPIO_BLOCK_START, 0)
			+ PM8018_NR_GPIOS - 1,
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
	SINGLE_IRQ_RESOURCE(NULL, PM8018_ADC_EOC_USR_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8018_ADC_BATT_TEMP_WARM_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8018_ADC_BATT_TEMP_COLD_IRQ),
};

static struct mfd_cell adc_cell __devinitdata = {
	.name		= PM8XXX_ADC_DEV_NAME,
	.id		= -1,
	.resources	= adc_cell_resources,
	.num_resources	= ARRAY_SIZE(adc_cell_resources),
};

static const struct resource mpp_cell_resources[] __devinitconst = {
	{
		.start	= PM8018_IRQ_BLOCK_BIT(PM8018_MPP_BLOCK_START, 0),
		.end	= PM8018_IRQ_BLOCK_BIT(PM8018_MPP_BLOCK_START, 0)
			  + PM8018_NR_MPPS - 1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct mfd_cell mpp_cell __devinitdata = {
	.name		= PM8XXX_MPP_DEV_NAME,
	.id		= -1,
	.resources	= mpp_cell_resources,
	.num_resources	= ARRAY_SIZE(mpp_cell_resources),
};

static const struct resource rtc_cell_resources[] __devinitconst = {
	[0] = SINGLE_IRQ_RESOURCE(NULL, PM8018_RTC_ALARM_IRQ),
	[1] = {
		.name   = "pmic_rtc_base",
		.start  = REG_RTC_BASE,
		.end    = REG_RTC_BASE,
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
	SINGLE_IRQ_RESOURCE(NULL, PM8018_PWRKEY_REL_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8018_PWRKEY_PRESS_IRQ),
};

static struct mfd_cell pwrkey_cell __devinitdata = {
	.name		= PM8XXX_PWRKEY_DEV_NAME,
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_pwrkey),
	.resources	= resources_pwrkey,
};

static struct mfd_cell misc_cell __devinitdata = {
	.name           = PM8XXX_MISC_DEV_NAME,
	.id             = -1,
};

static struct mfd_cell debugfs_cell __devinitdata = {
	.name		= "pm8xxx-debug",
	.id		= -1,
	.platform_data	= "pm8018-dbg",
	.pdata_size	= sizeof("pm8018-dbg"),
};

static struct mfd_cell pwm_cell __devinitdata = {
	.name           = PM8XXX_PWM_DEV_NAME,
	.id             = -1,
};

static struct mfd_cell leds_cell __devinitdata = {
	.name		= PM8XXX_LEDS_DEV_NAME,
	.id		= -1,
};

static struct pm8xxx_vreg regulator_data[] = {
	/*   name	     pc_name	    ctrl   test   hpm_min */
	PLDO("8018_l2",      "8018_l2_pc",  0x0B0, 0x0B1, LDO_50),
	PLDO("8018_l3",      "8018_l3_pc",  0x0B2, 0x0B3, LDO_50),
	PLDO("8018_l4",      "8018_l4_pc",  0x0B4, 0x0B5, LDO_300),
	PLDO("8018_l5",      "8018_l5_pc",  0x0B6, 0x0B7, LDO_150),
	PLDO("8018_l6",      "8018_l6_pc",  0x0B8, 0x0B9, LDO_150),
	PLDO("8018_l7",      "8018_l7_pc",  0x0BA, 0x0BB, LDO_300),
	NLDO("8018_l8",      "8018_l8_pc",  0x0BC, 0x0BD, LDO_150),
	NLDO1200("8018_l9",		    0x0BE, 0x0BF, LDO_1200),
	NLDO1200("8018_l10",		    0x0C0, 0x0C1, LDO_1200),
	NLDO1200("8018_l11",		    0x0C2, 0x0C3, LDO_1200),
	NLDO1200("8018_l12",		    0x0C4, 0x0C5, LDO_1200),
	PLDO("8018_l13",     "8018_l13_pc", 0x0C8, 0x0C9, LDO_50),
	PLDO("8018_l14",     "8018_l14_pc", 0x0CA, 0x0CB, LDO_50),

	/*   name	pc_name       ctrl   test2  clk    sleep  hpm_min */
	SMPS("8018_s1", "8018_s1_pc", 0x1D0, 0x1D5, 0x009, 0x1D2, SMPS_1500),
	SMPS("8018_s2", "8018_s2_pc", 0x1D8, 0x1DD, 0x00A, 0x1DA, SMPS_1500),
	SMPS("8018_s3", "8018_s3_pc", 0x1E0, 0x1E5, 0x00B, 0x1E2, SMPS_1500),
	SMPS("8018_s4", "8018_s4_pc", 0x1E8, 0x1ED, 0x00C, 0x1EA, SMPS_1500),
	SMPS("8018_s5", "8018_s5_pc", 0x1F0, 0x1F5, 0x00D, 0x1F2, SMPS_1500),

	/* name		     pc_name	     ctrl   test */
	VS("8018_lvs1",      "8018_lvs1_pc", 0x060, 0x061),
};

#define MAX_NAME_COMPARISON_LEN 32

static int __devinit match_regulator(
	struct pm8xxx_regulator_core_platform_data *core_data, char *name)
{
	int found = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(regulator_data); i++) {
		if (regulator_data[i].rdesc.name
		    && strncmp(regulator_data[i].rdesc.name, name,
				MAX_NAME_COMPARISON_LEN) == 0) {
			core_data->is_pin_controlled = false;
			core_data->vreg = &regulator_data[i];
			found = 1;
			break;
		} else if (regulator_data[i].rdesc_pc.name
			   && strncmp(regulator_data[i].rdesc_pc.name, name,
				MAX_NAME_COMPARISON_LEN) == 0) {
			core_data->is_pin_controlled = true;
			core_data->vreg = &regulator_data[i];
			found = 1;
			break;
		}
	}

	if (!found)
		pr_err("could not find a match for regulator: %s\n", name);

	return found;
}

static int __devinit
pm8018_add_regulators(const struct pm8018_platform_data *pdata,
		      struct pm8018 *pmic, int irq_base)
{
	int ret = 0;
	struct mfd_cell *mfd_regulators;
	struct pm8xxx_regulator_core_platform_data *cdata;
	int i;

	/* Add one device for each regulator used by the board. */
	mfd_regulators = kzalloc(sizeof(struct mfd_cell)
				 * (pdata->num_regulators), GFP_KERNEL);
	if (!mfd_regulators) {
		pr_err("Cannot allocate %d bytes for pm8018 regulator "
			"mfd cells\n", sizeof(struct mfd_cell)
					* (pdata->num_regulators));
		return -ENOMEM;
	}
	cdata = kzalloc(sizeof(struct pm8xxx_regulator_core_platform_data)
			* pdata->num_regulators, GFP_KERNEL);
	if (!cdata) {
		pr_err("Cannot allocate %d bytes for pm8018 regulator "
			"core data\n", pdata->num_regulators
			  * sizeof(struct pm8xxx_regulator_core_platform_data));
		kfree(mfd_regulators);
		return -ENOMEM;
	}
	for (i = 0; i < ARRAY_SIZE(regulator_data); i++)
		mutex_init(&regulator_data[i].pc_lock);

	for (i = 0; i < pdata->num_regulators; i++) {
		if (!pdata->regulator_pdatas[i].init_data.constraints.name) {
			pr_err("name missing for regulator %d\n", i);
			ret = -EINVAL;
			goto bail;
		}
		if (!match_regulator(&cdata[i],
		      pdata->regulator_pdatas[i].init_data.constraints.name)) {
			ret = -ENODEV;
			goto bail;
		}
		cdata[i].pdata = &(pdata->regulator_pdatas[i]);
		mfd_regulators[i].name = PM8XXX_REGULATOR_DEV_NAME;
		mfd_regulators[i].id = cdata[i].pdata->id;
		mfd_regulators[i].platform_data = &cdata[i];
		mfd_regulators[i].pdata_size =
			sizeof(struct pm8xxx_regulator_core_platform_data);
	}
	ret = mfd_add_devices(pmic->dev, 0, mfd_regulators,
			pdata->num_regulators, NULL, irq_base);
	if (ret)
		goto bail;

	pmic->mfd_regulators = mfd_regulators;
	pmic->regulator_cdata = cdata;
	return ret;

bail:
	for (i = 0; i < ARRAY_SIZE(regulator_data); i++)
		mutex_destroy(&regulator_data[i].pc_lock);
	kfree(mfd_regulators);
	kfree(cdata);
	return ret;
}

static int __devinit
pm8018_add_subdevices(const struct pm8018_platform_data *pdata,
		      struct pm8018 *pmic)
{
	int ret = 0, irq_base = 0;
	struct pm_irq_chip *irq_chip;

	if (pdata->irq_pdata) {
		pdata->irq_pdata->irq_cdata.nirqs = PM8018_NR_IRQS;
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
		pdata->gpio_pdata->gpio_cdata.ngpios = PM8018_NR_GPIOS;
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
		pdata->mpp_pdata->core_data.nmpps = PM8018_NR_MPPS;
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

	if (pdata->adc_pdata) {
		adc_cell.platform_data = pdata->adc_pdata;
		adc_cell.pdata_size = sizeof(struct pm8xxx_adc_platform_data);
		ret = mfd_add_devices(pmic->dev, 0, &adc_cell, 1, NULL,
				      irq_base);
		if (ret) {
			pr_err("Failed to add adc subdevice ret=%d\n", ret);
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

	if (pdata->num_regulators > 0 && pdata->regulator_pdatas) {
		ret = pm8018_add_regulators(pdata, pmic, irq_base);
		if (ret) {
			pr_err("Failed to add regulator subdevices ret=%d\n",
				ret);
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

static const char * const pm8018_restart_reason[] = {
	[0] = "Unknown",
	[1] = "Triggered from CBL (external charger)",
	[2] = "Triggered from KPD (power key press)",
	[3] = "Triggered from CHG (usb charger insertion)",
	[4] = "Triggered from SMPL (sudden momentary power loss)",
	[5] = "Triggered from RTC (real time clock)",
	[6] = "Triggered by Hard Reset",
	[7] = "Triggered by General Purpose Trigger",
};

static const char * const pm8018_rev_names[] = {
	[PM8XXX_REVISION_8018_TEST]	= "test",
	[PM8XXX_REVISION_8018_1p0]	= "1.0",
	[PM8XXX_REVISION_8018_1p1]	= "1.1",
	[PM8XXX_REVISION_8018_2p0]	= "2.0",
};

static int __devinit pm8018_probe(struct platform_device *pdev)
{
	const struct pm8018_platform_data *pdata = pdev->dev.platform_data;
	const char *revision_name = "unknown";
	struct pm8018 *pmic;
	enum pm8xxx_version version;
	int revision;
	int rc;
	u8 val;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	pmic = kzalloc(sizeof(struct pm8018), GFP_KERNEL);
	if (!pmic) {
		pr_err("Cannot alloc pm8018 struct\n");
		return -ENOMEM;
	}

	/* Read PMIC chip revision */
	rc = msm_ssbi_read(pdev->dev.parent, REG_HWREV, &val, sizeof(val));
	if (rc) {
		pr_err("Failed to read hw rev 1 reg %d:rc=%d\n", REG_HWREV, rc);
		goto err_read_rev;
	}
	pr_info("PMIC revision 1: %02X\n", val);
	pmic->rev_registers = val;

	/* Read PMIC chip revision 2 */
	rc = msm_ssbi_read(pdev->dev.parent, REG_HWREV_2, &val, sizeof(val));
	if (rc) {
		pr_err("Failed to read hw rev 2 reg %d:rc=%d\n", REG_HWREV_2,
			rc);
		goto err_read_rev;
	}
	pr_info("PMIC revision 2: %02X\n", val);
	pmic->rev_registers |= val << BITS_PER_BYTE;

	pmic->dev = &pdev->dev;
	pm8018_drvdata.pm_chip_data = pmic;
	platform_set_drvdata(pdev, &pm8018_drvdata);

	/* Print out human readable version and revision names. */
	version = pm8xxx_get_version(pmic->dev);
	if (version == PM8XXX_VERSION_8018) {
		revision = pm8xxx_get_revision(pmic->dev);
		if (revision >= 0 && revision < ARRAY_SIZE(pm8018_rev_names))
			revision_name = pm8018_rev_names[revision];
		pr_info("PMIC version: PM8018 rev %s\n", revision_name);
	} else {
		WARN_ON(version != PM8XXX_VERSION_8018);
	}
	/* Log human readable restart reason */
	rc = msm_ssbi_read(pdev->dev.parent, REG_PM8018_PON_CNTRL_3, &val, 1);
	if (rc) {
		pr_err("Cannot read restart reason rc=%d\n", rc);
		goto err_read_rev;
	}
	val &= PM8018_RESTART_REASON_MASK;
	pr_info("PMIC Restart Reason: %s\n", pm8018_restart_reason[val]);

	rc = pm8018_add_subdevices(pdata, pmic);
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
	kfree(pmic->mfd_regulators);
	kfree(pmic->regulator_cdata);
err_read_rev:
	kfree(pmic);
	return rc;
}

static int __devexit pm8018_remove(struct platform_device *pdev)
{
	struct pm8xxx_drvdata *drvdata;
	struct pm8018 *pmic = NULL;
	int i;

	drvdata = platform_get_drvdata(pdev);
	if (drvdata)
		pmic = drvdata->pm_chip_data;
	if (pmic) {
		if (pmic->dev)
			mfd_remove_devices(pmic->dev);
		if (pmic->irq_chip) {
			pm8xxx_irq_exit(pmic->irq_chip);
			pmic->irq_chip = NULL;
		}
		if (pmic->mfd_regulators) {
			for (i = 0; i < ARRAY_SIZE(regulator_data); i++)
				mutex_destroy(&regulator_data[i].pc_lock);
		}
		kfree(pmic->mfd_regulators);
		kfree(pmic->regulator_cdata);
		kfree(pmic);
	}
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver pm8018_driver = {
	.probe		= pm8018_probe,
	.remove		= __devexit_p(pm8018_remove),
	.driver		= {
		.name	= PM8018_CORE_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8018_init(void)
{
	return platform_driver_register(&pm8018_driver);
}
postcore_initcall(pm8018_init);

static void __exit pm8018_exit(void)
{
	platform_driver_unregister(&pm8018_driver);
}
module_exit(pm8018_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC 8018 core driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8018_CORE_DEV_NAME);
