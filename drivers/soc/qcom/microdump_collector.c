// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/ramdump.h>
#include <linux/soc/qcom/smem.h>

/* HQ-208762, xionghaifeng, 20220712, add for modem crash history begin */
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#define MAX_SSR_REASON_LEN	130U
/* HQ-208762, xionghaifeng, 20220712, add for modem crash history end */

#define SMEM_SSR_REASON_MSS0	421
#define SMEM_SSR_DATA_MSS0	611
#define SMEM_MODEM	1

/*
 * This program collects the data from SMEM regions whenever the modem crashes
 * and stores it in /dev/ramdump_microdump_modem so as to expose it to
 * user space.
 */

/* HQ-208762, xionghaifeng, 20220712, add for modem crash history begin */
static char last_modem_sfr_reason[MAX_SSR_REASON_LEN] = "none";
static struct proc_dir_entry *last_modem_sfr_entry;
/* HQ-208762, xionghaifeng, 20220712, add for modem crash history end */
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
	void *crash_reason = NULL;
	void *crash_data = NULL;
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

	segment[0].v_address = (void __iomem *) crash_reason;
	segment[0].size = size_reason;

	crash_data = qcom_smem_get(SMEM_MODEM
				, SMEM_SSR_DATA_MSS0, &size_data);

	if (IS_ERR_OR_NULL(crash_data)) {
		pr_info("%s: smem %d not available\n",
				__func__, SMEM_SSR_DATA_MSS0);
		goto out;
	}

	segment[1].v_address = (void __iomem *) crash_data;
	segment[1].size = size_data;

    /* HQ-208762, xionghaifeng, 20220712, add for modem crash history begin */
    strlcpy(last_modem_sfr_reason, crash_reason, MAX_SSR_REASON_LEN);
    pr_err("modem subsystem failure reason: %s.\n", last_modem_sfr_reason);
    /* HQ-208762, xionghaifeng, 20220712, add for modem crash history end */

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

/* HQ-208762, xionghaifeng, 20220712, add for modem crash history begin */
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
/* HQ-208762, xionghaifeng, 20220712, add for modem crash history end */

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
	/* HQ-208762, xionghaifeng, 20220712, add for modem crash history begin */
	last_modem_sfr_entry = proc_create("last_mcrash", S_IFREG | S_IRUGO, NULL, &last_modem_sfr_file_ops);
	if (!last_modem_sfr_entry) {
		printk(KERN_ERR "pil: cannot create proc entry last_mcrash\n");
	}
	/* HQ-208762, xionghaifeng, 20220712, add for modem crash history end */

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
	/* HQ-208762, xionghaifeng, 20220712, add for modem crash history begin */
	if (last_modem_sfr_entry) {
			remove_proc_entry("last_mcrash", NULL);
			last_modem_sfr_entry = NULL;
	}
	/* HQ-208762, xionghaifeng, 20220712, add for modem crash history end */

	microdump_modem_ssr_unregister_notifier(drv);
	destroy_ramdump_device(drv->microdump_dev);
	kfree(drv);
}

module_init(microdump_init);
module_exit(microdump_exit);

MODULE_DESCRIPTION("Microdump Collector");
MODULE_LICENSE("GPL v2");
