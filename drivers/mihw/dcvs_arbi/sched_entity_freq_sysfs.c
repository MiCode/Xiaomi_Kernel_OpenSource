/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025, Xiaomi Inc. All rights reserved.
 *
 * 基于调度实体的调频控制 - Sysfs接口
 */

#undef pr_fmt
#define pr_fmt(fmt) "sched_entity_freq: " fmt

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cpu.h>
#include "sched_entity_freq.h"

/* 配置变量 - 定义在sched_entity_freq_core.c中 */
extern struct sched_entity_config config;
extern bool cluster_idle_threshold_override;
extern bool cluster_active_threshold_override;

/* Sysfs属性显示函数 */
static ssize_t enable_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.enable) ? 1 : 0);
}

/* Sysfs属性存储函数 */
static ssize_t enable_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	int ret;
	bool enable;

	ret = kstrtobool(buf, &enable);
	if (ret < 0)
		return ret;

	sched_entity_freq_enable(enable);

	return count;
}

static ssize_t idle_threshold_show(struct kobject *kobj, struct kobj_attribute *attr,
				   char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.idle_threshold));
}

static ssize_t idle_threshold_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	WRITE_ONCE(config.idle_threshold, val);
	pr_info("idle_threshold set to %u\n", val);

	return count;
}

static ssize_t active_threshold_show(struct kobject *kobj, struct kobj_attribute *attr,
				    char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.active_threshold));
}

static ssize_t active_threshold_store(struct kobject *kobj, struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	WRITE_ONCE(config.active_threshold, val);
	pr_info("active_threshold set to %u\n", val);

	return count;
}

static ssize_t cluster_idle_threshold_show(struct kobject *kobj, struct kobj_attribute *attr,
					   char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.cluster_idle_threshold));
}

static ssize_t cluster_idle_threshold_store(struct kobject *kobj, struct kobj_attribute *attr,
					    const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	WRITE_ONCE(config.cluster_idle_threshold, val);
	cluster_idle_threshold_override = true;
	pr_info("cluster_idle_threshold set to %u (override)\n", val);

	return count;
}

static ssize_t cluster_active_threshold_show(struct kobject *kobj, struct kobj_attribute *attr,
					     char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.cluster_active_threshold));
}

static ssize_t cluster_active_threshold_store(struct kobject *kobj, struct kobj_attribute *attr,
					      const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	WRITE_ONCE(config.cluster_active_threshold, val);
	cluster_active_threshold_override = true;
	pr_info("cluster_active_threshold set to %u (override)\n", val);

	return count;
}

static ssize_t single_task_timeout_show(struct kobject *kobj, struct kobj_attribute *attr,
					char *buf)
{
	/* 单任务超时参数已废弃，使用固定的6ms纳秒计时 */
	return scnprintf(buf, PAGE_SIZE, "%u (已废弃，使用固定的6ms纳秒计时)\n", READ_ONCE(config.single_task_timeout_ms));
}

static ssize_t single_task_timeout_store(struct kobject *kobj, struct kobj_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	/* 单任务超时参数已废弃，使用固定的6ms纳秒计时 */
	pr_info("single_task_timeout参数已废弃，使用固定的6ms纳秒计时\n");

	return count;
}

static ssize_t min_freq_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.min_freq_khz));
}

static ssize_t min_freq_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	WRITE_ONCE(config.min_freq_khz, val);
	pr_info("min_freq set to %u kHz\n", val);

	return count;
}

static ssize_t mid_freq_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.mid_freq_khz));
}

static ssize_t mid_freq_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	WRITE_ONCE(config.mid_freq_khz, val);
	pr_info("mid_freq set to %u kHz\n", val);

	return count;
}

static ssize_t normal_freq_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.normal_freq_khz));
}

static ssize_t normal_freq_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	WRITE_ONCE(config.normal_freq_khz, val);
	pr_info("normal_freq set to %u kHz\n", val);

	return count;
}

static ssize_t fast_switch_delay_show(struct kobject *kobj, struct kobj_attribute *attr,
				      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.fast_switch_delay_ms));
}

static ssize_t fast_switch_delay_store(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	WRITE_ONCE(config.fast_switch_delay_ms, val);
	pr_info("fast_switch_delay set to %u ms\n", val);

	return count;
}

static ssize_t ddr_freq_sync_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.ddr_freq_sync) ? 1 : 0);
}

static ssize_t ddr_freq_sync_store(struct kobject *kobj, struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	bool val;

	ret = kstrtobool(buf, &val);
	if (ret < 0)
		return ret;

	WRITE_ONCE(config.ddr_freq_sync, val);
	pr_info("ddr_freq_sync set to %s\n", val ? "true" : "false");

	return count;
}

static ssize_t ddr_min_freq_show(struct kobject *kobj, struct kobj_attribute *attr,
				 char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(config.ddr_min_freq_khz));
}

static ssize_t ddr_min_freq_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	WRITE_ONCE(config.ddr_min_freq_khz, val);
	pr_info("ddr_min_freq set to %u kHz\n", val);

	return count;
}

