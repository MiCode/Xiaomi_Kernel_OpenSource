/*
 *
 * (C) COPYRIGHT 2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM gpu_mem
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mali_gpu_mem_trace

#if !defined(_TRACE_MALI_GPU_MEM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MALI_GPU_MEM_H

#include <linux/tracepoint.h>

/*
 * trace_gpu_mem_total
 *
 * The gpu_memory_total event indicates that there's an update to either the
 * global or process total gpu memory counters.
 *
 * This event should be emitted whenever the kernel device driver allocates,
 * frees, imports, unimports memory in the GPU addressable space.
 *
 * @gpu_id: Kbase device id.
 * @pid: This is either the thread group ID of the process for which there was
 *       an update in the GPU memory usage or 0 so as to indicate an update in
 *       the device wide GPU memory usage.
 * @size: GPU memory usage in bytes.
 */
TRACE_EVENT(gpu_mem_total,
	TP_PROTO(uint32_t gpu_id, uint32_t pid, uint64_t size),

	TP_ARGS(gpu_id, pid, size),

	TP_STRUCT__entry(
		__field(uint32_t, gpu_id)
		__field(uint32_t, pid)
		__field(uint64_t, size)
	),

	TP_fast_assign(
		__entry->gpu_id = gpu_id;
		__entry->pid = pid;
		__entry->size = size;
	),

	TP_printk("gpu_id=%u pid=%u size=%llu",
		__entry->gpu_id,
		__entry->pid,
		__entry->size)
);
#endif /* _TRACE_MALI_GPU_MEM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
