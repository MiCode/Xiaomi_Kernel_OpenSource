/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#if !defined(TRACE_MDSS_MDP_H) || defined(TRACE_HEADER_MULTI_READ)
#define TRACE_MDSS_MDP_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mdss
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mdss_mdp_trace

#include <linux/tracepoint.h>
#include "mdss_mdp.h"

DECLARE_EVENT_CLASS(mdp_sspp_template,
	TP_PROTO(struct mdss_mdp_pipe *pipe),
	TP_ARGS(pipe),
	TP_STRUCT__entry(
			__field(u32, num)
			__field(u32, play_cnt)
			__field(u32, mixer)
			__field(u32, stage)
			__field(u32, flags)
			__field(u32, format)
			__field(u16, img_w)
			__field(u16, img_h)
			__field(u16, src_x)
			__field(u16, src_y)
			__field(u16, src_w)
			__field(u16, src_h)
			__field(u16, dst_x)
			__field(u16, dst_y)
			__field(u16, dst_w)
			__field(u16, dst_h)
	),
	TP_fast_assign(
			__entry->num = pipe->num;
			__entry->play_cnt = pipe->play_cnt;
			__entry->mixer = pipe->mixer->num;
			__entry->stage = pipe->mixer_stage;
			__entry->flags = pipe->flags;
			__entry->format = pipe->src_fmt ?
					pipe->src_fmt->format : -1;
			__entry->img_w = pipe->img_width;
			__entry->img_h = pipe->img_height;
			__entry->src_x = pipe->src.x;
			__entry->src_y = pipe->src.y;
			__entry->src_w = pipe->src.w;
			__entry->src_h = pipe->src.h;
			__entry->dst_x = pipe->dst.x;
			__entry->dst_y = pipe->dst.y;
			__entry->dst_w = pipe->dst.w;
			__entry->dst_h = pipe->dst.h;
	),

	TP_printk("num=%d mixer=%d play_cnt=%d flags=0x%x stage=%d format=%d img=%dx%d src=[%d,%d,%d,%d] dst=[%d,%d,%d,%d]",
			__entry->num, __entry->mixer, __entry->play_cnt,
			__entry->flags, __entry->stage,
			__entry->format, __entry->img_w, __entry->img_h,
			__entry->src_x, __entry->src_y,
			__entry->src_w, __entry->src_h,
			__entry->dst_x, __entry->dst_y,
			__entry->dst_w, __entry->dst_h)
);

DEFINE_EVENT(mdp_sspp_template, mdp_sspp_set,
	TP_PROTO(struct mdss_mdp_pipe *pipe),
	TP_ARGS(pipe)
);

DEFINE_EVENT(mdp_sspp_template, mdp_sspp_change,
	TP_PROTO(struct mdss_mdp_pipe *pipe),
	TP_ARGS(pipe)
);

TRACE_EVENT(mdp_mixer_update,
	TP_PROTO(u32 mixer_num),
	TP_ARGS(mixer_num),
	TP_STRUCT__entry(
			__field(u32, mixer_num)
	),
	TP_fast_assign(
			__entry->mixer_num = mixer_num;
	),
	TP_printk("mixer=%d",
			__entry->mixer_num)
);

TRACE_EVENT(mdp_commit,
	TP_PROTO(struct mdss_mdp_ctl *ctl),
	TP_ARGS(ctl),
	TP_STRUCT__entry(
			__field(u32, num)
			__field(u32, play_cnt)
			__field(u32, clk_rate)
			__field(u64, bandwidth)
	),
	TP_fast_assign(
			__entry->num = ctl->num;
			__entry->play_cnt = ctl->play_cnt;
			__entry->clk_rate = ctl->new_perf.mdp_clk_rate;
			__entry->bandwidth = ctl->new_perf.bw_ctl;
	),
	TP_printk("num=%d play_cnt=%d bandwidth=%llu clk_rate=%u",
			__entry->num,
			__entry->play_cnt,
			__entry->bandwidth,
			__entry->clk_rate)
);

TRACE_EVENT(mdp_video_underrun_done,
	TP_PROTO(u32 ctl_num, u32 underrun_cnt),
	TP_ARGS(ctl_num, underrun_cnt),
	TP_STRUCT__entry(
			__field(u32, ctl_num)
			__field(u32, underrun_cnt)
	),
	TP_fast_assign(
			__entry->ctl_num = ctl_num;
			__entry->underrun_cnt = underrun_cnt;
	),
	TP_printk("ctl=%d count=%d",
			__entry->ctl_num, __entry->underrun_cnt)
);

#endif /* if !defined(TRACE_MDSS_MDP_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
