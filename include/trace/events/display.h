/*
 * include/trace/events/display.h
 *
 * Display event logging to ftrace.
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM display

#if !defined(_TRACE_DISPLAY_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DISPLAY_H

#include "../../../drivers/video/tegra/dc/dc_priv_defs.h"
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(display_basic_template,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc),
	TP_STRUCT__entry(
		__field(	bool,		enabled)
		__field(	u8,		dev_id)
		__field(	int,		bw_rate)
		__field(	int,		new_bw_rate)
		__field(	int,		underflows_a)
		__field(	int,		underflows_b)
		__field(	int,		underflows_c)
	),
	TP_fast_assign(
		__entry->enabled = dc->enabled;
		__entry->dev_id = dc->ndev->id;
		__entry->bw_rate = dc->bw_kbps;
		__entry->new_bw_rate = dc->new_bw_kbps;
		__entry->underflows_a = dc->stats.underflows_a;
		__entry->underflows_b = dc->stats.underflows_b;
		__entry->underflows_c = dc->stats.underflows_c;
	),
	TP_printk("dc%u enabled=%d bw_rate=%d new_bw_rate=%d underflows=%d/%d/%d",
		__entry->dev_id, __entry->enabled,
		__entry->bw_rate, __entry->new_bw_rate,
		__entry->underflows_a, __entry->underflows_b,
		__entry->underflows_c)
);

DEFINE_EVENT(display_basic_template, display_enable,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

DEFINE_EVENT(display_basic_template, display_disable,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

DEFINE_EVENT(display_basic_template, display_suspend,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

DEFINE_EVENT(display_basic_template, display_resume,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

DEFINE_EVENT(display_basic_template, display_reset,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

DEFINE_EVENT(display_basic_template, update_windows,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

DEFINE_EVENT(display_basic_template, sync_windows,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

DEFINE_EVENT(display_basic_template, clear_bandwidth,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

DEFINE_EVENT(display_basic_template, program_bandwidth,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

DEFINE_EVENT(display_basic_template, set_dynamic_emc,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

DEFINE_EVENT(display_basic_template, underflow,
	TP_PROTO(struct tegra_dc *dc),
	TP_ARGS(dc)
);

TRACE_EVENT(display_syncpt_flush,
	TP_PROTO(struct tegra_dc *dc, u32 id, u32 min, u32 max),
	TP_ARGS(dc, id, min, max),
	TP_STRUCT__entry(
		__field(	bool,		enabled)
		__field(	u8,		dev_id)
		__field(	u32,		syncpt_id)
		__field(	u32,		syncpt_min)
		__field(	u32,		syncpt_max)
	),
	TP_fast_assign(
		__entry->enabled = dc->enabled;
		__entry->dev_id = dc->ndev->id;
		__entry->syncpt_id = id;
		__entry->syncpt_min = min;
		__entry->syncpt_max = max;
	),
	TP_printk("dc%u enabled=%d syncpt: id=%x min=%x max=%x",
		__entry->dev_id, __entry->enabled,
		__entry->syncpt_id, __entry->syncpt_min, __entry->syncpt_max)
);

DECLARE_EVENT_CLASS(display_io_template,
	TP_PROTO(struct tegra_dc *dc, unsigned long val, const void *reg),
	TP_ARGS(dc, val, reg),
	TP_STRUCT__entry(
		__field(	bool,		enabled)
		__field(	u8,		dev_id)
		__field(	const void *,	reg)
		__field(	u32,		val)
	),
	TP_fast_assign(
		__entry->enabled = dc->enabled;
		__entry->dev_id = dc->ndev->id;
		__entry->reg = reg;
		__entry->val = val;
	),
	TP_printk("dc%u enabled=%d reg=%p val=0x%08x",
		__entry->dev_id, __entry->enabled,
		__entry->reg, __entry->val)
);

DEFINE_EVENT(display_io_template, display_writel,
	TP_PROTO(struct tegra_dc *dc, unsigned long val, const void *reg),
	TP_ARGS(dc, val, reg)
);

DEFINE_EVENT(display_io_template, display_readl,
	TP_PROTO(struct tegra_dc *dc, unsigned long val, const void *reg),
	TP_ARGS(dc, val, reg)
);

TRACE_EVENT(display_mode,
	TP_PROTO(struct tegra_dc *dc, struct tegra_dc_mode *mode),
	TP_ARGS(dc, mode),
	TP_STRUCT__entry(
		__field(	bool,		enabled)
		__field(	u8,		dev_id)
		__field(	unsigned long,	pclk)
		__field(	unsigned short,	h_active)
		__field(	unsigned short,	v_active)
		__field(	unsigned short,	h_front_porch)
		__field(	unsigned short,	v_front_porch)
		__field(	unsigned short,	h_back_porch)
		__field(	unsigned short,	v_back_porch)
		__field(	unsigned short,	h_ref_to_sync)
		__field(	unsigned short,	v_ref_to_sync)
		__field(	unsigned short,	h_sync_width)
		__field(	unsigned short,	v_sync_width)
		__field(	bool,		stereo_mode)
	),
	TP_fast_assign(
		__entry->enabled = dc->enabled;
		__entry->dev_id = dc->ndev->id;
		__entry->pclk = mode->pclk;
		__entry->stereo_mode = mode->stereo_mode;
		__entry->h_active = mode->h_active;
		__entry->v_active = mode->v_active;
		__entry->h_front_porch = mode->h_front_porch;
		__entry->v_front_porch = mode->v_front_porch;
		__entry->h_back_porch = mode->h_back_porch;
		__entry->v_back_porch = mode->v_back_porch;
		__entry->h_sync_width = mode->h_sync_width;
		__entry->v_sync_width = mode->v_sync_width;
		__entry->h_ref_to_sync = mode->h_ref_to_sync;
		__entry->v_ref_to_sync = mode->v_ref_to_sync;
	),
	TP_printk("dc%u enabled=%d "
                "ref_to_sync: H=%d V=%d "
                "sync_width: H=%d V=%d "
                "back_porch: H=%d V=%d "
                "active: H=%d V=%d "
                "front_porch: H=%d V=%d "
                "pclk=%ld stereo mode=%d\n",
		__entry->dev_id, __entry->enabled,
                __entry->h_ref_to_sync, __entry->v_ref_to_sync,
                __entry->h_sync_width, __entry->v_sync_width,
                __entry->h_back_porch, __entry->v_back_porch,
                __entry->h_active, __entry->v_active,
                __entry->h_front_porch, __entry->v_front_porch,
                __entry->pclk, __entry->stereo_mode
	)
);

TRACE_EVENT(window_update,
	TP_PROTO(struct tegra_dc *dc, struct tegra_dc_win *win),
	TP_ARGS(dc, win),
	TP_STRUCT__entry(
		__field(	bool,		enabled)
		__field(	u8,		dev_id)
		__field(	u32,		win_fmt)
		__field(	unsigned short,	win_x)
		__field(	unsigned short,	win_y)
		__field(	unsigned short,	win_w)
		__field(	unsigned short,	win_h)
		__field(	unsigned short,	win_out_x)
		__field(	unsigned short,	win_out_y)
		__field(	unsigned short,	win_out_w)
		__field(	unsigned short,	win_out_h)
	),
	TP_fast_assign(
		__entry->enabled = dc->enabled;
		__entry->dev_id = dc->ndev->id;
		__entry->win_fmt = win->fmt;
		__entry->win_x = dfixed_trunc(win->x);
		__entry->win_y = dfixed_trunc(win->y);
		__entry->win_w = dfixed_trunc(win->w);
		__entry->win_h = dfixed_trunc(win->h);
		__entry->win_out_x = win->out_x;
		__entry->win_out_y = win->out_y;
		__entry->win_out_w = win->out_w;
		__entry->win_out_h = win->out_h;
	),
	TP_printk("dc%u enabled=%d fmt=%#x in=[x:%u y:%u w:%u h:%u] "
		"out=[x:%u y:%u w:%u h:%u] ",
		__entry->dev_id, __entry->enabled, __entry->win_fmt,
		__entry->win_x, __entry->win_y,
		__entry->win_w, __entry->win_h,
		__entry->win_out_x, __entry->win_out_y,
		__entry->win_out_w, __entry->win_out_h
	)
);
#endif /* _TRACE_DISPLAY_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
