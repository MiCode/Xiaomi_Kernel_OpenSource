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
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/log2.h>

#include <linux/mfd/pm8xxx/core.h>
#include <linux/input/pmic8xxx-pwrkey.h>

#define PON_CNTL_1 0x1C
#define PON_CNTL_PULL_UP BIT(7)
#define PON_CNTL_TRIG_DELAY_MASK (0x7)

/**
 * struct pmic8xxx_pwrkey - pmic8xxx pwrkey information
 * @key_press_irq: key press irq number
 * @pdata: platform data
 */
struct pmic8xxx_pwrkey {
	struct input_dev *pwr;
	int key_press_irq;
	int key_release_irq;
	bool press;
	const struct pm8xxx_pwrkey_platform_data *pdata;
};

static irqreturn_t pwrkey_press_irq(int irq, void *_pwrkey)
{
	struct pmic8xxx_pwrkey *pwrkey = _pwrkey;

	if (pwrkey->press == true) {
		pwrkey->press = false;
		return IRQ_HANDLED;
	} else {
		pwrkey->press = true;
	}

	input_report_key(pwrkey->pwr, KEY_POWER, 1);
	input_sync(pwrkey->pwr);

	return IRQ_HANDLED;
}

static irqreturn_t pwrkey_release_irq(int irq, void *_pwrkey)
{
	struct pmic8xxx_pwrkey *pwrkey = _pwrkey;

	if (pwrkey->press == false) {
		input_report_key(pwrkey->pwr, KEY_POWER, 1);
		input_sync(pwrkey->pwr);
		pwrkey->press = true;
	} else {
		pwrkey->press = false;
	}

	input_report_key(pwrkey->pwr, KEY_POWER, 0);
	input_sync(pwrkey->pwr);

	return IRQ_HANDLED;
}

#ifdef CONFIG_PM_SLEEP
static int pmic8xxx_pwrkey_suspend(struct device *dev)
{
	struct pmic8xxx_pwrkey *pwrkey = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		enable_irq_wake(pwrkey->key_press_irq);
		enable_irq_wake(pwrkey->key_release_irq);
	}

	return 0;
}

static int pmic8xxx_pwrkey_resume(struct device *dev)
{
	struct pmic8xxx_pwrkey *pwrkey = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		disable_irq_wake(pwrkey->key_press_irq);
		disable_irq_wake(pwrkey->key_release_irq);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(pm8xxx_pwr_key_pm_ops,
		pmic8xxx_pwrkey_suspend, pmic8xxx_pwrkey_resume);

static int pmic8xxx_set_pon1(struct device *dev, u32 debounce_us, bool pull_up)
{
	int err;
	u32 delay;
	u8 pon_cntl;

	/* Valid range of pwr key trigger delay is 1/64 sec to 2 seconds. */
	if (debounce_us > USEC_PER_SEC * 2 ||
		debounce_us < USEC_PER_SEC / 64) {
		dev_err(dev, "invalid power key trigger delay\n");
		return -EINVAL;
	}

	delay = (debounce_us << 6) / USEC_PER_SEC;
	delay = ilog2(delay);

	err = pm8xxx_readb(dev->parent, PON_CNTL_1, &pon_cntl);
	if (err < 0) {
		dev_err(dev, "failed reading PON_CNTL_1 err=%d\n", err);
		return err;
	}

	pon_cntl &= ~PON_CNTL_TRIG_DELAY_MASK;
	pon_cntl |= (delay & PON_CNTL_TRIG_DELAY_MASK);

	if (pull_up)
		pon_cntl |= PON_CNTL_PULL_UP;
	else
		pon_cntl &= ~PON_CNTL_PULL_UP;

	err = pm8xxx_writeb(dev->parent, PON_CNTL_1, pon_cntl);
	if (err < 0) {
		dev_err(dev, "failed writing PON_CNTL_1 err=%d\n", err);
		return err;
	}

	return 0;
}

static ssize_t pmic8xxx_debounce_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct pmic8xxx_pwrkey *pwrkey = dev_get_drvdata(dev);
	int err;
	unsigned long val;

	if (size > 8)
		return -EINVAL;

	err = kstrtoul(buf, 10, &val);
	if (err < 0)
		return err;

	err = pmic8xxx_set_pon1(dev, val, pwrkey->pdata->pull_up);
	if (err < 0)
		return err;

	return size;
}

static DEVICE_ATTR(debounce_us, 0664, NULL, pmic8xxx_debounce_store);

