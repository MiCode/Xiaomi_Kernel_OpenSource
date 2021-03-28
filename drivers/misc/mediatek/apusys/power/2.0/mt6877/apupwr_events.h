/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM apupwr_events
#if !defined(_APUPWR_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _APUPWR_EVENTS_H
#include <linux/tracepoint.h>

TRACE_EVENT(apupwr_pwr,
	TP_PROTO(unsigned int vvpu, unsigned int vcore,
		 unsigned int vsram, unsigned int vpu0_freq,
		 unsigned int vpu1_freq, unsigned int mdla0_freq,
		 unsigned int conn_freq, unsigned int iommu_freq,
		 unsigned long long id),
	TP_ARGS(vvpu, vcore, vsram, vpu0_freq, vpu1_freq,
		mdla0_freq, conn_freq, iommu_freq, id),
	TP_STRUCT__entry(
		__field(unsigned int, vvpu)
		__field(unsigned int, vcore)
		__field(unsigned int, vsram)
		__field(unsigned int, vpu0_freq)
		__field(unsigned int, vpu1_freq)
		__field(unsigned int, mdla0_freq)
		__field(unsigned int, conn_freq)
		__field(unsigned int, iommu_freq)
		__field(unsigned long long, id)
	),
	TP_fast_assign(
		__entry->vvpu = vvpu;
		__entry->vcore = vcore;
		__entry->vsram = vsram;
		__entry->vpu0_freq = vpu0_freq;
		__entry->vpu1_freq = vpu1_freq;
		__entry->mdla0_freq = mdla0_freq;
		__entry->conn_freq = conn_freq;
		__entry->iommu_freq = iommu_freq;
		__entry->id = id;
	),
	TP_printk(
		"V_vpu=%d,V_core=%d,V_sram=%d,F_v0=%d,F_v1=%d,F_m0=%d,F_conn=%d,F_iommu=%d,time_id=%llu",
		__entry->vvpu,
		__entry->vcore,
		__entry->vsram,
		__entry->vpu0_freq,
		__entry->vpu1_freq,
		__entry->mdla0_freq,
		__entry->conn_freq,
		__entry->iommu_freq,
		__entry->id)
);

TRACE_EVENT(apupwr_rpc,
	TP_PROTO(unsigned int spm_wakeup, unsigned int rpc_intf_rdy,
	unsigned int vcore_cg_stat, unsigned int conn_cg_stat,
	unsigned int conn1_cg_stat, unsigned int vpu0_cg_stat,
	unsigned int vpu1_cg_stat, unsigned int mdla0_cg_stat),
	TP_ARGS(spm_wakeup, rpc_intf_rdy, vcore_cg_stat, conn_cg_stat,
	conn1_cg_stat, vpu0_cg_stat, vpu1_cg_stat, mdla0_cg_stat),
	TP_STRUCT__entry(
		__field(unsigned int, spm_wakeup)
		__field(unsigned int, rpc_intf_rdy)
		__field(unsigned int, vcore_cg_stat)
		__field(unsigned int, conn_cg_stat)
		__field(unsigned int, conn1_cg_stat)
		__field(unsigned int, vpu0_cg_stat)
		__field(unsigned int, vpu1_cg_stat)
		__field(unsigned int, mdla0_cg_stat)
	),
	TP_fast_assign(
		__entry->spm_wakeup = spm_wakeup;
		__entry->rpc_intf_rdy = rpc_intf_rdy;
		__entry->vcore_cg_stat = vcore_cg_stat;
		__entry->conn_cg_stat = conn_cg_stat;
		__entry->conn1_cg_stat = conn1_cg_stat;
		__entry->vpu0_cg_stat = vpu0_cg_stat;
		__entry->vpu1_cg_stat = vpu1_cg_stat;
		__entry->mdla0_cg_stat = mdla0_cg_stat;
	),
	TP_printk(
		"spm_wakeup=0x%x,rpc_intf_rdy=0x%x,vcore_cg=0x%x,conn_cg=0x%x,conn1_cg=0x%x,v0_cg=0x%x,v1_cg=0x%x,m0_cg=0x%x",
		__entry->spm_wakeup,
		__entry->rpc_intf_rdy,
		__entry->vcore_cg_stat,
		__entry->conn_cg_stat,
		__entry->conn1_cg_stat,
		__entry->vpu0_cg_stat,
		__entry->vpu1_cg_stat,
		__entry->mdla0_cg_stat)
);

TRACE_EVENT(apupwr_dvfs,
	TP_PROTO(char *log_str),
	TP_ARGS(log_str),
	TP_STRUCT__entry(
		__array(char, log_str, LOG_STR_LEN)
	),
	TP_fast_assign(
		if (snprintf(__entry->log_str,
			LOG_STR_LEN, "%s", log_str) < 0)
			__entry->log_str[0] = '\0';
	),
	TP_printk("dvfs= %s", __entry->log_str)
);

#endif /* if !defined(_APUPWR_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE apupwr_events
#include <trace/define_trace.h>

