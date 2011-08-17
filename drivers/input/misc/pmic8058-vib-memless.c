/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pmic8058-vibrator.h>
#include <linux/mfd/pmic8058.h>
#include <linux/pm.h>
#include <linux/input.h>
#include <linux/slab.h>

#define VIB_DRV			0x4A

#define VIB_DRV_SEL_MASK	0xf8
#define VIB_DRV_SEL_SHIFT	0x03
#define VIB_DRV_EN_MANUAL_MASK	0xfc

#define VIB_MAX_LEVEL_mV	(3100)
#define VIB_MIN_LEVEL_mV	(1200)
#define VIB_MAX_LEVELS		(VIB_MAX_LEVEL_mV - VIB_MIN_LEVEL_mV)

#define MAX_FF_SPEED		0xff

struct pmic8058_vib {
	struct input_dev *info;
	spinlock_t lock;
	struct work_struct work;

	bool enabled;
	int speed;
	struct device *dev;
	struct pmic8058_vibrator_pdata *pdata;
	int state;
	int level;
	u8  reg_vib_drv;

	struct pm8058_chip	*pm_chip;
};

/* REVISIT: just for debugging, will be removed in final working version */
static void __dump_vib_regs(struct pmic8058_vib *vib, char *msg)
{
	u8 temp;

	dev_dbg(vib->dev, "%s\n", msg);

	pm8058_read(vib->pm_chip, VIB_DRV, &temp, 1);
	dev_dbg(vib->dev, "VIB_DRV - %X\n", temp);
}

static int pmic8058_vib_read_u8(struct pmic8058_vib *vib,
				 u8 *data, u16 reg)
{
	int rc;

	rc = pm8058_read(vib->pm_chip, reg, data, 1);
	if (rc < 0)
		dev_warn(vib->dev, "Error reading pmic8058: %X - ret %X\n",
				reg, rc);

	return rc;
}

static int pmic8058_vib_write_u8(struct pmic8058_vib *vib,
				 u8 data, u16 reg)
{
	int rc;

	rc = pm8058_write(vib->pm_chip, reg, &data, 1);
	if (rc < 0)
		dev_warn(vib->dev, "Error writing pmic8058: %X - ret %X\n",
				reg, rc);
	return rc;
}

static int pmic8058_vib_set(struct pmic8058_vib *vib, int on)
{
	int rc;
	u8 val;

	if (on) {
		val = vib->reg_vib_drv;
		val |= ((vib->level << VIB_DRV_SEL_SHIFT) & VIB_DRV_SEL_MASK);
		rc = pmic8058_vib_write_u8(vib, val, VIB_DRV);
		if (rc < 0)
			return rc;
		vib->reg_vib_drv = val;
		vib->enabled = 1;

	} else {
		val = vib->reg_vib_drv;
		val &= ~VIB_DRV_SEL_MASK;
		rc = pmic8058_vib_write_u8(vib, val, VIB_DRV);
		if (rc < 0)
			return rc;
		vib->reg_vib_drv = val;
		vib->enabled = 0;
	}
	__dump_vib_regs(vib, "vib_set_end");

	return rc;
}

static void pmic8058_work_handler(struct work_struct *work)
{
	u8 val;
	int rc;
	struct pmic8058_vib *info;

	info  = container_of(work, struct pmic8058_vib, work);

	rc = pmic8058_vib_read_u8(info, &val, VIB_DRV);
	if (rc < 0)
		return;

	/*
	 * Vibrator support voltage ranges from 1.2 to 3.1V, so
	 * scale the FF speed to these range.
	 */
	if (info->speed) {
		info->state = 1;
		info->level = ((VIB_MAX_LEVELS * info->speed) / MAX_FF_SPEED) +
						VIB_MIN_LEVEL_mV;
		info->level /= 100;
	} else {
		info->state = 0;
		info->level = VIB_MIN_LEVEL_mV / 100;
	}
	pmic8058_vib_set(info, info->state);
}

static int pmic8058_vib_play_effect(struct input_dev *dev, void *data,
		      struct ff_effect *effect)
{
	struct pmic8058_vib *info = input_get_drvdata(dev);

