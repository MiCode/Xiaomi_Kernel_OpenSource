/* Copyright (c) 2016, 2019, The Linux Foundation. All rights reserved.
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
#undef TRACE_SYSTEM
#define TRACE_SYSTEM msm_cam

#if !defined(_TRACE_MSM_VFE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MSM_VFE_H

#include "msm_isp.h"
#include <linux/types.h>
#include <linux/tracepoint.h>

#define STRING_LEN 80


TRACE_EVENT(msm_cam_string,
	TP_PROTO(const char *str),
	TP_ARGS(str),
	TP_STRUCT__entry(
		__array(char, str, STRING_LEN)
	),
	TP_fast_assign(
		strlcpy(__entry->str, str, STRING_LEN);
	),
	TP_printk("msm_cam: %s", __entry->str)
);

TRACE_EVENT(msm_cam_tasklet_debug_dump,
	TP_PROTO(struct msm_vfe_irq_debug_info tasklet_state),
	TP_ARGS(tasklet_state),
	TP_STRUCT__entry(
		__field(unsigned int, vfe_id)
		__field(unsigned int, core_id)
		__field(unsigned int, irq_status0)
		__field(unsigned int, irq_status1)
		__field(unsigned int, ping_pong_status)
		__field(long, tv_sec)
		__field(long, tv_usec)
	),
	TP_fast_assign(
		__entry->vfe_id = tasklet_state.vfe_id;
		__entry->irq_status0 =
			tasklet_state.irq_status0[tasklet_state.vfe_id];
		__entry->irq_status1 =
			tasklet_state.irq_status1[tasklet_state.vfe_id];
		__entry->core_id = tasklet_state.core_id;
		__entry->ping_pong_status =
			tasklet_state.ping_pong_status[tasklet_state.vfe_id];
		__entry->tv_sec =
			tasklet_state.ts.buf_time.tv_sec;
		__entry->tv_usec =
			tasklet_state.ts.buf_time.tv_usec;
	),
	TP_printk("vfe_id %d, core %d, irq_st0 0x%x, irq_st1 0x%x\n"
		"pi_po_st 0x%x, time %ld:%ld",
		__entry->vfe_id,
		__entry->core_id,
		__entry->irq_status0,
		__entry->irq_status1,
		__entry->ping_pong_status,
		__entry->tv_sec,
		__entry->tv_usec
	)
);

TRACE_EVENT(msm_cam_ping_pong_debug_dump,
	TP_PROTO(struct msm_vfe_irq_debug_info ping_pong_state),
	TP_ARGS(ping_pong_state),
	TP_STRUCT__entry(
		__field(unsigned int, curr_vfe_id)
		__field(unsigned int, curr_irq_status0)
		__field(unsigned int, curr_irq_status1)
		__field(unsigned int, curr_ping_pong_status)
		__field(unsigned int, othr_vfe_id)
		__field(unsigned int, othr_irq_status0)
		__field(unsigned int, othr_irq_status1)
		__field(unsigned int, othr_ping_pong_status)
		__field(long, othr_tv_sec)
		__field(long, othr_tv_usec)
		__field(unsigned int, core_id)
	),
	TP_fast_assign(
		__entry->curr_vfe_id =
			ping_pong_state.vfe_id;
		__entry->curr_irq_status0 =
			ping_pong_state.irq_status0[ping_pong_state.vfe_id];
		__entry->curr_irq_status1 =
			ping_pong_state.irq_status1[ping_pong_state.vfe_id];
		__entry->curr_ping_pong_status =
			ping_pong_state.ping_pong_status[
			ping_pong_state.vfe_id];
		__entry->othr_vfe_id =
			!ping_pong_state.vfe_id;
		__entry->othr_irq_status0 =
			ping_pong_state.irq_status0[!ping_pong_state.vfe_id];
		__entry->othr_irq_status1 =
			ping_pong_state.irq_status1[!ping_pong_state.vfe_id];
		__entry->othr_ping_pong_status =
			ping_pong_state.ping_pong_status[
			!ping_pong_state.vfe_id];
		__entry->othr_tv_sec =
			ping_pong_state.ts.buf_time.tv_sec;
		__entry->othr_tv_usec =
			ping_pong_state.ts.buf_time.tv_usec;
		__entry->core_id = ping_pong_state.core_id;
	),
	TP_printk("vfe_id %d, irq_st0 0x%x, irq_st1 0x%x, pi_po_st 0x%x\n"
		"other vfe_id %d, irq_st0 0x%x, irq_st1 0x%x\n"
		"pi_po_st 0x%x, time %ld:%ld core %d",
		__entry->curr_vfe_id,
		__entry->curr_irq_status0,
		__entry->curr_irq_status1,
		__entry->curr_ping_pong_status,
		__entry->othr_vfe_id,
		__entry->othr_irq_status0,
		__entry->othr_irq_status1,
		__entry->othr_ping_pong_status,
		__entry->othr_tv_sec,
		__entry->othr_tv_usec,
		__entry->core_id
	)
);

TRACE_EVENT(msm_cam_isp_status_dump,
	TP_PROTO(char *event, uint32_t vfe_id, uint32_t frame_id,
		uint32_t irq_status0, uint32_t irq_status1,
		uint32_t dual_irq_status),
	TP_ARGS(event, vfe_id, frame_id, irq_status0,
		irq_status1, dual_irq_status),
	TP_STRUCT__entry(
		__field(char *, event)
		__field(unsigned int, vfe_id)
		__field(unsigned int, frame_id)
		__field(unsigned int, irq_status0)
		__field(unsigned int, irq_status1)
		__field(unsigned int, dual_irq_status)
	),
	TP_fast_assign(
		__entry->event = event;
		__entry->vfe_id = vfe_id;
		__entry->frame_id = frame_id;
		__entry->irq_status0 = irq_status0;
		__entry->irq_status1 = irq_status1;
		__entry->dual_irq_status = dual_irq_status;
	),
	TP_printk("%s vfe %d, frame %d, irq_st0 %x, irq_st1 %x dual_irq %x",
		__entry->event,
		__entry->vfe_id,
		__entry->frame_id,
		__entry->irq_status0,
		__entry->irq_status1,
		__entry->dual_irq_status
	)
);

#endif /* _MSM_CAM_TRACE_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
