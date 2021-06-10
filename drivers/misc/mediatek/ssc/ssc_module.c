// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/atomic.h>
#include <linux/scmi_protocol.h>
#include <tinysys-scmi.h>
#include <ssc_module.h>
#include <mt-plat/aee.h>
#include <mt-plat/ssc.h>

/* FIXME */
#define SSC_SCMI_FEATURE_ID 0x10

#define MTK_SSC_DTS_COMPATIBLE "mediatek,ssc"

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
static BLOCKING_NOTIFIER_HEAD(vlogic_bound_chain);

static int xxx_vlogic_bound_event(struct notifier_block *notifier, unsigned long event,
				void *data)
{

	unsigned int request_id = *((unsigned int*)data);

	pr_info("[SSC] request ID = 0x%x\n", request_id);

	switch(event) {
		case SSC_ENABLE_VLOGIC_BOUND:
			return NOTIFY_DONE;
		case SSC_DISABLE_VLOGIC_BOUND:
			return NOTIFY_DONE;
		default:
			return NOTIFY_BAD;
	}
	return NOTIFY_OK;
}
static struct notifier_block ssc_vlogic_notifier_func = {
	.notifier_call = xxx_vlogic_bound_event,
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


static atomic_t vlogic_bound_counter;
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

static unsigned int safe_vlogic_level = 0xFFFFFFFF;
unsigned int ssc_get_safe_vlogic_level(void)
{
	return safe_vlogic_level;
}

static char* ssc_timeout_name[] = {
	"ISP_TIMEOUT",
	"VCORE_TIMEOUT",
	"APU_TIMEOUT",
	"GPU_TIMEOUT",
};

static char*  ssc_get_timeout_id_name(unsigned int ssc_timeout_id)
{
	return ssc_timeout_name[ssc_timeout_id-1];
}


static void ssc_notification_handler(u32 feature_id, scmi_tinysys_report *report)
{
	char* timeout_name;
	pr_info("[SSC] %s\n", __func__);

	timeout_name = ssc_get_timeout_id_name(report->p1);

	ssc_aee_print("[SSC] timeout!\n CRDISPATCH_KEY: SSC_%s VIOLATION\n", timeout_name);
	return;
}

static DEFINE_SPINLOCK(ssc_locker);
static int __init ssc_init(void)
{
	struct device_node *ssc_node;
	int ret;
	unsigned long flags;

	pr_info("[SSC] %s\n", __func__);

	spin_lock_irqsave(&ssc_locker, flags);

	ssc_node = of_find_compatible_node(NULL, NULL, MTK_SSC_DTS_COMPATIBLE);

	if (ssc_node) {
		ret = of_property_read_u32(ssc_node,
				MTK_SSC_SAFE_VLOGIC_STRING,
				&safe_vlogic_level);

		/* This property is not defined*/
		if (ret)
			safe_vlogic_level = 0xFFFFFFFF;

		of_node_put(ssc_node);
	}
	spin_unlock_irqrestore(&ssc_locker, flags);

	scmi_tinysys_register_event_notifier(SSC_SCMI_FEATURE_ID,
			(f_handler_t)ssc_notification_handler);

	scmi_tinysys_event_notify(SSC_SCMI_FEATURE_ID, 1);

	ssc_vlogic_bound_register_notifier(&ssc_vlogic_notifier_func);

	atomic_set(&vlogic_bound_counter, 0);
	return 0;
}
static void __exit ssc_deinit(void)
{
	return;
}

module_init(ssc_init);
module_exit(ssc_deinit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk ssc module");
MODULE_AUTHOR("MediaTek Inc.");
