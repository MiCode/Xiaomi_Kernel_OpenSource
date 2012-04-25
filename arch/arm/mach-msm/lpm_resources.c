/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/cpu.h>
#include <mach/mpm.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include "spm.h"
#include "lpm_resources.h"
#include "rpm-notifier.h"
#include <mach/rpm-smd.h>
#include "idle.h"

/*Debug Definitions*/
enum {
	MSM_LPMRS_DEBUG_RPM = BIT(0),
	MSM_LPMRS_DEBUG_PXO = BIT(1),
	MSM_LPMRS_DEBUG_VDD_DIG = BIT(2),
	MSM_LPMRS_DEBUG_VDD_MEM = BIT(3),
	MSM_LPMRS_DEBUG_L2 = BIT(4),
	MSM_LPMRS_DEBUG_LVLS = BIT(5),
};

static int msm_lpm_debug_mask;
module_param_named(
	debug_mask, msm_lpm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

static bool msm_lpm_get_rpm_notif = true;

/*Macros*/
#define VDD_DIG_ACTIVE		(950000)
#define VDD_MEM_ACTIVE		(1050000)
#define MAX_RS_NAME		(16)
#define MAX_RS_SIZE		(4)
#define IS_RPM_CTL(rs) \
	(!strncmp(rs->name, "rpm_ctl", MAX_RS_NAME))

static bool msm_lpm_beyond_limits_vdd_dig(struct msm_rpmrs_limits *limits);
static void msm_lpm_aggregate_vdd_dig(struct msm_rpmrs_limits *limits);
static void msm_lpm_flush_vdd_dig(int notify_rpm);
static void msm_lpm_notify_vdd_dig(struct msm_rpm_notifier_data
					*rpm_notifier_cb);

static bool msm_lpm_beyond_limits_vdd_mem(struct msm_rpmrs_limits *limits);
static void msm_lpm_aggregate_vdd_mem(struct msm_rpmrs_limits *limits);
static void msm_lpm_flush_vdd_mem(int notify_rpm);
static void msm_lpm_notify_vdd_mem(struct msm_rpm_notifier_data
					*rpm_notifier_cb);

static bool msm_lpm_beyond_limits_pxo(struct msm_rpmrs_limits *limits);
static void msm_lpm_aggregate_pxo(struct msm_rpmrs_limits *limits);
static void msm_lpm_flush_pxo(int notify_rpm);
static void msm_lpm_notify_pxo(struct msm_rpm_notifier_data
					*rpm_notifier_cb);


static bool msm_lpm_beyond_limits_l2(struct msm_rpmrs_limits *limits);
static void msm_lpm_flush_l2(int notify_rpm);
static void msm_lpm_aggregate_l2(struct msm_rpmrs_limits *limits);

static void msm_lpm_flush_rpm_ctl(int notify_rpm);

static int msm_lpm_rpm_callback(struct notifier_block *rpm_nb,
				unsigned long action, void *rpm_notif);

static int msm_lpm_cpu_callback(struct notifier_block *cpu_nb,
				unsigned long action, void *hcpu);

static ssize_t msm_lpm_resource_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t msm_lpm_resource_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);


#define RPMRS_ATTR(_name) \
	__ATTR(_name, S_IRUGO|S_IWUSR, \
		msm_lpm_resource_attr_show, msm_lpm_resource_attr_store)

/*Data structures*/
struct msm_lpm_rs_data {
	uint32_t type;
	uint32_t id;
	uint32_t key;
	uint32_t value;
	uint32_t default_value;
	struct msm_rpm_request *handle;
};

struct msm_lpm_resource {
	struct msm_lpm_rs_data rs_data;
	uint32_t sleep_value;
	char name[MAX_RS_NAME];

	uint32_t  enable_low_power;
	bool valid;

	bool (*beyond_limits)(struct msm_rpmrs_limits *limits);
	void (*aggregate)(struct msm_rpmrs_limits *limits);
	void (*flush)(int notify_rpm);
	void (*notify)(struct msm_rpm_notifier_data *rpm_notifier_cb);
	struct kobj_attribute ko_attr;
};


