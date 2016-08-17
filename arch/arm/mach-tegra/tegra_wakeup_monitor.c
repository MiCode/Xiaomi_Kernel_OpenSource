/*
 * arch/arm/mach-tegra/tegra_wakeup_monitor.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <mach/tegra_wakeup_monitor.h>

#include "pm-irq.h"

struct tegra_wakeup_monitor {
		struct tegra_wakeup_monitor_platform_data *pdata;
		struct notifier_block pm_notifier;
		struct platform_device *pdev;
		bool wow_enabled;
		bool monitor_enable;
		int wakeup_source;
		struct semaphore suspend_prepare_sem;
};

static ssize_t show_monitor_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", twm->monitor_enable ? 1 : 0);
}

static ssize_t store_monitor_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int enable;
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &enable) != 1 || enable < 0 || enable > 1)
		return -EINVAL;

	dev_info(dev, "wakeup moniter: monitor enable = %d\n", enable);
	twm->monitor_enable = enable;

	return count;
}

static DEVICE_ATTR(monitor_enable, S_IRUSR | S_IWUSR, show_monitor_enable,
		store_monitor_enable);

static int set_wow(struct tegra_wakeup_monitor *twm, bool enable)
{
	char *envp[2];

	if (enable)
		envp[0] = TEGRA_WOW_WAKEUP_ENABLE;
	else
		envp[0] = TEGRA_WOW_WAKEUP_DISABLE;
	envp[1] = NULL;
	/* Sent out a uevent to broadcast wow enable change*/
	kobject_uevent_env(&twm->pdev->dev.kobj, KOBJ_CHANGE, envp);
	dev_info(&twm->pdev->dev,
		"wakeup moniter: set_wow = %d\n", (int)enable);
	return 0;
}

static ssize_t show_wow_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", twm->wow_enabled ? 1 : 0);
}

static ssize_t store_wow_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int enable;
	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &enable) != 1 || enable < 0 || enable > 1)
		return -EINVAL;

	dev_info(dev, "wakeup moniter: wow_enable = %d\n", enable);
	set_wow(twm, enable);
	twm->wow_enabled = enable;

	return count;
}

static DEVICE_ATTR(wow_enable, S_IRUSR | S_IWUSR, show_wow_enable,
		store_wow_enable);

