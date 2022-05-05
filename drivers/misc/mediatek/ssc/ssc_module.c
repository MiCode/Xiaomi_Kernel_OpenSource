// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <ssc_module.h>
#include <mt-plat/ssc.h>

//#define SSC_SYSFS_VLOGIC_BOUND_SUPPORT

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#if IS_ENABLED(CONFIG_ARM_SCMI_PROTOCOL)
#include <linux/scmi_protocol.h>
#include <tinysys-scmi.h>
#endif

#if IS_ENABLED(CONFIG_GPU_SUPPORT)
#include <gpufreq_v2.h>
static unsigned int gpueb_enable;
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define ssc_aee_print(string, args...) do {\
        char ssc_name[100];\
        int ret;\
        ret = snprintf(ssc_name, 100, "[SSC] "string, ##args); \
        if (ret > 0)\
                aee_kernel_exception_api(__FILE__, __LINE__, \
                        DB_OPT_MMPROFILE_BUFFER | DB_OPT_NE_JBT_TRACES, \
                        ssc_name, "[SSC] error:"string, ##args); \
        pr_info("[SSC] error:"string, ##args);  \
        } while (0)
#else
#define ssc_aee_print(string, args...) \
	pr_info("[SSC] error:"string, ##args)
#endif

#define MTK_SSC_DTS_COMPATIBLE "mediatek,ssc"
#define MTK_GPU_DTS_COMPATIBLE "mediatek,gpueb"
#define SSC_VCORE_REGULATOR "dvfsrc-vcore"
#define PLT_SSC_INIT     (0x504C5403) /*magic*/

struct kobject *ssc_kobj;
EXPORT_SYMBOL_GPL(ssc_kobj);

static unsigned int ssc_disable;

static BLOCKING_NOTIFIER_HEAD(vlogic_bound_chain);
static struct regulator *ssc_vcore_voter;

static int set_vcore_vlogic_bound(en)
{
	int ret;

	if (!ssc_vcore_voter) {
		pr_info("[SSC] invalid vcore regulator\n");
		return -1;
	}

	if (en)
		ret = regulator_set_voltage(ssc_vcore_voter, 600000, INT_MAX);
	else
		ret = regulator_set_voltage(ssc_vcore_voter, 575000, INT_MAX);

	if (ret) {
		pr_info("[SSC] vcore vlogic bound %s fail\n", en ? "enable" : "disable");
		return -1;
	}

	return 0;
}


static int set_gpu_vlogic_bound(int en)
{

	int ret = 0;

#if IS_ENABLED(CONFIG_GPU_SUPPORT)
	/* floor value for GPU = mV * 100 */
	if (en)
		ret = gpufreq_set_limit(TARGET_DEFAULT, LIMIT_SRAMRC,
					GPUPPM_KEEP_IDX, 60000);
	else
		ret = gpufreq_set_limit(TARGET_DEFAULT, LIMIT_SRAMRC,
					GPUPPM_KEEP_IDX, GPUPPM_RESET_IDX);

	if (ret)
		pr_info("[SSC] gpu vlogic bound %s fail\n", en ? "enable" : "disable");
#endif
	return ret;
}

static int ssc_vlogic_bound_event(struct notifier_block *notifier, unsigned long event,
				void *data)
{

	unsigned int request_id = *((unsigned int*)data);

	pr_info("[SSC] request ID = 0x%x, event = 0x%lx\n", request_id, event);

	switch(event) {
		case SSC_ENABLE_VLOGIC_BOUND:
			set_gpu_vlogic_bound(1);
			set_vcore_vlogic_bound(1);
			return NOTIFY_DONE;
		case SSC_DISABLE_VLOGIC_BOUND:
			set_vcore_vlogic_bound(0);
			set_gpu_vlogic_bound(0);
			return NOTIFY_DONE;
		case SSC_TIMEOUT:
			return NOTIFY_DONE;
		default:
			return NOTIFY_BAD;
	}
	return NOTIFY_OK;
}
static struct notifier_block ssc_vlogic_notifier_func = {
	.notifier_call = ssc_vlogic_bound_event,
	.priority = 0,
};

static int ssc_vlogic_bound_call_chain(unsigned int val, unsigned int request_id)
{
	int ret;

	ret = blocking_notifier_call_chain(&vlogic_bound_chain, val, &request_id);

	if (ret == NOTIFY_DONE || ret == NOTIFY_OK)
		return 0;
	else
		return -1;
}
static atomic_t vlogic_bound_counter;

int ssc_vlogic_bound_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&vlogic_bound_chain, nb);
}
EXPORT_SYMBOL_GPL(ssc_vlogic_bound_register_notifier);

int ssc_vlogic_bound_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&vlogic_bound_chain, nb);
}
EXPORT_SYMBOL_GPL(ssc_vlogic_bound_unregister_notifier);