static struct msm_lpm_resource msm_lpm_l2 = {
	.name = "l2",
	.beyond_limits = msm_lpm_beyond_limits_l2,
	.aggregate = msm_lpm_aggregate_l2,
	.flush = msm_lpm_flush_l2,
	.notify = NULL,
	.valid = true,
	.rs_data = {
		.value = MSM_LPM_L2_CACHE_ACTIVE,
		.default_value = MSM_LPM_L2_CACHE_ACTIVE,
	},
	.ko_attr = RPMRS_ATTR(l2),
};

static struct msm_lpm_resource msm_lpm_vdd_dig = {
	.name = "vdd-dig",
	.beyond_limits = msm_lpm_beyond_limits_vdd_dig,
	.aggregate = msm_lpm_aggregate_vdd_dig,
	.flush = msm_lpm_flush_vdd_dig,
	.notify = msm_lpm_notify_vdd_dig,
	.valid = false,
	.rs_data = {
		.value = VDD_DIG_ACTIVE,
		.default_value = VDD_DIG_ACTIVE,
	},
	.ko_attr = RPMRS_ATTR(vdd_dig),
};

static struct msm_lpm_resource msm_lpm_vdd_mem = {
	.name = "vdd-mem",
	.beyond_limits = msm_lpm_beyond_limits_vdd_mem,
	.aggregate = msm_lpm_aggregate_vdd_mem,
	.flush = msm_lpm_flush_vdd_mem,
	.notify = msm_lpm_notify_vdd_mem,
	.valid = false,
	.rs_data = {
		.value = VDD_MEM_ACTIVE,
		.default_value = VDD_MEM_ACTIVE,
	},
	.ko_attr = RPMRS_ATTR(vdd_mem),
};

static struct msm_lpm_resource msm_lpm_pxo = {
	.name = "pxo",
	.beyond_limits = msm_lpm_beyond_limits_pxo,
	.aggregate = msm_lpm_aggregate_pxo,
	.flush = msm_lpm_flush_pxo,
	.notify = msm_lpm_notify_pxo,
	.valid = false,
	.rs_data = {
		.value = MSM_LPM_PXO_ON,
		.default_value = MSM_LPM_PXO_ON,
	},
	.ko_attr = RPMRS_ATTR(pxo),
};

static struct msm_lpm_resource *msm_lpm_resources[] = {
	&msm_lpm_vdd_dig,
	&msm_lpm_vdd_mem,
	&msm_lpm_pxo,
	&msm_lpm_l2,
};

static struct msm_lpm_resource msm_lpm_rpm_ctl = {
	.name = "rpm_ctl",
	.beyond_limits = NULL,
	.aggregate = NULL,
	.flush = msm_lpm_flush_rpm_ctl,
	.valid = true,
	.ko_attr = RPMRS_ATTR(rpm_ctl),
};

static struct notifier_block msm_lpm_rpm_nblk = {
	.notifier_call = msm_lpm_rpm_callback,
};

static struct notifier_block __refdata msm_lpm_cpu_nblk = {
	.notifier_call = msm_lpm_cpu_callback,
};

static DEFINE_SPINLOCK(msm_lpm_sysfs_lock);

/* Attribute Definitions */
static struct attribute *msm_lpm_attributes[] = {
	&msm_lpm_vdd_dig.ko_attr.attr,
	&msm_lpm_vdd_mem.ko_attr.attr,
	&msm_lpm_pxo.ko_attr.attr,
	&msm_lpm_l2.ko_attr.attr,
	NULL,
};

static struct attribute_group msm_lpm_attribute_group = {
	.attrs = msm_lpm_attributes,
};

static struct attribute *msm_lpm_rpm_ctl_attribute[] = {
	&msm_lpm_rpm_ctl.ko_attr.attr,
	NULL,
};

static struct attribute_group msm_lpm_rpm_ctl_attr_group = {
	.attrs = msm_lpm_rpm_ctl_attribute,
};

#define GET_RS_FROM_ATTR(attr) \
	(container_of(attr, struct msm_lpm_resource, ko_attr))

/* RPM */
static struct msm_rpm_request *msm_lpm_create_rpm_request
				(uint32_t rsc_type, uint32_t rsc_id)
{
	struct msm_rpm_request *handle = NULL;

	handle = msm_rpm_create_request(MSM_RPM_CTX_SLEEP_SET,
						rsc_type,
						rsc_id, 1);
	return handle;
}

