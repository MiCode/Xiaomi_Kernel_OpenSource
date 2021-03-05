/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/ramdump.h>
#include <linux/soc/qcom/smem.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>

#define MAX_SSR_REASON_LEN	130U
#define SMEM_SSR_REASON_MSS0	421
#define SMEM_SSR_DATA_MSS0	611
#define SMEM_MODEM	1

/*
 * This program collects the data from SMEM regions whenever the modem crashes
 * and stores it in /dev/ramdump_microdump_modem so as to expose it to
 * user space.
 */

#define STR_NV_SIGNATURE_DESTROYED "CRITICAL_DATA_CHECK_FAILED"

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
static struct proc_dir_entry *last_modem_sfr_entry;
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
	size_t size_reason = 0, size_data = 0;
	char *crash_reason = NULL;
	char *crash_data = NULL;
	struct ramdump_segment segment[2];

	if (SUBSYS_RAMDUMP_NOTIFICATION != code && SUBSYS_SOC_RESET != code)
		return NOTIFY_OK;

	memset(segment, 0, sizeof(segment));

	crash_reason = qcom_smem_get(QCOM_SMEM_HOST_ANY
				, SMEM_SSR_REASON_MSS0, &size_reason);

	if (IS_ERR_OR_NULL(crash_reason)) {
		pr_info("%s: smem %d not available\n",
				__func__, SMEM_SSR_REASON_MSS0);
		goto out;
	}

	segment[0].v_address = crash_reason;
	segment[0].size = size_reason;

	crash_data = qcom_smem_get(SMEM_MODEM
				, SMEM_SSR_DATA_MSS0, &size_data);

	if (IS_ERR_OR_NULL(crash_data)) {
		pr_info("%s: smem %d not available\n",
				__func__, SMEM_SSR_DATA_MSS0);
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

static int last_modem_sfr_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", last_modem_sfr_reason);
	return 0;
}

static int last_modem_sfr_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, last_modem_sfr_proc_show, NULL);
}

static const struct file_operations last_modem_sfr_file_ops = {
	.owner   = THIS_MODULE,
	.open    = last_modem_sfr_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

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
	last_modem_sfr_entry = proc_create("last_mcrash", S_IFREG | S_IRUGO, NULL, &last_modem_sfr_file_ops);
	if (!last_modem_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry last_mcrash\n");
	}
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
	if (last_modem_sfr_entry) {
		remove_proc_entry("last_mcrash", NULL);
		last_modem_sfr_entry = NULL;
	}
	microdump_modem_ssr_unregister_notifier(drv);
	destroy_ramdump_device(drv->microdump_dev);
	kfree(drv);
}

module_init(microdump_init);
module_exit(microdump_exit);

MODULE_DESCRIPTION("Microdump Collector");
MODULE_LICENSE("GPL v2");
