/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#if !defined(_ADRENO_A2XX_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _ADRENO_A2XX_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM kgsl
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE adreno_a2xx_trace

#include <linux/tracepoint.h>

struct kgsl_device;

/*
 * Tracepoint for a2xx irq. Includes status info
 */
TRACE_EVENT(kgsl_a2xx_irq_status,

	TP_PROTO(struct kgsl_device *device, unsigned int master_status,
		 unsigned int status),

	TP_ARGS(device, master_status, status),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, master_status)
		__field(unsigned int, status)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->master_status = master_status;
		__entry->status = status;
	),

	TP_printk(
		"d_name=%s master=%s status=%s",
		__get_str(device_name),
		__entry->master_status ? __print_flags(__entry->master_status,
			"|",
			{ MASTER_INT_SIGNAL__MH_INT_STAT, "MH" },
			{ MASTER_INT_SIGNAL__SQ_INT_STAT, "SQ" },
			{ MASTER_INT_SIGNAL__CP_INT_STAT, "CP" },
			{ MASTER_INT_SIGNAL__RBBM_INT_STAT, "RBBM" }) : "None",
		__entry->status ? __print_flags(__entry->status, "|",
			{ CP_INT_CNTL__SW_INT_MASK, "SW" },
			{ CP_INT_CNTL__T0_PACKET_IN_IB_MASK,
				"T0_PACKET_IN_IB" },
			{ CP_INT_CNTL__OPCODE_ERROR_MASK, "OPCODE_ERROR" },
			{ CP_INT_CNTL__PROTECTED_MODE_ERROR_MASK,
				"PROTECTED_MODE_ERROR" },
			{ CP_INT_CNTL__RESERVED_BIT_ERROR_MASK,
				"RESERVED_BIT_ERROR" },
			{ CP_INT_CNTL__IB_ERROR_MASK, "IB_ERROR" },
			{ CP_INT_CNTL__IB2_INT_MASK, "IB2" },
			{ CP_INT_CNTL__IB1_INT_MASK, "IB1" },
			{ CP_INT_CNTL__RB_INT_MASK, "RB" }) : "None"
	)
);

#endif /* _ADRENO_A2XX_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