/* Sysfs属性定义 */
static struct kobj_attribute enable_attr =
	__ATTR(enable, 0664, enable_show, enable_store);

static struct kobj_attribute idle_threshold_attr =
	__ATTR(idle_threshold, 0664, idle_threshold_show, idle_threshold_store);

static struct kobj_attribute active_threshold_attr =
	__ATTR(active_threshold, 0664, active_threshold_show, active_threshold_store);

static struct kobj_attribute cluster_idle_threshold_attr =
	__ATTR(cluster_idle_threshold, 0664, cluster_idle_threshold_show, cluster_idle_threshold_store);

static struct kobj_attribute cluster_active_threshold_attr =
	__ATTR(cluster_active_threshold, 0664, cluster_active_threshold_show, cluster_active_threshold_store);

static struct kobj_attribute single_task_timeout_attr =
	__ATTR(single_task_timeout, 0664, single_task_timeout_show, single_task_timeout_store);

static struct kobj_attribute min_freq_attr =
	__ATTR(min_freq, 0664, min_freq_show, min_freq_store);

static struct kobj_attribute mid_freq_attr =
	__ATTR(mid_freq, 0664, mid_freq_show, mid_freq_store);

static struct kobj_attribute normal_freq_attr =
	__ATTR(normal_freq, 0664, normal_freq_show, normal_freq_store);

static struct kobj_attribute fast_switch_delay_attr =
	__ATTR(fast_switch_delay, 0664, fast_switch_delay_show, fast_switch_delay_store);

static struct kobj_attribute ddr_freq_sync_attr =
	__ATTR(ddr_freq_sync, 0664, ddr_freq_sync_show, ddr_freq_sync_store);

static struct kobj_attribute ddr_min_freq_attr =
	__ATTR(ddr_min_freq, 0664, ddr_min_freq_show, ddr_min_freq_store);

static struct attribute *sched_entity_freq_attrs[] = {
	&enable_attr.attr,
	&idle_threshold_attr.attr,
	&active_threshold_attr.attr,
	&cluster_idle_threshold_attr.attr,
	&cluster_active_threshold_attr.attr,
	&single_task_timeout_attr.attr,
	&min_freq_attr.attr,
	&mid_freq_attr.attr,
	&normal_freq_attr.attr,
	&fast_switch_delay_attr.attr,
	&ddr_freq_sync_attr.attr,
	&ddr_min_freq_attr.attr,
	NULL,
};

static struct attribute_group sched_entity_freq_attr_group = {
	.attrs = sched_entity_freq_attrs,
};

static struct kobject *sched_entity_freq_kobj;
static struct kobject *dcvs_arbi_kobj;

/* 初始化sysfs接口 */
int sched_entity_freq_sysfs_init(void)
{
	int ret;
	struct device *cpu_dev;
	struct kobject *cpu_kobj;

	/* 获取 /sys/devices/system/cpu 对应的 kobject */
	cpu_dev = get_cpu_device(0);
	if (!cpu_dev) {
		pr_err("Failed to get cpu device\n");
		return -ENODEV;
	}
	cpu_kobj = cpu_dev->kobj.parent;

	/* 创建 dcvs_arbi kobject */
	dcvs_arbi_kobj = kobject_create_and_add("dcvs_arbi", cpu_kobj);
	if (!dcvs_arbi_kobj) {
		pr_err("Failed to create dcvs_arbi kobject\n");
		return -ENOMEM;
	}

	sched_entity_freq_kobj = kobject_create_and_add("sched_entity_freq1", dcvs_arbi_kobj);
	if (!sched_entity_freq_kobj) {
		pr_err("Failed to create kobject\n");
		kobject_put(dcvs_arbi_kobj);
		dcvs_arbi_kobj = NULL;
		return -ENOMEM;
	}

	ret = sysfs_create_group(sched_entity_freq_kobj, &sched_entity_freq_attr_group);
	if (ret) {
		pr_err("Failed to create sysfs group: %d\n", ret);
		kobject_put(sched_entity_freq_kobj);
		kobject_put(dcvs_arbi_kobj);
		sched_entity_freq_kobj = NULL;
		dcvs_arbi_kobj = NULL;
		return ret;
	}

	pr_info("Sysfs interface initialized under /sys/devices/system/cpu/dcvs_arbi/sched_entity_freq\n");
	return 0;
}

/* 清理sysfs接口 */
void sched_entity_freq_sysfs_exit(void)
{
	if (sched_entity_freq_kobj) {
		sysfs_remove_group(sched_entity_freq_kobj, &sched_entity_freq_attr_group);
		kobject_put(sched_entity_freq_kobj);
		sched_entity_freq_kobj = NULL;
	}
	if (dcvs_arbi_kobj) {
		kobject_put(dcvs_arbi_kobj);
		dcvs_arbi_kobj = NULL;
	}
	pr_info("Sysfs interface cleaned up\n");
}