static int pmic8xxx_pwrkey_probe(struct platform_device *pdev)
{
	struct input_dev *pwr;
	int key_release_irq = platform_get_irq(pdev, 0);
	int key_press_irq = platform_get_irq(pdev, 1);
	int err;
	struct pmic8xxx_pwrkey *pwrkey;
	const struct pm8xxx_pwrkey_platform_data *pdata =
					dev_get_platdata(&pdev->dev);

	if (!pdata) {
		dev_err(&pdev->dev, "power key platform data not supplied\n");
		return -EINVAL;
	}

	pwrkey = kzalloc(sizeof(*pwrkey), GFP_KERNEL);
	if (!pwrkey)
		return -ENOMEM;

	pwrkey->pdata = pdata;

	pwr = input_allocate_device();
	if (!pwr) {
		dev_dbg(&pdev->dev, "Can't allocate power button\n");
		err = -ENOMEM;
		goto free_pwrkey;
	}

	input_set_capability(pwr, EV_KEY, KEY_POWER);

	pwr->name = "pmic8xxx_pwrkey";
	pwr->phys = "pmic8xxx_pwrkey/input0";
	pwr->dev.parent = &pdev->dev;

	err = pmic8xxx_set_pon1(&pdev->dev,
			pdata->kpd_trigger_delay_us, pdata->pull_up);
	if (err) {
		dev_dbg(&pdev->dev, "Can't set PON CTRL1 register: %d\n", err);
		goto free_input_dev;
	}

	err = input_register_device(pwr);
	if (err) {
		dev_dbg(&pdev->dev, "Can't register power key: %d\n", err);
		goto free_input_dev;
	}

	pwrkey->key_press_irq = key_press_irq;
	pwrkey->key_release_irq = key_release_irq;
	pwrkey->pwr = pwr;

	platform_set_drvdata(pdev, pwrkey);

	/* check power key status during boot */
	err = pm8xxx_read_irq_stat(pdev->dev.parent, key_press_irq);
	if (err < 0) {
		dev_err(&pdev->dev, "reading irq status failed\n");
		goto unreg_input_dev;
	}
	pwrkey->press = !!err;

	if (pwrkey->press) {
		input_report_key(pwrkey->pwr, KEY_POWER, 1);
		input_sync(pwrkey->pwr);
	}

	err = request_any_context_irq(key_press_irq, pwrkey_press_irq,
		IRQF_TRIGGER_RISING, "pmic8xxx_pwrkey_press", pwrkey);
	if (err < 0) {
		dev_dbg(&pdev->dev, "Can't get %d IRQ for pwrkey: %d\n",
				 key_press_irq, err);
		goto unreg_input_dev;
	}

	err = request_any_context_irq(key_release_irq, pwrkey_release_irq,
		 IRQF_TRIGGER_RISING, "pmic8xxx_pwrkey_release", pwrkey);
	if (err < 0) {
		dev_dbg(&pdev->dev, "Can't get %d IRQ for pwrkey: %d\n",
				 key_release_irq, err);

		goto free_press_irq;
	}

	err = device_create_file(&pdev->dev, &dev_attr_debounce_us);
	if (err < 0) {
		dev_err(&pdev->dev,
				"dev file creation for debounce failed: %d\n",
				err);
		goto free_rel_irq;
	}

	device_init_wakeup(&pdev->dev, pdata->wakeup);

	return 0;

free_rel_irq:
	free_irq(key_release_irq, pwrkey);
free_press_irq:
	free_irq(key_press_irq, pwrkey);
unreg_input_dev:
	platform_set_drvdata(pdev, NULL);
	input_unregister_device(pwr);
	pwr = NULL;
free_input_dev:
	input_free_device(pwr);
free_pwrkey:
	kfree(pwrkey);
	return err;
}

static int pmic8xxx_pwrkey_remove(struct platform_device *pdev)
{
	struct pmic8xxx_pwrkey *pwrkey = platform_get_drvdata(pdev);
	int key_release_irq = platform_get_irq(pdev, 0);
	int key_press_irq = platform_get_irq(pdev, 1);

	device_init_wakeup(&pdev->dev, 0);

	device_remove_file(&pdev->dev, &dev_attr_debounce_us);
	free_irq(key_press_irq, pwrkey);
	free_irq(key_release_irq, pwrkey);
	input_unregister_device(pwrkey->pwr);
	platform_set_drvdata(pdev, NULL);
	kfree(pwrkey);

	return 0;
}

static struct platform_driver pmic8xxx_pwrkey_driver = {
	.probe		= pmic8xxx_pwrkey_probe,
	.remove		= pmic8xxx_pwrkey_remove,
	.driver		= {
		.name	= PM8XXX_PWRKEY_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &pm8xxx_pwr_key_pm_ops,
	},
};
module_platform_driver(pmic8xxx_pwrkey_driver);

MODULE_ALIAS("platform:pmic8xxx_pwrkey");
MODULE_DESCRIPTION("PMIC8XXX Power Key driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Trilok Soni <tsoni@codeaurora.org>");
