/* Copyright (c) 2010,2011 The Linux Foundation. All rights reserved.
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
 * Qualcomm PM8XXX UPL driver
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/upl.h>

/* PMIC8XXX UPL registers */
#define SSBI_REG_UPL_CTRL		0x17B
#define SSBI_REG_UPL_TRUTHTABLE1	0x17C
#define SSBI_REG_UPL_TRUTHTABLE2	0x17D

struct pm8xxx_upl_device {
	struct device		*dev;
	struct mutex		upl_mutex;
#if defined(CONFIG_DEBUG_FS)
	struct dentry		*dent;
#endif
};
static struct pm8xxx_upl_device *upl_dev;

/* APIs */

/*
 * pm8xxx_upl_request - request a handle to access UPL device
 */
struct pm8xxx_upl_device *pm8xxx_upl_request(void)
{
	return upl_dev;
}
EXPORT_SYMBOL(pm8xxx_upl_request);

/*
 * pm8xxx_upl_read_truthtable - read value currently stored in UPL truth table
 *
 * @upldev: the UPL device
 * @truthtable: value read from UPL truth table
 */
int pm8xxx_upl_read_truthtable(struct pm8xxx_upl_device *upldev,
				u16 *truthtable)
{
	int rc = 0;
	u8 table[2];

	if (upldev == NULL || IS_ERR(upldev))
		return -EINVAL;

	mutex_lock(&upldev->upl_mutex);

	rc = pm8xxx_readb(upldev->dev->parent, SSBI_REG_UPL_TRUTHTABLE1,
							&(table[0]));
	if (rc) {
		pr_err("%s: FAIL pm8xxx_readb(0x%X)=0x%02X: rc=%d\n",
			__func__, SSBI_REG_UPL_TRUTHTABLE1, table[0], rc);
		goto upl_read_done;
	}

	rc = pm8xxx_readb(upldev->dev->parent, SSBI_REG_UPL_TRUTHTABLE2,
							&(table[1]));
	if (rc)
		pr_err("%s: FAIL pm8xxx_readb(0x%X)=0x%02X: rc=%d\n",
			__func__, SSBI_REG_UPL_TRUTHTABLE2, table[1], rc);
upl_read_done:
	mutex_unlock(&upldev->upl_mutex);
	*truthtable = (((u16)table[1]) << 8) | table[0];
	return rc;
}
EXPORT_SYMBOL(pm8xxx_upl_read_truthtable);

/*
 * pm8xxx_upl_writes_truthtable - write value into UPL truth table
 *
 * @upldev: the UPL device
 * @truthtable: value written to UPL truth table
 *
 * Each bit in parameter "truthtable" corresponds to the UPL output for a given
 * set of input pin values. For example, if the input pins have the following
 * values: A=1, B=1, C=1, D=0, then the UPL would output the value of bit 14
 * (0b1110) in parameter "truthtable".
 */
int pm8xxx_upl_write_truthtable(struct pm8xxx_upl_device *upldev,
				u16 truthtable)
{
	int rc = 0;
	u8 table[2];

	if (upldev == NULL || IS_ERR(upldev))
		return -EINVAL;

	table[0] = truthtable & 0xFF;
	table[1] = (truthtable >> 8) & 0xFF;

	mutex_lock(&upldev->upl_mutex);

	rc = pm8xxx_writeb(upldev->dev->parent, SSBI_REG_UPL_TRUTHTABLE1,
								table[0]);
	if (rc) {
		pr_err("%s: FAIL pm8xxx_writeb(0x%X)=0x%04X: rc=%d\n",
			__func__, SSBI_REG_UPL_TRUTHTABLE1, table[0], rc);
		goto upl_write_done;
	}

	rc = pm8xxx_writeb(upldev->dev->parent, SSBI_REG_UPL_TRUTHTABLE2,
								table[1]);
	if (rc)
		pr_err("%s: FAIL pm8xxx_writeb(0x%X)=0x%04X: rc=%d\n",
			__func__, SSBI_REG_UPL_TRUTHTABLE2, table[1], rc);
upl_write_done:
	mutex_unlock(&upldev->upl_mutex);
	return rc;
}
EXPORT_SYMBOL(pm8xxx_upl_write_truthtable);

/*
 * pm8xxx_upl_config - configure UPL I/O settings and UPL enable/disable
 *
 * @upldev: the UPL device
 * @mask: setting mask to configure
 * @flags: setting flags
 */
int pm8xxx_upl_config(struct pm8xxx_upl_device *upldev, u32 mask, u32 flags)
{
	int rc;
	u8 upl_ctrl, m, f;

	if (upldev == NULL || IS_ERR(upldev))
		return -EINVAL;

	mutex_lock(&upldev->upl_mutex);

	rc = pm8xxx_readb(upldev->dev->parent, SSBI_REG_UPL_CTRL, &upl_ctrl);
	if (rc) {
		pr_err("%s: FAIL pm8xxx_readb(0x%X)=0x%02X: rc=%d\n",
			__func__, SSBI_REG_UPL_CTRL, upl_ctrl, rc);
		goto upl_config_done;
	}

	m = mask & 0x00ff;
	f = flags & 0x00ff;
	upl_ctrl &= ~m;
	upl_ctrl |= m & f;

	rc = pm8xxx_writeb(upldev->dev->parent, SSBI_REG_UPL_CTRL, upl_ctrl);
	if (rc)
		pr_err("%s: FAIL pm8xxx_writeb(0x%X)=0x%02X: rc=%d\n",
			__func__, SSBI_REG_UPL_CTRL, upl_ctrl, rc);
upl_config_done:
	mutex_unlock(&upldev->upl_mutex);
	return rc;
}
EXPORT_SYMBOL(pm8xxx_upl_config);

