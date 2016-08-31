/*
 * include/trace/events/isomgr.h
 *
 * isomgr logging to ftrace.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
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
#define TRACE_SYSTEM isomgr

#if !defined(_TRACE_ISOMGR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ISOMGR_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>
#include <mach/isomgr.h>

TRACE_EVENT(tegra_isomgr_register,
	TP_PROTO(enum tegra_iso_client client,
		 u32 dedi_bw,
		 tegra_isomgr_renegotiate renegotiate,
		 void *priv,
		 char *name,
		 char *msg),

	TP_ARGS(client, dedi_bw, renegotiate, priv, name, msg),

	TP_STRUCT__entry(
		__field(enum tegra_iso_client, client)
		__field(u32, dedi_bw)
		__field(tegra_isomgr_renegotiate, renegotiate)
		__field(void *, priv)
		__field(char *, name)
		__field(char *, msg)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->dedi_bw = dedi_bw;
		__entry->renegotiate = renegotiate;
		__entry->priv = priv;
		__entry->name = name;
		__entry->msg = msg;
	),

	TP_printk("client=%d, dedi_bw=%dKB, renegotiate=%p, priv=%p, %s %s",
		__entry->client, __entry->dedi_bw,
		__entry->renegotiate, __entry->priv,
		__entry->name, __entry->msg)
);

TRACE_EVENT(tegra_isomgr_unregister,
	TP_PROTO(tegra_isomgr_handle handle,
		 char *name),

	TP_ARGS(handle, name),

	TP_STRUCT__entry(
		__field(tegra_isomgr_handle, handle)
		__field(char *, name)
	),

	TP_fast_assign(
		__entry->handle = handle;
		__entry->name = name;
	),

	TP_printk("handle=%p %s", __entry->handle, __entry->name)
);

TRACE_EVENT(tegra_isomgr_unregister_iso_client,
	TP_PROTO(char *name,
		 char *msg),

	TP_ARGS(name, msg),

	TP_STRUCT__entry(
		__field(char *, name)
		__field(char *, msg)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->msg = msg;
	),

	TP_printk("%s %s", __entry->name, __entry->msg)
);

TRACE_EVENT(tegra_isomgr_reserve,
	TP_PROTO(tegra_isomgr_handle handle,
		 u32 bw,
		 u32 lt,
		 char *name,
		 char *msg),

	TP_ARGS(handle, bw, lt, name, msg),

	TP_STRUCT__entry(
		__field(tegra_isomgr_handle, handle)
		__field(u32, bw)
		__field(u32, lt)
		__field(char *, name)
		__field(char *, msg)
	),

	TP_fast_assign(
		__entry->handle = handle;
		__entry->bw = bw;
		__entry->lt = lt;
		__entry->name = name;
		__entry->msg = msg;
	),

	TP_printk("handle=%p, bw=%dKB, lt=%dus %s %s",
		__entry->handle, __entry->bw, __entry->lt,
		__entry->name, __entry->msg)
);

TRACE_EVENT(tegra_isomgr_realize,
	TP_PROTO(tegra_isomgr_handle handle, char *name, char *msg),

	TP_ARGS(handle, name, msg),

	TP_STRUCT__entry(
		__field(tegra_isomgr_handle, handle)
		__field(char *, name)
		__field(char *, msg)
	),

	TP_fast_assign(
		__entry->handle = handle;
		__entry->name = name;
		__entry->msg = msg;
	),

	TP_printk("handle=%p %s %s", __entry->handle,
		__entry->name, __entry->msg)
);

TRACE_EVENT(tegra_isomgr_set_margin,
	TP_PROTO(enum tegra_iso_client client,
		 u32 bw,
		 bool wait,
		 char *msg),

	TP_ARGS(client, bw, wait, msg),

	TP_STRUCT__entry(
		__field(enum tegra_iso_client, client)
		__field(u32, bw)
		__field(bool, wait)
		__field(char *, msg)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->bw = bw;
		__entry->wait = wait;
		__entry->msg = msg;
	),

	TP_printk("client=%d, bw=%dKB, wait=%d %s",
		__entry->client, __entry->bw, __entry->wait, __entry->msg)
);

TRACE_EVENT(tegra_isomgr_get_imp_time,
	TP_PROTO(enum tegra_iso_client client,
		 u32 bw,
		 u32 time,
		 char *name),

	TP_ARGS(client, bw, time, name),

	TP_STRUCT__entry(
		__field(enum tegra_iso_client, client)
		__field(u32, bw)
		__field(u32, time)
		__field(char *, name)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->bw = bw;
		__entry->time = time;
		__entry->name = name;
	),

	TP_printk("client=%d, bw=%dKB, imp_time=%dms %s",
		__entry->client, __entry->bw, __entry->time, __entry->name)
);

TRACE_EVENT(tegra_isomgr_get_available_iso_bw,
	TP_PROTO(u32 bw),

	TP_ARGS(bw),

	TP_STRUCT__entry(
		__field(u32, bw)
	),

	TP_fast_assign(
		__entry->bw = bw;
	),

	TP_printk("bw=%dKB", __entry->bw)
);

TRACE_EVENT(tegra_isomgr_get_total_iso_bw,
	TP_PROTO(u32 bw),

	TP_ARGS(bw),

	TP_STRUCT__entry(
		__field(u32, bw)
	),

	TP_fast_assign(
		__entry->bw = bw;
	),

	TP_printk("bw=%dKB", __entry->bw)
);

TRACE_EVENT(tegra_isomgr_scavenge,
	TP_PROTO(enum tegra_iso_client client,
		 u32 avail_bw,
		 char *name,
		 char *msg),

	TP_ARGS(client, avail_bw, name, msg),

	TP_STRUCT__entry(
		__field(enum tegra_iso_client, client)
		__field(u32, avail_bw)
		__field(char *, name)
		__field(char *, msg)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->avail_bw = avail_bw;
		__entry->name = name;
		__entry->msg = msg;
	),

	TP_printk("client=%d, avail_bw=%dKB, %s %s",
		__entry->client, __entry->avail_bw,
		__entry->name, __entry->msg)
);

TRACE_EVENT(tegra_isomgr_scatter,
	TP_PROTO(enum tegra_iso_client client,
		 u32 avail_bw,
		 char *name,
		 char *msg),

	TP_ARGS(client, avail_bw, name, msg),

	TP_STRUCT__entry(
		__field(enum tegra_iso_client, client)
		__field(u32, avail_bw)
		__field(char *, name)
		__field(char *, msg)
	),

	TP_fast_assign(
		__entry->client = client;
		__entry->avail_bw = avail_bw;
		__entry->name = name;
		__entry->msg = msg;
	),

	TP_printk("client=%d, avail_bw=%dKB, %s %s",
		__entry->client, __entry->avail_bw,
		__entry->name, __entry->msg)
);

#endif /* _TRACE_ISOMGR_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

