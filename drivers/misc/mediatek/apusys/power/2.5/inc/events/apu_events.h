/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM apupwr_events
#if !defined(_APUPWR_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _APUPWR_EVENTS_H
#include <linux/tracepoint.h>
#include "apupw_tag.h"
#include "apu_common.h"

TRACE_EVENT(apupwr_pwr,
	TP_PROTO(struct apupwr_tag_pwr *pw),
	TP_ARGS(pw),
	TP_STRUCT__entry(
		__field(unsigned int, vvpu)
		__field(unsigned int, vmdla)
		__field(unsigned int, vcore)
		__field(unsigned int, vsram)
		__field(unsigned int, dsp1_freq)
		__field(unsigned int, dsp2_freq)
		__field(unsigned int, dsp3_freq)
		__field(unsigned int, dsp5_freq)
		__field(unsigned int, dsp6_freq)
		__field(unsigned int, dsp7_freq)
		__field(unsigned int, dsp_freq)
		__field(unsigned int, ipuif_freq)
	),
	TP_fast_assign(
		__entry->vvpu = pw->vvpu;
		__entry->vmdla = pw->vmdla;
		__entry->vcore = pw->vcore;
		__entry->vsram = pw->vsram;
		__entry->dsp1_freq = pw->dsp1_freq;
		__entry->dsp2_freq = pw->dsp2_freq;
		__entry->dsp3_freq = pw->dsp3_freq;
		__entry->dsp5_freq = pw->dsp5_freq;
		__entry->dsp6_freq = pw->dsp6_freq;
		__entry->dsp7_freq = pw->dsp7_freq;
		__entry->dsp_freq = pw->dsp_freq;
		__entry->ipuif_freq = pw->ipuif_freq;
	),
	TP_printk(
		"V_vpu=%d,V_mdla=%d,V_core=%d,V_sram=%d,F_v0=%d,F_v1=%d,F_v2=%d,F_m0=%d,F_m1=%d,F_iommu= %d,F_conn=%d,F_ipuif=%d",
		TOMV(__entry->vvpu),
		TOMV(__entry->vmdla),
		TOMV(__entry->vcore),
		TOMV(__entry->vsram),
		TOMHZ(__entry->dsp1_freq),
		TOMHZ(__entry->dsp2_freq),
		TOMHZ(__entry->dsp3_freq),
		TOMHZ(__entry->dsp5_freq),
		TOMHZ(__entry->dsp6_freq),
		TOMHZ(__entry->dsp7_freq),
		TOMHZ(__entry->dsp_freq),
		TOMHZ(__entry->ipuif_freq))
);

TRACE_EVENT(apupwr_rpc,
	TP_PROTO(struct apupwr_tag_rpc *rpc),
	TP_ARGS(rpc),
	TP_STRUCT__entry(
		__field(unsigned int, spm_wakeup)
		__field(unsigned int, rpc_intf_rdy)
		__field(unsigned int, vcore_cg_stat)
		__field(unsigned int, conn_cg_stat)
		__field(unsigned int, vpu0_cg_stat)
		__field(unsigned int, vpu1_cg_stat)
		__field(unsigned int, vpu2_cg_stat)
		__field(unsigned int, mdla0_cg_stat)
		__field(unsigned int, mdla1_cg_stat)
	),
	TP_fast_assign(
		__entry->spm_wakeup = rpc->spm_wakeup;
		__entry->rpc_intf_rdy = rpc->rpc_intf_rdy;
		__entry->vcore_cg_stat = rpc->vcore_cg_stat;
		__entry->conn_cg_stat = rpc->conn_cg_stat;
		__entry->vpu0_cg_stat = rpc->vpu0_cg_stat;
		__entry->vpu1_cg_stat = rpc->vpu1_cg_stat;
		__entry->vpu2_cg_stat = rpc->vpu2_cg_stat;
		__entry->mdla0_cg_stat = rpc->mdla0_cg_stat;
		__entry->mdla1_cg_stat = rpc->mdla1_cg_stat;
	),
	TP_printk(
		"spm_wakeup=0x%x,rpc_intf_rdy=0x%x,vcore_cg=0x%x,conn_cg=0x%x,v0_cg=0x%x,v1_cg=0x%x,v2_cg=0x%x,m0_cg=0x%x,m1_cg=0x%x",
		__entry->spm_wakeup,
		__entry->rpc_intf_rdy,
		__entry->vcore_cg_stat,
		__entry->conn_cg_stat,
		__entry->vpu0_cg_stat,
		__entry->vpu1_cg_stat,
		__entry->vpu2_cg_stat,
		__entry->mdla0_cg_stat,
		__entry->mdla1_cg_stat)
);

TRACE_EVENT(apupwr_dvfs,
	TP_PROTO(char *gov_name, const char *p_name, const char *c_name, u32 opp, ulong freq),
	TP_ARGS(gov_name, p_name, c_name, opp, freq),
	TP_STRUCT__entry(
		__string(gov_name, gov_name)
		__string(p_name, p_name)
		__string(c_name, c_name)
		__field(u32, opp)
		__field(ulong, freq)
	),
	TP_fast_assign(
		__assign_str(gov_name, gov_name);
		__assign_str(p_name, p_name);
		__assign_str(c_name, c_name);
		__entry->opp = opp;
		__entry->freq = freq;
	),
	TP_printk("[%s] %s->%s[%d/%lu]",
		  __get_str(gov_name), __get_str(p_name), __get_str(c_name),
		  __entry->opp, __entry->freq)
);

#endif /* if !defined(_APUPWR_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE events/apu_events
#include <trace/define_trace.h>