int ssc_enable_vlogic_bound(int request_id)
{
	/* check counter value */
	if (atomic_read(&vlogic_bound_counter) < 0) {
		return -1;
	}

	/* check request_id */
	if (request_id < 0 || request_id >= SSC_REQUEST_NUM) {
		return -1;
	}

	if (atomic_inc_return(&vlogic_bound_counter) == 1) {
		return ssc_vlogic_bound_call_chain(SSC_ENABLE_VLOGIC_BOUND, request_id);
	}

	/* vlogic has been bounded */
	return 0;
}
EXPORT_SYMBOL_GPL(ssc_enable_vlogic_bound);

int ssc_disable_vlogic_bound(int request_id)
{
	/* check counter value */
	if (atomic_read(&vlogic_bound_counter) < 0) {
		return -1;
	}

	/* check request_id */
	if (request_id < 0 || request_id >= SSC_REQUEST_NUM) {
		return -1;
	}

	if (atomic_dec_return(&vlogic_bound_counter) == 0) {
		return ssc_vlogic_bound_call_chain(SSC_DISABLE_VLOGIC_BOUND, request_id);
	}

	/* vlogic still bounded */
	return 0;
}
EXPORT_SYMBOL_GPL(ssc_disable_vlogic_bound);

#ifdef SSC_SYSFS_VLOGIC_BOUND_SUPPORT
static ssize_t vlogic_bound_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char mode[10];

	if (sscanf(buf, "%9s", mode) != 1)
		return -EPERM;

	if (strcmp(mode, "enable") == 0)
		ssc_enable_vlogic_bound(SSC_SW);
	else if (strcmp(mode, "disable") == 0)
		ssc_disable_vlogic_bound(SSC_SW);

	return count;
}
static ssize_t vlogic_bound_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = snprintf(buf, PAGE_SIZE, "[SSC] bound cnt = %d\n",
			atomic_read(&vlogic_bound_counter));

	return len;

}
DEFINE_ATTR_RW(vlogic_bound);
#endif

static unsigned int safe_vlogic_uV = 0xFFFFFFFF;
unsigned int ssc_get_safe_vlogic_uV(void)
{
	return safe_vlogic_uV;
}

#define SSC_TIMEOUT_NUM 6
static const char * const ssc_timeout_name[] = {
	"SSC",
	"GPU",
	"ISP",
	"CORE",
	"APU",
	"SRAM",
};

#if IS_ENABLED(CONFIG_ARM_SCMI_PROTOCOL)
static int ssc_scmi_feature_id;
static int plt_scmi_feature_id;

#define SSC_TIMEOUT_MASK_SHIFT (16)

static void ssc_notification_handler(u32 feature_id, scmi_tinysys_report *report)
{
#define LOG_BUF_SIZE 30
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0;
	int i, timeout;
	int timeout_mask_shift = SSC_TIMEOUT_MASK_SHIFT;

	if (report->p1 == SSC_STATUS_ERR) {
		/* SSC timeout notifiy */
		ssc_vlogic_bound_call_chain(SSC_TIMEOUT, SSC_ERR);

		timeout = report->p2;

		pr_info("[SSC] %s, timeout_sta = 0x%x, sram_sta = 0x%x, misc = 0x%x\n",
			__func__, report->p2, report->p3, report->p4);

		for (i = 1 ; i < SSC_TIMEOUT_NUM ; i++) {
			timeout_mask_shift = (i - 1) * 2 + SSC_TIMEOUT_MASK_SHIFT;
			if ((timeout & (0x1U << i)) &&
			    ((timeout & (0x3 << timeout_mask_shift)) == 0x0)) {
				log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size, "%s",
				ssc_timeout_name[i]);
			}
		}
		pr_info("[SSC] CRDISPATCH_KEY:  SSC VIOLATION: %s\n", log_buf);
		BUG_ON(1);

	}

	return;
}
#endif


static DEFINE_SPINLOCK(ssc_locker);

static const struct of_device_id ssc_of_ids[] = {
	{.compatible = "mediatek,ssc",},
	{}
};
static int mt_ssc_pdrv_probe(struct platform_device *pdev)
{

	ssc_vcore_voter = regulator_get(&pdev->dev, SSC_VCORE_REGULATOR);
	if (IS_ERR(ssc_vcore_voter)) {
		pr_info("[SSC] get ssc vcore regulator fail\n");
		ssc_vcore_voter = NULL;
		return -1;
	}
	return 0;
}
static int mt_ssc_pdrv_remove(struct platform_device *pdev)
{
	return 0;
}
static struct platform_driver mt_ssc_pdrv = {
	.probe = mt_ssc_pdrv_probe,
	.remove = mt_ssc_pdrv_remove,
	.driver = {
		.name = "ssc_dvfs",
		.owner = THIS_MODULE,
		.of_match_table = ssc_of_ids,
	},

};

