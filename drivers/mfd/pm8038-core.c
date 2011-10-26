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
#include <linux/mfd/pm8xxx/pm8038.h>
#include <linux/mfd/pm8xxx/core.h>

#define REG_HWREV		0x002  /* PMIC4 revision */
#define REG_HWREV_2		0x0E8  /* PMIC4 revision 2 */

#define REG_MPP_BASE		0x050
#define REG_RTC_BASE		0x11D
#define REG_IRQ_BASE            0x1BB

#define PM8038_VERSION_MASK	0xFFF0
#define PM8038_VERSION_VALUE	0x09F0
#define PM8038_REVISION_MASK	0x000F

#define REG_PM8038_PON_CNTRL_3	0x01D
#define PM8038_RESTART_REASON_MASK	0x07

#define SINGLE_IRQ_RESOURCE(_name, _irq) \
{ \
	.name	= _name, \
	.start	= _irq, \
	.end	= _irq, \
	.flags	= IORESOURCE_IRQ, \
}

struct pm8038 {
	struct device			*dev;
	struct pm_irq_chip		*irq_chip;
	u32				rev_registers;
};

static int pm8038_readb(const struct device *dev, u16 addr, u8 *val)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, val, 1);
}

static int pm8038_writeb(const struct device *dev, u16 addr, u8 val)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, &val, 1);
}

static int pm8038_read_buf(const struct device *dev, u16 addr, u8 *buf,
									int cnt)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return msm_ssbi_read(pmic->dev->parent, addr, buf, cnt);
}

static int pm8038_write_buf(const struct device *dev, u16 addr, u8 *buf,
									int cnt)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return msm_ssbi_write(pmic->dev->parent, addr, buf, cnt);
}

static int pm8038_read_irq_stat(const struct device *dev, int irq)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return pm8xxx_get_irq_stat(pmic->irq_chip, irq);
}

static enum pm8xxx_version pm8038_get_version(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;
	enum pm8xxx_version version = -ENODEV;

	if ((pmic->rev_registers & PM8038_VERSION_MASK) == PM8038_VERSION_VALUE)
		version = PM8XXX_VERSION_8038;

	return version;
}

static int pm8038_get_revision(const struct device *dev)
{
	const struct pm8xxx_drvdata *pm8038_drvdata = dev_get_drvdata(dev);
	const struct pm8038 *pmic = pm8038_drvdata->pm_chip_data;

	return pmic->rev_registers & PM8038_REVISION_MASK;
}

static struct pm8xxx_drvdata pm8038_drvdata = {
	.pmic_readb		= pm8038_readb,
	.pmic_writeb		= pm8038_writeb,
	.pmic_read_buf		= pm8038_read_buf,
	.pmic_write_buf		= pm8038_write_buf,
	.pmic_read_irq_stat	= pm8038_read_irq_stat,
	.pmic_get_version	= pm8038_get_version,
	.pmic_get_revision	= pm8038_get_revision,
};

