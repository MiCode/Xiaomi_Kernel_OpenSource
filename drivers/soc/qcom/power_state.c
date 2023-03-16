// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/pm_wakeup.h>
#include <linux/compat.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/qcom_rproc.h>
#include "linux/power_state.h"

#define POWER_STATE "power_state"
#define STRING_LEN 32

LIST_HEAD(sub_sys_list);

struct subsystem_list {
	struct list_head list;
	const char *name;
	bool status;
	phandle rproc_handle;
};

static struct class *ps_class;
struct device *ps_ret;
static  struct cdev ps_cdev;
static  dev_t ps_dev;
struct kobject *kobj_ref;
static struct wakeup_source *notify_ws;
static int subsys_count;
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

enum ssr_domain_info {
	SSR_DOMAIN_MODEM,
	SSR_DOMAIN_ADSP,
	SSR_DOMAIN_MAX,
};

struct service_info {
	const char name[STRING_LEN];
	const char ssr_domains[STRING_LEN];
	int domain_id;
	void *handle;
	struct notifier_block *nb;
};

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

static int subsys_suspend(void *rproc_handle, uint32_t state)
{
	int ret = 0;

	switch (state) {
	case SUBSYS_DEEPSLEEP:
	case SUBSYS_HIBERNATE:
		ignore_ssr = 1;
		/*Add call for Subsys Shutdown*/
		rproc_shutdown(rproc_handle);
		ignore_ssr = 0;
		break;
	default:
		pr_err("%s: Invalid subsys suspend state\n", __func__);
		ret = -1;
		break;
	}
	return ret;
}

static int subsys_resume(void *rproc_handle, uint32_t state)
{
	int ret = 0;

	switch (state) {
	case SUBSYS_DEEPSLEEP:
	case SUBSYS_HIBERNATE:
		ignore_ssr = 1;
		/*Add call for Subsys Restart*/
		ret = rproc_boot(rproc_handle);
		ignore_ssr = 0;
		break;
	default:
		pr_err("%s: Invalid subsys suspend exit state\n", __func__);
		ret = -1;
		break;
	}
	return ret;
}

static int subsystem_resume(uint32_t state)
{
	struct subsystem_list *ss_list;
	int ret;
	void *pil_h = NULL;

	list_for_each_entry(ss_list, &sub_sys_list, list) {
		pr_err("%s: initiating %s resume\n", __func__, ss_list->name);
		pil_h = rproc_get_by_phandle(ss_list->rproc_handle);
		ret = subsys_resume(pil_h, state);
		if (ret) {
			pr_err("%s : subsys resume failed for %s\n", __func__, ss_list->name);
			BUG();
		}
		pr_err("%s: %s resume complete\n", __func__, ss_list->name);
	}
	return ret;
}