static int msm_lpm_send_sleep_data(struct msm_rpm_request *handle,
					uint32_t key, uint8_t *value)
{
	int ret = 0;

	if (!handle)
		return ret;

	ret = msm_rpm_add_kvp_data_noirq(handle, key, value, MAX_RS_SIZE);

	if (ret < 0) {
		pr_err("%s: Error adding kvp data key %u, size %d\n",
				__func__, key, MAX_RS_SIZE);
		return ret;
	}

	ret = msm_rpm_send_request_noirq(handle);
	if (ret < 0) {
		pr_err("%s: Error sending RPM request key %u, handle 0x%x\n",
				__func__, key, (unsigned int)handle);
		return ret;
	}
	if (msm_lpm_debug_mask & MSM_LPMRS_DEBUG_RPM)
		pr_info("Rs key %u, value %u, size %d\n", key,
				*(unsigned int *)value, MAX_RS_SIZE);
	return ret;
}

/* RPM Notifier */
static int msm_lpm_rpm_callback(struct notifier_block *rpm_nb,
					unsigned long action,
					void *rpm_notif)
{
	int i;
	struct msm_lpm_resource *rs = NULL;
	struct msm_rpm_notifier_data *rpm_notifier_cb =
			(struct msm_rpm_notifier_data *)rpm_notif;

	if (!msm_lpm_get_rpm_notif)
		return NOTIFY_DONE;

	if (!(rpm_nb && rpm_notif))
		return NOTIFY_BAD;

	for (i = 0; i < ARRAY_SIZE(msm_lpm_resources); i++) {
		rs = msm_lpm_resources[i];
		if (rs && rs->valid && rs->notify)
			rs->notify(rpm_notifier_cb);
	}

	return NOTIFY_OK;
}

/* SYSFS */
static ssize_t msm_lpm_resource_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct kernel_param kp;
	unsigned long flags;
	unsigned int temp;
	int rc;

	spin_lock_irqsave(&msm_lpm_sysfs_lock, flags);
	temp = GET_RS_FROM_ATTR(attr)->enable_low_power;
	spin_unlock_irqrestore(&msm_lpm_sysfs_lock, flags);

	kp.arg = &temp;
	rc = param_get_uint(buf, &kp);

	if (rc > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		rc++;
	}

	return rc;
}

static ssize_t msm_lpm_resource_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct kernel_param kp;
	unsigned long flags;
	unsigned int temp;
	int rc;

	kp.arg = &temp;
	rc = param_set_uint(buf, &kp);
	if (rc)
		return rc;

	spin_lock_irqsave(&msm_lpm_sysfs_lock, flags);
	GET_RS_FROM_ATTR(attr)->enable_low_power = temp;

	if (IS_RPM_CTL(GET_RS_FROM_ATTR(attr))) {
		struct msm_lpm_resource *rs = GET_RS_FROM_ATTR(attr);
		rs->flush(false);
	}

	spin_unlock_irqrestore(&msm_lpm_sysfs_lock, flags);

	return count;
}

/* lpm resource handling functions */
/* Common */
static void msm_lpm_notify_common(struct msm_rpm_notifier_data *rpm_notifier_cb,
				struct msm_lpm_resource *rs)
{
	if ((rpm_notifier_cb->rsc_type == rs->rs_data.type) &&
			(rpm_notifier_cb->rsc_id == rs->rs_data.id) &&
			(rpm_notifier_cb->key == rs->rs_data.key)) {
		BUG_ON(rpm_notifier_cb->size > MAX_RS_SIZE);

		if (rs->valid) {
			if (rpm_notifier_cb->value)
				memcpy(&rs->rs_data.value,
				rpm_notifier_cb->value, rpm_notifier_cb->size);
			else
				rs->rs_data.value = rs->rs_data.default_value;

			if (msm_lpm_debug_mask & MSM_LPMRS_DEBUG_RPM)
				pr_info("Notification received Rs %s value %u\n",
						rs->name, rs->rs_data.value);
		}
	}
}

