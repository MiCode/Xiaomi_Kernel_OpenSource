/*
 * as3722_wdt.c -- AS3722 Watchdog Timer.
 *
 * Watchdog timer for AS3722 PMIC.
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * Author: Bibek Basu <bbasu@nvidia.com>
 * based on Palmas_wdt driver (c) Laxman Dewangan , 2013
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/as3722.h>
#include <linux/mfd/as3722-plat.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/watchdog.h>

static bool nowayout = WATCHDOG_NOWAYOUT;

struct as3722_wdt {
	struct watchdog_device wdt_dev;
	struct device *dev;
	struct as3722 *as3722;
	int timeout;
	int irq;
};

static irqreturn_t as3722_wdt_irq(int irq, void *data)
{
	struct as3722_wdt *wdt = data;
	int ret;

	ret = as3722_update_bits(wdt->as3722,
			AS3722_WATCHDOG_SOFTWARE_SIGNAL_REG,
			AS3722_WATCHDOG_SW_SIG, AS3722_WATCHDOG_SW_SIG);
	if (ret < 0)
		dev_err(wdt->dev, "WATCHDOG kick failed: %d\n", ret);
	dev_info(wdt->dev, "WDT interrupt occur\n");
	return IRQ_HANDLED;
}

static int as3722_wdt_start(struct watchdog_device *wdt_dev)
{
	struct as3722_wdt *wdt = watchdog_get_drvdata(wdt_dev);
	int ret;

	ret = as3722_update_bits(wdt->as3722, AS3722_WATCHDOG_CONTROL_REG,
			AS3722_WATCHDOG_ON, AS3722_WATCHDOG_ON);
	if (ret < 0) {
		dev_err(wdt->dev, "WATCHDOG update failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int as3722_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct as3722_wdt *wdt = watchdog_get_drvdata(wdt_dev);
	int ret;

	ret = as3722_update_bits(wdt->as3722, AS3722_WATCHDOG_CONTROL_REG,
			AS3722_WATCHDOG_ON, 0);
	if (ret < 0) {
		dev_err(wdt->dev, "WATCHDOG update failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int as3722_wdt_set_timeout(struct watchdog_device *wdt_dev,
		unsigned int timeout)
{
	struct as3722_wdt *wdt = watchdog_get_drvdata(wdt_dev);
	int ret;
	unsigned int val = timeout - 1;

	ret = as3722_update_bits(wdt->as3722, AS3722_WATCHDOG_TIMER_REG,
			AS3722_WATCHDOG_TIMER_MAX, val);
	if (ret < 0) {
		dev_err(wdt->dev, "WATCHDOG update failed: %d\n", ret);
		return ret;
	}
	wdt->timeout = timeout;
	return 0;
}

static const struct watchdog_info as3722_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE,
	.identity = "AS3722 Watchdog",
};

static const struct watchdog_ops as3722_wdt_ops = {
	.owner = THIS_MODULE,
	.start = as3722_wdt_start,
	.stop = as3722_wdt_stop,
	.set_timeout = as3722_wdt_set_timeout,
};

static int as3722_wdt_probe(struct platform_device *pdev)
{
	struct as3722 *as3722 = dev_get_drvdata(pdev->dev.parent);
	struct as3722_platform_data *pdata = as3722->dev->platform_data;
	struct as3722_wdt *wdt;
	struct watchdog_device *wdt_dev;
	int ret;

	wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdt->dev = &pdev->dev;
	wdt->as3722 = as3722;
	wdt->irq = as3722_irq_get_virq(wdt->as3722, AS3722_IRQ_WATCHDOG);
	wdt_dev = &wdt->wdt_dev;

	wdt_dev->info = &as3722_wdt_info;
	wdt_dev->ops = &as3722_wdt_ops;
	wdt_dev->timeout = 128;
	wdt_dev->min_timeout = 8;
	wdt_dev->max_timeout = 128;
	watchdog_set_nowayout(wdt_dev, nowayout);
	watchdog_set_drvdata(wdt_dev, wdt);
	platform_set_drvdata(pdev, wdt);

	ret = request_threaded_irq(wdt->irq, NULL, as3722_wdt_irq,
			IRQF_ONESHOT | IRQF_EARLY_RESUME,
			dev_name(&pdev->dev), wdt);
	if (ret < 0) {
		dev_err(&pdev->dev, "request IRQ:%d failed, err = %d\n",
			 wdt->irq, ret);
		return ret;
	}

	ret = watchdog_register_device(wdt_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "watchdog registration failed: %d\n", ret);
		return ret;
	}

	/* set mode */
	if (pdata && (pdata->watchdog_timer_mode >= 0)) {
		ret = as3722_update_bits(wdt->as3722,
			AS3722_WATCHDOG_CONTROL_REG,
			AS3722_WATCHDOG_MODE_MASK, pdata->watchdog_timer_mode);
		if (ret < 0) {
			dev_err(wdt->dev, "WATCHDOG update mode failed: %d\n",
					ret);
			goto scrub;
		}
	}

	/* configure timer */
	if (pdata && (pdata->watchdog_timer_initial_period > 0)) {
		ret = as3722_wdt_set_timeout(wdt_dev,
				pdata->watchdog_timer_initial_period);
		if (ret < 0) {
			dev_err(wdt->dev, "wdt set timeout failed: %d\n", ret);
			goto scrub;
		}
		ret = as3722_wdt_start(wdt_dev);
		if (ret < 0) {
			dev_err(wdt->dev,
					"wdt start failed: %d\n", ret);
			goto scrub;
		}
	}

	device_set_wakeup_capable(&pdev->dev, 1);
	return 0;
