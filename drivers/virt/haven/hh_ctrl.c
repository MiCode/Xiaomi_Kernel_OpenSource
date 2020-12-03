// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "haven: " fmt

#include <linux/arm-smccc.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/haven/hcall.h>
#include <linux/haven/hh_errno.h>

#define QC_HYP_SMCCC_CALL_UID                                                  \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,              \
			   ARM_SMCCC_OWNER_VENDOR_HYPERVISOR, 0xff01)
#define QC_HYP_SMCCC_REVISION                                                  \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,              \
			   ARM_SMCCC_OWNER_VENDOR_HYPERVISOR, 0xff03)

#define QC_HYP_UID0 0x19bd54bd
#define QC_HYP_UID1 0x0b37571b
#define QC_HYP_UID2 0x946f609b
#define QC_HYP_UID3 0x54539de6

#define HH_API_INFO_API_VERSION(x)	(((x) >> 0) & 0x3fff)
#define HH_API_INFO_BIG_ENDIAN(x)	(((x) >> 14) & 1)
#define HH_API_INFO_IS_64BIT(x)		(((x) >> 15) & 1)
#define HH_API_INFO_VARIANT(x)		(((x) >> 56) & 0xff)

#define HH_IDENTIFY_PARTITION_CSPACE(x)	(((x) >> 0) & 1)
#define HH_IDENTIFY_DOORBELL(x)		(((x) >> 1) & 1)
#define HH_IDENTIFY_MSGQUEUE(x)		(((x) >> 2) & 1)
#define HH_IDENTIFY_VIC(x)		(((x) >> 3) & 1)
#define HH_IDENTIFY_VPM(x)		(((x) >> 4) & 1)
#define HH_IDENTIFY_VCPU(x)		(((x) >> 5) & 1)
#define HH_IDENTIFY_MEMEXTENT(x)	(((x) >> 6) & 1)
#define HH_IDENTIFY_TRACE_CTRL(x)	(((x) >> 7) & 1)
#define HH_IDENTIFY_ROOTVM_CHANNEL(x)	(((x) >> 16) & 1)
#define HH_IDENTIFY_SCHEDULER(x)	(((x) >> 28) & 0xf)

static bool qc_hyp_calls;
static struct hh_hcall_hyp_identify_resp haven_api;

static ssize_t type_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buffer)
{
	return scnprintf(buffer, PAGE_SIZE, "haven\n");
}
static struct kobj_attribute type_attr = __ATTR_RO(type);

static ssize_t api_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buffer)
{
	return scnprintf(buffer, PAGE_SIZE, "%d\n",
		(int)HH_API_INFO_API_VERSION(haven_api.api_info));
}
static struct kobj_attribute api_attr = __ATTR_RO(api);

static ssize_t variant_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buffer)
{
	return scnprintf(buffer, PAGE_SIZE, "%d\n",
		(int)HH_API_INFO_VARIANT(haven_api.api_info));
}
static struct kobj_attribute variant_attr = __ATTR_RO(variant);

static struct attribute *version_attrs[] = { &api_attr.attr,
					     &variant_attr.attr, NULL };

static const struct attribute_group version_group = {
	.name = "version",
	.attrs = version_attrs,
};

static int __init hh_sysfs_register(void)
{
	int ret;

	ret = sysfs_create_file(hypervisor_kobj, &type_attr.attr);
	if (ret)
		return ret;

	return sysfs_create_group(hypervisor_kobj, &version_group);
}

static void __exit hh_sysfs_unregister(void)
{
	sysfs_remove_file(hypervisor_kobj, &type_attr.attr);
	sysfs_remove_group(hypervisor_kobj, &version_group);
}

#if defined(CONFIG_DEBUG_FS)

#define QC_HYP_SMCCC_UART_DISABLE                                              \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,              \
			   ARM_SMCCC_OWNER_VENDOR_HYPERVISOR, 0x0)
#define QC_HYP_SMCCC_UART_ENABLE                                              \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,              \
			   ARM_SMCCC_OWNER_VENDOR_HYPERVISOR, 0x1)
#define ENABLE 1
#define DISABLE 0

static struct dentry *hh_dbgfs_dir;
static int hyp_uart_enable;

static void hh_control_hyp_uart(int val)
{
	switch (val) {
	case ENABLE:
	if (!hyp_uart_enable) {
		hyp_uart_enable = val;
		pr_info("Haven: enabling HYP UART\n");
		arm_smccc_1_1_smc(QC_HYP_SMCCC_UART_ENABLE, NULL);
	} else {
		pr_info("Haven: HYP UART already enabled\n");
	}
	break;
	case DISABLE:
	if (hyp_uart_enable) {
		hyp_uart_enable = val;
		pr_info("Haven: disabling HYP UART\n");
		arm_smccc_1_1_smc(QC_HYP_SMCCC_UART_DISABLE, NULL);
	} else {
		pr_info("Haven: HYP UART already disabled\n");
	}
	break;
	default:
		pr_info("Haven: supported values disable(0)/enable(1)\n");
	}
}