static int subsystem_suspend(uint32_t state)
{
	struct subsystem_list *ss_list;
	int ret;
	void *pil_h = NULL;

	list_for_each_entry(ss_list, &sub_sys_list, list) {
		pr_err("%s: initiating %s suspend\n", __func__, ss_list->name);
		pil_h = rproc_get_by_phandle(ss_list->rproc_handle);
		ret = subsys_suspend(pil_h, state);
		if (ret) {
			pr_err("%s : subsys suspend failed for %s\n", __func__, ss_list->name);
			BUG();
		}
		pr_err("%s: %s suspend complete\n", __func__, ss_list->name);
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
	.priority = 100,
};

static int ssr_register(void);

static int ps_probe(struct platform_device *pdev)
{
	int ret, i;
	const char *name;
	struct subsystem_list *ss_list;
	phandle rproc_handle;

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

	/*Get Rproc handles for ADSP and Modem*/
	if (pdev) {
		if (pdev->dev.of_node) {
			subsys_count = of_property_count_strings(pdev->dev.of_node,
								"qcom,subsys-name");

			if (subsys_count > 0) {
				for (i = 0; i < subsys_count; i++) {

					if (of_property_read_string_index(pdev->dev.of_node,
							"qcom,subsys-name", i, &name)) {
						pr_err("%s: error reading subsystem name\n",
								__func__);
						continue;
					}

					if (of_property_read_u32_index(pdev->dev.of_node,
							"qcom,rproc-handle", i, &rproc_handle)) {
						pr_err("%s: error reading %s rproc-handle\n",
									__func__, name);
						continue;
					}

					ss_list = devm_kzalloc(&pdev->dev,
							sizeof(struct subsystem_list), GFP_KERNEL);
					if (!ss_list) {
						ret = -ENOMEM;
						return ret;
					}

					ss_list->name = name;
					ss_list->rproc_handle = rproc_handle;
					ss_list->status = false;
					list_add_tail(&ss_list->list, &sub_sys_list);
				}
			}
		} else {
			pr_err("%s: device tree information missing\n", __func__);
			ret = -EFAULT;
		}
	} else {
		pr_err("%s: platform device null\n", __func__);
		ret = -ENODEV;
	}

	ret = ssr_register();
	if (ret)
		pr_err("%s: Error registering SSR\n", __func__);

	if (!ret)
		pr_debug("%s, success\n", __func__);

	return ret;
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
	case POWER_STATE_LPM_ACTIVE:
		pr_debug("%s: Changed to Active\n", __func__);
		if (pm_suspend_via_firmware()) {
			pm_suspend_clear_flags();
			/*Release Wakeup Source*/
			__pm_relax(notify_ws);
		}
		current_state = ACTIVE;
		break;

	case ENTER_DEEPSLEEP:
	case POWER_STATE_ENTER_DEEPSLEEP:
		pr_debug("%s: Enter Deep Sleep\n", __func__);
		ret = subsystem_suspend(SUBSYS_DEEPSLEEP);
		current_state = DEEPSLEEP;
		break;

	case ENTER_HIBERNATE:
	case POWER_STATE_ENTER_HIBERNATE:
		pr_debug("%s: Enter Hibernate\n", __func__);
		ret = subsystem_suspend(SUBSYS_HIBERNATE);
		current_state = HIBERNATE;
		break;

	case EXIT_DEEPSLEEP_STATE:
	case POWER_STATE_EXIT_DEEPSLEEP_STATE:
		pr_err("%s: Exit Deep Sleep\n", __func__);
		ret = subsystem_resume(SUBSYS_DEEPSLEEP);
		break;

	case EXIT_HIBERNATE_STATE:
	case POWER_STATE_EXIT_HIBERNATE_STATE:
		pr_debug("%s: Exit Hibernate\n", __func__);
		ret = subsystem_resume(SUBSYS_HIBERNATE);
		break;

	case MODEM_SUSPEND:
	case POWER_STATE_MODEM_SUSPEND:
		pr_debug("%s: Initiating Modem Shutdown\n", __func__);
		if (copy_from_user(&ui_obj_msg, (void __user *)arg,
					sizeof(ui_obj_msg))) {
			pr_err("%s: The copy from user failed - modem suspend\n", __func__);
			ret = -EFAULT;
		}

		break;

	case ADSP_SUSPEND:
	case POWER_STATE_ADSP_SUSPEND:
		pr_debug("%s: Initiating ADSP Shutdown\n", __func__);
		if (copy_from_user(&ui_obj_msg, (void __user *)arg,
					sizeof(ui_obj_msg))) {
			pr_err("%s: The copy from user failed - adsp suspend\n", __func__);
			ret = -EFAULT;
		}

		break;

	case MODEM_EXIT:
	case POWER_STATE_MODEM_EXIT:
		pr_debug("%s: Loading Modem Subsystem\n", __func__);
		if (copy_from_user(&ui_obj_msg, (void __user *)arg,
					sizeof(ui_obj_msg))) {
			pr_err("%s: The copy from user failed - modem suspend exit\n", __func__);
			ret = -EFAULT;
		}

		break;

	case ADSP_EXIT:
	case POWER_STATE_ADSP_EXIT:
		pr_debug("%s: Loading ADSP Subsystem\n", __func__);
		if (copy_from_user(&ui_obj_msg, (void __user *)arg,
					sizeof(ui_obj_msg))) {
			pr_err("%s: The copy from user failed - adsp suspend exit\n", __func__);
			ret = -EFAULT;
		}

		break;

	default:
		ret = -ENOIOCTLCMD;
		pr_err("%s: Default\n", __func__);
		break;
	}
	return ret;
}

