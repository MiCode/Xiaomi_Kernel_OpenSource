/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/cpu.h>
#include <mach/rpm.h>
#include <mach/msm_iomap.h>
#include <asm/mach-types.h>
#include <linux/io.h>
#include <mach/socinfo.h>
#include <mach/mpm.h>
#include "rpm_resources.h"
#include "spm.h"
#include "idle.h"

/******************************************************************************
 * Debug Definitions
 *****************************************************************************/

enum {
	MSM_RPMRS_DEBUG_OUTPUT = BIT(0),
	MSM_RPMRS_DEBUG_BUFFER = BIT(1),
};

static int msm_rpmrs_debug_mask;
module_param_named(
	debug_mask, msm_rpmrs_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

static struct msm_rpmrs_level *msm_rpmrs_levels;
static int msm_rpmrs_level_count;

static bool msm_rpmrs_pxo_beyond_limits(struct msm_rpmrs_limits *limits);
static void msm_rpmrs_aggregate_pxo(struct msm_rpmrs_limits *limits);
static void msm_rpmrs_restore_pxo(void);
static bool msm_rpmrs_l2_cache_beyond_limits(struct msm_rpmrs_limits *limits);
static void msm_rpmrs_aggregate_l2_cache(struct msm_rpmrs_limits *limits);
static void msm_rpmrs_restore_l2_cache(void);
static bool msm_rpmrs_vdd_mem_beyond_limits(struct msm_rpmrs_limits *limits);
static void msm_rpmrs_aggregate_vdd_mem(struct msm_rpmrs_limits *limits);
static void msm_rpmrs_restore_vdd_mem(void);
static bool msm_rpmrs_vdd_dig_beyond_limits(struct msm_rpmrs_limits *limits);
static void msm_rpmrs_aggregate_vdd_dig(struct msm_rpmrs_limits *limits);
static void msm_rpmrs_restore_vdd_dig(void);

static ssize_t msm_rpmrs_resource_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t msm_rpmrs_resource_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);

static int vdd_dig_vlevels[MSM_RPMRS_VDD_DIG_LAST];
static int vdd_mem_vlevels[MSM_RPMRS_VDD_MEM_LAST];
static int vdd_mask;

#define MSM_RPMRS_MAX_RS_REGISTER_COUNT 2

#define RPMRS_ATTR(_name) \
	__ATTR(_name, S_IRUGO|S_IWUSR, \
		msm_rpmrs_resource_attr_show, msm_rpmrs_resource_attr_store)

struct msm_rpmrs_resource {
	struct msm_rpm_iv_pair rs[MSM_RPMRS_MAX_RS_REGISTER_COUNT];
	uint32_t size;
	char *name;

	uint32_t enable_low_power;

	bool (*beyond_limits)(struct msm_rpmrs_limits *limits);
	void (*aggregate)(struct msm_rpmrs_limits *limits);
	void (*restore)(void);

	struct kobj_attribute ko_attr;
};

static struct msm_rpmrs_resource msm_rpmrs_pxo = {
	.size = 1,
	.name = "pxo",
	.beyond_limits = msm_rpmrs_pxo_beyond_limits,
	.aggregate = msm_rpmrs_aggregate_pxo,
	.restore = msm_rpmrs_restore_pxo,
	.ko_attr = RPMRS_ATTR(pxo),
};

static struct msm_rpmrs_resource msm_rpmrs_l2_cache = {
	.size = 1,
	.name = "L2_cache",
	.beyond_limits = msm_rpmrs_l2_cache_beyond_limits,
	.aggregate = msm_rpmrs_aggregate_l2_cache,
	.restore = msm_rpmrs_restore_l2_cache,
	.ko_attr = RPMRS_ATTR(L2_cache),
};

static struct msm_rpmrs_resource msm_rpmrs_vdd_mem = {
	.size = 2,
	.name = "vdd_mem",
	.beyond_limits = msm_rpmrs_vdd_mem_beyond_limits,
	.aggregate = msm_rpmrs_aggregate_vdd_mem,
	.restore = msm_rpmrs_restore_vdd_mem,
	.ko_attr = RPMRS_ATTR(vdd_mem),
};

static struct msm_rpmrs_resource msm_rpmrs_vdd_dig = {
	.size = 2,
	.name = "vdd_dig",
	.beyond_limits = msm_rpmrs_vdd_dig_beyond_limits,
	.aggregate = msm_rpmrs_aggregate_vdd_dig,
	.restore = msm_rpmrs_restore_vdd_dig,
	.ko_attr = RPMRS_ATTR(vdd_dig),
};

static struct msm_rpmrs_resource msm_rpmrs_rpm_ctl = {
	.size = 1,
	.name = "rpm_ctl",
	.beyond_limits = NULL,
	.aggregate = NULL,
	.restore = NULL,
	.ko_attr = RPMRS_ATTR(rpm_ctl),
};

