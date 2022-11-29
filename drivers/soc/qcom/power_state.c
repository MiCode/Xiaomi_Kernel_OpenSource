// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#define pr_fmt(msg) "power_state: " msg

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/ioctl.h>
#include <linux/kdev_t.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/pm_wakeup.h>
#include "linux/power_state.h"

#define POWER_STATE "power_state"
#define STRING_LEN 32

static struct class *ps_class;
struct device *ps_ret;
static  struct cdev ps_cdev;
static  dev_t ps_dev;
struct kobject *kobj_ref;
static struct wakeup_source *notify_ws;
static const char *adsp_subsys = "adsp";
static const char *mdsp_subsys = "modem";
static int ignore_ssr;

enum power_states {
	ACTIVE,
	DEEPSLEEP,
	HIBERNATE,
};

static char * const power_state[] = {
	[ACTIVE] = "active",
	[DEEPSLEEP] = "deepsleep",
	[HIBERNATE] = "hibernate",
};

enum power_states current_state = ACTIVE;

struct ps_event {
	enum ps_event_type event;
};

static ssize_t state_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = strlen(power_state[current_state]);

	return scnprintf(buf, len + 2, "%s\n", power_state[current_state]);
}

struct kobj_attribute power_state_attr =
				__ATTR_RO_MODE(state, 0440);

static int send_uevent(struct ps_event *pse)
{
	char event_string[STRING_LEN];
	char *envp[2] = { event_string, NULL };

	snprintf(event_string, ARRAY_SIZE(event_string), "POWER_STATE_EVENT = %d", pse->event);
	return kobject_uevent_env(&ps_ret->kobj, KOBJ_CHANGE, envp);
}

static int subsys_suspend(const char *subsystem, uint32_t *ui_obj_msg)
{
	int ret = 0;
	uint32_t state = *ui_obj_msg;

	switch (state) {
	case SUBSYS_DEEPSLEEP:
		/*Add call for Subsys Suspend*/
		break;
	case SUBSYS_HIBERNATE:
		ignore_ssr = 1;
		/*Add call for Subsys Shutdown*/
		ignore_ssr = 0;
		break;
	default:
		pr_err("%s: Invalid subsys suspend state\n", __func__);
		ret = -1;
		break;
	}
	return ret;
}

static int subsys_resume(const char *subsystem, uint32_t *ui_obj_msg)
{
	int ret = 0;
	uint32_t state = *ui_obj_msg;

	switch (state) {
	case SUBSYS_DEEPSLEEP:
		/*Add call for Subsys Power Up*/
		break;
	case SUBSYS_HIBERNATE:
		ignore_ssr = 1;
		/*Add call for Subsys Restart*/
		ignore_ssr = 0;
		break;
	default:
		pr_err("%s: Invalid subsys suspend exit state\n", __func__);
		ret = -1;
		break;
	}
	return ret;
}