static int __init ssc_init(void)
{
	struct device_node *ssc_node;
	int ret;
	unsigned long flags;
#if IS_ENABLED(CONFIG_ARM_SCMI_PROTOCOL)
	struct scmi_tinysys_info_st *tinfo = NULL;
#endif

	pr_info("[SSC] %s\n", __func__);

	ret = platform_driver_register(&mt_ssc_pdrv);
	if (ret)
		pr_info("[SSC] fail to register SSC platform driver\n");

	spin_lock_irqsave(&ssc_locker, flags);

	ssc_node = of_find_compatible_node(NULL, NULL, MTK_SSC_DTS_COMPATIBLE);

	if (ssc_node) {
		ret = of_property_read_u32(ssc_node,
				MTK_SSC_SAFE_VLOGIC_STRING,
				&safe_vlogic_uV);

		pr_info("[SSC] safe_vlogic_uV = %d uV", safe_vlogic_uV);
		/* This property is not defined*/
		if (ret)
			safe_vlogic_uV = 0xFFFFFFFF;

		ret = of_property_read_u32(ssc_node,
				"ssc_disable",
				&ssc_disable);

		if (!ret && ssc_disable == 1) {
			spin_unlock_irqrestore(&ssc_locker, flags);
			pr_info("[SSC] disabled\n");
			return 0;
		}

		of_node_put(ssc_node);
	}

	/* set gpu vlogic bound 0.6V if gpueb not ready */
#if IS_ENABLED(CONFIG_GPU_SUPPORT)
	ssc_node = of_find_compatible_node(NULL, NULL, MTK_GPU_DTS_COMPATIBLE);

	if (ssc_node) {
		ret = of_property_read_u32(ssc_node,
			"gpueb-support",
			&gpueb_enable);

		pr_info("[SSC] gpueb enable = 0x%x\n", gpueb_enable);

		if (gpueb_enable == 0)
			gpufreq_set_limit(TARGET_DEFAULT, LIMIT_SRAMRC, GPUPPM_KEEP_IDX, 60000);

		of_node_put(ssc_node);
	}
#endif
	spin_unlock_irqrestore(&ssc_locker, flags);

	/* scmi interface initialization */

#if IS_ENABLED(CONFIG_ARM_SCMI_PROTOCOL)
	tinfo = get_scmi_tinysys_info();
	if (!tinfo) {
		pr_info("[SSC] get SCMI info fail\n");
		goto SKIP_SCMI;
	}
	ret = of_property_read_u32(tinfo->sdev->dev.of_node, "scmi_ssc", &ssc_scmi_feature_id);

	pr_info("[SSC] ssc scmi id = %d\n", ssc_scmi_feature_id);

	scmi_tinysys_register_event_notifier(ssc_scmi_feature_id,
			(f_handler_t)ssc_notification_handler);

	ret = scmi_tinysys_event_notify(ssc_scmi_feature_id, 1);
	if (ret < 0)
		pr_info("[SSC] SCMI notify register fail\n");

	ret = of_property_read_u32(tinfo->sdev->dev.of_node, "scmi_plt", &plt_scmi_feature_id);
	ret = scmi_tinysys_common_set(tinfo->ph, plt_scmi_feature_id, PLT_SSC_INIT,
					0, 0, 0, 0);

	if (ret)
		ssc_aee_print("[SSC] SCMI common set fail!\n");
	else
		pr_info("[SSC] notify done!\n");
SKIP_SCMI:
#endif

	ssc_vlogic_bound_register_notifier(&ssc_vlogic_notifier_func);
	atomic_set(&vlogic_bound_counter, 0);
	/* create sysfs entry for voltage bound */
	ssc_kobj = kobject_create_and_add("ssc", kernel_kobj);

#ifdef SSC_SYSFS_VLOGIC_BOUND_SUPPORT
	if (ssc_kobj)
		ret = sysfs_create_file(ssc_kobj, __ATTR_OF(vlogic_bound));
#endif
	return 0;
}
static void __exit ssc_deinit(void)
{
	if (ssc_disable == 1)
		return;

#ifdef SSC_SYSFS_VLOGIC_BOUND_SUPPORT
	sysfs_remove_file(ssc_kobj, __ATTR_OF(vlogic_bound));
#endif
	kobject_del(ssc_kobj);
	ssc_vlogic_bound_unregister_notifier(&ssc_vlogic_notifier_func);
}

module_init(ssc_init);
module_exit(ssc_deinit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk ssc module");
MODULE_AUTHOR("MediaTek Inc.");