static long compat_ps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int nr = 0;

	nr = _IOC_NR(cmd);

	return (long)ps_ioctl(file, nr, arg);
}
static const struct file_operations ps_fops = {
	.owner = THIS_MODULE,
	.open = ps_open,
	.release = ps_release,
	.unlocked_ioctl = ps_ioctl,
	.compat_ioctl = compat_ps_ioctl,
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
		sysfs_remove_file(kernel_kobj, &power_state_attr.attr);
		kobject_put(kobj_ref);
	}

	return 0;
}

static void __exit exit_power_state_func(void)
{
	struct subsystem_list *ss_list;

	unregister_pm_notifier(&powerstate_pm_nb);
	wakeup_source_unregister(notify_ws);
	sysfs_remove_file(kernel_kobj, &power_state_attr.attr);
	kobject_put(kobj_ref);
	device_destroy(ps_class, ps_dev);
	class_destroy(ps_class);
	cdev_del(&ps_cdev);
	unregister_chrdev_region(ps_dev, 1);
	platform_driver_unregister(&ps_driver);
	list_for_each_entry(ss_list, &sub_sys_list, list)
		list_del(&ss_list->list);
}


static int ssr_modem_cb(struct notifier_block *this, unsigned long opcode, void *data)
{
	struct ps_event modeme;

	switch (opcode) {
	case QCOM_SSR_BEFORE_SHUTDOWN:
		pr_debug("%s: modem is shutdown\n", __func__);
		if (ignore_ssr != 1) {
			modeme.event = MDSP_BEFORE_POWERDOWN;
			send_uevent(&modeme);
		}
		break;
	case QCOM_SSR_AFTER_POWERUP:
		pr_debug("%s: modem is powered up\n", __func__);
		if (ignore_ssr != 1) {
			modeme.event = MDSP_AFTER_POWERUP;
			send_uevent(&modeme);
		}
		break;
	default:
		pr_debug("%s: ignore modem ssr event\n");
		break;
	}

	return NOTIFY_DONE;
}

static int ssr_adsp_cb(struct notifier_block *this, unsigned long opcode, void *data)
{
	struct ps_event adspe;

	switch (opcode) {
	case QCOM_SSR_BEFORE_SHUTDOWN:
		pr_debug("%s: adsp is shutdown\n", __func__);
		if (ignore_ssr != 1) {
			adspe.event = ADSP_BEFORE_POWERDOWN;
			send_uevent(&adspe);
		}
		break;
	case QCOM_SSR_AFTER_POWERUP:
		pr_debug("%s: adsp is powered up\n", __func__);
		if (ignore_ssr != 1) {
			adspe.event = ADSP_AFTER_POWERUP;
			send_uevent(&adspe);
		}
		break;
	default:
		pr_debug("%s: ignore adsp ssr event\n");
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block ssr_modem_nb = {
	.notifier_call = ssr_modem_cb,
	.priority = 0,
};

static struct notifier_block ssr_adsp_nb = {
	.notifier_call = ssr_adsp_cb,
	.priority = 0,
};

static struct service_info service_data[2] = {
	{
		.name = "SSR_MODEM",
		.ssr_domains = "mpss",
		.domain_id = SSR_DOMAIN_MODEM,
		.nb = &ssr_modem_nb,
		.handle = NULL,
	},
	{
		.name = "SSR_ADSP",
		.ssr_domains = "lpass",
		.domain_id = SSR_DOMAIN_ADSP,
		.nb = &ssr_adsp_nb,
		.handle = NULL,
	},
};

static int ssr_register(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(service_data); i++) {
		if ((service_data[i].domain_id < 0) ||
				(service_data[i].domain_id >= SSR_DOMAIN_MAX)) {
			pr_err("Invalid service ID = %d\n",
					service_data[i].domain_id);
		} else {
			service_data[i].handle =
					qcom_register_ssr_notifier(
					service_data[i].ssr_domains,
					service_data[i].nb);
			pr_err("subsys registration for id = %d, ssr domain = %s\n",
				service_data[i].domain_id,
				service_data[i].ssr_domains);
			if (IS_ERR_OR_NULL(service_data[i].handle)) {
				pr_err("subsys register failed for id = %d, ssr domain = %s\n",
						service_data[i].domain_id,
						service_data[i].ssr_domains);
				service_data[i].handle = NULL;
			}
		}
	}
	return 0;
}

module_init(init_power_state_func);
module_exit(exit_power_state_func);
MODULE_LICENSE("GPL v2");
