// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/soc/qcom/slate_events_bridge_intf.h>
#include "linux/slatecom_interface.h"

#define SLATECOM_EVENT "slate_com_event"

static  struct cdev              sce_cdev;
static  struct class             *sce_class;
static  dev_t                    sce_dev;

struct seb_notif_info *sce_handle;
static struct device *dev_ret;

struct msm_ssr_info {
	uint32_t ssid; /* SS_ID */
	uint32_t event; /* SS_NOTIFICATION */
};
struct slate_event {
	enum slate_event_type e_type;
};

static int client_notif_handler(struct notifier_block *, unsigned long, void *);

static const struct file_operations fops = {
	.owner          = THIS_MODULE,
};

static struct notifier_block sce_client_nb = {
	.notifier_call = client_notif_handler,
};

/**
 * send_uevent(): send events to user space
 * pce : ssr event handle value
 * Return: 0 on success, standard Linux error code on error
 *
 * It adds pce value to event and broadcasts to user space.
 */
static int send_uevent(struct slate_event *pce)
{
	char event_string[32];
	char *envp[2] = { event_string, NULL };

	snprintf(event_string, ARRAY_SIZE(event_string),
			"SLATE_EVENT=%d", pce->e_type);
	return kobject_uevent_env(&dev_ret->kobj, KOBJ_CHANGE, envp);
}

static int client_notif_handler(struct notifier_block *this,
						unsigned long event, void *data)
{
	struct msm_ssr_info *msm_ssr_info = NULL;
	struct slate_event slatee;

	if (event == GMI_SLATE_EVENT_SENSOR) {
		pr_info("Received GMI_SLATE_EVENT_SENSOR event\n");
		msm_ssr_info = (struct msm_ssr_info *)data;
		if (msm_ssr_info->event == 0)
			slatee.e_type = SLATE_SNS_READY;
		else
			slatee.e_type = SLATE_SNS_ERROR;
		send_uevent(&slatee);
		pr_info("%s:sent SNS event to userspace ssid: %d and event: %d\n",
			__func__, msm_ssr_info->ssid, msm_ssr_info->event);
	}
	return NOTIFY_DONE;
}

static int __init init_sce_dev(void)
{
	int ret;

	ret = alloc_chrdev_region(&sce_dev, 0, 1, SLATECOM_EVENT);
	if (ret  < 0) {
		pr_err("failed with error %d\n", ret);
		return ret;
	}
	cdev_init(&sce_cdev, &fops);

	ret = cdev_add(&sce_cdev, sce_dev, 1);
	if (ret < 0) {
		unregister_chrdev_region(sce_dev, 1);
		pr_err("device registration failed\n");
		return ret;
	}
	sce_class = class_create(THIS_MODULE, "sce_class");
	if (IS_ERR_OR_NULL(sce_class)) {
		cdev_del(&sce_cdev);
		unregister_chrdev_region(sce_dev, 1);
		pr_err("class creation failed\n");
		return PTR_ERR(sce_class);
	}

	dev_ret = device_create(sce_class, NULL, sce_dev, NULL, SLATECOM_EVENT);
	if (IS_ERR_OR_NULL(dev_ret)) {
		class_destroy(sce_class);
		cdev_del(&sce_cdev);
		unregister_chrdev_region(sce_dev, 1);
		pr_err("device create failed\n");
		return PTR_ERR(dev_ret);
	}

	sce_handle = seb_register_for_slate_event(GMI_SLATE_EVENT_SENSOR, &sce_client_nb);
	if (sce_handle ==  NULL)
		pr_err("%s: seb_register_for_slate_event failed\n", __func__);

	return 0;
}

static void __exit exit_sce_dev(void)
{
	device_destroy(sce_class, sce_dev);
	class_destroy(sce_class);
	cdev_del(&sce_cdev);
	unregister_chrdev_region(sce_dev, 1);
	seb_unregister_for_slate_event(sce_handle, &sce_client_nb);
	pr_info("%s: driver removed!\n", __func__);
}

module_init(init_sce_dev);
module_exit(exit_sce_dev);
MODULE_LICENSE("GPL v2");

