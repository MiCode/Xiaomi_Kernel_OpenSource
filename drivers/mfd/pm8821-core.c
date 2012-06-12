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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/msm_ssbi.h>
#include <linux/mfd/core.h>
#include <linux/mfd/pm8xxx/pm8821.h>
#include <linux/mfd/pm8xxx/core.h>

#define REG_HWREV		0x002  /* PMIC4 revision */
#define REG_HWREV_2		0x0E8  /* PMIC4 revision 2 */

#define REG_MPP_BASE		0x050
#define REG_IRQ_BASE		0x100

#define PM8821_VERSION_MASK	0xFFF0
#define PM8821_VERSION_VALUE	0x0BF0
#define PM8821_REVISION_MASK	0x000F

#define SINGLE_IRQ_RESOURCE(_name, _irq) \
{ \
	.name	= _name, \
	.start	= _irq, \
	.end	= _irq, \
	.flags	= IORESOURCE_IRQ, \
}

struct pm8821 {
	struct device			*dev;
	struct pm_irq_chip		*irq_chip;
	u32				rev_registers;
};

static int pm8821_readb(const struct device *dev, u16 addr, u8 *val)
{
	const struct pm8xxx_drvdata *pm8821_drvdata = dev_get_drvdata(dev);
	const struct pm8821 *pmic = pm8821_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, val, 1);
}

static int pm8821_writeb(const struct device *dev, u16 addr, u8 val)
{
	const struct pm8xxx_drvdata *pm8821_drvdata = dev_get_drvdata(dev);
	const struct pm8821 *pmic = pm8821_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, &val, 1);
}

static int pm8821_read_buf(const struct device *dev, u16 addr, u8 *buf,
									int cnt)
{
	const struct pm8xxx_drvdata *pm8821_drvdata = dev_get_drvdata(dev);
	const struct pm8821 *pmic = pm8821_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, buf, cnt);
}

static int pm8821_write_buf(const struct device *dev, u16 addr, u8 *buf,
									int cnt)
{
	const struct pm8xxx_drvdata *pm8821_drvdata = dev_get_drvdata(dev);
	const struct pm8821 *pmic = pm8821_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, buf, cnt);
}

static int pm8821_read_irq_stat(const struct device *dev, int irq)
{
	const struct pm8xxx_drvdata *pm8821_drvdata = dev_get_drvdata(dev);
	const struct pm8821 *pmic = pm8821_drvdata->pm_chip_data;

	return pm8821_get_irq_stat(pmic->irq_chip, irq);
}

static enum pm8xxx_version pm8821_get_version(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8821_drvdata = dev_get_drvdata(dev);
	const struct pm8821 *pmic = pm8821_drvdata->pm_chip_data;
	enum pm8xxx_version version = -ENODEV;

	if ((pmic->rev_registers & PM8821_VERSION_MASK) == PM8821_VERSION_VALUE)
		version = PM8XXX_VERSION_8821;

	return version;
}

static int pm8821_get_revision(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8821_drvdata = dev_get_drvdata(dev);
	const struct pm8821 *pmic = pm8821_drvdata->pm_chip_data;

	return pmic->rev_registers & PM8821_REVISION_MASK;
}

static struct pm8xxx_drvdata pm8821_drvdata = {
	.pmic_readb		= pm8821_readb,
	.pmic_writeb		= pm8821_writeb,
	.pmic_read_buf		= pm8821_read_buf,
	.pmic_write_buf		= pm8821_write_buf,
	.pmic_read_irq_stat	= pm8821_read_irq_stat,
	.pmic_get_version	= pm8821_get_version,
	.pmic_get_revision	= pm8821_get_revision,
};