	info->speed = effect->u.rumble.strong_magnitude >> 8;
	if (!info->speed)
		info->speed = effect->u.rumble.weak_magnitude >> 9;
	schedule_work(&info->work);
	return 0;
}

static int __devinit pmic8058_vib_probe(struct platform_device *pdev)

{
	struct pmic8058_vibrator_pdata *pdata = pdev->dev.platform_data;
	struct pmic8058_vib *vib;
	u8 val;
	int rc;

	struct pm8058_chip	*pm_chip;

	pm_chip = dev_get_drvdata(pdev->parent.dev);
	if (pm_chip == NULL) {
		dev_err(&pdev->dev, "no parent data passed in\n");
		return -EFAULT;
	}

	if (!pdata)
		return -EINVAL;

	if (pdata->level_mV < VIB_MIN_LEVEL_mV ||
			 pdata->level_mV > VIB_MAX_LEVEL_mV)
		return -EINVAL;

	vib = kzalloc(sizeof(*vib), GFP_KERNEL);
	if (!vib)
		return -ENOMEM;

	vib->pm_chip	= pm_chip;
	vib->enabled	= 0;
	vib->pdata	= pdata;
	vib->level	= pdata->level_mV / 100;
	vib->dev	= &pdev->dev;

	spin_lock_init(&vib->lock);
	INIT_WORK(&vib->work, pmic8058_work_handler);

	vib->info = input_allocate_device();

	if (vib->info == NULL) {
		dev_err(&pdev->dev, "couldn't allocate input device\n");
		return -ENOMEM;
	}

	input_set_drvdata(vib->info, vib);

	vib->info->name = "pmic8058:vibrator";
	vib->info->id.version = 1;
	vib->info->dev.parent = pdev->dev.parent;

	__set_bit(FF_RUMBLE, vib->info->ffbit);
	__dump_vib_regs(vib, "boot_vib_default");

	/* operate in manual mode */
	rc = pmic8058_vib_read_u8(vib, &val, VIB_DRV);
	if (rc < 0)
		goto err_read_vib;
	val &= ~VIB_DRV_EN_MANUAL_MASK;
	rc = pmic8058_vib_write_u8(vib, val, VIB_DRV);
	if (rc < 0)
		goto err_read_vib;

	vib->reg_vib_drv = val;

	rc = input_ff_create_memless(vib->info, NULL, pmic8058_vib_play_effect);
	if (rc < 0) {
		dev_dbg(&pdev->dev, "couldn't register vibrator to FF\n");
		goto create_memless_err;
	}

	platform_set_drvdata(pdev, vib);

	rc = input_register_device(vib->info);
	if (rc < 0) {
		dev_dbg(&pdev->dev, "couldn't register input device\n");
		goto reg_err;
	}

	return 0;

reg_err:
	input_ff_destroy(vib->info);
create_memless_err:
	input_free_device(vib->info);
err_read_vib:
	kfree(vib);
	return rc;
}

static int __devexit pmic8058_vib_remove(struct platform_device *pdev)
{
	struct pmic8058_vib *vib = platform_get_drvdata(pdev);

	cancel_work_sync(&vib->work);
	if (vib->enabled)
		pmic8058_vib_set(vib, 0);

	input_unregister_device(vib->info);
	kfree(vib);

	return 0;
}

static struct platform_driver pmic8058_vib_driver = {
	.probe		= pmic8058_vib_probe,
	.remove		= __devexit_p(pmic8058_vib_remove),
	.driver		= {
		.name	= "pm8058-vib",
		.owner	= THIS_MODULE,
	},
};

static int __init pmic8058_vib_init(void)
{
	return platform_driver_register(&pmic8058_vib_driver);
}
module_init(pmic8058_vib_init);

static void __exit pmic8058_vib_exit(void)
{
	platform_driver_unregister(&pmic8058_vib_driver);
}
module_exit(pmic8058_vib_exit);

MODULE_ALIAS("platform:pmic8058_vib");
MODULE_DESCRIPTION("PMIC8058 vibrator driver memless framework");
MODULE_LICENSE("GPL v2");
