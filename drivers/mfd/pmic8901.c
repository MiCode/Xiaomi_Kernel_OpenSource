/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/core.h>
#include <linux/msm_ssbi.h>
#include <linux/mfd/pmic8901.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/module.h>

/* PMIC8901 Revision */
#define PM8901_REG_REV			0x002
#define PM8901_VERSION_MASK		0xF0
#define PM8901_REVISION_MASK		0x0F
#define PM8901_VERSION_VALUE		0xF0

#define REG_IRQ_BASE			0xD5
#define REG_MPP_BASE			0x27

#define REG_TEMP_ALRM_CTRL		0x23
#define REG_TEMP_ALRM_PWM		0x24

#define SINGLE_IRQ_RESOURCE(_name, _irq) \
{ \
	.name	= _name, \
	.start	= _irq, \
	.end	= _irq, \
	.flags	= IORESOURCE_IRQ, \
}

struct pm8901_chip {
	struct pm8901_platform_data	pdata;
	struct device			*dev;
	struct pm_irq_chip		*irq_chip;
	struct mfd_cell			*mfd_regulators;
	u8				revision;
};

static int pm8901_readb(const struct device *dev, u16 addr, u8 *val)
{
	const struct pm8xxx_drvdata *pm8901_drvdata = dev_get_drvdata(dev);
	const struct pm8901_chip *pmic = pm8901_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, val, 1);
}

static int pm8901_writeb(const struct device *dev, u16 addr, u8 val)
{
	const struct pm8xxx_drvdata *pm8901_drvdata = dev_get_drvdata(dev);
	const struct pm8901_chip *pmic = pm8901_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, &val, 1);
}

static int pm8901_read_buf(const struct device *dev, u16 addr, u8 *buf,
								int cnt)
{
	const struct pm8xxx_drvdata *pm8901_drvdata = dev_get_drvdata(dev);
	const struct pm8901_chip *pmic = pm8901_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, buf, cnt);
}

static int pm8901_write_buf(const struct device *dev, u16 addr, u8 *buf,
								int cnt)
{
	const struct pm8xxx_drvdata *pm8901_drvdata = dev_get_drvdata(dev);
	const struct pm8901_chip *pmic = pm8901_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, buf, cnt);
}

static int pm8901_read_irq_stat(const struct device *dev, int irq)
{
	const struct pm8xxx_drvdata *pm8901_drvdata = dev_get_drvdata(dev);
	const struct pm8901_chip *pmic = pm8901_drvdata->pm_chip_data;

	return pm8xxx_get_irq_stat(pmic->irq_chip, irq);

	return 0;
}

static enum pm8xxx_version pm8901_get_version(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8901_drvdata = dev_get_drvdata(dev);
	const struct pm8901_chip *pmic = pm8901_drvdata->pm_chip_data;
	enum pm8xxx_version version = -ENODEV;

	if ((pmic->revision & PM8901_VERSION_MASK) == PM8901_VERSION_VALUE)
		version = PM8XXX_VERSION_8901;

	return version;
}

static int pm8901_get_revision(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8901_drvdata = dev_get_drvdata(dev);
	const struct pm8901_chip *pmic = pm8901_drvdata->pm_chip_data;

	return pmic->revision & PM8901_REVISION_MASK;
}

static struct pm8xxx_drvdata pm8901_drvdata = {
	.pmic_readb		= pm8901_readb,
	.pmic_writeb		= pm8901_writeb,
	.pmic_read_buf		= pm8901_read_buf,
	.pmic_write_buf		= pm8901_write_buf,
	.pmic_read_irq_stat	= pm8901_read_irq_stat,
	.pmic_get_version	= pm8901_get_version,
	.pmic_get_revision	= pm8901_get_revision,
};

static struct mfd_cell misc_cell = {
	.name		= PM8XXX_MISC_DEV_NAME,
	.id		= 1,
};

static struct mfd_cell debugfs_cell = {
	.name		= "pm8xxx-debug",
	.id		= 1,
	.platform_data	= "pm8901-dbg",
	.pdata_size	= sizeof("pm8901-dbg"),
};

static const struct resource thermal_alarm_cell_resources[] = {
	SINGLE_IRQ_RESOURCE("pm8901_tempstat_irq", PM8901_TEMPSTAT_IRQ),
	SINGLE_IRQ_RESOURCE("pm8901_overtemp_irq", PM8901_OVERTEMP_IRQ),
};

static struct pm8xxx_tm_core_data thermal_alarm_cdata = {
	.adc_type			= PM8XXX_TM_ADC_NONE,
	.reg_addr_temp_alarm_ctrl	= REG_TEMP_ALRM_CTRL,
	.reg_addr_temp_alarm_pwm	= REG_TEMP_ALRM_PWM,
	.tm_name			= "pm8901_tz",
	.irq_name_temp_stat		= "pm8901_tempstat_irq",
	.irq_name_over_temp		= "pm8901_overtemp_irq",
};

static struct mfd_cell thermal_alarm_cell = {
	.name		= PM8XXX_TM_DEV_NAME,
	.id		= 1,
	.resources	= thermal_alarm_cell_resources,
	.num_resources	= ARRAY_SIZE(thermal_alarm_cell_resources),
	.platform_data	= &thermal_alarm_cdata,
	.pdata_size	= sizeof(struct pm8xxx_tm_core_data),
};

