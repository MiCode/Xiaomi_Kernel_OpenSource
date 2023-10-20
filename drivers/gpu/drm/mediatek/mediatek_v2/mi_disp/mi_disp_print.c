// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#include <linux/types.h>
#include <linux/rtc.h>
#include <linux/time.h>

#include "mi_disp_print.h"

void mi_disp_printk(const char *level, const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk("%s" "[" DISP_NAME ":%ps] %pV",
			level, __builtin_return_address(0), &vaf);

	va_end(args);
}

void mi_disp_dbg(const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	if (!is_enable_debug_log())
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk(KERN_DEBUG "[" DISP_NAME ":%ps] %pV",
			__builtin_return_address(0), &vaf);

	va_end(args);
}

void mi_disp_printk_utc(const char *level, const char *format, ...)
{
	time64_t time;
	struct tm tm;
	struct va_format vaf;
	va_list args;

	time = ktime_get_real_seconds();
	time64_to_tm(time, 0, &tm);

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk("%s" "[" DISP_NAME ":%ps][%d-%02d-%02d %02d:%02d:%02d] %pV",
			level, __builtin_return_address(0),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			&vaf);

	va_end(args);
}

void mi_disp_dbg_utc(const char *format, ...)
{
	time64_t time;
	struct tm tm;
	struct va_format vaf;
	va_list args;

	if (!is_enable_debug_log())
		return;

	time = ktime_get_real_seconds();
	time64_to_tm(time, 0, &tm);

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk(KERN_DEBUG "[" DISP_NAME ":%ps][%d-%02d-%02d %02d:%02d:%02d] %pV",
			 __builtin_return_address(0),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			&vaf);

	va_end(args);
}
