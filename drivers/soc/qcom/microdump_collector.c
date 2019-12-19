/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/ramdump.h>
#include <soc/qcom/smem.h>
#include <linux/workqueue.h>

/*
 * This program collects the data from SMEM regions whenever the modem crashes
 * and stores it in /dev/ramdump_microdump_modem so as to expose it to
 * user space.
 */

#define STR_NV_SIGNATURE_DESTROYED "CRITICAL_DATA_CHECK_FAILED"
#define MAX_SSR_REASON_LEN 130U

static struct kobject *checknv_kobj;
static struct kset *checknv_kset;

static const struct sysfs_ops checknv_sysfs_ops = {
};

static void kobj_release(struct kobject *kobj)
{
	kfree(kobj);
}

static struct kobj_type checknv_ktype = {
	.sysfs_ops = &checknv_sysfs_ops,
	.release = kobj_release,
};

static void checknv_kobj_clean(struct work_struct *work)
{
	kobject_uevent(checknv_kobj, KOBJ_REMOVE);
	kobject_put(checknv_kobj);
	kset_unregister(checknv_kset);
}

static void checknv_kobj_create(struct work_struct *work)
{
	int ret;

	if (checknv_kset != NULL) {
		pr_err("checknv_kset is not NULL, should clean up.");
		kobject_uevent(checknv_kobj, KOBJ_REMOVE);
		kobject_put(checknv_kobj);
	}

	checknv_kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!checknv_kobj) {
		pr_err("kobject alloc failed.");
		return;
	}

	if (checknv_kset == NULL) {
		checknv_kset = kset_create_and_add("checknv_errimei", NULL, NULL);
		if (!checknv_kset) {
			pr_err("kset creation failed.");
			goto free_kobj;
		}
	}

	checknv_kobj->kset = checknv_kset;

	ret = kobject_init_and_add(checknv_kobj, &checknv_ktype, NULL, "%s", "errimei");
	if (ret) {
		pr_err("%s: Error in creation kobject", __func__);
		goto del_kobj;
	}

	kobject_uevent(checknv_kobj, KOBJ_ADD);
	return;

del_kobj:
	kobject_put(checknv_kobj);
	kset_unregister(checknv_kset);

free_kobj:
	kfree(checknv_kobj);
}

static DECLARE_DELAYED_WORK(create_kobj_work, checknv_kobj_create);
static DECLARE_WORK(clean_kobj_work, checknv_kobj_clean);
static char last_modem_sfr_reason[MAX_SSR_REASON_LEN] = "none";

struct microdump_data {
	struct ramdump_device *microdump_dev;
	void *microdump_modem_notify_handler;
	struct notifier_block microdump_modem_ssr_nb;
};

static struct microdump_data *drv;

static int microdump_modem_notifier_nb(struct notifier_block *nb,
		unsigned long code, void *data)
{
	int ret = 0;
	unsigned int size_reason = 0, size_data = 0;
	char *crash_reason = NULL;
	char *crash_data = NULL;
	unsigned int smem_id = 611;
	struct ramdump_segment segment[2];

	if (SUBSYS_RAMDUMP_NOTIFICATION == code || SUBSYS_SOC_RESET == code) {

		memset(segment, 0, sizeof(segment));

		crash_reason = smem_get_entry(SMEM_SSR_REASON_MSS0, &size_reason
				, 0, SMEM_ANY_HOST_FLAG);
		if (IS_ERR_OR_NULL(crash_reason)) {
			pr_info("%s: smem %d not available\n",
				__func__, SMEM_SSR_REASON_MSS0);
			goto out;
		}

		segment[0].v_address = crash_reason;
		segment[0].size = size_reason;

		crash_data = smem_get_entry(smem_id, &size_data, SMEM_MODEM, 0);
		if (IS_ERR_OR_NULL(crash_data)) {
			pr_info("%s: smem %d not available\n ",
				__func__, smem_id);
			goto out;
		}

		segment[1].v_address = crash_data;
		segment[1].size = size_data;
		strlcpy(last_modem_sfr_reason, crash_reason, MAX_SSR_REASON_LEN);
		pr_err("modem subsystem failure reason: %s.\n", last_modem_sfr_reason);

		// If the NV protected file (critical_info) is destroyed, restart to recovery to inform user
		if (strnstr(last_modem_sfr_reason, STR_NV_SIGNATURE_DESTROYED, strlen(last_modem_sfr_reason))) {
			pr_err("errimei_dev: the NV has been destroyed, should restart to recovery\n");
			schedule_delayed_work(&create_kobj_work, msecs_to_jiffies(1*1000));
			goto out;
		}

		ret = do_ramdump(drv->microdump_dev, segment, 2);
		if (ret)
			pr_info("%s: do_ramdump() failed\n", __func__);
	}

out:
	return NOTIFY_OK;
}

static int microdump_modem_ssr_register_notifier(struct microdump_data *drv)
{
	int ret = 0;

	drv->microdump_modem_ssr_nb.notifier_call = microdump_modem_notifier_nb;

	drv->microdump_modem_notify_handler =
		subsys_notif_register_notifier("modem",
			&drv->microdump_modem_ssr_nb);

	if (IS_ERR(drv->microdump_modem_notify_handler)) {
		pr_err("Modem register notifier failed: %ld\n",
			PTR_ERR(drv->microdump_modem_notify_handler));
		ret = -EINVAL;
	}

	return ret;
}

static void microdump_modem_ssr_unregister_notifier(struct microdump_data *drv)
{
	subsys_notif_unregister_notifier(drv->microdump_modem_notify_handler,
					&drv->microdump_modem_ssr_nb);
	drv->microdump_modem_notify_handler = NULL;
}

/*
 * microdump_init() - Registers kernel module for microdump collector
 *
 * Creates device file /dev/ramdump_microdump_modem and registers handler for
 * modem SSR events.
 *
 * Returns 0 on success and negative error code in case of errors
 */
static int __init microdump_init(void)
{
	int ret = -ENOMEM;

	drv = kzalloc(sizeof(struct microdump_data), GFP_KERNEL);
	if (!drv)
		goto out;

	drv->microdump_dev = create_ramdump_device("microdump_modem", NULL);
	if (!drv->microdump_dev) {
		pr_err("%s: Unable to create a microdump_modem ramdump device\n"
			, __func__);
		ret = -ENODEV;
		goto out_kfree;
	}

	ret = microdump_modem_ssr_register_notifier(drv);
	if (ret) {
		destroy_ramdump_device(drv->microdump_dev);
		goto out_kfree;
	}
	return ret;
out_kfree:
	pr_err("%s: Failed to register microdump collector\n", __func__);
	kfree(drv);
	drv = NULL;
out:
	return ret;
}

static void __exit microdump_exit(void)
{
	schedule_work(&clean_kobj_work);
	if (!drv)
		return;

	if (!IS_ERR(drv->microdump_modem_notify_handler))
		microdump_modem_ssr_unregister_notifier(drv);

	if (drv->microdump_dev)
		destroy_ramdump_device(drv->microdump_dev);

	kfree(drv);
}

module_init(microdump_init);
module_exit(microdump_exit);

MODULE_DESCRIPTION("Microdump Collector");
MODULE_LICENSE("GPL v2");
