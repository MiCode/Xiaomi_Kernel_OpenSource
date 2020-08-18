/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#define MNOC_PMU_POLL_STR1 "c1=%u, c2=%u, c3=%u, c4=%u, c5=%u, c6=%u, "
#define MNOC_PMU_POLL_STR2 "c7=%u, c8=%u, c9=%u, c10=%u, c11=%u, "
#define MNOC_PMU_POLL_STR3 "c12=%u, c13=%u, c14=%u, c15=%u, c16=%u"
#define MNOC_EXCEP_STR "rt_id%d: sw_irq=0x%x,mni_qos_irq=0x%x,"\
	"addr_dec_err=0x%x,mst_parity_err=0x%x,mst_misro_err=0x%x,"\
	"mst_crdt_err=0x%x,slv_parity_err=0x%x,slv_misro_err=0x%x,"\
	"slv_crdt_err=0x%x,req_misro_err=0x%x,rsp_misro_err=0x%x,"\
	"req_to_err=0x%x,rsp_to_err=0x%x,req_cbuf_err=0x%x,"\
	"rsp_cbuf_err=0x%x,req_crdt_err=0x%x,rsp_crdt_err=0x%x"
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mnoc_met_events
#if !defined(_TRACE_MNOC_MET_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MNOC_MET_EVENTS_H
#include <linux/tracepoint.h>
#include "mnoc_hw.h"
TRACE_EVENT(mnoc_pmu_polling,
	TP_PROTO(u32 c[NR_MNOC_PMU_CNTR]),
	TP_ARGS(c),
	TP_STRUCT__entry(
		__array(u32, c, NR_MNOC_PMU_CNTR)
		),
	TP_fast_assign(
		memcpy(__entry->c, c, NR_MNOC_PMU_CNTR * sizeof(u32));
	),
#ifdef MNOC_MET_PMU_FTRACE
	TP_printk("c1=%u", __entry->c[0])
#else
	TP_printk(MNOC_PMU_POLL_STR1 MNOC_PMU_POLL_STR2 MNOC_PMU_POLL_STR3,
		__entry->c[0],
		__entry->c[1],
		__entry->c[2],
		__entry->c[3],
		__entry->c[4],
		__entry->c[5],
		__entry->c[6],
		__entry->c[7],
		__entry->c[8],
		__entry->c[9],
		__entry->c[10],
		__entry->c[11],
		__entry->c[12],
		__entry->c[13],
		__entry->c[14],
		__entry->c[15])
#endif
);

#ifdef MNOC_TAG_TP
TRACE_EVENT(mnoc_excep,
	TP_PROTO(unsigned int rt_id, unsigned int sw_irq,
	unsigned int mni_qos_irq, unsigned int addr_dec_err,
	unsigned int mst_parity_err, unsigned int mst_misro_err,
	unsigned int mst_crdt_err, unsigned int slv_parity_err,
	unsigned int slv_misro_err, unsigned int slv_crdt_err,
	unsigned int req_misro_err, unsigned int rsp_misro_err,
	unsigned int req_to_err, unsigned int rsp_to_err,
	unsigned int req_cbuf_err, unsigned int rsp_cbuf_err,
	unsigned int req_crdt_err, unsigned int rsp_crdt_err),
	TP_ARGS(rt_id, sw_irq, mni_qos_irq, addr_dec_err, mst_parity_err,
	mst_misro_err, mst_crdt_err, slv_parity_err, slv_misro_err,
	slv_crdt_err, req_misro_err, rsp_misro_err, req_to_err, rsp_to_err,
	req_cbuf_err, rsp_cbuf_err, req_crdt_err, rsp_crdt_err),
	TP_STRUCT__entry(
		__field(unsigned int, rt_id)
		__field(unsigned int, sw_irq)
		__field(unsigned int, mni_qos_irq)
		__field(unsigned int, addr_dec_err)
		__field(unsigned int, mst_parity_err)
		__field(unsigned int, mst_misro_err)
		__field(unsigned int, mst_crdt_err)
		__field(unsigned int, slv_parity_err)
		__field(unsigned int, slv_misro_err)
		__field(unsigned int, slv_crdt_err)
		__field(unsigned int, req_misro_err)
		__field(unsigned int, rsp_misro_err)
		__field(unsigned int, req_to_err)
		__field(unsigned int, rsp_to_err)
		__field(unsigned int, req_cbuf_err)
		__field(unsigned int, rsp_cbuf_err)
		__field(unsigned int, req_crdt_err)
		__field(unsigned int, rsp_crdt_err)
	),
	TP_fast_assign(
		__entry->rt_id = rt_id;
		__entry->sw_irq = sw_irq;
		__entry->mni_qos_irq = mni_qos_irq;
		__entry->addr_dec_err = addr_dec_err;
		__entry->mst_parity_err = mst_parity_err;
		__entry->mst_misro_err = mst_misro_err;
		__entry->mst_crdt_err = mst_crdt_err;
		__entry->slv_parity_err = slv_parity_err;
		__entry->slv_misro_err = slv_misro_err;
		__entry->slv_crdt_err = slv_crdt_err;
		__entry->req_misro_err = req_misro_err;
		__entry->rsp_misro_err = rsp_misro_err;
		__entry->req_to_err = req_to_err;
		__entry->rsp_to_err = rsp_to_err;
		__entry->req_cbuf_err = req_cbuf_err;
		__entry->rsp_cbuf_err = rsp_cbuf_err;
		__entry->req_crdt_err = req_crdt_err;
		__entry->rsp_crdt_err = rsp_crdt_err;
	),
	TP_printk(
		MNOC_EXCEP_STR,
		__entry->rt_id,
		__entry->sw_irq,
		__entry->mni_qos_irq,
		__entry->addr_dec_err,
		__entry->mst_parity_err,
		__entry->mst_misro_err,
		__entry->mst_crdt_err,
		__entry->slv_parity_err,
		__entry->slv_misro_err,
		__entry->slv_crdt_err,
		__entry->req_misro_err,
		__entry->rsp_misro_err,
		__entry->req_to_err,
		__entry->rsp_to_err,
		__entry->req_cbuf_err,
		__entry->rsp_cbuf_err,
		__entry->req_crdt_err,
		__entry->rsp_crdt_err)
);
#endif /* MNOC_TAG_TP */

#endif /* _TRACE_MNOC_MET_EVENTS_H */
/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mnoc_met_events
#include <trace/define_trace.h>
