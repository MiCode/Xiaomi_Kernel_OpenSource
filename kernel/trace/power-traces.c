// SPDX-License-Identifier: GPL-2.0
/*
 * Power trace points
 *
 * Copyright (C) 2009 Arjan van de Ven <arjan@linux.intel.com>
 */

#include <linux/string.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/module.h>

#define CREATE_TRACE_POINTS
#include <trace/events/power.h>

EXPORT_TRACEPOINT_SYMBOL_GPL(suspend_resume);
EXPORT_TRACEPOINT_SYMBOL_GPL(cpu_idle);
EXPORT_TRACEPOINT_SYMBOL_GPL(cpu_frequency);
EXPORT_TRACEPOINT_SYMBOL_GPL(powernv_throttle);
EXPORT_TRACEPOINT_SYMBOL(memlat_dev_update);
EXPORT_TRACEPOINT_SYMBOL(memlat_dev_meas);
EXPORT_TRACEPOINT_SYMBOL(cache_hwmon_update);
EXPORT_TRACEPOINT_SYMBOL(cache_hwmon_meas);
EXPORT_TRACEPOINT_SYMBOL(bw_hwmon_update);
EXPORT_TRACEPOINT_SYMBOL(bw_hwmon_meas);
EXPORT_TRACEPOINT_SYMBOL(bw_hwmon_debug);
EXPORT_TRACEPOINT_SYMBOL_GPL(device_pm_callback_start);
EXPORT_TRACEPOINT_SYMBOL_GPL(device_pm_callback_end);
