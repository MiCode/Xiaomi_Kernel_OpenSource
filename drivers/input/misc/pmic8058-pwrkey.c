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
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mfd/pmic8058.h>
#include <linux/pmic8058-pwrkey.h>
#include <linux/log2.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#define PON_CNTL_1	0x1C
#define PON_CNTL_PULL_UP BIT(7)
#define PON_CNTL_TRIG_DELAY_MASK (0x7)

struct pmic8058_pwrkey {
	struct input_dev *pwr;
	int key_press_irq;
	int key_release_irq;
	struct pm8058_chip	*pm_chip;
	struct hrtimer timer;
	bool key_pressed;
	bool pressed_first;
	struct pmic8058_pwrkey_pdata *pdata;
	spinlock_t lock;
};

static enum hrtimer_restart pmic8058_pwrkey_timer(struct hrtimer *timer)
{
	unsigned long flags;
	struct pmic8058_pwrkey *pwrkey = container_of(timer,
						struct pmic8058_pwrkey,	timer);

	spin_lock_irqsave(&pwrkey->lock, flags);
	pwrkey->key_pressed = true;

	input_report_key(pwrkey->pwr, KEY_POWER, 1);
	input_sync(pwrkey->pwr);
	spin_unlock_irqrestore(&pwrkey->lock, flags);

	return HRTIMER_NORESTART;
}

static irqreturn_t pwrkey_press_irq(int irq, void *_pwrkey)
{
	struct pmic8058_pwrkey *pwrkey = _pwrkey;
	struct pmic8058_pwrkey_pdata *pdata = pwrkey->pdata;
	unsigned long flags;

	spin_lock_irqsave(&pwrkey->lock, flags);
	if (pwrkey->pressed_first) {
		/*
		 * If pressed_first flag is set already then release interrupt
		 * has occured first. Events are handled in the release IRQ so
		 * return.
		 */
		pwrkey->pressed_first = false;
		spin_unlock_irqrestore(&pwrkey->lock, flags);
		return IRQ_HANDLED;
	} else {
		pwrkey->pressed_first = true;
		/*no pwrkey time duration, means no end key simulation*/
		if (!pwrkey->pdata->pwrkey_time_ms) {
			input_report_key(pwrkey->pwr, KEY_POWER, 1);
			input_sync(pwrkey->pwr);
			spin_unlock_irqrestore(&pwrkey->lock, flags);
			return IRQ_HANDLED;
		}

		input_report_key(pwrkey->pwr, KEY_END, 1);
		input_sync(pwrkey->pwr);

		hrtimer_start(&pwrkey->timer,
				ktime_set(pdata->pwrkey_time_ms / 1000,
				(pdata->pwrkey_time_ms % 1000) * 1000000),
				HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&pwrkey->lock, flags);

	return IRQ_HANDLED;
}

static irqreturn_t pwrkey_release_irq(int irq, void *_pwrkey)
{
	struct pmic8058_pwrkey *pwrkey = _pwrkey;
	unsigned long flags;

	spin_lock_irqsave(&pwrkey->lock, flags);
	if (pwrkey->pressed_first) {
		pwrkey->pressed_first = false;
		/* no pwrkey time, means no delay in pwr key reporting */
		if (!pwrkey->pdata->pwrkey_time_ms) {
			input_report_key(pwrkey->pwr, KEY_POWER, 0);
			input_sync(pwrkey->pwr);
			spin_unlock_irqrestore(&pwrkey->lock, flags);
			return IRQ_HANDLED;
		}

		hrtimer_cancel(&pwrkey->timer);

		if (pwrkey->key_pressed) {
			pwrkey->key_pressed = false;
			input_report_key(pwrkey->pwr, KEY_POWER, 0);
			input_sync(pwrkey->pwr);
		}

		input_report_key(pwrkey->pwr, KEY_END, 0);
		input_sync(pwrkey->pwr);
	} else {
		/*
		 * Set this flag true so that in the subsequent interrupt of
		 * press we can know release interrupt came first
		 */
		pwrkey->pressed_first = true;
		/* no pwrkey time, means no delay in pwr key reporting */
		if (!pwrkey->pdata->pwrkey_time_ms) {
			input_report_key(pwrkey->pwr, KEY_POWER, 1);
			input_sync(pwrkey->pwr);
			input_report_key(pwrkey->pwr, KEY_POWER, 0);
			input_sync(pwrkey->pwr);
			spin_unlock_irqrestore(&pwrkey->lock, flags);
			return IRQ_HANDLED;
		}
		input_report_key(pwrkey->pwr, KEY_END, 1);
		input_sync(pwrkey->pwr);
		input_report_key(pwrkey->pwr, KEY_END, 0);
		input_sync(pwrkey->pwr);
	}
	spin_unlock_irqrestore(&pwrkey->lock, flags);

	return IRQ_HANDLED;
}

#ifdef CONFIG_PM
static int pmic8058_pwrkey_suspend(struct device *dev)
{
	struct pmic8058_pwrkey *pwrkey = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		enable_irq_wake(pwrkey->key_press_irq);
		enable_irq_wake(pwrkey->key_release_irq);
	}

	return 0;
}

