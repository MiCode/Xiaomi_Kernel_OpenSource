// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/trace_events.h>
#include <trace/events/fpsgo.h>
#include "thermal_base.h"

#define EARA_THRM_SYSFS_DIR_NAME "eara_thermal"

struct kobject *thrm_kobj;
static unsigned long __read_mostly mark_addr;

void eara_thrm_systrace(pid_t pid, int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (pid <= 1)
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (len == 256)
		log[255] = '\0';

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
	if (kobj_attr == NULL || thrm_kobj == NULL)
		return;

	sysfs_create_file(thrm_kobj, &(kobj_attr->attr));
}

void eara_thrm_sysfs_remove_file(struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL || thrm_kobj == NULL)
		return;

	sysfs_remove_file(thrm_kobj, &(kobj_attr->attr));
}

int eara_thrm_base_init(void)
{
	mark_addr = kallsyms_lookup_name("tracing_mark_write");

	if (kernel_kobj == NULL)
		return -1;

	thrm_kobj = kobject_create_and_add(EARA_THRM_SYSFS_DIR_NAME,
			kernel_kobj);

	if (thrm_kobj == NULL)
		return -1;

	return 0;
}

void eara_thrm_base_exit(void)
{
	if (thrm_kobj == NULL)
		return;

	kobject_put(thrm_kobj);
	thrm_kobj = NULL;
}