static const struct resource mpp_cell_resources[] = {
	{
		.start	= PM8901_IRQ_BLOCK_BIT(PM8901_MPP_BLOCK_START, 0),
		.end	= PM8901_IRQ_BLOCK_BIT(PM8901_MPP_BLOCK_START, 0)
			  + PM8901_MPPS - 1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct mfd_cell mpp_cell = {
	.name		= PM8XXX_MPP_DEV_NAME,
	.id		= 1,
	.resources	= mpp_cell_resources,
	.num_resources	= ARRAY_SIZE(mpp_cell_resources),
};

static int __devinit
pm8901_add_subdevices(const struct pm8901_platform_data *pdata,
				struct pm8901_chip *pmic)
{
	int rc = 0, irq_base = 0, i;
	struct pm_irq_chip *irq_chip;
	static struct mfd_cell *mfd_regulators;

	if (pdata->irq_pdata) {
		pdata->irq_pdata->irq_cdata.nirqs = PM8901_NR_IRQS;
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

	if (pdata->mpp_pdata) {
		pdata->mpp_pdata->core_data.nmpps = PM8901_MPPS;
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
			pr_err("Cannot allocate %d bytes for pm8901 regulator "
				"mfd cells\n", sizeof(struct mfd_cell)
						* (pdata->num_regulators));
			rc = -ENOMEM;
			goto bail;
		}
		for (i = 0; i < pdata->num_regulators; i++) {
			mfd_regulators[i].name = "pm8901-regulator";
			mfd_regulators[i].id = pdata->regulator_pdatas[i].id;
			mfd_regulators[i].platform_data =
				&(pdata->regulator_pdatas[i]);
			mfd_regulators[i].pdata_size =
					sizeof(struct pm8901_vreg_pdata);
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

	rc = mfd_add_devices(pmic->dev, 0, &thermal_alarm_cell, 1, NULL,
							irq_base);
	if (rc) {
		pr_err("Failed to add thermal alarm subdevice ret=%d\n",
			rc);
		goto bail;
	}

	rc = mfd_add_devices(pmic->dev, 0, &debugfs_cell, 1, NULL, irq_base);
	if (rc) {
		pr_err("Failed to add debugfs subdevice ret=%d\n", rc);
		goto bail;
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

	return rc;

bail:
	if (pmic->irq_chip) {
		pm8xxx_irq_exit(pmic->irq_chip);
		pmic->irq_chip = NULL;
	}
	return rc;
}

static const char * const pm8901_rev_names[] = {
	[PM8XXX_REVISION_8901_TEST]	= "test",
	[PM8XXX_REVISION_8901_1p0]	= "1.0",
	[PM8XXX_REVISION_8901_1p1]	= "1.1",
	[PM8XXX_REVISION_8901_2p0]	= "2.0",
	[PM8XXX_REVISION_8901_2p1]	= "2.1",
	[PM8XXX_REVISION_8901_2p2]	= "2.2",
	[PM8XXX_REVISION_8901_2p3]	= "2.3",
};

static int __devinit pm8901_probe(struct platform_device *pdev)
{
	int rc;
	struct pm8901_platform_data *pdata = pdev->dev.platform_data;
	const char *revision_name = "unknown";
	struct pm8901_chip *pmic;
	int revision;

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

	pm8901_drvdata.pm_chip_data = pmic;
	platform_set_drvdata(pdev, &pm8901_drvdata);

	/* Read PMIC chip revision */
	rc = pm8901_readb(pmic->dev, PM8901_REG_REV, &pmic->revision);
	if (rc)
		pr_err("%s: Failed reading version register rc=%d.\n",
			__func__, rc);

	pr_info("%s: PMIC revision reg: %02X\n", __func__, pmic->revision);
	revision =  pm8xxx_get_revision(pmic->dev);
	if (revision >= 0 && revision < ARRAY_SIZE(pm8901_rev_names))
		revision_name = pm8901_rev_names[revision];
	pr_info("%s: PMIC version: PM8901 rev %s\n", __func__, revision_name);

	(void) memcpy((void *)&pmic->pdata, (const void *)pdata,
		      sizeof(pmic->pdata));

	rc = pm8901_add_subdevices(pdata, pmic);
	if (rc) {
		pr_err("Cannot add subdevices rc=%d\n", rc);
		goto err;
	}

	return 0;

err:
	platform_set_drvdata(pdev, NULL);
	kfree(pmic);
	return rc;
}

static int __devexit pm8901_remove(struct platform_device *pdev)
{
	struct pm8xxx_drvdata *drvdata;
	struct pm8901_chip *pmic = NULL;

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

static struct platform_driver pm8901_driver = {
	.probe		= pm8901_probe,
	.remove		= __devexit_p(pm8901_remove),
	.driver		= {
		.name	= "pm8901-core",
		.owner	= THIS_MODULE,
	},
};

static int __init pm8901_init(void)
{
	return  platform_driver_register(&pm8901_driver);
}
postcore_initcall(pm8901_init);

static void __exit pm8901_exit(void)
{
	platform_driver_unregister(&pm8901_driver);
}
module_exit(pm8901_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8901 core driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pmic8901-core");