static struct msm_rpmrs_resource *msm_rpmrs_resources[] = {
	&msm_rpmrs_pxo,
	&msm_rpmrs_l2_cache,
	&msm_rpmrs_vdd_mem,
	&msm_rpmrs_vdd_dig,
	&msm_rpmrs_rpm_ctl,
};

static uint32_t msm_rpmrs_buffer[MSM_RPM_ID_LAST];
static DECLARE_BITMAP(msm_rpmrs_buffered, MSM_RPM_ID_LAST);
static DECLARE_BITMAP(msm_rpmrs_listed, MSM_RPM_ID_LAST);
static DEFINE_SPINLOCK(msm_rpmrs_lock);

#define MSM_RPMRS_VDD(v)  ((v) & (vdd_mask))

/******************************************************************************
 * Attribute Definitions
 *****************************************************************************/
static struct attribute *msm_rpmrs_attributes[] = {
	&msm_rpmrs_pxo.ko_attr.attr,
	&msm_rpmrs_l2_cache.ko_attr.attr,
	&msm_rpmrs_vdd_mem.ko_attr.attr,
	&msm_rpmrs_vdd_dig.ko_attr.attr,
	NULL,
};
static struct attribute *msm_rpmrs_mode_attributes[] = {
	&msm_rpmrs_rpm_ctl.ko_attr.attr,
	NULL,
};

static struct attribute_group msm_rpmrs_attribute_group = {
	.attrs = msm_rpmrs_attributes,
};

static struct attribute_group msm_rpmrs_mode_attribute_group = {
	.attrs = msm_rpmrs_mode_attributes,
};

#define GET_RS_FROM_ATTR(attr) \
	(container_of(attr, struct msm_rpmrs_resource, ko_attr))


/******************************************************************************
 * Resource Specific Functions
 *****************************************************************************/

static void msm_rpmrs_aggregate_sclk(uint32_t sclk_count)
{
	msm_rpmrs_buffer[MSM_RPM_ID_TRIGGER_TIMED_TO] = 0;
	set_bit(MSM_RPM_ID_TRIGGER_TIMED_TO, msm_rpmrs_buffered);
	msm_rpmrs_buffer[MSM_RPM_ID_TRIGGER_TIMED_SCLK_COUNT] = sclk_count;
	set_bit(MSM_RPM_ID_TRIGGER_TIMED_SCLK_COUNT, msm_rpmrs_buffered);
}

static void msm_rpmrs_restore_sclk(void)
{
	clear_bit(MSM_RPM_ID_TRIGGER_TIMED_SCLK_COUNT, msm_rpmrs_buffered);
	msm_rpmrs_buffer[MSM_RPM_ID_TRIGGER_TIMED_SCLK_COUNT] = 0;
	clear_bit(MSM_RPM_ID_TRIGGER_TIMED_TO, msm_rpmrs_buffered);
	msm_rpmrs_buffer[MSM_RPM_ID_TRIGGER_TIMED_TO] = 0;
}

static bool msm_rpmrs_pxo_beyond_limits(struct msm_rpmrs_limits *limits)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_pxo;
	uint32_t pxo;

	if (rs->enable_low_power && test_bit(rs->rs[0].id, msm_rpmrs_buffered))
		pxo = msm_rpmrs_buffer[rs->rs[0].id];
	else
		pxo = MSM_RPMRS_PXO_ON;

	return pxo > limits->pxo;
}

static void msm_rpmrs_aggregate_pxo(struct msm_rpmrs_limits *limits)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_pxo;
	uint32_t *buf = &msm_rpmrs_buffer[rs->rs[0].id];

	if (test_bit(rs->rs[0].id, msm_rpmrs_buffered)) {
		rs->rs[0].value = *buf;
		if (limits->pxo > *buf)
			*buf = limits->pxo;
		if (MSM_RPMRS_DEBUG_OUTPUT & msm_rpmrs_debug_mask)
			pr_info("%s: %d (0x%x)\n", __func__, *buf, *buf);
	}
}

static void msm_rpmrs_restore_pxo(void)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_pxo;

	if (test_bit(rs->rs[0].id, msm_rpmrs_buffered))
		msm_rpmrs_buffer[rs->rs[0].id] = rs->rs[0].value;
}

static bool msm_rpmrs_l2_cache_beyond_limits(struct msm_rpmrs_limits *limits)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_l2_cache;
	uint32_t l2_cache;

	if (rs->enable_low_power && test_bit(rs->rs[0].id, msm_rpmrs_buffered))
		l2_cache = msm_rpmrs_buffer[rs->rs[0].id];
	else
		l2_cache = MSM_RPMRS_L2_CACHE_ACTIVE;

	return l2_cache > limits->l2_cache;
}

static void msm_rpmrs_aggregate_l2_cache(struct msm_rpmrs_limits *limits)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_l2_cache;
	uint32_t *buf = &msm_rpmrs_buffer[rs->rs[0].id];

	if (test_bit(rs->rs[0].id, msm_rpmrs_buffered)) {
		rs->rs[0].value = *buf;
		if (limits->l2_cache > *buf)
			*buf = limits->l2_cache;

		if (MSM_RPMRS_DEBUG_OUTPUT & msm_rpmrs_debug_mask)
			pr_info("%s: %d (0x%x)\n", __func__, *buf, *buf);
	}
}

