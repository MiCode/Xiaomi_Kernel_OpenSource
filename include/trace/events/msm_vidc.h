/*
 * Copyright (c) 2014-2017, 2019 The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM msm_vidc

#if !defined(_TRACE_MSM_VIDC_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MSM_VIDC_H
#include <linux/types.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(msm_v4l2_vidc,

	TP_PROTO(char *dummy),

	TP_ARGS(dummy),

	TP_STRUCT__entry(
		__field(char *, dummy)
	),

	TP_fast_assign(
		__entry->dummy = dummy;
	),

	TP_printk("%s", __entry->dummy)
);

DEFINE_EVENT(msm_v4l2_vidc, msm_v4l2_vidc_open_start,

	TP_PROTO(char *dummy),

	TP_ARGS(dummy)
);

DEFINE_EVENT(msm_v4l2_vidc, msm_v4l2_vidc_open_end,

	TP_PROTO(char *dummy),

	TP_ARGS(dummy)
);

DEFINE_EVENT(msm_v4l2_vidc, msm_v4l2_vidc_close_start,

	TP_PROTO(char *dummy),

	TP_ARGS(dummy)
);

DEFINE_EVENT(msm_v4l2_vidc, msm_v4l2_vidc_close_end,

	TP_PROTO(char *dummy),

	TP_ARGS(dummy)
);

DEFINE_EVENT(msm_v4l2_vidc, msm_v4l2_vidc_fw_load_start,

	TP_PROTO(char *dummy),

	TP_ARGS(dummy)
);

DEFINE_EVENT(msm_v4l2_vidc, msm_v4l2_vidc_fw_load_end,

	TP_PROTO(char *dummy),

	TP_ARGS(dummy)
);

DECLARE_EVENT_CLASS(msm_vidc_common,

	TP_PROTO(void *instp, int old_state, int new_state),

	TP_ARGS(instp, old_state, new_state),

	TP_STRUCT__entry(
		__field(void *, instp)
		__field(int, old_state)
		__field(int, new_state)
	),

	TP_fast_assign(
		__entry->instp = instp;
		__entry->old_state = old_state;
		__entry->new_state = new_state;
	),

	TP_printk("Moved inst: %p from 0x%x to 0x%x",
		__entry->instp,
		__entry->old_state,
		__entry->new_state)
);

DEFINE_EVENT(msm_vidc_common, msm_vidc_common_state_change,

	TP_PROTO(void *instp, int old_state, int new_state),

	TP_ARGS(instp, old_state, new_state)
);

DECLARE_EVENT_CLASS(venus_hfi_var,

	TP_PROTO(u32 cp_start, u32 cp_size,
		u32 cp_nonpixel_start, u32 cp_nonpixel_size),

	TP_ARGS(cp_start, cp_size, cp_nonpixel_start, cp_nonpixel_size),

	TP_STRUCT__entry(
		__field(u32, cp_start)
		__field(u32, cp_size)
		__field(u32, cp_nonpixel_start)
		__field(u32, cp_nonpixel_size)
	),

	TP_fast_assign(
		__entry->cp_start = cp_start;
		__entry->cp_size = cp_size;
		__entry->cp_nonpixel_start = cp_nonpixel_start;
		__entry->cp_nonpixel_size = cp_nonpixel_size;
	),

	TP_printk(
		"TZBSP_MEM_PROTECT_VIDEO_VAR done, cp_start : 0x%x, cp_size : 0x%x, cp_nonpixel_start : 0x%x, cp_nonpixel_size : 0x%x",
		__entry->cp_start,
		__entry->cp_size,
		__entry->cp_nonpixel_start,
		__entry->cp_nonpixel_size)
);

DEFINE_EVENT(venus_hfi_var, venus_hfi_var_done,

	TP_PROTO(u32 cp_start, u32 cp_size,
		u32 cp_nonpixel_start, u32 cp_nonpixel_size),

	TP_ARGS(cp_start, cp_size, cp_nonpixel_start, cp_nonpixel_size)
);

DECLARE_EVENT_CLASS(msm_v4l2_vidc_buffer_events,

	TP_PROTO(char *event_type, u32 device_addr, int64_t timestamp,
		u32 alloc_len, u32 filled_len, u32 offset),

	TP_ARGS(event_type, device_addr, timestamp, alloc_len,
		filled_len, offset),

	TP_STRUCT__entry(
		__field(char *, event_type)
		__field(u32, device_addr)
		__field(int64_t, timestamp)
		__field(u32, alloc_len)
		__field(u32, filled_len)
		__field(u32, offset)
	),

	TP_fast_assign(
		__entry->event_type = event_type;
		__entry->device_addr = device_addr;
		__entry->timestamp = timestamp;
		__entry->alloc_len = alloc_len;
		__entry->filled_len = filled_len;
		__entry->offset = offset;
	),

	TP_printk(
		"%s, device_addr : 0x%x, timestamp : %lld, alloc_len : 0x%x, filled_len : 0x%x, offset : 0x%x",
		__entry->event_type,
		__entry->device_addr,
		__entry->timestamp,
		__entry->alloc_len,
		__entry->filled_len,
		__entry->offset)
);

DEFINE_EVENT(msm_v4l2_vidc_buffer_events, msm_v4l2_vidc_buffer_event_start,

	TP_PROTO(char *event_type, u32 device_addr, int64_t timestamp,
		u32 alloc_len, u32 filled_len, u32 offset),

	TP_ARGS(event_type, device_addr, timestamp, alloc_len,
		filled_len, offset)
);

DEFINE_EVENT(msm_v4l2_vidc_buffer_events, msm_v4l2_vidc_buffer_event_end,

	TP_PROTO(char *event_type, u32 device_addr, int64_t timestamp,
		u32 alloc_len, u32 filled_len, u32 offset),

	TP_ARGS(event_type, device_addr, timestamp, alloc_len,
		filled_len, offset)
);

DECLARE_EVENT_CLASS(msm_smem_buffer_ion_ops,

	TP_PROTO(char *buffer_op, u32 buffer_type, u32 heap_mask,
		size_t size, u32 align, u32 flags, int map_kernel),

	TP_ARGS(buffer_op, buffer_type, heap_mask, size, align,
		flags, map_kernel),

	TP_STRUCT__entry(
		__string(buffer_op, buffer_op)
		__field(u32, buffer_type)
		__field(u32, heap_mask)
		__field(u32, size)
		__field(u32, align)
		__field(u32, flags)
		__field(int, map_kernel)
	),

	TP_fast_assign(
		 __assign_str(buffer_op, buffer_op);
		__entry->buffer_type = buffer_type;
		__entry->heap_mask = heap_mask;
		__entry->size = size;
		__entry->align = align;
		__entry->flags = flags;
		__entry->map_kernel = map_kernel;
	),

	TP_printk(
		"%s, buffer_type : 0x%x, heap_mask : 0x%x, size : 0x%x, align : 0x%x, flags : 0x%x, map_kernel : %d",
		__get_str(buffer_op),
		__entry->buffer_type,
		__entry->heap_mask,
		__entry->size,
		__entry->align,
		__entry->flags,
		__entry->map_kernel)
);

DEFINE_EVENT(msm_smem_buffer_ion_ops, msm_smem_buffer_ion_op_start,

	TP_PROTO(char *buffer_op, u32 buffer_type, u32 heap_mask,
		size_t size, u32 align, u32 flags, int map_kernel),

	TP_ARGS(buffer_op, buffer_type, heap_mask, size, align,
		flags, map_kernel)
);

DEFINE_EVENT(msm_smem_buffer_ion_ops, msm_smem_buffer_ion_op_end,

	TP_PROTO(char *buffer_op, u32 buffer_type, u32 heap_mask,
		size_t size, u32 align, u32 flags, int map_kernel),

	TP_ARGS(buffer_op, buffer_type, heap_mask, size, align,
		flags, map_kernel)
);
DECLARE_EVENT_CLASS(msm_smem_buffer_dma_ops,

	TP_PROTO(char *buffer_op, u32 buffer_type, u32 heap_mask,
		size_t size, u32 align, u32 flags, int map_kernel),

	TP_ARGS(buffer_op, buffer_type, heap_mask, size, align,
		flags, map_kernel),

	TP_STRUCT__entry(
		__string(buffer_op, buffer_op)
		__field(u32, buffer_type)
		__field(u32, heap_mask)
		__field(u32, size)
		__field(u32, align)
		__field(u32, flags)
		__field(int, map_kernel)
	),

	TP_fast_assign(
		 __assign_str(buffer_op, buffer_op);
		__entry->buffer_type = buffer_type;
		__entry->heap_mask = heap_mask;
		__entry->size = size;
		__entry->align = align;
		__entry->flags = flags;
		__entry->map_kernel = map_kernel;
	),

	 TP_printk(
		"%s, buffer_type : 0x%x, heap_mask : 0x%x, size : 0x%x, align : 0x%x, flags : 0x%x, map_kernel : %d",
		__get_str(buffer_op),
		__entry->buffer_type,
		__entry->heap_mask,
		__entry->size,
		__entry->align,
		__entry->flags,
		__entry->map_kernel)
);

DEFINE_EVENT(msm_smem_buffer_dma_ops, msm_smem_buffer_dma_op_start,

	TP_PROTO(char *buffer_op, u32 buffer_type, u32 heap_mask,
		size_t size, u32 align, u32 flags, int map_kernel),

	TP_ARGS(buffer_op, buffer_type, heap_mask, size, align,
		flags, map_kernel)
);

DEFINE_EVENT(msm_smem_buffer_dma_ops, msm_smem_buffer_dma_op_end,

	TP_PROTO(char *buffer_op, u32 buffer_type, u32 heap_mask,
		size_t size, u32 align, u32 flags, int map_kernel),

	TP_ARGS(buffer_op, buffer_type, heap_mask, size, align,
		flags, map_kernel)
);

DECLARE_EVENT_CLASS(msm_smem_buffer_iommu_ops,

	TP_PROTO(char *buffer_op, int domain_num, int partition_num,
		unsigned long align, unsigned long iova,
		unsigned long buffer_size),

	TP_ARGS(buffer_op, domain_num, partition_num, align, iova, buffer_size),

	TP_STRUCT__entry(
		__string(buffer_op, buffer_op)
		__field(int, domain_num)
		__field(int, partition_num)
		__field(unsigned long, align)
		__field(unsigned long, iova)
		__field(unsigned long, buffer_size)
	),

	TP_fast_assign(
		__assign_str(buffer_op, buffer_op);
		__entry->domain_num = domain_num;
		__entry->partition_num = partition_num;
		__entry->align = align;
		__entry->iova = iova;
		__entry->buffer_size = buffer_size;
	),

	TP_printk(
		"%s, domain : %d, partition : %d, align : %lx, iova : 0x%lx, buffer_size=%lx",
		__get_str(buffer_op),
		__entry->domain_num,
		__entry->partition_num,
		__entry->align,
		__entry->iova,
		__entry->buffer_size)
);

DEFINE_EVENT(msm_smem_buffer_iommu_ops, msm_smem_buffer_iommu_op_start,

	TP_PROTO(char *buffer_op, int domain_num, int partition_num,
		unsigned long align, unsigned long iova,
		unsigned long buffer_size),

	TP_ARGS(buffer_op, domain_num, partition_num, align, iova, buffer_size)
);

DEFINE_EVENT(msm_smem_buffer_iommu_ops, msm_smem_buffer_iommu_op_end,

	TP_PROTO(char *buffer_op, int domain_num, int partition_num,
		unsigned long align, unsigned long iova,
		unsigned long buffer_size),

	TP_ARGS(buffer_op, domain_num, partition_num, align, iova, buffer_size)
);

DECLARE_EVENT_CLASS(msm_vidc_perf,

	TP_PROTO(const char *name, unsigned long value),

	TP_ARGS(name, value),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(unsigned long, value)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->value = value;
	),

	TP_printk("%s %lu", __entry->name, __entry->value)
);

DEFINE_EVENT(msm_vidc_perf, msm_vidc_perf_clock_scale,

	TP_PROTO(const char *clock_name, unsigned long frequency),

	TP_ARGS(clock_name, frequency)
);

DEFINE_EVENT(msm_vidc_perf, msm_vidc_perf_bus_vote,

	TP_PROTO(const char *governor_mode, unsigned long ab),

	TP_ARGS(governor_mode, ab)
);

#endif

#include <trace/define_trace.h>
