// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/soc/qcom/smem.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MSM_SUBSYSTEM_RESTART)
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/ramdump.h>
#endif

#if IS_ENABLED(CONFIG_QCOM_RAMDUMP)
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/remoteproc/qcom_rproc.h>
#include <soc/qcom/qcom_ramdump.h>
#endif

#define MAX_SSR_REASON_LEN	130U
#define SMEM_SSR_REASON_MSS0	421
#define SMEM_SSR_DATA_MSS0	611
#define SMEM_MODEM	1

static char last_modem_sfr_reason[MAX_SSR_REASON_LEN] = "none";

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

/*
 * This program collects the data from SMEM regions whenever the modem crashes
 * and stores it in /dev/ramdump_microdump_modem so as to expose it to
 * user space.
 */

#if IS_ENABLED(CONFIG_MSM_SUBSYSTEM_RESTART)
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
	microdump_modem_ssr_unregister_notifier(drv);
	destroy_ramdump_device(drv->microdump_dev);
	kfree(drv);
}

module_init(microdump_init);
module_exit(microdump_exit);
#endif

/* From msm-5.10 SSR/PIL was deprecated. Remoteproc is used instead. */
#if IS_ENABLED(CONFIG_QCOM_RAMDUMP)

struct microdump_data {
	struct device *microdump_dev;
	void *microdump_modem_notify_handler;
	struct notifier_block microdump_modem_ssr_nb;
	/* struct notifier_block microdump_modem_panic_notifier; */
};

static int enable_microdump;
module_param(enable_microdump, int, 0644);

static int start_qcomdump;
module_param(start_qcomdump, int, 0644);

static struct microdump_data *drv;

static int microdump_crash_collection(void)
{
	int ret;
	size_t size_reason = 0, size_data = 0;
	void *crash_reason = NULL;
	void *crash_data = NULL;
	struct qcom_dump_segment segment[2];
	struct list_head head;

	INIT_LIST_HEAD(&head);
	memset(segment, 0, sizeof(segment));

	crash_data = qcom_smem_get(SMEM_MODEM
				, SMEM_SSR_DATA_MSS0, &size_data);

	if (IS_ERR_OR_NULL(crash_data)) {
		pr_err("%s: smem %d not available\n",
				__func__, SMEM_SSR_DATA_MSS0);
		goto out;
	}

	segment[0].va = crash_data;
	segment[0].size = size_data;
	list_add(&segment[0].node, &head);

	crash_reason = qcom_smem_get(QCOM_SMEM_HOST_ANY
				, SMEM_SSR_REASON_MSS0, &size_reason);

	if (!crash_reason) {
		pr_err("%s: smem %d is null\n",
				__func__, SMEM_SSR_REASON_MSS0);

		goto out;
	}

	if (IS_ERR(crash_reason)) {
		pr_err("%s: smem %d not available because of %d\n",
			__func__, SMEM_SSR_REASON_MSS0, PTR_ERR(crash_reason));

		goto out;
	}

	strlcpy(last_modem_sfr_reason, crash_reason, MAX_SSR_REASON_LEN);
	pr_err("modem subsystem failure reason: %s.\n", last_modem_sfr_reason);

	// If the NV protected file (critical_info) is destroyed, restart to recovery to inform user
	if (strnstr(last_modem_sfr_reason, STR_NV_SIGNATURE_DESTROYED, strlen(last_modem_sfr_reason))) {
		pr_err("errimei_dev: the NV has been destroyed, should restart to recovery\n");
		schedule_delayed_work(&create_kobj_work, msecs_to_jiffies(1*1000));
		goto out;
	}


	segment[1].va = crash_reason;
	segment[1].size = size_reason;
	list_add(&segment[1].node, &head);

	if (!enable_microdump) {
		pr_err("%s: enable_microdump is false\n", __func__);
		goto out;
	}

	start_qcomdump = 1;
	ret = qcom_dump(&head, drv->microdump_dev);
	if (ret)
		pr_err("%s: qcom_dump() failed\n", __func__);

	start_qcomdump = 0;
out:
	return NOTIFY_OK;
}