static int hh_dbgfs_trace_class_set(void *data, u64 val)
{
	return hh_remap_error(hh_hcall_trace_update_class_flags(val, 0, NULL));
}

static int hh_dbgfs_trace_class_clear(void *data, u64 val)
{
	return hh_remap_error(hh_hcall_trace_update_class_flags(0, val, NULL));
}

static int hh_dbgfs_trace_class_get(void *data, u64 *val)
{
	*val = 0;
	return hh_remap_error(hh_hcall_trace_update_class_flags(0, 0, val));
}

static int hh_dbgfs_hyp_uart_set(void *data, u64 val)
{
	hh_control_hyp_uart(val);
	return 0;
}

static int hh_dbgfs_hyp_uart_get(void *data, u64 *val)
{
	*val = hyp_uart_enable;
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(hh_dbgfs_trace_class_set_fops,
			 hh_dbgfs_trace_class_get,
			 hh_dbgfs_trace_class_set,
			 "0x%llx\n");

DEFINE_DEBUGFS_ATTRIBUTE(hh_dbgfs_trace_class_clear_fops,
			 hh_dbgfs_trace_class_get,
			 hh_dbgfs_trace_class_clear,
			 "0x%llx\n");

DEFINE_DEBUGFS_ATTRIBUTE(hh_dbgfs_hyp_uart_ctrl_fops,
			 hh_dbgfs_hyp_uart_get,
			 hh_dbgfs_hyp_uart_set,
			 "0x%llx\n");

static int __init hh_dbgfs_register(void)
{
	struct dentry *dentry;

	hh_dbgfs_dir = debugfs_create_dir("haven", NULL);
	if (IS_ERR_OR_NULL(hh_dbgfs_dir))
		return PTR_ERR(hh_dbgfs_dir);

	if (HH_IDENTIFY_TRACE_CTRL(haven_api.flags[0])) {
		dentry = debugfs_create_file("trace_set", 0600, hh_dbgfs_dir,
					NULL, &hh_dbgfs_trace_class_set_fops);
		if (IS_ERR(dentry))
			return PTR_ERR(dentry);

		dentry = debugfs_create_file("trace_clear", 0600, hh_dbgfs_dir,
					NULL, &hh_dbgfs_trace_class_clear_fops);
		if (IS_ERR(dentry))
			return PTR_ERR(dentry);

		dentry = debugfs_create_file("hyp_uart_ctrl", 0600, hh_dbgfs_dir,
					NULL, &hh_dbgfs_hyp_uart_ctrl_fops);
		if (IS_ERR(dentry))
			return PTR_ERR(dentry);
	}

	return 0;
}

static void __exit hh_dbgfs_unregister(void)
{
	debugfs_remove_recursive(hh_dbgfs_dir);
}
#else /* !defined (CONFIG_DEBUG_FS) */
static inline int hh_dbgfs_register(void) { return 0; }
static inline void hh_dbgfs_unregister(void) { return; }
#endif

static int __init hh_ctrl_init(void)
{
	int ret;
	struct device_node *hyp;
	struct arm_smccc_res res;

	hyp = of_find_node_by_path("/hypervisor");

	if (!hyp || !of_device_is_compatible(hyp, "qcom,haven-hypervisor"))
		return -ENODEV;

	(void)hh_hcall_hyp_identify(&haven_api);

	if (HH_API_INFO_API_VERSION(haven_api.api_info) != 1) {
		pr_err("unknown version\n");
		return -ENODEV;
	}

	/* Check for ARM SMCCC VENDOR_HYP service calls by UID. */
	arm_smccc_1_1_smc(QC_HYP_SMCCC_CALL_UID, &res);
	if ((res.a0 == QC_HYP_UID0) && (res.a1 == QC_HYP_UID1) &&
	    (res.a2 == QC_HYP_UID2) && (res.a3 == QC_HYP_UID3))
		qc_hyp_calls = true;

	if (qc_hyp_calls) {
		ret = hh_sysfs_register();
		if (ret)
			return ret;

		ret = hh_dbgfs_register();
		if (ret)
			pr_warn("failed to register dbgfs: %d\n", ret);
	} else {
		pr_info("Haven: no QC HYP interface detected\n");
	}

	return 0;
}
module_init(hh_ctrl_init);

static void __exit hh_ctrl_exit(void)
{
	hh_sysfs_unregister();
	hh_dbgfs_unregister();
}
module_exit(hh_ctrl_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Haven Hypervisor Control Driver");