static bool msm_spm_l2_cache_beyond_limits(struct msm_rpmrs_limits *limits)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_l2_cache;
	uint32_t l2_cache = rs->rs[0].value;

	if (!rs->enable_low_power)
		l2_cache = MSM_RPMRS_L2_CACHE_ACTIVE;

	return l2_cache > limits->l2_cache;
}

static void msm_rpmrs_restore_l2_cache(void)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_l2_cache;

	if (test_bit(rs->rs[0].id, msm_rpmrs_buffered))
		msm_rpmrs_buffer[rs->rs[0].id] = rs->rs[0].value;
}

static bool msm_rpmrs_vdd_mem_beyond_limits(struct msm_rpmrs_limits *limits)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_vdd_mem;
	uint32_t vdd_mem;

	if (test_bit(rs->rs[0].id, msm_rpmrs_buffered)) {
		uint32_t buffered_value = msm_rpmrs_buffer[rs->rs[0].id];

		if (rs->enable_low_power == 0)
			vdd_mem = vdd_mem_vlevels[MSM_RPMRS_VDD_MEM_ACTIVE];
		else if (rs->enable_low_power == 1)
			vdd_mem = vdd_mem_vlevels[MSM_RPMRS_VDD_MEM_RET_HIGH];
		else
			vdd_mem = vdd_mem_vlevels[MSM_RPMRS_VDD_MEM_RET_LOW];

		if (MSM_RPMRS_VDD(buffered_value) > MSM_RPMRS_VDD(vdd_mem))
			vdd_mem = MSM_RPMRS_VDD(buffered_value);
	} else {
		vdd_mem = vdd_mem_vlevels[MSM_RPMRS_VDD_MEM_ACTIVE];
	}

	return vdd_mem > vdd_mem_vlevels[limits->vdd_mem_upper_bound];
}

static void msm_rpmrs_aggregate_vdd_mem(struct msm_rpmrs_limits *limits)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_vdd_mem;
	uint32_t *buf = &msm_rpmrs_buffer[rs->rs[0].id];

	if (test_bit(rs->rs[0].id, msm_rpmrs_buffered)) {
		rs->rs[0].value = *buf;
		if (vdd_mem_vlevels[limits->vdd_mem] > MSM_RPMRS_VDD(*buf)) {
			*buf &= ~vdd_mask;
			*buf |= vdd_mem_vlevels[limits->vdd_mem];
		}

		if (MSM_RPMRS_DEBUG_OUTPUT & msm_rpmrs_debug_mask)
			pr_info("%s: vdd %d (0x%x)\n", __func__,
				MSM_RPMRS_VDD(*buf), MSM_RPMRS_VDD(*buf));
	}
}

static void msm_rpmrs_restore_vdd_mem(void)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_vdd_mem;

	if (test_bit(rs->rs[0].id, msm_rpmrs_buffered))
		msm_rpmrs_buffer[rs->rs[0].id] = rs->rs[0].value;
}

static bool msm_rpmrs_vdd_dig_beyond_limits(struct msm_rpmrs_limits *limits)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_vdd_dig;
	uint32_t vdd_dig;

	if (test_bit(rs->rs[0].id, msm_rpmrs_buffered)) {
		uint32_t buffered_value = msm_rpmrs_buffer[rs->rs[0].id];

		if (rs->enable_low_power == 0)
			vdd_dig = vdd_dig_vlevels[MSM_RPMRS_VDD_DIG_ACTIVE];
		else if (rs->enable_low_power == 1)
			vdd_dig = vdd_dig_vlevels[MSM_RPMRS_VDD_DIG_RET_HIGH];
		else
			vdd_dig = vdd_dig_vlevels[MSM_RPMRS_VDD_DIG_RET_LOW];

		if (MSM_RPMRS_VDD(buffered_value) > MSM_RPMRS_VDD(vdd_dig))
			vdd_dig = MSM_RPMRS_VDD(buffered_value);
	} else {
		vdd_dig = vdd_dig_vlevels[MSM_RPMRS_VDD_DIG_ACTIVE];
	}

	return vdd_dig > vdd_dig_vlevels[limits->vdd_dig_upper_bound];
}

static void msm_rpmrs_aggregate_vdd_dig(struct msm_rpmrs_limits *limits)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_vdd_dig;
	uint32_t *buf = &msm_rpmrs_buffer[rs->rs[0].id];

	if (test_bit(rs->rs[0].id, msm_rpmrs_buffered)) {
		rs->rs[0].value = *buf;
		if (vdd_dig_vlevels[limits->vdd_dig] > MSM_RPMRS_VDD(*buf)) {
			*buf &= ~vdd_mask;
			*buf |= vdd_dig_vlevels[limits->vdd_dig];
		}


		if (MSM_RPMRS_DEBUG_OUTPUT & msm_rpmrs_debug_mask)
			pr_info("%s: vdd %d (0x%x)\n", __func__,
				MSM_RPMRS_VDD(*buf), MSM_RPMRS_VDD(*buf));
	}
}

