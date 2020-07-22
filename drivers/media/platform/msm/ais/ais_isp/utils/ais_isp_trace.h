/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#if !defined(_AIS_ISP_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _AIS_ISP_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM camera
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ais_isp_trace

#include <linux/tracepoint.h>
#include <media/cam_req_mgr.h>
#include "ais_isp_hw.h"

TRACE_EVENT(ais_isp_vfe_irq_activated,
	TP_PROTO(uint8_t id, uint32_t status0, uint32_t status1),
	TP_ARGS(id, status0, status1),
	TP_STRUCT__entry(
		__field(uint8_t, id)
		__field(uint32_t, status0)
		__field(uint32_t, status1)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->status0 = status0;
		__entry->status1 = status1;
	),
	TP_printk(
		"vfe%d: irq 0x%08x 0x%08x",
		__entry->id,
		__entry->status0, __entry->status1
	)
);

TRACE_EVENT(ais_isp_irq_process,
	TP_PROTO(uint8_t id, uint8_t evt, uint8_t state),
	TP_ARGS(id, evt, state),
	TP_STRUCT__entry(
		__field(uint8_t, id)
		__field(uint8_t, evt)
		__field(uint8_t, state)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->evt = evt;
		__entry->state = state;
	),
	TP_printk(
		"vfe%d: irq event %d (%d)",
		__entry->id, __entry->evt, __entry->state
	)
);

TRACE_EVENT(ais_isp_vfe_state,
	TP_PROTO(uint8_t id, uint8_t path, uint8_t state),
	TP_ARGS(id, path, state),
	TP_STRUCT__entry(
		__field(uint8_t, id)
		__field(uint8_t, path)
		__field(uint8_t, state)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->path = path;
		__entry->state = state;
	),
	TP_printk(
		"vfe%d:%d: state %d",
		__entry->id, __entry->path, __entry->state
	)
);

TRACE_EVENT(ais_isp_vfe_sof,
	TP_PROTO(uint8_t id, uint8_t path, struct ais_ife_rdi_timestamps *ts,
			uint32_t fifo, uint32_t miss),
	TP_ARGS(id, path, ts, fifo, miss),
	TP_STRUCT__entry(
		__field(uint8_t, id)
		__field(uint8_t, path)
		__field(uint32_t, fifo)
		__field(uint32_t, miss)
		__field(uint64_t, ts_cur)
		__field(uint64_t, ts_prev)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->path = path;
		__entry->fifo = fifo;
		__entry->miss = miss;
		__entry->ts_cur = ts->cur_sof_ts;
		__entry->ts_prev = ts->prev_sof_ts;
	),
	TP_printk(
		"vfe%d:%d: sof %llu %llu fifo %u miss %u",
		__entry->id, __entry->path, __entry->ts_cur, __entry->ts_prev,
		__entry->fifo, __entry->miss
	)
);

TRACE_EVENT(ais_isp_vfe_q_sof,
	TP_PROTO(uint8_t id, uint8_t path, uint32_t frame, uint64_t ts),
	TP_ARGS(id, path, frame, ts),
	TP_STRUCT__entry(
		__field(uint8_t, id)
		__field(uint8_t, path)
		__field(uint32_t, frame)
		__field(uint64_t, ts)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->path = path;
		__entry->frame = frame;
		__entry->ts = ts;
	),
	TP_printk(
		"vfe%d:%d: sof %d %llu",
		__entry->id, __entry->path, __entry->frame, __entry->ts
	)
);

TRACE_EVENT(ais_isp_vfe_buf_done,
	TP_PROTO(uint8_t id, uint8_t path, uint8_t idx, uint32_t frame,
			uint8_t fifo, uint8_t match),
	TP_ARGS(id, path, idx, frame, fifo, match),
	TP_STRUCT__entry(
		__field(uint8_t, id)
		__field(uint8_t, path)
		__field(uint8_t, idx)
		__field(uint32_t, frame)
		__field(uint8_t, fifo)
		__field(uint8_t, match)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->path = path;
		__entry->idx = idx;
		__entry->frame = frame;
		__entry->fifo = fifo;
		__entry->match = match;
	),
	TP_printk(
		"vfe%d:%d: buf_done %d (%d fifo %d match %d)",
		__entry->id, __entry->path, __entry->idx, __entry->frame,
		__entry->fifo, __entry->match
	)
);

TRACE_EVENT(ais_isp_vfe_enq_buf_hw,
	TP_PROTO(uint8_t id, uint8_t path, uint8_t idx,
			uint8_t fifo, uint8_t full),
	TP_ARGS(id, path, idx, fifo, full),
	TP_STRUCT__entry(
		__field(uint8_t, id)
		__field(uint8_t, path)
		__field(uint8_t, idx)
		__field(uint8_t, fifo)
		__field(uint8_t, full)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->path = path;
		__entry->idx = idx;
		__entry->fifo = fifo;
		__entry->full = full;
	),
	TP_printk(
		"vfe%d:%d: enq buf hw %d fifo %d full %d",
		__entry->id, __entry->path, __entry->idx,
		__entry->fifo, __entry->full
	)
);

TRACE_EVENT(ais_isp_vfe_enq_req,
	TP_PROTO(uint8_t id, uint8_t path, uint8_t idx),
	TP_ARGS(id, path, idx),
	TP_STRUCT__entry(
		__field(uint8_t, id)
		__field(uint8_t, path)
		__field(uint8_t, idx)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->path = path;
		__entry->idx = idx;
	),
	TP_printk(
		"vfe%d:%d: enq req %d",
		__entry->id, __entry->path, __entry->idx
	)
);

TRACE_EVENT(ais_isp_vfe_error,
	TP_PROTO(uint8_t id, uint8_t path, uint8_t err, uint8_t payload),
	TP_ARGS(id, path, err, payload),
	TP_STRUCT__entry(
		__field(uint8_t, id)
		__field(uint8_t, path)
		__field(uint8_t, err)
		__field(uint8_t, payload)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->path = path;
		__entry->err = err;
		__entry->payload = payload;
	),
	TP_printk(
		"vfe%d:%d: error %d %d",
		__entry->id, __entry->path, __entry->err, __entry->payload
	)
);

#endif /* _AIS_ISP_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
