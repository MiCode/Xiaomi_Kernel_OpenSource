/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#if !defined(_ADRENO_A3XX_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _ADRENO_A3XX_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM kgsl
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE adreno_a3xx_trace

#include <linux/tracepoint.h>

struct kgsl_device;

/*
 * Tracepoint for a3xx irq. Includes status info
 */
TRACE_EVENT(kgsl_a3xx_irq_status,

	TP_PROTO(struct kgsl_device *device, unsigned int status),

	TP_ARGS(device, status),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, status)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->status = status;
	),

	TP_printk(
		"d_name=%s status=%s",
		__get_str(device_name),
		__entry->status ? __print_flags(__entry->status, "|",
			{ 1 << A3XX_INT_RBBM_GPU_IDLE, "RBBM_GPU_IDLE" },
			{ 1 << A3XX_INT_RBBM_AHB_ERROR, "RBBM_AHB_ERR" },
			{ 1 << A3XX_INT_RBBM_REG_TIMEOUT, "RBBM_REG_TIMEOUT" },
			{ 1 << A3XX_INT_RBBM_ME_MS_TIMEOUT,
				"RBBM_ME_MS_TIMEOUT" },
			{ 1 << A3XX_INT_RBBM_PFP_MS_TIMEOUT,
				"RBBM_PFP_MS_TIMEOUT" },
			{ 1 << A3XX_INT_RBBM_ATB_BUS_OVERFLOW,
				"RBBM_ATB_BUS_OVERFLOW" },
			{ 1 << A3XX_INT_VFD_ERROR, "RBBM_VFD_ERROR" },
			{ 1 << A3XX_INT_CP_SW_INT, "CP_SW" },
			{ 1 << A3XX_INT_CP_T0_PACKET_IN_IB,
				"CP_T0_PACKET_IN_IB" },
			{ 1 << A3XX_INT_CP_OPCODE_ERROR, "CP_OPCODE_ERROR" },
			{ 1 << A3XX_INT_CP_RESERVED_BIT_ERROR,
				"CP_RESERVED_BIT_ERROR" },
			{ 1 << A3XX_INT_CP_HW_FAULT, "CP_HW_FAULT" },
			{ 1 << A3XX_INT_CP_DMA, "CP_DMA" },
			{ 1 << A3XX_INT_CP_IB2_INT, "CP_IB2_INT" },
			{ 1 << A3XX_INT_CP_IB1_INT, "CP_IB1_INT" },
			{ 1 << A3XX_INT_CP_RB_INT, "CP_RB_INT" },
			{ 1 << A3XX_INT_CP_REG_PROTECT_FAULT,
				"CP_REG_PROTECT_FAULT" },
			{ 1 << A3XX_INT_CP_RB_DONE_TS, "CP_RB_DONE_TS" },
			{ 1 << A3XX_INT_CP_VS_DONE_TS, "CP_VS_DONE_TS" },
			{ 1 << A3XX_INT_CP_PS_DONE_TS, "CP_PS_DONE_TS" },
			{ 1 << A3XX_INT_CACHE_FLUSH_TS, "CACHE_FLUSH_TS" },
			{ 1 << A3XX_INT_CP_AHB_ERROR_HALT,
				"CP_AHB_ERROR_HALT" },
			{ 1 << A3XX_INT_MISC_HANG_DETECT, "MISC_HANG_DETECT" },
			{ 1 << A3XX_INT_UCHE_OOB_ACCESS, "UCHE_OOB_ACCESS" })
		: "None"
	)
);

#endif /* _ADRENO_A3XX_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