static void msm_rpmrs_restore_vdd_dig(void)
{
	struct msm_rpmrs_resource *rs = &msm_rpmrs_vdd_dig;

	if (test_bit(rs->rs[0].id, msm_rpmrs_buffered))
		msm_rpmrs_buffer[rs->rs[0].id] = rs->rs[0].value;
}

/******************************************************************************
 * Buffering Functions
 *****************************************************************************/

static bool msm_rpmrs_irqs_detectable(struct msm_rpmrs_limits *limits,
		bool irqs_detect, bool gpio_detect)
{

	if (vdd_dig_vlevels[limits->vdd_dig_upper_bound] <=
			vdd_dig_vlevels[MSM_RPMRS_VDD_DIG_RET_HIGH])
		return irqs_detect;

	if (limits->pxo == MSM_RPMRS_PXO_OFF)
		return gpio_detect;

	return true;
}

static bool msm_rpmrs_use_mpm(struct msm_rpmrs_limits *limits)
{
	return (limits->pxo == MSM_RPMRS_PXO_OFF) ||
		(vdd_dig_vlevels[limits->vdd_dig] <=
		 vdd_dig_vlevels[MSM_RPMRS_VDD_DIG_RET_HIGH]);
}

static void msm_rpmrs_update_levels(void)
{
	int i, k;

	for (i = 0; i < msm_rpmrs_level_count; i++) {
		struct msm_rpmrs_level *level = &msm_rpmrs_levels[i];

		if (level->sleep_mode != MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
			continue;

		level->available = true;

		for (k = 0; k < ARRAY_SIZE(msm_rpmrs_resources); k++) {
			struct msm_rpmrs_resource *rs = msm_rpmrs_resources[k];

			if (rs->beyond_limits &&
					rs->beyond_limits(&level->rs_limits)) {
				level->available = false;
				break;
			}
		}

	}
}

/*
 * Return value:
 *   0: no entries in <req> is on our resource list
 *   1: one or more entries in <req> is on our resource list
 *   -EINVAL: invalid id in <req> array
 */
static int msm_rpmrs_buffer_request(struct msm_rpm_iv_pair *req, int count)
{
	bool listed;
	int i;

	for (i = 0; i < count; i++)
		if (req[i].id >= MSM_RPM_ID_LAST)
			return -EINVAL;

	for (i = 0, listed = false; i < count; i++) {
		msm_rpmrs_buffer[req[i].id] = req[i].value;
		set_bit(req[i].id, msm_rpmrs_buffered);

		if (MSM_RPMRS_DEBUG_BUFFER & msm_rpmrs_debug_mask)
			pr_info("%s: reg %d: 0x%x\n",
				__func__, req[i].id, req[i].value);

		if (listed)
			continue;

		if (test_bit(req[i].id, msm_rpmrs_listed))
			listed = true;
	}

	return listed ? 1 : 0;
}

/*
 * Return value:
 *   0: no entries in <req> is on our resource list
 *   1: one or more entries in <req> is on our resource list
 *   -EINVAL: invalid id in <req> array
 */
static int msm_rpmrs_clear_buffer(struct msm_rpm_iv_pair *req, int count)
{
	bool listed;
	int i;

	for (i = 0; i < count; i++)
		if (req[i].id >= MSM_RPM_ID_LAST)
			return -EINVAL;

	for (i = 0, listed = false; i < count; i++) {
		msm_rpmrs_buffer[req[i].id] = 0;
		clear_bit(req[i].id, msm_rpmrs_buffered);

		if (MSM_RPMRS_DEBUG_BUFFER & msm_rpmrs_debug_mask)
			pr_info("%s: reg %d\n", __func__, req[i].id);

		if (listed)
			continue;

		if (test_bit(req[i].id, msm_rpmrs_listed))
			listed = true;
	}

	return listed ? 1 : 0;
}

#ifdef CONFIG_MSM_L2_SPM
static int msm_rpmrs_flush_L2(struct msm_rpmrs_limits *limits, int notify_rpm)
{
	int rc = 0;
	int lpm;

	switch (limits->l2_cache) {
	case MSM_RPMRS_L2_CACHE_HSFS_OPEN:
		lpm = MSM_SPM_L2_MODE_POWER_COLLAPSE;
		msm_pm_set_l2_flush_flag(1);
		break;
	case MSM_RPMRS_L2_CACHE_GDHS:
		lpm = MSM_SPM_L2_MODE_GDHS;
		break;
	case MSM_RPMRS_L2_CACHE_RETENTION:
		lpm = MSM_SPM_L2_MODE_RETENTION;
		break;
	default:
	case MSM_RPMRS_L2_CACHE_ACTIVE:
		lpm = MSM_SPM_L2_MODE_DISABLED;
		break;
	}

	rc = msm_spm_l2_set_low_power_mode(lpm, notify_rpm);
	if (MSM_RPMRS_DEBUG_BUFFER & msm_rpmrs_debug_mask)
		pr_info("%s: Requesting low power mode %d returned %d\n",
				__func__, lpm, rc);

	return rc;
}
static void msm_rpmrs_L2_restore(struct msm_rpmrs_limits *limits,
		bool notify_rpm, bool collapsed)
{
	msm_spm_l2_set_low_power_mode(MSM_SPM_MODE_DISABLED, notify_rpm);
	msm_pm_set_l2_flush_flag(0);
}
#else
static int msm_rpmrs_flush_L2(struct msm_rpmrs_limits *limits, int notify_rpm)
{
	return 0;
}
static void msm_rpmrs_L2_restore(struct msm_rpmrs_limits *limits,
		bool notify_rpm, bool collapsed)
{
}
#endif

static int msm_rpmrs_flush_buffer(
	uint32_t sclk_count, struct msm_rpmrs_limits *limits, int from_idle)
{
	struct msm_rpm_iv_pair *req;
	int count;
	int rc;
	int i;

	msm_rpmrs_aggregate_sclk(sclk_count);
	for (i = 0; i < ARRAY_SIZE(msm_rpmrs_resources); i++) {
		if (msm_rpmrs_resources[i]->aggregate)
			msm_rpmrs_resources[i]->aggregate(limits);
	}

	count = bitmap_weight(msm_rpmrs_buffered, MSM_RPM_ID_LAST);

	req = kmalloc(sizeof(*req) * count, GFP_ATOMIC);
	if (!req) {
		rc = -ENOMEM;
		goto flush_buffer_restore;
	}

	count = 0;
	i = find_first_bit(msm_rpmrs_buffered, MSM_RPM_ID_LAST);

	while (i < MSM_RPM_ID_LAST) {
		if (MSM_RPMRS_DEBUG_OUTPUT & msm_rpmrs_debug_mask)
			pr_info("%s: reg %d: 0x%x\n",
				__func__, i, msm_rpmrs_buffer[i]);

		req[count].id = i;
		req[count].value = msm_rpmrs_buffer[i];
		count++;

		i = find_next_bit(msm_rpmrs_buffered, MSM_RPM_ID_LAST, i + 1);
	}

	rc = msm_rpm_set_noirq(MSM_RPM_CTX_SET_SLEEP, req, count);
	kfree(req);

	if (rc)
		goto flush_buffer_restore;

	bitmap_and(msm_rpmrs_buffered,
		msm_rpmrs_buffered, msm_rpmrs_listed, MSM_RPM_ID_LAST);

flush_buffer_restore:
	for (i = 0; i < ARRAY_SIZE(msm_rpmrs_resources); i++) {
		if (msm_rpmrs_resources[i]->restore)
			msm_rpmrs_resources[i]->restore();
	}
	msm_rpmrs_restore_sclk();

	if (rc)
		pr_err("%s: failed: %d\n", __func__, rc);
	return rc;
}

static int msm_rpmrs_set_common(
	int ctx, struct msm_rpm_iv_pair *req, int count, bool noirq)
{
	if (ctx == MSM_RPM_CTX_SET_SLEEP) {
		unsigned long flags;
		int rc;

		spin_lock_irqsave(&msm_rpmrs_lock, flags);
		rc = msm_rpmrs_buffer_request(req, count);
		if (rc > 0) {
			msm_rpmrs_update_levels();
			rc = 0;
		}
		spin_unlock_irqrestore(&msm_rpmrs_lock, flags);

		return rc;
	}

	if (noirq)
		return msm_rpm_set_noirq(ctx, req, count);
	else
		return msm_rpm_set(ctx, req, count);
}

static int msm_rpmrs_clear_common(
	int ctx, struct msm_rpm_iv_pair *req, int count, bool noirq)
{
	if (ctx == MSM_RPM_CTX_SET_SLEEP) {
		unsigned long flags;
		int rc;

		spin_lock_irqsave(&msm_rpmrs_lock, flags);
		rc = msm_rpmrs_clear_buffer(req, count);
		if (rc > 0) {
			msm_rpmrs_update_levels();
			rc = 0;
		}
		spin_unlock_irqrestore(&msm_rpmrs_lock, flags);

		if (rc < 0)
			return rc;
	}

	if (noirq)
		return msm_rpm_clear_noirq(ctx, req, count);
	else
		return msm_rpm_clear(ctx, req, count);
}

/******************************************************************************
 * Attribute Functions
 *****************************************************************************/

static ssize_t msm_rpmrs_resource_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct kernel_param kp;
	unsigned long flags;
	unsigned int temp;
	int rc;

	spin_lock_irqsave(&msm_rpmrs_lock, flags);
	/* special case active-set signal for MSM_RPMRS_ID_RPM_CTL */
	if (GET_RS_FROM_ATTR(attr)->rs[0].id ==
			msm_rpmrs_rpm_ctl.rs[0].id)
		temp = GET_RS_FROM_ATTR(attr)->rs[0].value;
	else
		temp = GET_RS_FROM_ATTR(attr)->enable_low_power;
	spin_unlock_irqrestore(&msm_rpmrs_lock, flags);

	kp.arg = &temp;
	rc = param_get_uint(buf, &kp);

	if (rc > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		rc++;
	}

	return rc;
}