/* L2 */
static bool msm_lpm_beyond_limits_l2(struct msm_rpmrs_limits *limits)
{
	uint32_t l2;
	bool ret = true;
	struct msm_lpm_resource *rs = &msm_lpm_l2;

	if (rs->valid) {
		uint32_t l2_buf = rs->rs_data.value;

		if (rs->enable_low_power == 1)
			l2 = MSM_LPM_L2_CACHE_GDHS;
		else if (rs->enable_low_power == 2)
			l2 = MSM_LPM_L2_CACHE_HSFS_OPEN;
		else
			l2 = MSM_LPM_L2_CACHE_ACTIVE ;

		if (l2_buf > l2)
			l2 = l2_buf;
		ret = (l2 > limits->l2_cache);

		if (msm_lpm_debug_mask & MSM_LPMRS_DEBUG_L2)
			pr_info("%s: l2 buf %u, l2 %u, limits %u\n",
				__func__, l2_buf, l2, limits->l2_cache);
	}
	return ret;
}

static void msm_lpm_aggregate_l2(struct msm_rpmrs_limits *limits)
{
	struct msm_lpm_resource *rs = &msm_lpm_l2;

	if (rs->valid)
		rs->sleep_value = limits->l2_cache;
}

static void msm_lpm_flush_l2(int notify_rpm)
{
	struct msm_lpm_resource *rs = &msm_lpm_l2;
	int lpm;
	int rc;

	switch (rs->sleep_value) {
	case MSM_LPM_L2_CACHE_HSFS_OPEN:
		lpm = MSM_SPM_L2_MODE_POWER_COLLAPSE;
		msm_pm_set_l2_flush_flag(1);
		break;
	case MSM_LPM_L2_CACHE_GDHS:
		lpm = MSM_SPM_L2_MODE_GDHS;
		break;
	case MSM_LPM_L2_CACHE_RETENTION:
		lpm = MSM_SPM_L2_MODE_RETENTION;
		break;
	default:
	case MSM_LPM_L2_CACHE_ACTIVE:
		lpm = MSM_SPM_L2_MODE_DISABLED;
		break;
	}

	rc = msm_spm_l2_set_low_power_mode(lpm, notify_rpm);

	if (rc < 0)
		pr_err("%s: Failed to set L2 low power mode %d",
			__func__, lpm);

	if (msm_lpm_debug_mask & MSM_LPMRS_DEBUG_L2)
		pr_info("%s: Requesting low power mode %d\n",
				__func__, lpm);
}

/* RPM CTL */
static void msm_lpm_flush_rpm_ctl(int notify_rpm)
{
	struct msm_lpm_resource *rs = &msm_lpm_rpm_ctl;
	msm_lpm_send_sleep_data(rs->rs_data.handle,
				rs->rs_data.key,
				(uint8_t *)&rs->sleep_value);
}

/*VDD Dig*/
static bool msm_lpm_beyond_limits_vdd_dig(struct msm_rpmrs_limits *limits)
{
	bool ret = true;
	struct msm_lpm_resource *rs = &msm_lpm_vdd_dig;

	if (rs->valid) {
		uint32_t vdd_buf = rs->rs_data.value;
		uint32_t vdd_dig = rs->enable_low_power ? rs->enable_low_power :
					rs->rs_data.default_value;

		if (vdd_buf > vdd_dig)
			vdd_dig = vdd_buf;

		ret = (vdd_dig > limits->vdd_dig_upper_bound);

		if (msm_lpm_debug_mask & MSM_LPMRS_DEBUG_VDD_DIG)
			pr_info("%s:buf %d vdd dig %d limits%d\n",
				__func__, vdd_buf, vdd_dig,
				limits->vdd_dig_upper_bound);
	}
	return ret;
}

static void msm_lpm_aggregate_vdd_dig(struct msm_rpmrs_limits *limits)
{
	struct msm_lpm_resource *rs = &msm_lpm_vdd_dig;

	if (rs->valid) {
		uint32_t vdd_buf = rs->rs_data.value;
		if (limits->vdd_dig_lower_bound > vdd_buf)
			rs->sleep_value = limits->vdd_dig_lower_bound;
		else
			rs->sleep_value = vdd_buf;
	}
}

