/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

/*
 * ISP_Pass1_CAM:
 * Define 2 ftrace event:
 *        1. enter event
 *        2. leave event
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM met_events_camsys
#if !defined(_TRACE_CAMSYS_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CAMSYS_EVENTS_H
#include <linux/tracepoint.h>

TRACE_EVENT(ISP__Pass1_CAM_enter,
	TP_PROTO(unsigned int hw_module, int imgo_en, int rrzo_en,
			 int imgo_bpp, int rrzo_bpp,
			 int imgo_w_in_byte, int imgo_h_in_byte,
			 int rrzo_w_in_byte, int rrzo_h_in_byte,
			 int rrz_src_w, int rrz_src_h, int rrz_dst_w,
			 int rrz_dst_h, int rrz_hori_step, int rrz_vert_step,
			 u32 ctl_en, u32 ctl_dma_en, u32 ctl_en2),
	TP_ARGS(hw_module, imgo_en, rrzo_en, imgo_bpp, rrzo_bpp, imgo_w_in_byte,
		    imgo_h_in_byte, rrzo_w_in_byte, rrzo_h_in_byte, rrz_src_w,
		    rrz_src_h, rrz_dst_w, rrz_dst_h, rrz_hori_step,
			rrz_vert_step, ctl_en, ctl_dma_en, ctl_en2),
	TP_STRUCT__entry(
		__field(unsigned int, hw_module)
		__field(int, imgo_en)
		__field(int, rrzo_en)
		__field(int, imgo_bpp)
		__field(int, rrzo_bpp)
		__field(int, imgo_w_in_byte)
		__field(int, imgo_h_in_byte)
		__field(int, rrzo_w_in_byte)
		__field(int, rrzo_h_in_byte)
		__field(int, rrz_src_w)
		__field(int, rrz_src_h)
		__field(int, rrz_dst_w)
		__field(int, rrz_dst_h)
		__field(int, rrz_hori_step)
		__field(int, rrz_vert_step)
		__field(u32, ctl_en)
		__field(u32, ctl_dma_en)
		__field(u32, ctl_en2)
	),
	TP_fast_assign(
		__entry->hw_module = hw_module;
		__entry->imgo_en = imgo_en;
		__entry->rrzo_en = rrzo_en;
		__entry->imgo_bpp = imgo_bpp;
		__entry->rrzo_bpp = rrzo_bpp;
		__entry->imgo_w_in_byte = imgo_w_in_byte;
		__entry->imgo_h_in_byte = imgo_h_in_byte;
		__entry->rrzo_w_in_byte = rrzo_w_in_byte;
		__entry->rrzo_h_in_byte = rrzo_h_in_byte;
		__entry->rrz_src_w = rrz_src_w;
		__entry->rrz_src_h = rrz_src_h;
		__entry->rrz_dst_w = rrz_dst_w;
		__entry->rrz_dst_h = rrz_dst_h;
		__entry->rrz_hori_step = rrz_hori_step;
		__entry->rrz_vert_step = rrz_vert_step;
		__entry->ctl_en = ctl_en;
		__entry->ctl_dma_en = ctl_dma_en;
		__entry->ctl_en2 = ctl_en2;
	),
	TP_printk(
		"_id=%d, imgo_en=%d, rrzo_en=%d\n",
		__entry->hw_module,
		__entry->imgo_en,
		__entry->rrzo_en)
);

/* TP_printk(
 *	"_id=%d, imgo_en=%d, rrzo_en=%d, imgo_bpp=%d,"
 *	"rrzo_bpp=%d,imgo_xsize=%d,"
 *	"imgo_ysize=%d, rrzo_xsize=%d, rrzo_ysize=%d,"
 *	"rrz_src_w=%d, rrz_src_h=%d,"
 *	"rrz_dst_w=%d, rrz_dst_h=%d, rrz_hori_step=%d,"
 *	"rrz_vert_step=%d, CAM_A__CTL_EN=%d,"
 *	"CAM_A__CTL_DMA_EN=%d, CAM_A__CTL_EN2=%d\n",
 *	__entry->hw_module,
 *	__entry->imgo_en,
 *	__entry->rrzo_en,
 *	__entry->imgo_bpp,
 *	__entry->rrzo_bpp,
 *	__entry->imgo_w_in_byte,
 *	__entry->imgo_h_in_byte,
 *	__entry->rrzo_w_in_byte,
 *	__entry->rrzo_h_in_byte,
 *	__entry->rrz_src_w,
 *	__entry->rrz_src_h,
 *	__entry->rrz_dst_w,
 *	__entry->rrz_dst_h,
 *	__entry->rrz_hori_step,
 *	__entry->rrz_vert_step,
 *	__entry->ctl_en,
 *	__entry->ctl_dma_en,
 *	__entry->ctl_en2)
 * #endif
 */

TRACE_EVENT(ISP__Pass1_CAM_leave,
	TP_PROTO(unsigned int hw_module, int dummy),
	TP_ARGS(hw_module, dummy),
	TP_STRUCT__entry(
	__field(unsigned int, hw_module)
	__field(int, dummy)
	),
	TP_fast_assign(
		__entry->hw_module = hw_module;
		__entry->dummy  = dummy;
	),
	TP_printk("_id=%d", __entry->hw_module)
);


#endif /* _TRACE_CAMSYS_EVENTS_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ./inc
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE met_events_camsys
#include <trace/define_trace.h>

