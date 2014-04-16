/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM msm_bus

#if !defined(_TRACE_MSM_BUS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MSM_BUS_H

#include <linux/tracepoint.h>

TRACE_EVENT(bus_update_request,

	TP_PROTO(int sec, int nsec, const char *name, unsigned int index,
		int src, int dest, unsigned long long ab,
		unsigned long long ib),

	TP_ARGS(sec, nsec, name, index, src, dest, ab, ib),

	TP_STRUCT__entry(
		__field(int, sec)
		__field(int, nsec)
		__string(name, name)
		__field(u32, index)
		__field(int, src)
		__field(int, dest)
		__field(u64, ab)
		__field(u64, ib)
	),

	TP_fast_assign(
		__entry->sec = sec;
		__entry->nsec = nsec;
		__assign_str(name, name);
		__entry->index = index;
		__entry->src = src;
		__entry->dest = dest;
		__entry->ab = ab;
		__entry->ib = ib;
	),

	TP_printk("time= %d.%d name=%s index=%u src=%d dest=%d ab=%llu ib=%llu",
		__entry->sec,
		__entry->nsec,
		__get_str(name),
		(unsigned int)__entry->index,
		__entry->src,
		__entry->dest,
		(unsigned long long)__entry->ab,
		(unsigned long long)__entry->ib)
);

TRACE_EVENT(bus_bimc_config_limiter,

	TP_PROTO(int mas_id, unsigned long long cur_lim_bw),

	TP_ARGS(mas_id, cur_lim_bw),

	TP_STRUCT__entry(
		__field(int, mas_id)
		__field(u64, cur_lim_bw)
	),

	TP_fast_assign(
		__entry->mas_id = mas_id;
		__entry->cur_lim_bw = cur_lim_bw;
	),

	TP_printk("Master=%d cur_lim_bw=%llu",
		__entry->mas_id,
		(unsigned long long)__entry->cur_lim_bw)
);

TRACE_EVENT(bus_avail_bw,

	TP_PROTO(unsigned long long cur_bimc_bw, unsigned long long cur_mdp_bw),

	TP_ARGS(cur_bimc_bw, cur_mdp_bw),

	TP_STRUCT__entry(
		__field(u64, cur_bimc_bw)
		__field(u64, cur_mdp_bw)
	),

	TP_fast_assign(
		__entry->cur_bimc_bw = cur_bimc_bw;
		__entry->cur_mdp_bw = cur_mdp_bw;
	),

	TP_printk("cur_bimc_bw = %llu cur_mdp_bw = %llu",
		(unsigned long long)__entry->cur_bimc_bw,
		(unsigned long long)__entry->cur_mdp_bw)
);

TRACE_EVENT(bus_bke_params,

	TP_PROTO(u32 gc, u32 gp, u32 thl, u32 thm, u32 thh),

	TP_ARGS(gc, gp, thl, thm, thh),

	TP_STRUCT__entry(
		__field(u32, gc)
		__field(u32, gp)
		__field(u32, thl)
		__field(u32, thm)
		__field(u32, thh)
	),

	TP_fast_assign(
		__entry->gc = gc;
		__entry->gp = gp;
		__entry->thl = thl;
		__entry->thm = thm;
		__entry->thh = thh;
	),

	TP_printk("BKE Params GC=0x%x GP=0x%x THL=0x%x THM=0x%x THH=0x%x",
		__entry->gc, __entry->gp, __entry->thl, __entry->thm,
			__entry->thh)
);

#endif
#define TRACE_INCLUDE_FILE trace_msm_bus
#include <trace/define_trace.h>