static void msm_lpm_flush_vdd_dig(int notify_rpm)
{
	if (notify_rpm) {
		struct msm_lpm_resource *rs = &msm_lpm_vdd_dig;
		msm_lpm_send_sleep_data(rs->rs_data.handle,
					rs->rs_data.key,
					(uint8_t *)&rs->sleep_value);
	}
}

static void msm_lpm_notify_vdd_dig(struct msm_rpm_notifier_data
					*rpm_notifier_cb)
{
	struct msm_lpm_resource *rs = &msm_lpm_vdd_dig;
	msm_lpm_notify_common(rpm_notifier_cb, rs);
}

/*VDD Mem*/
static bool msm_lpm_beyond_limits_vdd_mem(struct msm_rpmrs_limits *limits)
{
	bool ret = true;
	struct msm_lpm_resource *rs = &msm_lpm_vdd_mem;

	if (rs->valid) {
		uint32_t vdd_buf = rs->rs_data.value;
		uint32_t vdd_mem = rs->enable_low_power ? rs->enable_low_power :
					rs->rs_data.default_value;

		if (vdd_buf > vdd_mem)
			vdd_mem = vdd_buf;

		ret = (vdd_mem > limits->vdd_mem_upper_bound);

		if (msm_lpm_debug_mask & MSM_LPMRS_DEBUG_VDD_MEM)
			pr_info("%s:buf %d vdd mem %d limits%d\n",
				__func__, vdd_buf, vdd_mem,
				limits->vdd_mem_upper_bound);
	}
	return ret;
}

static void msm_lpm_aggregate_vdd_mem(struct msm_rpmrs_limits *limits)
{
	struct msm_lpm_resource *rs = &msm_lpm_vdd_mem;

	if (rs->valid) {
		uint32_t vdd_buf = rs->rs_data.value;
		if (limits->vdd_mem_lower_bound > vdd_buf)
			rs->sleep_value = limits->vdd_mem_lower_bound;
		else
			rs->sleep_value = vdd_buf;
	}
}

static void msm_lpm_flush_vdd_mem(int notify_rpm)
{
	if (notify_rpm) {
		struct msm_lpm_resource *rs = &msm_lpm_vdd_mem;
		msm_lpm_send_sleep_data(rs->rs_data.handle,
					rs->rs_data.key,
					(uint8_t *)&rs->sleep_value);
	}
}

static void msm_lpm_notify_vdd_mem(struct msm_rpm_notifier_data
					*rpm_notifier_cb)
{
	struct msm_lpm_resource *rs = &msm_lpm_vdd_mem;
	msm_lpm_notify_common(rpm_notifier_cb, rs);
}

/*PXO*/
static bool msm_lpm_beyond_limits_pxo(struct msm_rpmrs_limits *limits)
{
	bool ret = true;
	struct msm_lpm_resource *rs = &msm_lpm_pxo;

	if (rs->valid) {
		uint32_t pxo_buf = rs->rs_data.value;
		uint32_t pxo = rs->enable_low_power ? MSM_LPM_PXO_OFF :
					rs->rs_data.default_value;

		if (pxo_buf > pxo)
			pxo = pxo_buf;

		ret = (pxo > limits->pxo);

		if (msm_lpm_debug_mask & MSM_LPMRS_DEBUG_PXO)
			pr_info("%s:pxo buf %d pxo %d limits pxo %d\n",
					__func__, pxo_buf, pxo, limits->pxo);
	}
	return ret;
}

static void msm_lpm_aggregate_pxo(struct msm_rpmrs_limits *limits)
{
	struct msm_lpm_resource *rs = &msm_lpm_pxo;

	if (rs->valid) {
		uint32_t pxo_buf = rs->rs_data.value;
		if (limits->pxo > pxo_buf)
			rs->sleep_value = limits->pxo;
		else
			rs->sleep_value = pxo_buf;

		if (msm_lpm_debug_mask & MSM_LPMRS_DEBUG_PXO)
			pr_info("%s: pxo buf %d sleep value %d\n",
					__func__, pxo_buf, rs->sleep_value);
	}
}