/* if the wakeup monitor is enabled, it will receive a command before suspend */
static ssize_t store_cmd(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{

	struct tegra_wakeup_monitor *twm = dev_get_drvdata(dev);

	if (strncmp(buf, "0", 1))
		return -EINVAL;

	dev_info(dev, "wakeup moniter: get done cmd\n");
	up(&twm->suspend_prepare_sem);

	return count;
}

static DEVICE_ATTR(cmd, S_IWUSR, NULL, store_cmd);

static int tegra_wakeup_monitor_pm_notifier(struct notifier_block *notifier,
				   unsigned long pm_event, void *unused)
{
	struct tegra_wakeup_monitor *twm =
	    container_of(notifier, struct tegra_wakeup_monitor, pm_notifier);
	char *envp[2];
	unsigned long const timeout =
			msecs_to_jiffies(TEGRA_WAKEUP_MONITOR_CMD_TIMEOUT_MS);

	envp[1] = NULL;
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (twm->monitor_enable) {
			dev_info(&twm->pdev->dev, "enter suspend_prepare\n");
			switch (twm->wakeup_source) {
			case TEGRA_WAKEUP_SOURCE_WIFI:
				envp[0] = TEGRA_SUSPEND_PREPARE_UEVENT_WIFI;
				break;
			default:
				envp[0] = TEGRA_SUSPEND_PREPARE_UEVENT_OTHERS;
			}
			/* send out a uevent to boardcast suspend prepare */
			kobject_uevent_env(&twm->pdev->dev.kobj, KOBJ_CHANGE,
						envp);
			/* clean the wakeup source flag */
			twm->wakeup_source = TEGRA_WAKEUP_SOURCE_OTHERS;

			/* waiting for cmd feedback */
			if (down_timeout(&twm->suspend_prepare_sem,
				timeout) != 0)
				dev_err(&twm->pdev->dev, "wakeup monitor: cmd time out\n");
		}
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		if (twm->monitor_enable) {
			dev_info(&twm->pdev->dev, "enter post_suspend\n");
			if (twm->wow_enabled == false) {
				set_wow(twm, true);
				twm->wow_enabled = true;
			}
			envp[0] = TEGRA_POST_SUSPEND_UEVENT;
			/* send out a uevent to boardcast post suspend*/
			kobject_uevent_env(&twm->pdev->dev.kobj, KOBJ_CHANGE,
						envp);
		}
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}
static inline int twm_init(struct tegra_wakeup_monitor *twm,
					struct platform_device *pdev)
{
	struct tegra_wakeup_monitor_platform_data *pdata =
		pdev->dev.platform_data;
	int ret = 0;

	twm->pdata = pdata;
	twm->pdev  = pdev;

	/* create sysfs node */
	ret = device_create_file(&pdev->dev, &dev_attr_monitor_enable);
	if (ret)
		goto error;

	ret =  device_create_file(&pdev->dev, &dev_attr_wow_enable);
	if (ret)
		goto error;

	ret =  device_create_file(&pdev->dev, &dev_attr_cmd);
	if (ret)
		goto error;

	twm->monitor_enable = false;
	twm->wow_enabled = true;
	twm->wakeup_source = TEGRA_WAKEUP_SOURCE_OTHERS;
	twm->pm_notifier.notifier_call = tegra_wakeup_monitor_pm_notifier;
	sema_init(&(twm->suspend_prepare_sem), 1);

	ret = register_pm_notifier(&twm->pm_notifier);

	return ret;

error:
	device_remove_file(&pdev->dev, &dev_attr_cmd);
	device_remove_file(&pdev->dev, &dev_attr_wow_enable);
	device_remove_file(&pdev->dev, &dev_attr_monitor_enable);

	return ret;
}

static int tegra_wakeup_monitor_probe(struct platform_device *pdev)
{
	struct tegra_wakeup_monitor_platform_data *pdata =
	    pdev->dev.platform_data;
	struct tegra_wakeup_monitor *twm;
	int ret = 0;

	if (!pdata) {
		dev_dbg(&pdev->dev, "platform_data not available\n");
		return -EINVAL;
	}

	twm = devm_kzalloc(&pdev->dev,
		sizeof(struct tegra_wakeup_monitor), GFP_KERNEL);
	if (!twm) {
		dev_dbg(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	twm_init(twm, pdev);
	dev_set_drvdata(&pdev->dev, twm);

	return ret;
}

static int __exit tegra_wakeup_monitor_remove(struct platform_device *pdev)
{
	struct tegra_wakeup_monitor *twm = platform_get_drvdata(pdev);

	unregister_pm_notifier(&twm->pm_notifier);

	device_remove_file(&pdev->dev, &dev_attr_monitor_enable);
	device_remove_file(&pdev->dev, &dev_attr_wow_enable);
	device_remove_file(&pdev->dev, &dev_attr_cmd);

	kfree(twm);
	return 0;
}

static int tegra_wakeup_monitor_resume(struct platform_device *pdev)
{
	struct tegra_wakeup_monitor *twm = platform_get_drvdata(pdev);

	/* read and save wake status */
	u64 wake_status = tegra_read_pmc_wake_status();

	if (twm->pdata->wifi_wakeup_source != -1 &&
		(wake_status & BIT(twm->pdata->wifi_wakeup_source)))
		twm->wakeup_source = TEGRA_WAKEUP_SOURCE_WIFI;
	else
		twm->wakeup_source = TEGRA_WAKEUP_SOURCE_OTHERS;
	pr_info("wakeup monitor: wakeup source =%d\n", twm->wakeup_source);
	return 0;
}


static struct platform_driver tegra_wakeup_monitor_driver = {
	.driver = {
		   .name = "tegra_wakeup_monitor",
		   .owner = THIS_MODULE,
		   },
	.probe = tegra_wakeup_monitor_probe,
	.remove = __exit_p(tegra_wakeup_monitor_remove),
#ifdef CONFIG_PM
	.resume = tegra_wakeup_monitor_resume,
#endif
};

static int __init tegra_wakeup_monitor_init(void)
{
	return platform_driver_register(&tegra_wakeup_monitor_driver);
}

subsys_initcall(tegra_wakeup_monitor_init);

static void __exit tegra_wakeup_monitor_exit(void)
{
	platform_driver_unregister(&tegra_wakeup_monitor_driver);
}

module_exit(tegra_wakeup_monitor_exit);

MODULE_DESCRIPTION("Tegra Wakeup Monitor driver");
MODULE_LICENSE("GPL");