scrub:
	free_irq(wdt->irq, wdt);
	watchdog_unregister_device(&wdt->wdt_dev);
	return ret;
}

static int as3722_wdt_remove(struct platform_device *pdev)
{
	struct as3722_wdt *wdt = platform_get_drvdata(pdev);

	as3722_wdt_stop(&wdt->wdt_dev);
	watchdog_unregister_device(&wdt->wdt_dev);
	free_irq(wdt->irq, wdt);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int as3722_wdt_suspend(struct device *dev)
{
	struct as3722_wdt *wdt = dev_get_drvdata(dev);
	int ret;

	if (device_may_wakeup(dev)) {
		enable_irq_wake(wdt->irq);
	} else if (wdt->timeout > 0) {
		ret = as3722_wdt_stop(&wdt->wdt_dev);
		if (ret < 0)
			dev_err(wdt->dev, "wdt stop failed: %d\n", ret);
	}
	return 0;
}

static int as3722_wdt_resume(struct device *dev)
{
	struct as3722_wdt *wdt = dev_get_drvdata(dev);
	int ret;

	if (device_may_wakeup(dev)) {
		disable_irq_wake(wdt->irq);
	} else if (wdt->timeout > 0) {
		ret = as3722_wdt_start(&wdt->wdt_dev);
		if (ret < 0)
			dev_err(wdt->dev, "wdt start failed: %d\n", ret);
	}
	return 0;
}
#endif

static const struct dev_pm_ops as3722_wdt_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(as3722_wdt_suspend, as3722_wdt_resume)
};

static struct platform_driver as3722_wdt_driver = {
	.driver	= {
		.name	= "as3722-wdt",
		.owner	= THIS_MODULE,
		.pm = &as3722_wdt_pm_ops,
	},
	.probe	= as3722_wdt_probe,
	.remove	= as3722_wdt_remove,
};

static int __init as3722_wdt_init(void)
{
	return platform_driver_register(&as3722_wdt_driver);
}
subsys_initcall(as3722_wdt_init);

static void __exit as3722_wdt_exit(void)
{
	platform_driver_unregister(&as3722_wdt_driver);
}
module_exit(as3722_wdt_exit);

MODULE_ALIAS("platform:as3722-wdt");
MODULE_DESCRIPTION("AMS AS3722 watchdog timer driver");
MODULE_AUTHOR("Bibek Basu <bbasu@nvidia.com>");
MODULE_LICENSE("GPL v2");
