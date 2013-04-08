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
#define TRACE_SYSTEM mmc

#if !defined(_TRACE_MMC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMC_H

#include <linux/tracepoint.h>

TRACE_EVENT(mmc_clk,
		TP_PROTO(char *print_info),

		TP_ARGS(print_info),

		TP_STRUCT__entry(
			__string(print_info, print_info)
		),

		TP_fast_assign(
			__assign_str(print_info, print_info);
		),

		TP_printk("%s",
			__get_str(print_info)
		)
);

#endif /* if !defined(_TRACE_MMC_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