static void msm_lpm_flush_pxo(int notify_rpm)
{
	if (notify_rpm) {
		struct msm_lpm_resource *rs = &msm_lpm_pxo;
		msm_lpm_send_sleep_data(rs->rs_data.handle,
					rs->rs_data.key,
					(uint8_t *)&rs->sleep_value);
	}
}

static void msm_lpm_notify_pxo(struct msm_rpm_notifier_data
					*rpm_notifier_cb)
{
	struct msm_lpm_resource *rs = &msm_lpm_pxo;
	msm_lpm_notify_common(rpm_notifier_cb, rs);
}

/* MPM
static bool msm_lpm_use_mpm(struct msm_rpmrs_limits *limits)
{
	return ((limits->pxo == MSM_LPM_PXO_OFF) ||
		(limits->vdd_dig_lower_bound <= VDD_DIG_RET_HIGH));
}*/

/* LPM levels interface */
bool msm_lpm_level_beyond_limit(struct msm_rpmrs_limits *limits)
{
	int i;
	struct msm_lpm_resource *rs;
	bool beyond_limit = false;

	for (i = 0; i < ARRAY_SIZE(msm_lpm_resources); i++) {
		rs = msm_lpm_resources[i];
		if (rs->beyond_limits && rs->beyond_limits(limits)) {
			beyond_limit = true;
			if (msm_lpm_debug_mask & MSM_LPMRS_DEBUG_LVLS)
				pr_info("%s: %s beyond limit", __func__,
						rs->name);
			break;
		}
	}

	return beyond_limit;
}

int msm_lpmrs_enter_sleep(struct msm_rpmrs_limits *limits,
				bool from_idle, bool notify_rpm)
{
	int ret = 0;
	int i;
	struct msm_lpm_resource *rs = NULL;

	for (i = 0; i < ARRAY_SIZE(msm_lpm_resources); i++) {
		rs = msm_lpm_resources[i];
		if (rs->aggregate)
			rs->aggregate(limits);
	}

	msm_lpm_get_rpm_notif = false;
	for (i = 0; i < ARRAY_SIZE(msm_lpm_resources); i++) {
		rs = msm_lpm_resources[i];
		if (rs->flush)
			rs->flush(notify_rpm);
	}
	msm_lpm_get_rpm_notif = true;

	/* MPM Enter sleep
	if (msm_lpm_use_mpm(limits))
		msm_mpm_enter_sleep(from_idle);*/

	return ret;
}

void msm_lpmrs_exit_sleep(uint32_t sclk_count, struct msm_rpmrs_limits *limits,
		bool from_idle, bool notify_rpm)
{
	/* MPM exit sleep
	if (msm_lpm_use_mpm(limits))
		msm_mpm_exit_sleep(from_idle);*/
}

static int msm_lpm_cpu_callback(struct notifier_block *cpu_nb,
		unsigned long action, void *hcpu)
{
	struct msm_lpm_resource *rs = &msm_lpm_l2;
	switch (action) {
	case CPU_ONLINE_FROZEN:
	case CPU_ONLINE:
		if (num_online_cpus() > 1)
			rs->rs_data.value = MSM_LPM_L2_CACHE_ACTIVE;
		break;
	case CPU_DEAD_FROZEN:
	case CPU_DEAD:
		if (num_online_cpus() == 1)
			rs->rs_data.value = MSM_LPM_L2_CACHE_GDHS;
		break;
	}
	return NOTIFY_OK;
}

/* RPM CTL */
static int __devinit msm_lpm_init_rpm_ctl(void)
{
	struct msm_lpm_resource *rs = &msm_lpm_rpm_ctl;

	rs->rs_data.handle = msm_rpm_create_request(
				MSM_RPM_CTX_ACTIVE_SET,
				rs->rs_data.type,
				rs->rs_data.id, 1);
	if (!rs->rs_data.handle)
		return -EIO;

	rs->valid = true;
	return 0;
}

