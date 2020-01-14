/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2016, 2019, The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM msm_bus

#if !defined(_TRACE_MSM_BUS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MSM_BUS_H

#include <linux/tracepoint.h>

TRACE_EVENT(bus_update_request,

	TP_PROTO(int sec, int nsec, const char *name, int src, int dest,
		unsigned long long ab, unsigned long long ib),

	TP_ARGS(sec, nsec, name, src, dest, ab, ib),

	TP_STRUCT__entry(
		__field(int, sec)
		__field(int, nsec)
		__string(name, name)
		__field(int, src)
		__field(int, dest)
		__field(u64, ab)
		__field(u64, ib)
	),

	TP_fast_assign(
		__entry->sec = sec;
		__entry->nsec = nsec;
		__assign_str(name, name);
		__entry->src = src;
		__entry->dest = dest;
		__entry->ab = ab;
		__entry->ib = ib;
	),

	TP_printk("time= %u.%09u name=%s src=%d dest=%d ab=%llu ib=%llu",
		__entry->sec,
		__entry->nsec,
		__get_str(name),
		__entry->src,
		__entry->dest,
		(unsigned long long)__entry->ab,
		(unsigned long long)__entry->ib)
);

TRACE_EVENT(bus_update_request_end,

	TP_PROTO(const char *name),

	TP_ARGS(name),

	TP_STRUCT__entry(
		__string(name, name)
	),

	TP_fast_assign(
		__assign_str(name, name);
	),

	TP_printk("client-name=%s", __get_str(name))
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

TRACE_EVENT(bus_rules_matches,

	TP_PROTO(int node_id, int rule_id, unsigned long long node_ab,
		unsigned long long node_ib, unsigned long long node_clk),

	TP_ARGS(node_id, rule_id, node_ab, node_ib, node_clk),

	TP_STRUCT__entry(
		__field(int, node_id)
		__field(int, rule_id)
		__field(u64, node_ab)
		__field(u64, node_ib)
		__field(u64, node_clk)
	),

	TP_fast_assign(
		__entry->node_id = node_id;
		__entry->rule_id = rule_id;
		__entry->node_ab = node_ab;
		__entry->node_ib = node_ib;
		__entry->node_clk = node_clk;
	),

	TP_printk("Rule match node%d rule%d node-ab%llu:ib%llu:clk%llu",
		__entry->node_id, __entry->rule_id,
		(unsigned long long)__entry->node_ab,
		(unsigned long long)__entry->node_ib,
		(unsigned long long)__entry->node_clk)
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

TRACE_EVENT(bus_client_status,

	TP_PROTO(const char *name, int src, int dest,
		unsigned long long ab, unsigned long long ib,
		int active_only, int vote_count),

	TP_ARGS(name, src, dest, ab, ib, active_only, vote_count),

	TP_STRUCT__entry(
		__string(name, name)
		__field(int, src)
		__field(int, dest)
		__field(u64, ab)
		__field(u64, ib)
		__field(int, active_only)
		__field(int, vote_count)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->src = src;
		__entry->dest = dest;
		__entry->ab = ab;
		__entry->ib = ib;
		__entry->active_only = active_only;
		__entry->vote_count = vote_count;
	),

	TP_printk("name=%s src=%d dest=%d ab=%llu ib=%llu active_only=%d vote_count=%d",
		__get_str(name),
		__entry->src,
		__entry->dest,
		(unsigned long long)__entry->ab,
		(unsigned long long)__entry->ib,
		__entry->active_only,
		__entry->vote_count)
);

TRACE_EVENT(bus_bcm_client_status,

	TP_PROTO(const char *bcm, const char *client,
		unsigned long long act_ab, unsigned long long act_ib,
		unsigned long long slp_ab, unsigned long long slp_ib),

	TP_ARGS(bcm, client, act_ab, act_ib, slp_ab, slp_ib),

	TP_STRUCT__entry(
		__string(bcm, bcm)
		__string(client, client)
		__field(u64, act_ab)
		__field(u64, act_ib)
		__field(u64, slp_ab)
		__field(u64, slp_ib)
	),

	TP_fast_assign(
		__assign_str(bcm, bcm);
		 __assign_str(client, client);
		__entry->act_ab = act_ab;
		__entry->act_ib = act_ib;
		__entry->slp_ab = slp_ab;
		__entry->slp_ib = slp_ib;
	),

	TP_printk(
		"bcm=%s cl=%s act_ab=%llu act_ib=%llu slp_ab=%llu slp_ib=%llu",
		__get_str(bcm),
		__get_str(client),
		(unsigned long long)__entry->act_ab,
		(unsigned long long)__entry->act_ib,
		(unsigned long long)__entry->slp_ab,
		(unsigned long long)__entry->slp_ib)
);

TRACE_EVENT(bus_agg_bw,

	TP_PROTO(unsigned int node_id, int rpm_id, int ctx_set,
		unsigned long long agg_ab),

	TP_ARGS(node_id, rpm_id, ctx_set, agg_ab),

	TP_STRUCT__entry(
		__field(unsigned int, node_id)
		__field(int, rpm_id)
		__field(int, ctx_set)
		__field(u64, agg_ab)
	),

	TP_fast_assign(
		__entry->node_id = node_id;
		__entry->rpm_id = rpm_id;
		__entry->ctx_set = ctx_set;
		__entry->agg_ab = agg_ab;
	),

	TP_printk("node_id:%u rpm_id:%d rpm_ctx:%d agg_ab:%llu",
		__entry->node_id,
		__entry->rpm_id,
		__entry->ctx_set,
		(unsigned long long)__entry->agg_ab)
);
#endif
#define TRACE_INCLUDE_FILE trace_msm_bus
#include <trace/define_trace.h>