static int powerstate_pm_notifier(struct notifier_block *nb, unsigned long event, void *unused)
{
	struct ps_event pse;

	switch (event) {

	case PM_SUSPEND_PREPARE:
		pr_debug("%s: PM_SUSPEND_PREPARE\n", __func__);

		if (current_state == DEEPSLEEP) {
			pr_info("%s: Deep Sleep entry\n", __func__);
			pm_set_suspend_via_firmware();
		} else {
			pr_debug("%s: RBSC Suspend\n", __func__);
		}
		break;

	case PM_POST_SUSPEND:
		pr_debug("%s: PM_POST_SUSPEND\n", __func__);

		if (pm_suspend_via_firmware()) {
			pr_info("%s: Deep Sleep exit\n", __func__);
			/*Take Wakeup Source*/
			__pm_stay_awake(notify_ws);
			pse.event = EXIT_DEEP_SLEEP;
			send_uevent(&pse);
		} else {
			pr_debug("%s: RBSC Resume\n", __func__);
		}
		break;

	case PM_HIBERNATION_PREPARE:
		pr_debug("%s: PM_HIBERNATION_PREPARE\n", __func__);

		pr_info("%s: Hibernate entry\n", __func__);
		/*Swap Partition & Drop Caches*/
		pse.event = PREPARE_FOR_HIBERNATION;
		send_uevent(&pse);

		current_state = HIBERNATE;
		break;

	case PM_RESTORE_PREPARE:
		pr_debug("%s: PM_RESTORE_PREPARE\n", __func__);
		pr_info("%s: Hibernate restore\n", __func__);
		break;

	case PM_POST_HIBERNATION:
		pr_debug("%s: PM_POST_HIBERNATION\n", __func__);
	case PM_POST_RESTORE:
		pr_debug("%s: PM_POST_RESTORE\n", __func__);
		pr_info("%s: Hibernate exit\n", __func__);
		pse.event = EXIT_HIBERNATE;
		send_uevent(&pse);
		break;

	default:
		WARN_ONCE(1, "Default case: PM Notifier\n");
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block powerstate_pm_nb = {
	.notifier_call = powerstate_pm_notifier,
};

static int ps_probe(struct platform_device *pdev)
{
	int ret;

	ret = register_pm_notifier(&powerstate_pm_nb);
	if (ret) {
		dev_err(&pdev->dev, " %s: power state notif error %d\n", __func__, ret);
		return ret;
	}

	notify_ws = wakeup_source_register(&pdev->dev, "power-state");
	if (!notify_ws) {
		unregister_pm_notifier(&powerstate_pm_nb);
		return -ENOMEM;
	}

	return 0;
}

static const struct of_device_id power_state_of_match[] = {
	{ .compatible = "qcom,power-state", },
	{ }
};
MODULE_DEVICE_TABLE(of, power_state_of_match);

static struct platform_driver ps_driver = {
	.probe = ps_probe,
	.driver = {
		.name = "power-state",
		.of_match_table = power_state_of_match,
	},
};

static int ps_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ps_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long ps_ioctl(struct file *filp, unsigned int ui_power_state_cmd, unsigned long arg)
{
	int ret = 0;
	uint32_t ui_obj_msg;

	switch (ui_power_state_cmd) {

	case LPM_ACTIVE:
		pr_debug("%s: Changed to Active\n", __func__);
		if (pm_suspend_via_firmware()) {
			pm_suspend_clear_flags();
			/*Release Wakeup Source*/
			__pm_relax(notify_ws);
		}
		current_state = ACTIVE;
		break;

	case ENTER_DEEPSLEEP:
		pr_debug("%s: Enter Deep Sleep\n", __func__);
		current_state = DEEPSLEEP;
		break;

	case ENTER_HIBERNATE:
		pr_debug("%s: Enter Hibernate\n", __func__);
		current_state = HIBERNATE;
		break;

	case MODEM_SUSPEND:
		pr_debug("%s: Initiating Modem Shutdown\n", __func__);
		if (copy_from_user(&ui_obj_msg, (void __user *)arg,
					sizeof(ui_obj_msg))) {
			pr_err("%s: The copy from user failed - modem suspend\n", __func__);
			ret = -EFAULT;
		}

		/*To Modem subsys*/
		ret = subsys_suspend(mdsp_subsys, &ui_obj_msg);
		if (ret != 0)
			pr_err("%s: Modem failed to Shutdown\n", __func__);
		else
			pr_debug("%s: Modem Shutdown Complete\n", __func__);
		break;

	case ADSP_SUSPEND:
		pr_debug("%s: Initiating ADSP Shutdown\n", __func__);
		if (copy_from_user(&ui_obj_msg, (void __user *)arg,
					sizeof(ui_obj_msg))) {
			pr_err("%s: The copy from user failed - adsp suspend\n", __func__);
			ret = -EFAULT;
		}

		/*To ADSP subsys*/
		ret = subsys_suspend(adsp_subsys, &ui_obj_msg);
		if (ret != 0)
			pr_err("%s: ADSP failed to Shutdown\n", __func__);
		else
			pr_debug("%s: ADSP Shutdown Complete\n", __func__);
		break;

	case MODEM_EXIT:
		pr_debug("%s: Loading Modem Subsystem\n", __func__);
		if (copy_from_user(&ui_obj_msg, (void __user *)arg,
					sizeof(ui_obj_msg))) {
			pr_err("%s: The copy from user failed - modem suspend exit\n", __func__);
			ret = -EFAULT;
		}

		/*To Modem subsys*/
		ret = subsys_resume(mdsp_subsys, &ui_obj_msg);
		if (ret != 0) {
			pr_err("%s: Modem Load Failed\n", __func__);
			ret = subsystem_restart(mdsp_subsys);
		} else
			pr_debug("%s: Modem Successfully Brought up\n", __func__);
		break;

	case ADSP_EXIT:
		pr_debug("%s: Loading ADSP Subsystem\n", __func__);
		if (copy_from_user(&ui_obj_msg, (void __user *)arg,
					sizeof(ui_obj_msg))) {
			pr_err("%s: The copy from user failed - adsp suspend exit\n", __func__);
			ret = -EFAULT;
		}

		/*To ADSP subsys*/
		ret = subsys_resume(adsp_subsys, &ui_obj_msg);
		if (ret != 0) {
			pr_err("%s: ADSP Load Failed\n", __func__);
			ret = subsystem_restart(adsp_subsys);
		} else
			pr_debug("%s: ADSP Successfully Brought up\n", __func__);
		break;

	default:
		ret = -ENOIOCTLCMD;
		pr_err("%s: Default\n", __func__);
		break;
	}
	return ret;
}

static const struct file_operations ps_fops = {
	.owner = THIS_MODULE,
	.open = ps_open,
	.release = ps_release,
	.unlocked_ioctl = ps_ioctl,
};

static int __init init_power_state_func(void)
{
	int ret;

	ret = alloc_chrdev_region(&ps_dev, 0, 1, POWER_STATE);
	if (ret  < 0) {
		pr_err("%s: Alloc Chrdev Region Failed %d\n", __func__, ret);
		return ret;
	}

	cdev_init(&ps_cdev, &ps_fops);
	ret = cdev_add(&ps_cdev, ps_dev, 1);
	if (ret < 0) {
		unregister_chrdev_region(ps_dev, 1);
		pr_err("%s: Device Registration Failed\n", __func__);
		return ret;
	}

	ps_class = class_create(THIS_MODULE, POWER_STATE);
	if (IS_ERR_OR_NULL(ps_class)) {
		cdev_del(&ps_cdev);
		unregister_chrdev_region(ps_dev, 1);
		pr_err("%s: Class Creation Failed\n", __func__);
		return PTR_ERR(ps_class);
	}

	ps_ret = device_create(ps_class, NULL, ps_dev, NULL, POWER_STATE);
	if (IS_ERR_OR_NULL(ps_ret)) {
		class_destroy(ps_class);
		cdev_del(&ps_cdev);
		unregister_chrdev_region(ps_dev, 1);
		pr_err("%s: Device Creation Failed\n", __func__);
		return PTR_ERR(ps_ret);
	}

	if (platform_driver_register(&ps_driver))
		pr_err("%s: Failed to Register ps_driver\n", __func__);

	kobj_ref = kobject_create_and_add("power_state", kernel_kobj);
	/*Creating sysfs file for power_state*/
	if (sysfs_create_file(kobj_ref, &power_state_attr.attr)) {
		pr_err("%s: Cannot create sysfs file\n", __func__);
		kobject_put(kobj_ref);
		sysfs_remove_file(kernel_kobj, &power_state_attr.attr);
	}

	return 0;
}

static void __exit exit_power_state_func(void)
{
	kobject_put(kobj_ref);
	sysfs_remove_file(kernel_kobj, &power_state_attr.attr);
	device_destroy(ps_class, ps_dev);
	class_destroy(ps_class);
	cdev_del(&ps_cdev);
	unregister_chrdev_region(ps_dev, 1);
	platform_driver_unregister(&ps_driver);
}

module_init(init_power_state_func);
module_exit(exit_power_state_func);
MODULE_LICENSE("GPL v2");