static ssize_t msm_rpmrs_resource_attr_store(struct kobject *kobj,
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

	spin_lock_irqsave(&msm_rpmrs_lock, flags);
	GET_RS_FROM_ATTR(attr)->enable_low_power = temp;

	/* special case active-set signal for MSM_RPMRS_ID_RPM_CTL */
	if (GET_RS_FROM_ATTR(attr)->rs[0].id ==
			msm_rpmrs_rpm_ctl.rs[0].id) {
		struct msm_rpm_iv_pair req;
		req.id = msm_rpmrs_rpm_ctl.rs[0].id;
		req.value = GET_RS_FROM_ATTR(attr)->enable_low_power;
		GET_RS_FROM_ATTR(attr)->rs[0].value = req.value;

		rc = msm_rpm_set_noirq(MSM_RPM_CTX_SET_0, &req, 1);
		if (rc) {
			pr_err("%s: failed to request RPM_CTL to %d: %d\n",
				__func__, req.value, rc);
		}
	}

	msm_rpmrs_update_levels();
	spin_unlock_irqrestore(&msm_rpmrs_lock, flags);

	return count;
}

static int __init msm_rpmrs_resource_sysfs_add(void)
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

	rc = sysfs_create_group(low_power_kobj, &msm_rpmrs_attribute_group);
	if (rc) {
		pr_err("%s: cannot create kobject attribute group\n", __func__);
		goto resource_sysfs_add_exit;
	}

	rc = sysfs_create_group(mode_kobj, &msm_rpmrs_mode_attribute_group);
	if (rc) {
		pr_err("%s: cannot create kobject attribute group\n", __func__);
		goto resource_sysfs_add_exit;
	}

	rc = 0;