#if defined(CONFIG_DEBUG_FS)

static int truthtable_set(void *data, u64 val)
{
	int rc;

	rc = pm8xxx_upl_write_truthtable(data, val);
	if (rc)
		pr_err("%s: pm8xxx_upl_write_truthtable: rc=%d, "
			"truthtable=0x%llX\n", __func__, rc, val);
	return rc;
}

static int truthtable_get(void *data, u64 *val)
{
	int rc;
	u16 truthtable;

	rc = pm8xxx_upl_read_truthtable(data, &truthtable);
	if (rc)
		pr_err("%s: pm8xxx_upl_read_truthtable: rc=%d, "
			"truthtable=0x%X\n", __func__, rc, truthtable);
	if (val)
		*val = truthtable;

	return rc;
}

DEFINE_SIMPLE_ATTRIBUTE(upl_truthtable_fops, truthtable_get,
			truthtable_set, "0x%04llX\n");

/* enter values as 0xMMMMFFFF where MMMM is the mask and FFFF is the flags */
static int control_set(void *data, u64 val)
{
	u8 mask, flags;
	int rc;

	flags = val & 0xFFFF;
	mask = (val >> 16) & 0xFFFF;

	rc = pm8xxx_upl_config(data, mask, flags);
	if (rc)
		pr_err("%s: pm8xxx_upl_config: rc=%d, mask = 0x%X, "
			"flags = 0x%X\n", __func__, rc, mask, flags);
	return rc;
}

static int control_get(void *data, u64 *val)
{
	struct pm8xxx_upl_device *upldev;
	int rc = 0;
	u8 ctrl;

	upldev = data;

	mutex_lock(&upldev->upl_mutex);

	rc = pm8xxx_readb(upldev->dev->parent, SSBI_REG_UPL_CTRL, &ctrl);
	if (rc)
		pr_err("%s: FAIL pm8xxx_readb(): rc=%d (ctrl=0x%02X)\n",
		       __func__, rc, ctrl);

	mutex_unlock(&upldev->upl_mutex);

	*val = ctrl;

	return rc;
}

DEFINE_SIMPLE_ATTRIBUTE(upl_control_fops, control_get,
			control_set, "0x%02llX\n");

static int pm8xxx_upl_debug_init(struct pm8xxx_upl_device *upldev)
{
	struct dentry *dent;
	struct dentry *temp;

	dent = debugfs_create_dir("pm8xxx-upl", NULL);
	if (dent == NULL || IS_ERR(dent)) {
		pr_err("%s: ERR debugfs_create_dir: dent=0x%X\n",
					__func__, (unsigned)dent);
		return -ENOMEM;
	}

	temp = debugfs_create_file("truthtable", S_IRUSR | S_IWUSR, dent,
					upldev, &upl_truthtable_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("%s: ERR debugfs_create_file: dent=0x%X\n",
					__func__, (unsigned)dent);
		goto debug_error;
	}

	temp = debugfs_create_file("control", S_IRUSR | S_IWUSR, dent,
					upldev, &upl_control_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("%s: ERR debugfs_create_file: dent=0x%X\n",
					__func__, (unsigned)dent);
		goto debug_error;
	}

	upldev->dent = dent;
	return 0;

debug_error:
	debugfs_remove_recursive(dent);
	return -ENOMEM;
}

static int __devexit pm8xxx_upl_debug_remove(struct pm8xxx_upl_device *upldev)
{
	debugfs_remove_recursive(upldev->dent);
	return 0;
}

#endif /* CONFIG_DEBUG_FS */

static int __devinit pm8xxx_upl_probe(struct platform_device *pdev)
{
	struct pm8xxx_upl_device	*upldev;

	upldev = kzalloc(sizeof *upldev, GFP_KERNEL);
	if (upldev == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&upldev->upl_mutex);

	upl_dev = upldev;
	upldev->dev = &pdev->dev;
	platform_set_drvdata(pdev, upldev);

#if defined(CONFIG_DEBUG_FS)
	pm8xxx_upl_debug_init(upl_dev);
#endif
	pr_notice("%s: OK\n", __func__);
	return 0;
}

static int __devexit pm8xxx_upl_remove(struct platform_device *pdev)
{
	struct pm8xxx_upl_device *upldev = platform_get_drvdata(pdev);

#if defined(CONFIG_DEBUG_FS)
	pm8xxx_upl_debug_remove(upldev);
#endif

	platform_set_drvdata(pdev, NULL);
	kfree(upldev);
	pr_notice("%s: OK\n", __func__);

	return 0;
}

static struct platform_driver pm8xxx_upl_driver = {
	.probe		= pm8xxx_upl_probe,
	.remove		= __devexit_p(pm8xxx_upl_remove),
	.driver		= {
		.name = PM8XXX_UPL_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init pm8xxx_upl_init(void)
{
	return platform_driver_register(&pm8xxx_upl_driver);
}

static void __exit pm8xxx_upl_exit(void)
{
	platform_driver_unregister(&pm8xxx_upl_driver);
}

module_init(pm8xxx_upl_init);
module_exit(pm8xxx_upl_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PM8XXX UPL driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8XXX_UPL_DEV_NAME);
