/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufs

#if !defined(_TRACE_UFS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_UFS_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(ufshcd_template,
	TP_PROTO(const char *dev_name, int err, s64 usecs),

	TP_ARGS(dev_name, err, usecs),

	TP_STRUCT__entry(
		__field(s64, usecs)
		__field(int, err)
		__string(dev_name, dev_name)
	),

	TP_fast_assign(
		__entry->usecs = usecs;
		__entry->err = err;
		__assign_str(dev_name, dev_name);
	),

	TP_printk(
		"took %lld usecs, %s err %d",
		__entry->usecs,
		__get_str(dev_name),
		__entry->err
	)
);

DEFINE_EVENT(ufshcd_template, ufshcd_system_suspend,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(ufshcd_template, ufshcd_system_resume,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(ufshcd_template, ufshcd_runtime_suspend,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(ufshcd_template, ufshcd_runtime_resume,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(ufshcd_template, ufshcd_init,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));
#endif /* if !defined(_TRACE_UFS_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