resource_sysfs_add_exit:
	if (rc) {
		if (low_power_kobj)
			sysfs_remove_group(low_power_kobj,
					&msm_rpmrs_attribute_group);
		kobject_del(low_power_kobj);
		kobject_del(mode_kobj);
	}

	return rc;
}

/******************************************************************************
 * Public Functions
 *****************************************************************************/

int msm_rpmrs_set(int ctx, struct msm_rpm_iv_pair *req, int count)
{
	return msm_rpmrs_set_common(ctx, req, count, false);
}

int msm_rpmrs_set_noirq(int ctx, struct msm_rpm_iv_pair *req, int count)
{
	WARN(!irqs_disabled(), "msm_rpmrs_set_noirq can only be called "
		"safely when local irqs are disabled.  Consider using "
		"msm_rpmrs_set or msm_rpmrs_set_nosleep instead.");
	return msm_rpmrs_set_common(ctx, req, count, true);
}

/* Allow individual bits of an rpm resource be set, currently used only for
 * active context resource viz. RPM_CTL. The API is generic enough to possibly
 * extend it to other resources as well in the future.
 */
int msm_rpmrs_set_bits_noirq(int ctx, struct msm_rpm_iv_pair *req, int count,
		int *mask)
{
	unsigned long flags;
	int i, j;
	int rc = -1;
	struct msm_rpmrs_resource *rs;

	if (ctx != MSM_RPM_CTX_SET_0)
		return -ENOSYS;

	spin_lock_irqsave(&msm_rpmrs_lock, flags);
	for (i = 0; i < ARRAY_SIZE(msm_rpmrs_resources); i++) {
		rs = msm_rpmrs_resources[i];
		if (rs->rs[0].id == req[0].id && rs->size == count) {
			for (j = 0; j < rs->size; j++) {
				rs->rs[j].value &= ~mask[j];
				rs->rs[j].value |= req[j].value & mask[j];
			}
			break;
		}
	}

	if (i != ARRAY_SIZE(msm_rpmrs_resources)) {
		rc = msm_rpm_set_noirq(MSM_RPM_CTX_SET_0, &rs->rs[0], rs->size);
		if (rc) {
			for (j = 0; j < rs->size; j++) {
				pr_err("%s: failed to request %d to %d: %d\n",
				__func__,
				rs->rs[j].id, rs->rs[j].value, rc);
			}
		}
	}
	spin_unlock_irqrestore(&msm_rpmrs_lock, flags);

	return rc;

}

int msm_rpmrs_clear(int ctx, struct msm_rpm_iv_pair *req, int count)
{
	return msm_rpmrs_clear_common(ctx, req, count, false);
}

int msm_rpmrs_clear_noirq(int ctx, struct msm_rpm_iv_pair *req, int count)
{
	WARN(!irqs_disabled(), "msm_rpmrs_clear_noirq can only be called "
		"safely when local irqs are disabled.  Consider using "
		"msm_rpmrs_clear or msm_rpmrs_clear_nosleep instead.");
	return msm_rpmrs_clear_common(ctx, req, count, true);
}

