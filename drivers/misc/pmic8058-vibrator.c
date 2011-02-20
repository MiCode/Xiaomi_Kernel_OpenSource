/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include "../staging/android/timed_output.h"

#define VIB_DRV			0x4A

#define VIB_DRV_SEL_MASK	0xf8
#define VIB_DRV_SEL_SHIFT	0x03
#define VIB_DRV_EN_MANUAL_MASK	0xfc

#define VIB_MAX_LEVEL_mV	3100
#define VIB_MIN_LEVEL_mV	1200

struct pmic8058_vib {
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	spinlock_t lock;
	struct work_struct work;

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
		rc = pm_runtime_resume(vib->dev);
		if (rc < 0)
			dev_dbg(vib->dev, "pm_runtime_resume failed\n");

		val = vib->reg_vib_drv;
		val |= ((vib->level << VIB_DRV_SEL_SHIFT) & VIB_DRV_SEL_MASK);
		rc = pmic8058_vib_write_u8(vib, val, VIB_DRV);
		if (rc < 0)
			return rc;
		vib->reg_vib_drv = val;
	} else {
		val = vib->reg_vib_drv;
		val &= ~VIB_DRV_SEL_MASK;
		rc = pmic8058_vib_write_u8(vib, val, VIB_DRV);
		if (rc < 0)
			return rc;
		vib->reg_vib_drv = val;

		rc = pm_runtime_suspend(vib->dev);
		if (rc < 0)
			dev_dbg(vib->dev, "pm_runtime_suspend failed\n");
	}
	__dump_vib_regs(vib, "vib_set_end");

	return rc;
}

static void pmic8058_vib_enable(struct timed_output_dev *dev, int value)
{
	struct pmic8058_vib *vib = container_of(dev, struct pmic8058_vib,
					 timed_dev);
	unsigned long flags;

retry:
	spin_lock_irqsave(&vib->lock, flags);
	if (hrtimer_try_to_cancel(&vib->vib_timer) < 0) {
		spin_unlock_irqrestore(&vib->lock, flags);
		cpu_relax();
		goto retry;
	}

	if (value == 0)
		vib->state = 0;
	else {
		value = (value > vib->pdata->max_timeout_ms ?
				 vib->pdata->max_timeout_ms : value);
		vib->state = 1;
		hrtimer_start(&vib->vib_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&vib->lock, flags);
	schedule_work(&vib->work);
}

static void pmic8058_vib_update(struct work_struct *work)
{
	struct pmic8058_vib *vib = container_of(work, struct pmic8058_vib,
					 work);

	pmic8058_vib_set(vib, vib->state);
}

static int pmic8058_vib_get_time(struct timed_output_dev *dev)
{
	struct pmic8058_vib *vib = container_of(dev, struct pmic8058_vib,
					 timed_dev);

	if (hrtimer_active(&vib->vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);
		return (int) ktime_to_us(r);
	} else
		return 0;
}

static enum hrtimer_restart pmic8058_vib_timer_func(struct hrtimer *timer)
{
	struct pmic8058_vib *vib = container_of(timer, struct pmic8058_vib,
					 vib_timer);
	vib->state = 0;
	schedule_work(&vib->work);
	return HRTIMER_NORESTART;
}

#ifdef CONFIG_PM
static int pmic8058_vib_suspend(struct device *dev)
{
	struct pmic8058_vib *vib = dev_get_drvdata(dev);

	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	/* turn-off vibrator */
	pmic8058_vib_set(vib, 0);
	return 0;
}

static struct dev_pm_ops pmic8058_vib_pm_ops = {
	.suspend = pmic8058_vib_suspend,
};
#endif

static int __devinit pmic8058_vib_probe(struct platform_device *pdev)

{
	struct pmic8058_vibrator_pdata *pdata = pdev->dev.platform_data;
	struct pmic8058_vib *vib;
	u8 val;
	int rc;

	struct pm8058_chip	*pm_chip;

	pm_chip = dev_get_drvdata(pdev->dev.parent);
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

	/* Enable runtime PM ops, start in ACTIVE mode */
	rc = pm_runtime_set_active(&pdev->dev);
	if (rc < 0)
		dev_dbg(&pdev->dev, "unable to set runtime pm state\n");
	pm_runtime_enable(&pdev->dev);

	vib->pm_chip	= pm_chip;
	vib->pdata	= pdata;
	vib->level	= pdata->level_mV / 100;
	vib->dev	= &pdev->dev;

	spin_lock_init(&vib->lock);
	INIT_WORK(&vib->work, pmic8058_vib_update);

	hrtimer_init(&vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_timer.function = pmic8058_vib_timer_func;

	vib->timed_dev.name = "vibrator";
	vib->timed_dev.get_time = pmic8058_vib_get_time;
	vib->timed_dev.enable = pmic8058_vib_enable;

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

	rc = timed_output_dev_register(&vib->timed_dev);
	if (rc < 0)
		goto err_read_vib;

	pmic8058_vib_enable(&vib->timed_dev, pdata->initial_vibrate_ms);

	platform_set_drvdata(pdev, vib);

	pm_runtime_set_suspended(&pdev->dev);
	return 0;

err_read_vib:
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	kfree(vib);
	return rc;
}

static int __devexit pmic8058_vib_remove(struct platform_device *pdev)
{
	struct pmic8058_vib *vib = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	cancel_work_sync(&vib->work);
	hrtimer_cancel(&vib->vib_timer);
	timed_output_dev_unregister(&vib->timed_dev);
	kfree(vib);

	return 0;
}

static struct platform_driver pmic8058_vib_driver = {
	.probe		= pmic8058_vib_probe,
	.remove		= __devexit_p(pmic8058_vib_remove),
	.driver		= {
		.name	= "pm8058-vib",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pmic8058_vib_pm_ops,
#endif
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
MODULE_DESCRIPTION("PMIC8058 vibrator driver");
MODULE_LICENSE("GPL v2");
