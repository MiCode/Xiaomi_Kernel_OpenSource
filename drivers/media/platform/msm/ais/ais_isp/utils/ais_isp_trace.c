/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

/* Instantiate tracepoints */
#define CREATE_TRACE_POINTS

#include <linux/io.h>
#include <linux/module.h>

#include "ais_isp_trace.h"

static uint debug_trace;
module_param(debug_trace, uint, 0644);

void ais_trace_print(char c, int value, const char *fmt, ...)
{
	if (debug_trace) {
		char str_buffer[256];
		va_list args;

		va_start(args, fmt);
		vsnprintf(str_buffer, 256, fmt, args);

		trace_ais_tracing_mark_write(c, current, str_buffer, value);
		va_end(args);
	}
}