/**
 * static int microdump_modem_panic_notifier_nb(struct notifier_block *nb,
 * unsigned long code, void *data)
 * {
 * return microdump_crash_collection();
 * }
 */

static int microdump_modem_ssr_notifier_nb(struct notifier_block *nb,
		unsigned long code, void *data)
{
	struct qcom_ssr_notify_data *notify_data = data;

	if (code == QCOM_SSR_BEFORE_SHUTDOWN && notify_data->crashed)
		return microdump_crash_collection();
	else
		return NOTIFY_OK;
}

/**
 * To detect crash when SSR is disabled
 * static int microdump_modem_panic_register_notifier(struct microdump_data *drv)
 * {
 * int ret;
 * drv->microdump_modem_panic_notifier.notifier_call = microdump_modem_panic_notifier_nb;
 * ret = atomic_notifier_chain_register(&panic_notifier_list,
 * &drv->microdump_modem_panic_notifier);
 * if (ret) {
 * pr_err("Failed to register panic handler\n");
 * return -EINVAL;
 * }
 * return 0;
 * }
 */

/* to detect crash when SSR is enabled */
static int microdump_modem_ssr_register_notifier(struct microdump_data *drv)
{
	int ret = 0;

	drv->microdump_modem_ssr_nb.notifier_call = microdump_modem_ssr_notifier_nb;

	drv->microdump_modem_notify_handler =
		qcom_register_ssr_notifier("mpss",
			&drv->microdump_modem_ssr_nb);

	if (IS_ERR(drv->microdump_modem_notify_handler)) {
		pr_err("Modem register notifier failed: %ld\n",
			PTR_ERR(drv->microdump_modem_notify_handler));
		ret = -EINVAL;
	}

	return ret;
}

/**
 * static void microdump_modem_panic_unregister_notifier(struct microdump_data *drv)
 * {
 * int ret;
 * ret = atomic_notifier_chain_unregister(&panic_notifier_list,
 * &drv->microdump_modem_panic_notifier);
 * if (ret)
 * pr_err("Failed to unregister panic handler\n");
 * }
 */

static void microdump_modem_ssr_unregister_notifier(struct microdump_data *drv)
{
	qcom_unregister_ssr_notifier(drv->microdump_modem_notify_handler,
		&drv->microdump_modem_ssr_nb);

	drv->microdump_modem_notify_handler = NULL;
}

static void __exit microdump_exit(void)
{
	/* microdump_modem_panic_unregister_notifier(drv); */

	microdump_modem_ssr_unregister_notifier(drv);

	kfree(drv);
}


static int microdump_probe(struct platform_device *pdev)
{
	int ret = -ENOMEM;

	drv = kzalloc(sizeof(struct microdump_data), GFP_KERNEL);
	if (!drv)
		goto out;

	drv->microdump_dev = &pdev->dev;
	if (!drv->microdump_dev) {
		pr_err("%s: Unable to create a microdump_modem ramdump device\n"
			, __func__);
		ret = -ENODEV;
		goto out_kfree;
	}

	/**
	 * ret = microdump_modem_panic_register_notifier(drv);
	 * if (ret) {
	 * pr_err("%s: microdump_modem_panic_register_notifier failed\n", __func__);
	 * goto out_kfree;
	 * }
	 */

	ret = microdump_modem_ssr_register_notifier(drv);
	if (ret) {
		pr_err("%s: microdump_modem_ssr_register_notifier failed\n", __func__);
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

static const struct of_device_id microdump_match_table[] = {
{.compatible = "qcom,microdump_modem",},
	{}
};

static struct platform_driver microdump_driver = {
	.probe = microdump_probe,
	.driver = {
		.name = "msm_microdump_modem",
		.of_match_table = microdump_match_table,
	},
};

static int __init microdump_init(void)
{
	int ret;

	ret = platform_driver_register(&microdump_driver);

	if (ret) {
		pr_err("%s: register failed %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

module_init(microdump_init)
module_exit(microdump_exit);
#endif

MODULE_DESCRIPTION("Microdump Collector");
MODULE_LICENSE("GPL v2");