void msm_rpmrs_show_resources(void)
{
	struct msm_rpmrs_resource *rs;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&msm_rpmrs_lock, flags);
	for (i = 0; i < ARRAY_SIZE(msm_rpmrs_resources); i++) {
		rs = msm_rpmrs_resources[i];
		if (rs->rs[0].id < MSM_RPM_ID_LAST)
			pr_info("%s: resource %s: buffered %d, value 0x%x\n",
				__func__, rs->name,
				test_bit(rs->rs[0].id, msm_rpmrs_buffered),
				msm_rpmrs_buffer[rs->rs[0].id]);
		else
			pr_info("%s: resource %s: value %d\n",
				__func__, rs->name, rs->rs[0].value);
	}
	spin_unlock_irqrestore(&msm_rpmrs_lock, flags);
}

s32 msm_cpuidle_get_deep_idle_latency(void)
{
	int i;
	struct msm_rpmrs_level *level = msm_rpmrs_levels, *best = level;

	if (!level)
		return 0;

	for (i = 0; i < msm_rpmrs_level_count; i++, level++) {
		if (!level->available)
			continue;
		if (level->sleep_mode != MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
			continue;
		/* Pick the first power collapse mode by default */
		if (best->sleep_mode != MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
			best = level;
		/* Find the lowest latency for power collapse */
		if (level->latency_us < best->latency_us)
			best = level;
	}
	return best->latency_us - 1;
}

static void *msm_rpmrs_lowest_limits(bool from_idle,
		enum msm_pm_sleep_mode sleep_mode, uint32_t latency_us,
		uint32_t sleep_us, uint32_t *power)
{
	unsigned int cpu = smp_processor_id();
	struct msm_rpmrs_level *best_level = NULL;
	bool irqs_detectable = false;
	bool gpio_detectable = false;
	int i;
	uint32_t pwr;

	if (sleep_mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE) {
		irqs_detectable = msm_mpm_irqs_detectable(from_idle);
		gpio_detectable = msm_mpm_gpio_irqs_detectable(from_idle);
	}

	for (i = 0; i < msm_rpmrs_level_count; i++) {
		struct msm_rpmrs_level *level = &msm_rpmrs_levels[i];

		if (!level->available)
			continue;

		if (sleep_mode != level->sleep_mode)
			continue;

		if (latency_us < level->latency_us)
			continue;

		if (sleep_us <= level->time_overhead_us)
			continue;

		if (!msm_rpmrs_irqs_detectable(&level->rs_limits,
					irqs_detectable, gpio_detectable))
			continue;

		if (MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE == sleep_mode)
			if (!cpu && msm_rpm_local_request_is_outstanding())
					break;


		if (sleep_us <= 1) {
			pwr = level->energy_overhead;
		} else if (sleep_us <= level->time_overhead_us) {
			pwr = level->energy_overhead / sleep_us;
		} else if ((sleep_us >> 10) > level->time_overhead_us) {
			pwr = level->steady_state_power;
		} else {
			pwr = level->steady_state_power;
			pwr -= (level->time_overhead_us *
					level->steady_state_power)/sleep_us;
			pwr += level->energy_overhead / sleep_us;
		}

		if (!best_level ||
				best_level->rs_limits.power[cpu] >= pwr) {
			level->rs_limits.latency_us[cpu] = level->latency_us;
			level->rs_limits.power[cpu] = pwr;
			best_level = level;
			if (power)
				*power = pwr;
		}
	}

	return best_level ? &best_level->rs_limits : NULL;
}

static int msm_rpmrs_enter_sleep(uint32_t sclk_count, void *limits,
		bool from_idle, bool notify_rpm)
{
	int rc = 0;

	if (notify_rpm) {
		rc = msm_rpmrs_flush_buffer(sclk_count, limits, from_idle);
		if (rc)
			return rc;

		if (msm_rpmrs_use_mpm(limits))
			msm_mpm_enter_sleep(from_idle);
	}

	rc = msm_rpmrs_flush_L2(limits, notify_rpm);
	return rc;
}

static void msm_rpmrs_exit_sleep(void *limits, bool from_idle,
		bool notify_rpm, bool collapsed)
{

	/* Disable L2 for now, we dont want L2 to do retention by default */
	msm_rpmrs_L2_restore(limits, notify_rpm, collapsed);

	if (msm_rpmrs_use_mpm(limits))
		msm_mpm_exit_sleep(from_idle);
}

static int rpmrs_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_ONLINE_FROZEN:
	case CPU_ONLINE:
		if (num_online_cpus() > 1)
			msm_rpmrs_l2_cache.rs[0].value =
				MSM_RPMRS_L2_CACHE_ACTIVE;
		break;
	case CPU_DEAD_FROZEN:
	case CPU_DEAD:
		if (num_online_cpus() == 1)
			msm_rpmrs_l2_cache.rs[0].value =
				MSM_RPMRS_L2_CACHE_HSFS_OPEN;
		break;
	}

	msm_rpmrs_update_levels();
	return NOTIFY_OK;
}

