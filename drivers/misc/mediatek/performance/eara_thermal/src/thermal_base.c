// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/trace_events.h>
#include <trace/events/fpsgo.h>

#include "thermal_base.h"
#include "thermal_budget.h"
#include "thermal_prethrottle.h"

#define EARA_THRM_SYSFS_DIR_NAME "eara_thermal"

struct kobject *thrm_kobj;
static unsigned long __read_mostly mark_addr;

static int eara_thrm_update_tracemark(void)
{
	if (mark_addr)
		return 1;

	mark_addr = kallsyms_lookup_name("tracing_mark_write");

	if (unlikely(!mark_addr))
		return 0;

	return 1;
}

void eara_thrm_systrace(pid_t pid, int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (pid <= 1)
		return;

	if (unlikely(!eara_thrm_update_tracemark()))
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);

	preempt_disable();
	event_trace_printk(mark_addr, "C|%d|%s|%d\n", pid, log, val);
	preempt_enable();
}

void eara_thrm_tracelog(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);

	trace_eara_thrm_log(log);
}

void eara_thrm_sysfs_create_file(struct kobj_attribute *kobj_attr)
{
	int ret;

	if (kobj_attr == NULL || thrm_kobj == NULL)
		return;

	ret = sysfs_create_file(thrm_kobj, &(kobj_attr->attr));
	if (ret)
		pr_debug("Failed to create sysfs file\n");
}

void eara_thrm_sysfs_remove_file(struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL || thrm_kobj == NULL)
		return;

	sysfs_remove_file(thrm_kobj, &(kobj_attr->attr));
}

static int __init eara_thrm_base_init(void)
{
	eara_thrm_update_tracemark();

	if (kernel_kobj == NULL)
		return -1;

	thrm_kobj = kobject_create_and_add(EARA_THRM_SYSFS_DIR_NAME,
			kernel_kobj);

	if (thrm_kobj == NULL)
		return -1;

	eara_thrm_pb_init();
	eara_thrm_pre_init();

	return 0;
}

static void __exit eara_thrm_base_exit(void)
{
	eara_thrm_pb_exit();
	eara_thrm_pre_exit();

	if (thrm_kobj == NULL)
		return;

	kobject_put(thrm_kobj);
	thrm_kobj = NULL;
}

module_init(eara_thrm_base_init);
module_exit(eara_thrm_base_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek EARA-QoS");
MODULE_AUTHOR("MediaTek Inc.");