static const struct resource gpio_cell_resources[] __devinitconst = {
	[0] = {
		.start = PM8038_IRQ_BLOCK_BIT(PM8038_GPIO_BLOCK_START, 0),
		.end   = PM8038_IRQ_BLOCK_BIT(PM8038_GPIO_BLOCK_START, 0)
			+ PM8038_NR_GPIOS - 1,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell gpio_cell __devinitdata = {
	.name		= PM8XXX_GPIO_DEV_NAME,
	.id		= -1,
	.resources	= gpio_cell_resources,
	.num_resources	= ARRAY_SIZE(gpio_cell_resources),
};

static const struct resource mpp_cell_resources[] __devinitconst = {
	{
		.start	= PM8038_IRQ_BLOCK_BIT(PM8038_MPP_BLOCK_START, 0),
		.end	= PM8038_IRQ_BLOCK_BIT(PM8038_MPP_BLOCK_START, 0)
			  + PM8038_NR_MPPS - 1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct mfd_cell mpp_cell __devinitdata = {
	.name		= PM8XXX_MPP_DEV_NAME,
	.id		= 1,
	.resources	= mpp_cell_resources,
	.num_resources	= ARRAY_SIZE(mpp_cell_resources),
};

static const struct resource rtc_cell_resources[] __devinitconst = {
	[0] = SINGLE_IRQ_RESOURCE(NULL, PM8038_RTC_ALARM_IRQ),
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
	SINGLE_IRQ_RESOURCE(NULL, PM8038_PWRKEY_REL_IRQ),
	SINGLE_IRQ_RESOURCE(NULL, PM8038_PWRKEY_PRESS_IRQ),
};

static struct mfd_cell pwrkey_cell __devinitdata = {
	.name		= PM8XXX_PWRKEY_DEV_NAME,
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_pwrkey),
	.resources	= resources_pwrkey,
};

static struct mfd_cell pwm_cell __devinitdata = {
	.name           = PM8XXX_PWM_DEV_NAME,
	.id             = -1,
};

static struct mfd_cell misc_cell __devinitdata = {
	.name           = PM8XXX_MISC_DEV_NAME,
	.id             = -1,
};

static struct mfd_cell debugfs_cell __devinitdata = {
	.name		= "pm8xxx-debug",
	.id		= 0,
	.platform_data	= "pm8038-dbg",
	.pdata_size	= sizeof("pm8038-dbg"),
};


static int __devinit
pm8038_add_subdevices(const struct pm8038_platform_data *pdata,
		      struct pm8038 *pmic)
{
	int ret = 0, irq_base = 0;
	struct pm_irq_chip *irq_chip;

	if (pdata->irq_pdata) {
		pdata->irq_pdata->irq_cdata.nirqs = PM8038_NR_IRQS;
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
		pdata->gpio_pdata->gpio_cdata.ngpios = PM8038_NR_GPIOS;
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
		pdata->mpp_pdata->core_data.nmpps = PM8038_NR_MPPS;
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

	ret = mfd_add_devices(pmic->dev, 0, &debugfs_cell, 1, NULL, irq_base);
	if (ret) {
		pr_err("Failed to add debugfs subdevice ret=%d\n", ret);
		goto bail;
	}

	return 0;
bail:
	if (pmic->irq_chip) {
		pm8xxx_irq_exit(pmic->irq_chip);
		pmic->irq_chip = NULL;
	}
	return ret;
}

static const char * const pm8038_restart_reason[] = {
	[0] = "Unknown",
	[1] = "Triggered from CBL (external charger)",
	[2] = "Triggered from KPD (power key press)",
	[3] = "Triggered from CHG (usb charger insertion)",
	[4] = "Triggered from SMPL (sudden momentary power loss)",
	[5] = "Triggered from RTC (real time clock)",
	[6] = "Triggered by Hard Reset",
	[7] = "Triggered by General Purpose Trigger",
};

static const char * const pm8038_rev_names[] = {
	[PM8XXX_REVISION_8038_TEST]	= "test",
	[PM8XXX_REVISION_8038_1p0]	= "1.0",
	[PM8XXX_REVISION_8038_2p0]	= "2.0",
	[PM8XXX_REVISION_8038_2p1]	= "2.1",
};

static int __devinit pm8038_probe(struct platform_device *pdev)
{
	const struct pm8038_platform_data *pdata = pdev->dev.platform_data;
	const char *revision_name = "unknown";
	struct pm8038 *pmic;
	enum pm8xxx_version version;
	int revision;
	int rc;
	u8 val;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	pmic = kzalloc(sizeof(struct pm8038), GFP_KERNEL);
	if (!pmic) {
		pr_err("Cannot alloc pm8038 struct\n");
		return -ENOMEM;
	}

	/* Read PMIC chip revision */
	rc = msm_ssbi_read(pdev->dev.parent, REG_HWREV, &val, sizeof(val));
	if (rc) {
		pr_err("Failed to read hw rev reg %d:rc=%d\n", REG_HWREV, rc);
		goto err_read_rev;
	}
	pr_info("PMIC revision 1: PM8038 rev %02X\n", val);
	pmic->rev_registers = val;

	/* Read PMIC chip revision 2 */
	rc = msm_ssbi_read(pdev->dev.parent, REG_HWREV_2, &val, sizeof(val));
	if (rc) {
		pr_err("Failed to read hw rev 2 reg %d:rc=%d\n",
			REG_HWREV_2, rc);
		goto err_read_rev;
	}
	pr_info("PMIC revision 2: PM8038 rev %02X\n", val);
	pmic->rev_registers |= val << BITS_PER_BYTE;

	pmic->dev = &pdev->dev;
	pm8038_drvdata.pm_chip_data = pmic;
	platform_set_drvdata(pdev, &pm8038_drvdata);

	/* Print out human readable version and revision names. */
	version = pm8xxx_get_version(pmic->dev);
	if (version == PM8XXX_VERSION_8038) {
		revision = pm8xxx_get_revision(pmic->dev);
		if (revision >= 0 && revision < ARRAY_SIZE(pm8038_rev_names))
			revision_name = pm8038_rev_names[revision];
		pr_info("PMIC version: PM8038 ver %s\n", revision_name);
	} else {
		WARN_ON(version != PM8XXX_VERSION_8038);
	}

	/* Log human readable restart reason */
	rc = msm_ssbi_read(pdev->dev.parent, REG_PM8038_PON_CNTRL_3, &val, 1);
	if (rc) {
		pr_err("Cannot read restart reason rc=%d\n", rc);
		goto err_read_rev;
	}
	val &= PM8038_RESTART_REASON_MASK;
	pr_info("PMIC Restart Reason: %s\n", pm8038_restart_reason[val]);

	rc = pm8038_add_subdevices(pdata, pmic);
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

static int __devexit pm8038_remove(struct platform_device *pdev)
{
	struct pm8xxx_drvdata *drvdata;
	struct pm8038 *pmic = NULL;

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

		kfree(pmic);
	}

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver pm8038_driver = {
	.probe		= pm8038_probe,
	.remove		= __devexit_p(pm8038_remove),
	.driver		= {
		.name	= PM8038_CORE_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8038_init(void)
{
	return platform_driver_register(&pm8038_driver);
}
postcore_initcall(pm8038_init);

static void __exit pm8038_exit(void)
{
	platform_driver_unregister(&pm8038_driver);
}
module_exit(pm8038_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC 8038 core driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8038-core");
