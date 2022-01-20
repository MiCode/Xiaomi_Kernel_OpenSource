// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <rq_stats.h>

#define OVERUTIL_THRESHOLD_SIZE	2
/* cap(LL)*0.8, cap(BL)*0.7 */
static unsigned	int overutil_thres[OVERUTIL_THRESHOLD_SIZE] = {19, 60};

unsigned int get_overutil_threshold(int index)
{
	if (index >= 0 && index < OVERUTIL_THRESHOLD_SIZE)
		return overutil_thres[index];
	return 100;
}
EXPORT_SYMBOL(get_overutil_threshold);

void set_overutil_threshold(unsigned int index, unsigned int val)
{
	if (index >= OVERUTIL_THRESHOLD_SIZE)
		return;

	if (val <= 100) {
		overutil_thres[index] = (int)val;
		overutil_thresh_chg_notify();
	}
}
EXPORT_SYMBOL(set_overutil_threshold);

int inc_nr_heavy_running(int invoker, struct task_struct *p,
		int inc, bool ack_cap)
{
	sched_update_nr_heavy_prod(invoker, p,
			cpu_of(task_rq(p)), inc, ack_cap);

	return 0;
}
EXPORT_SYMBOL(inc_nr_heavy_running);

/* For read/write utilization related settings */
static ssize_t store_overutil(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val[OVERUTIL_THRESHOLD_SIZE];
	int ret, i;

	ret = sscanf(buf, "%u %u\n", &val[0], &val[1]);
	if (ret <= 0 || OVERUTIL_THRESHOLD_SIZE <= ret)
		return -EINVAL;

	if (ret == 1) {
		for (i = 0; i < OVERUTIL_THRESHOLD_SIZE; i++)
			overutil_thres[i] =
				val[0] < 100 ? val[0] : overutil_thres[i];
	} else {
		for (i = 0; i < OVERUTIL_THRESHOLD_SIZE; i++)
			overutil_thres[i] =
				val[i] < 100 ? val[i] : overutil_thres[i];
	}
	overutil_thresh_chg_notify();
	return count;
}

static ssize_t show_overutil(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0, i;
	unsigned int max_len = 4096;

	for (i = 0; i < OVERUTIL_THRESHOLD_SIZE; i++) {
		len += snprintf(buf+len,
			max_len-len, "over_util threshold[%u]=%d max=100\n",
				i, overutil_thres[i]);
	}
	len += get_overutil_stats(buf+len, max_len-len);

	return len;
}

static struct kobj_attribute over_util_attr =
__ATTR(over_util, 0600 /* S_IWUSR | S_IRUSR*/, show_overutil,
		store_overutil);

static struct attribute *rq_attrs[] = {
	&over_util_attr.attr,
	NULL,
};

static struct attribute_group rq_attr_group = {
	.attrs = rq_attrs,
};

static int init_rq_attribs(void)
{
	int ret = 0;
	struct kobject *kobj = NULL;

	/* Create /sys/devices/system/cpu/rq-stats/... */
	kobj = kobject_create_and_add("rq-stats",
				&cpu_subsys.dev_root->kobj);
	if (!kobj)
		return -ENOMEM;

	ret = sysfs_create_group(kobj, &rq_attr_group);
	if (ret)
		kobject_put(kobj);
	else
		kobject_uevent(kobj, KOBJ_ADD);

	return ret;
}

static int __init rq_stats_init(void)
{
	int ret = 0;

	ret = init_rq_attribs();
	return ret;
}
late_initcall(rq_stats_init);