static const struct resource mpp_cell_resources[] __devinitconst = {
	{
		.start	= PM8821_IRQ_BLOCK_BIT(PM8821_MPP_BLOCK_START, 0),
		.end	= PM8821_IRQ_BLOCK_BIT(PM8821_MPP_BLOCK_START, 0)
			  + PM8821_NR_MPPS - 1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct mfd_cell mpp_cell __devinitdata = {
	.name		= PM8XXX_MPP_DEV_NAME,
	.id		= 1,
	.resources	= mpp_cell_resources,
	.num_resources	= ARRAY_SIZE(mpp_cell_resources),
};

static struct mfd_cell debugfs_cell __devinitdata = {
	.name		= "pm8xxx-debug",
	.id		= 1,
	.platform_data	= "pm8821-dbg",
	.pdata_size	= sizeof("pm8821-dbg"),
};


static int __devinit
pm8821_add_subdevices(const struct pm8821_platform_data *pdata,
		      struct pm8821 *pmic)
{
	int ret = 0, irq_base = 0;
	struct pm_irq_chip *irq_chip;

	if (pdata->irq_pdata) {
		pdata->irq_pdata->irq_cdata.nirqs = PM8821_NR_IRQS;
		pdata->irq_pdata->irq_cdata.base_addr = REG_IRQ_BASE;
		irq_base = pdata->irq_pdata->irq_base;
		irq_chip = pm8821_irq_init(pmic->dev, pdata->irq_pdata);

		if (IS_ERR(irq_chip)) {
			pr_err("Failed to init interrupts ret=%ld\n",
					PTR_ERR(irq_chip));
			return PTR_ERR(irq_chip);
		}
		pmic->irq_chip = irq_chip;
	}

	if (pdata->mpp_pdata) {
		pdata->mpp_pdata->core_data.nmpps = PM8821_NR_MPPS;
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

	ret = mfd_add_devices(pmic->dev, 0, &debugfs_cell, 1, NULL, irq_base);
	if (ret) {
		pr_err("Failed to add debugfs subdevice ret=%d\n", ret);
		goto bail;
	}

	return 0;
bail:
	if (pmic->irq_chip) {
		pm8821_irq_exit(pmic->irq_chip);
		pmic->irq_chip = NULL;
	}
	return ret;
}

static const char * const pm8821_rev_names[] = {
	[PM8XXX_REVISION_8821_TEST]	= "test",
	[PM8XXX_REVISION_8821_1p0]	= "1.0",
	[PM8XXX_REVISION_8821_2p0]	= "2.0",
	[PM8XXX_REVISION_8821_2p1]	= "2.1",
};

static int __devinit pm8821_probe(struct platform_device *pdev)
{
	const struct pm8821_platform_data *pdata = pdev->dev.platform_data;
	const char *revision_name = "unknown";
	struct pm8821 *pmic;
	enum pm8xxx_version version;
	int revision;
	int rc;
	u8 val;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	pmic = kzalloc(sizeof(struct pm8821), GFP_KERNEL);
	if (!pmic) {
		pr_err("Cannot alloc pm8821 struct\n");
		return -ENOMEM;
	}

	/* Read PMIC chip revision */
	rc = msm_ssbi_read(pdev->dev.parent, REG_HWREV, &val, sizeof(val));
	if (rc) {
		pr_err("Failed to read hw rev reg %d:rc=%d\n", REG_HWREV, rc);
		goto err_read_rev;
	}
	pr_info("PMIC revision 1: PM8821 rev %02X\n", val);
	pmic->rev_registers = val;

	/* Read PMIC chip revision 2 */
	rc = msm_ssbi_read(pdev->dev.parent, REG_HWREV_2, &val, sizeof(val));
	if (rc) {
		pr_err("Failed to read hw rev 2 reg %d:rc=%d\n",
			REG_HWREV_2, rc);
		goto err_read_rev;
	}
	pr_info("PMIC revision 2: PM8821 rev %02X\n", val);
	pmic->rev_registers |= val << BITS_PER_BYTE;

	pmic->dev = &pdev->dev;
	pm8821_drvdata.pm_chip_data = pmic;
	platform_set_drvdata(pdev, &pm8821_drvdata);

	/* Print out human readable version and revision names. */
	version = pm8xxx_get_version(pmic->dev);
	if (version == PM8XXX_VERSION_8821) {
		revision = pm8xxx_get_revision(pmic->dev);
		if (revision >= 0 && revision < ARRAY_SIZE(pm8821_rev_names))
			revision_name = pm8821_rev_names[revision];
		pr_info("PMIC version: PM8821 ver %s\n", revision_name);
	} else {
		WARN_ON(version != PM8XXX_VERSION_8821);
	}

	rc = pm8821_add_subdevices(pdata, pmic);
	if (rc) {
		pr_err("Cannot add subdevices rc=%d\n", rc);
		goto err;
	}

	return 0;

err:
	mfd_remove_devices(pmic->dev);
	platform_set_drvdata(pdev, NULL);
err_read_rev:
	kfree(pmic);
	return rc;
}

static int __devexit pm8821_remove(struct platform_device *pdev)
{
	struct pm8xxx_drvdata *drvdata;
	struct pm8821 *pmic = NULL;

	drvdata = platform_get_drvdata(pdev);
	if (drvdata)
		pmic = drvdata->pm_chip_data;
	if (pmic)
		mfd_remove_devices(pmic->dev);
	if (pmic->irq_chip) {
		pm8821_irq_exit(pmic->irq_chip);
		pmic->irq_chip = NULL;
	}
	platform_set_drvdata(pdev, NULL);
	kfree(pmic);

	return 0;
}

static struct platform_driver pm8821_driver = {
	.probe		= pm8821_probe,
	.remove		= __devexit_p(pm8821_remove),
	.driver		= {
		.name	= "pm8821-core",
		.owner	= THIS_MODULE,
	},
};

static int __init pm8821_init(void)
{
	return platform_driver_register(&pm8821_driver);
}
postcore_initcall(pm8821_init);

static void __exit pm8821_exit(void)
{
	platform_driver_unregister(&pm8821_driver);
}
module_exit(pm8821_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC 8821 core driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8821-core");