static int pmic8058_pwrkey_resume(struct device *dev)
{
	struct pmic8058_pwrkey *pwrkey = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		disable_irq_wake(pwrkey->key_press_irq);
		disable_irq_wake(pwrkey->key_release_irq);
	}

	return 0;
}

static struct dev_pm_ops pm8058_pwr_key_pm_ops = {
	.suspend	= pmic8058_pwrkey_suspend,
	.resume		= pmic8058_pwrkey_resume,
};
#endif

static int __devinit pmic8058_pwrkey_probe(struct platform_device *pdev)
{
	struct input_dev *pwr;
	int key_release_irq = platform_get_irq(pdev, 0);
	int key_press_irq = platform_get_irq(pdev, 1);
	int err;
	unsigned int delay;
	u8 pon_cntl;
	struct pmic8058_pwrkey *pwrkey;
	struct pmic8058_pwrkey_pdata *pdata = pdev->dev.platform_data;
	struct pm8058_chip	*pm_chip;

	pm_chip = dev_get_drvdata(pdev->dev.parent);
	if (pm_chip == NULL) {
		dev_err(&pdev->dev, "no parent data passed in\n");
		return -EFAULT;
	}

	if (!pdata) {
		dev_err(&pdev->dev, "power key platform data not supplied\n");
		return -EINVAL;
	}

	if (pdata->kpd_trigger_delay_us > 62500) {
		dev_err(&pdev->dev, "invalid pwr key trigger delay\n");
		return -EINVAL;
	}

	if (pdata->pwrkey_time_ms &&
	     (pdata->pwrkey_time_ms < 500 || pdata->pwrkey_time_ms > 1000)) {
		dev_err(&pdev->dev, "invalid pwr key time supplied\n");
		return -EINVAL;
	}

	pwrkey = kzalloc(sizeof(*pwrkey), GFP_KERNEL);
	if (!pwrkey)
		return -ENOMEM;

	pwrkey->pm_chip = pm_chip;
	pwrkey->pdata   = pdata;
	pwrkey->pressed_first = false;
	/* Enable runtime PM ops, start in ACTIVE mode */
	err = pm_runtime_set_active(&pdev->dev);
	if (err < 0)
		dev_dbg(&pdev->dev, "unable to set runtime pm state\n");
	pm_runtime_enable(&pdev->dev);

	pwr = input_allocate_device();
	if (!pwr) {
		dev_dbg(&pdev->dev, "Can't allocate power button\n");
		err = -ENOMEM;
		goto free_pwrkey;
	}

	input_set_capability(pwr, EV_KEY, KEY_POWER);
	input_set_capability(pwr, EV_KEY, KEY_END);

	pwr->name = "pmic8058_pwrkey";
	pwr->phys = "pmic8058_pwrkey/input0";
	pwr->dev.parent = &pdev->dev;

	delay = (pdata->kpd_trigger_delay_us << 10) / USEC_PER_SEC;
	delay = 1 + ilog2(delay);

	err = pm8058_read(pwrkey->pm_chip, PON_CNTL_1, &pon_cntl, 1);
	if (err < 0) {
		dev_err(&pdev->dev, "failed reading PON_CNTL_1 err=%d\n", err);
		goto free_input_dev;
	}


	pon_cntl &= ~PON_CNTL_TRIG_DELAY_MASK;
	pon_cntl |= (delay & PON_CNTL_TRIG_DELAY_MASK);
	pon_cntl |= (pdata->pull_up ? PON_CNTL_PULL_UP : ~PON_CNTL_PULL_UP);
	err = pm8058_write(pwrkey->pm_chip, PON_CNTL_1, &pon_cntl, 1);
	if (err < 0) {
		dev_err(&pdev->dev, "failed writing PON_CNTL_1 err=%d\n", err);
		goto free_input_dev;
	}

	hrtimer_init(&pwrkey->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pwrkey->timer.function = pmic8058_pwrkey_timer;

	spin_lock_init(&pwrkey->lock);

	err = input_register_device(pwr);
	if (err) {
		dev_dbg(&pdev->dev, "Can't register power key: %d\n", err);
		goto free_input_dev;
	}

	pwrkey->key_press_irq = key_press_irq;
	pwrkey->key_release_irq = key_release_irq;
	pwrkey->pwr = pwr;

	platform_set_drvdata(pdev, pwrkey);

	/* Check if power-key is pressed at boot up */
	err = pm8058_irq_get_rt_status(pwrkey->pm_chip, key_press_irq);
	if (err < 0) {
		dev_err(&pdev->dev, "Key-press status at boot failed rc=%d\n",
									err);
		goto unreg_input_dev;
	}
	if (err) {
		if (!pwrkey->pdata->pwrkey_time_ms)
			input_report_key(pwrkey->pwr, KEY_POWER, 1);
		else
			input_report_key(pwrkey->pwr, KEY_END, 1);
		input_sync(pwrkey->pwr);
		pwrkey->pressed_first = true;
	}

	err = request_threaded_irq(key_press_irq, NULL, pwrkey_press_irq,
			 IRQF_TRIGGER_RISING, "pmic8058_pwrkey_press", pwrkey);
	if (err < 0) {
		dev_dbg(&pdev->dev, "Can't get %d IRQ for pwrkey: %d\n",
				 key_press_irq, err);
		goto unreg_input_dev;
	}

	err = request_threaded_irq(key_release_irq, NULL, pwrkey_release_irq,
			 IRQF_TRIGGER_RISING, "pmic8058_pwrkey_release",
				 pwrkey);
	if (err < 0) {
		dev_dbg(&pdev->dev, "Can't get %d IRQ for pwrkey: %d\n",
				 key_release_irq, err);

		goto free_press_irq;
	}

	device_init_wakeup(&pdev->dev, pdata->wakeup);

	return 0;

free_press_irq:
	free_irq(key_press_irq, NULL);
unreg_input_dev:
	input_unregister_device(pwr);
	pwr = NULL;
free_input_dev:
	input_free_device(pwr);
free_pwrkey:
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	kfree(pwrkey);
	return err;
}

static int __devexit pmic8058_pwrkey_remove(struct platform_device *pdev)
{
	struct pmic8058_pwrkey *pwrkey = platform_get_drvdata(pdev);
	int key_release_irq = platform_get_irq(pdev, 0);
	int key_press_irq = platform_get_irq(pdev, 1);

	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	device_init_wakeup(&pdev->dev, 0);

	free_irq(key_press_irq, pwrkey);
	free_irq(key_release_irq, pwrkey);
	input_unregister_device(pwrkey->pwr);
	kfree(pwrkey);

	return 0;
}

static struct platform_driver pmic8058_pwrkey_driver = {
	.probe		= pmic8058_pwrkey_probe,
	.remove		= __devexit_p(pmic8058_pwrkey_remove),
	.driver		= {
		.name	= "pm8058-pwrkey",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pm8058_pwr_key_pm_ops,
#endif
	},
};

static int __init pmic8058_pwrkey_init(void)
{
	return platform_driver_register(&pmic8058_pwrkey_driver);
}
module_init(pmic8058_pwrkey_init);

static void __exit pmic8058_pwrkey_exit(void)
{
	platform_driver_unregister(&pmic8058_pwrkey_driver);
}
module_exit(pmic8058_pwrkey_exit);

MODULE_ALIAS("platform:pmic8058_pwrkey");
MODULE_DESCRIPTION("PMIC8058 Power Key");
MODULE_LICENSE("GPL v2");