static int __devinit msm_lpm_resource_sysfs_add(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *low_power_kobj = NULL;
	struct kobject *mode_kobj = NULL;
	int rc = 0;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("%s: cannot find kobject for module %s\n",
			__func__, KBUILD_MODNAME);
		rc = -ENOENT;
		goto resource_sysfs_add_exit;
	}

	low_power_kobj = kobject_create_and_add(
				"enable_low_power", module_kobj);
	if (!low_power_kobj) {
		pr_err("%s: cannot create kobject\n", __func__);
		rc = -ENOMEM;
		goto resource_sysfs_add_exit;
	}

	mode_kobj = kobject_create_and_add(
				"mode", module_kobj);
	if (!mode_kobj) {
		pr_err("%s: cannot create kobject\n", __func__);
		rc = -ENOMEM;
		goto resource_sysfs_add_exit;
	}

	rc = sysfs_create_group(low_power_kobj, &msm_lpm_attribute_group);
	if (rc) {
		pr_err("%s: cannot create kobject attribute group\n", __func__);
		goto resource_sysfs_add_exit;
	}

	rc = sysfs_create_group(mode_kobj, &msm_lpm_rpm_ctl_attr_group);
	if (rc) {
		pr_err("%s: cannot create kobject attribute group\n", __func__);
		goto resource_sysfs_add_exit;
	}

resource_sysfs_add_exit:
	if (rc) {
		if (low_power_kobj)
			sysfs_remove_group(low_power_kobj,
					&msm_lpm_attribute_group);
		kobject_del(low_power_kobj);
		kobject_del(mode_kobj);
	}

	return rc;
}

late_initcall(msm_lpm_resource_sysfs_add);

static int __devinit msm_lpmrs_probe(struct platform_device *pdev)
{
	struct device_node *node = NULL;
	char *key = NULL;
	int ret = 0;

	for_each_child_of_node(pdev->dev.of_node, node) {
		struct msm_lpm_resource *rs = NULL;
		const char *val;
		int i;

		key = "qcom,name";
		ret = of_property_read_string(node, key, &val);
		if (ret) {
			pr_err("Cannot read string\n");
			goto fail;
		}

		for (i = 0; i < ARRAY_SIZE(msm_lpm_resources); i++) {
			char *lpmrs_name = msm_lpm_resources[i]->name;
			if (!msm_lpm_resources[i]->valid &&
				!strncmp(val, lpmrs_name, strnlen(lpmrs_name,
							MAX_RS_NAME))) {
				rs = msm_lpm_resources[i];
				break;
			}
		}

		if (!rs) {
			pr_err("LPM resource not found\n");
			continue;
		}

		key = "qcom,type";
		ret = of_property_read_u32(node, key, &rs->rs_data.type);
		if (ret) {
			pr_err("Failed to read type\n");
			goto fail;
		}

		key = "qcom,id";
		ret = of_property_read_u32(node, key, &rs->rs_data.id);
		if (ret) {
			pr_err("Failed to read id\n");
			goto fail;
		}

		key = "qcom,key";
		ret = of_property_read_u32(node, key, &rs->rs_data.key);
		if (ret) {
			pr_err("Failed to read key\n");
			goto fail;
		}

		rs->rs_data.handle = msm_lpm_create_rpm_request(
					rs->rs_data.type, rs->rs_data.id);

		if (!rs->rs_data.handle) {
			pr_err("%s: Failed to allocate handle for %s\n",
					__func__, rs->name);
			ret = -1;
			goto fail;
		}

		rs->valid = true;
	}
	msm_rpm_register_notifier(&msm_lpm_rpm_nblk);
	msm_lpm_init_rpm_ctl();
	register_hotcpu_notifier(&msm_lpm_cpu_nblk);
	/* For UP mode, set the default to HSFS OPEN*/
	if (num_possible_cpus() == 1) {
		msm_lpm_l2.rs_data.default_value = MSM_LPM_L2_CACHE_HSFS_OPEN;
		msm_lpm_l2.rs_data.value = MSM_LPM_L2_CACHE_HSFS_OPEN;
	}
	return 0;
fail:
	return ret;
}

static struct of_device_id msm_lpmrs_match_table[] = {
	{.compatible = "qcom,lpm-resources"},
	{},
};

static struct platform_driver msm_lpmrs_driver = {
	.probe = msm_lpmrs_probe,
	.driver = {
		.name = "lpm-resources",
		.owner = THIS_MODULE,
		.of_match_table = msm_lpmrs_match_table,
	},
};

int __init msm_lpmrs_module_init(void)
{
	return platform_driver_register(&msm_lpmrs_driver);
}