static struct notifier_block __refdata rpmrs_cpu_notifier = {
	.notifier_call = rpmrs_cpu_callback,
};

int __init msm_rpmrs_levels_init(struct msm_rpmrs_platform_data *data)
{
	int i, k;
	struct msm_rpmrs_level *levels = data->levels;

	msm_rpmrs_level_count = data->num_levels;

	msm_rpmrs_levels = kzalloc(sizeof(struct msm_rpmrs_level) *
			msm_rpmrs_level_count, GFP_KERNEL);
	if (!msm_rpmrs_levels)
		return -ENOMEM;

	memcpy(msm_rpmrs_levels, levels,
			msm_rpmrs_level_count * sizeof(struct msm_rpmrs_level));

	memcpy(vdd_dig_vlevels, data->vdd_dig_levels,
		(MSM_RPMRS_VDD_DIG_MAX + 1) * sizeof(vdd_dig_vlevels[0]));

	memcpy(vdd_mem_vlevels, data->vdd_mem_levels,
		(MSM_RPMRS_VDD_MEM_MAX + 1) * sizeof(vdd_mem_vlevels[0]));
	vdd_mask = data->vdd_mask;

	msm_rpmrs_pxo.rs[0].id = data->rpmrs_target_id[MSM_RPMRS_ID_PXO_CLK];
	msm_rpmrs_l2_cache.rs[0].id =
			data->rpmrs_target_id[MSM_RPMRS_ID_L2_CACHE_CTL];
	msm_rpmrs_vdd_mem.rs[0].id =
			data->rpmrs_target_id[MSM_RPMRS_ID_VDD_MEM_0];
	msm_rpmrs_vdd_mem.rs[1].id =
			data->rpmrs_target_id[MSM_RPMRS_ID_VDD_MEM_1];
	msm_rpmrs_vdd_dig.rs[0].id =
			data->rpmrs_target_id[MSM_RPMRS_ID_VDD_DIG_0];
	msm_rpmrs_vdd_dig.rs[1].id =
			data->rpmrs_target_id[MSM_RPMRS_ID_VDD_DIG_1];
	msm_rpmrs_rpm_ctl.rs[0].id =
			data->rpmrs_target_id[MSM_RPMRS_ID_RPM_CTL];

	/* Initialize listed bitmap for valid resource IDs */
	for (i = 0; i < ARRAY_SIZE(msm_rpmrs_resources); i++) {
		for (k = 0; k < msm_rpmrs_resources[i]->size; k++) {
			if (msm_rpmrs_resources[i]->rs[k].id >=
					MSM_RPM_ID_LAST)
				continue;
			set_bit(msm_rpmrs_resources[i]->rs[k].id,
				msm_rpmrs_listed);
		}
	}

	return 0;
}

static int __init msm_rpmrs_init(void)
{
	struct msm_rpm_iv_pair req;
	int rc;

	BUG_ON(!msm_rpmrs_levels);

	if (cpu_is_msm8x60()) {
		req.id = msm_rpmrs_l2_cache.rs[0].id;
		req.value = 1;

		rc = msm_rpm_set(MSM_RPM_CTX_SET_0, &req, 1);
		if (rc) {
			pr_err("%s: failed to request L2 cache: %d\n",
				__func__, rc);
			goto init_exit;
		}

		req.id = msm_rpmrs_l2_cache.rs[0].id;
		req.value = 0;

		rc = msm_rpmrs_set(MSM_RPM_CTX_SET_SLEEP, &req, 1);
		if (rc) {
			pr_err("%s: failed to initialize L2 cache for sleep: "
				"%d\n", __func__, rc);
			goto init_exit;
		}
	}

	rc = msm_rpmrs_resource_sysfs_add();

init_exit:
	return rc;
}
device_initcall(msm_rpmrs_init);

static struct msm_pm_sleep_ops msm_rpmrs_ops = {
	.lowest_limits = msm_rpmrs_lowest_limits,
	.enter_sleep = msm_rpmrs_enter_sleep,
	.exit_sleep = msm_rpmrs_exit_sleep,
};

static int __init msm_rpmrs_l2_init(void)
{
	if (cpu_is_msm8960() || cpu_is_msm8930() || cpu_is_msm8930aa() ||
	    cpu_is_apq8064() || cpu_is_msm8627()) {

		msm_pm_set_l2_flush_flag(0);

		msm_rpmrs_l2_cache.beyond_limits =
			msm_spm_l2_cache_beyond_limits;
		msm_rpmrs_l2_cache.aggregate = NULL;
		msm_rpmrs_l2_cache.restore = NULL;

		register_hotcpu_notifier(&rpmrs_cpu_notifier);

	} else if (cpu_is_msm9615()) {
		msm_rpmrs_l2_cache.beyond_limits = NULL;
		msm_rpmrs_l2_cache.aggregate = NULL;
		msm_rpmrs_l2_cache.restore = NULL;
	}

	msm_pm_set_sleep_ops(&msm_rpmrs_ops);

	return 0;
}
early_initcall(msm_rpmrs_l2_init);
