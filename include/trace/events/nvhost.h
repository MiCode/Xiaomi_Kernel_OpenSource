/*
 * Nvhost event logging to ftrace.
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.  All rights reserved.
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
#define TRACE_SYSTEM nvhost

#if !defined(_TRACE_NVHOST_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVHOST_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(nvhost,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(__field(const char *, name)),
	TP_fast_assign(__entry->name = name;),
	TP_printk("name=%s", __entry->name)
);

DEFINE_EVENT(nvhost, nvhost_channel_open,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(nvhost, nvhost_channel_release,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(nvhost, nvhost_ioctl_channel_flush,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

TRACE_EVENT(nvhost_channel_write_submit,
	TP_PROTO(const char *name, ssize_t count, u32 cmdbufs, u32 relocs,
			u32 syncpt_id, u32 syncpt_incrs),

	TP_ARGS(name, count, cmdbufs, relocs, syncpt_id, syncpt_incrs),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(ssize_t, count)
		__field(u32, cmdbufs)
		__field(u32, relocs)
		__field(u32, syncpt_id)
		__field(u32, syncpt_incrs)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->count = count;
		__entry->cmdbufs = cmdbufs;
		__entry->relocs = relocs;
		__entry->syncpt_id = syncpt_id;
		__entry->syncpt_incrs = syncpt_incrs;
	),

	TP_printk("name=%s, count=%d, cmdbufs=%u, relocs=%u, syncpt_id=%u, syncpt_incrs=%u",
	  __entry->name, __entry->count, __entry->cmdbufs, __entry->relocs,
	  __entry->syncpt_id, __entry->syncpt_incrs)
);

TRACE_EVENT(nvhost_channel_submit,
	TP_PROTO(const char *name, u32 cmdbufs, u32 relocs, u32 waitchks,
			u32 syncpt_id, u32 syncpt_incrs),

	TP_ARGS(name, cmdbufs, relocs, waitchks, syncpt_id, syncpt_incrs),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, cmdbufs)
		__field(u32, relocs)
		__field(u32, waitchks)
		__field(u32, syncpt_id)
		__field(u32, syncpt_incrs)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->cmdbufs = cmdbufs;
		__entry->relocs = relocs;
		__entry->waitchks = waitchks;
		__entry->syncpt_id = syncpt_id;
		__entry->syncpt_incrs = syncpt_incrs;
	),

	TP_printk("name=%s, cmdbufs=%u, relocs=%u, waitchks=%d,"
		"syncpt_id=%u, syncpt_incrs=%u",
	  __entry->name, __entry->cmdbufs, __entry->relocs, __entry->waitchks,
	  __entry->syncpt_id, __entry->syncpt_incrs)
);

TRACE_EVENT(nvhost_ioctl_channel_submit,
	TP_PROTO(const char *name, u32 version, u32 cmdbufs, u32 relocs,
		 u32 waitchks, u32 syncpt_id, u32 syncpt_incrs),

	TP_ARGS(name, version, cmdbufs, relocs, waitchks,
			syncpt_id, syncpt_incrs),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, version)
		__field(u32, cmdbufs)
		__field(u32, relocs)
		__field(u32, waitchks)
		__field(u32, syncpt_id)
		__field(u32, syncpt_incrs)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->version = version;
		__entry->cmdbufs = cmdbufs;
		__entry->relocs = relocs;
		__entry->waitchks = waitchks;
		__entry->syncpt_id = syncpt_id;
		__entry->syncpt_incrs = syncpt_incrs;
	),

	TP_printk("name=%s, version=%u, cmdbufs=%u, relocs=%u, waitchks=%u, syncpt_id=%u, syncpt_incrs=%u",
	  __entry->name, __entry->version, __entry->cmdbufs, __entry->relocs,
	  __entry->waitchks, __entry->syncpt_id, __entry->syncpt_incrs)
);

TRACE_EVENT(nvhost_channel_write_cmdbuf,
	TP_PROTO(const char *name, u32 mem_id,
			u32 words, u32 offset),

	TP_ARGS(name, mem_id, words, offset),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, mem_id)
		__field(u32, words)
		__field(u32, offset)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->mem_id = mem_id;
		__entry->words = words;
		__entry->offset = offset;
	),

	TP_printk("name=%s, mem_id=%08x, words=%u, offset=%d",
	  __entry->name, __entry->mem_id,
	  __entry->words, __entry->offset)
);

TRACE_EVENT(nvhost_cdma_end,
	TP_PROTO(const char *name, int prio,
		int hi_count, int med_count, int low_count),

	TP_ARGS(name, prio, hi_count, med_count, low_count),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(int, prio)
		__field(int, hi_count)
		__field(int, med_count)
		__field(int, low_count)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->prio = prio;
		__entry->hi_count = hi_count;
		__entry->med_count = med_count;
		__entry->low_count = low_count;
	),

	TP_printk("name=%s, prio=%d, hi=%d, med=%d, low=%d",
		__entry->name, __entry->prio,
		__entry->hi_count, __entry->med_count, __entry->low_count)
);

TRACE_EVENT(nvhost_cdma_flush,
	TP_PROTO(const char *name, int timeout),

	TP_ARGS(name, timeout),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(int, timeout)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->timeout = timeout;
	),

	TP_printk("name=%s, timeout=%d",
		__entry->name, __entry->timeout)
);

TRACE_EVENT(nvhost_cdma_push,
	TP_PROTO(const char *name, u32 op1, u32 op2),

	TP_ARGS(name, op1, op2),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, op1)
		__field(u32, op2)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->op1 = op1;
		__entry->op2 = op2;
	),

	TP_printk("name=%s, op1=%08x, op2=%08x",
		__entry->name, __entry->op1, __entry->op2)
);

TRACE_EVENT(nvhost_cdma_push_gather,
	TP_PROTO(const char *name, u32 mem_id,
			u32 words, u32 offset, void *cmdbuf),

	TP_ARGS(name, mem_id, words, offset, cmdbuf),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, mem_id)
		__field(u32, words)
		__field(u32, offset)
		__field(bool, cmdbuf)
		__dynamic_array(u32, cmdbuf, words)
	),

	TP_fast_assign(
		if (cmdbuf) {
			memcpy(__get_dynamic_array(cmdbuf), cmdbuf+offset,
					words * sizeof(u32));
		}
		__entry->cmdbuf = cmdbuf;
		__entry->name = name;
		__entry->mem_id = mem_id;
		__entry->words = words;
		__entry->offset = offset;
	),

	TP_printk("name=%s, mem_id=%08x, words=%u, offset=%d, contents=[%s]",
	  __entry->name, __entry->mem_id,
	  __entry->words, __entry->offset,
	  __print_hex(__get_dynamic_array(cmdbuf),
		  __entry->cmdbuf ? __entry->words * 4 : 0))
);

TRACE_EVENT(nvhost_channel_write_reloc,
	TP_PROTO(const char *name, u32 cmdbuf_mem, u32 cmdbuf_offset,
		u32 target, u32 target_offset),

	TP_ARGS(name, cmdbuf_mem, cmdbuf_offset, target, target_offset),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, cmdbuf_mem)
		__field(u32, cmdbuf_offset)
		__field(u32, target)
		__field(u32, target_offset)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->cmdbuf_mem = cmdbuf_mem;
		__entry->cmdbuf_offset = cmdbuf_offset;
		__entry->target = target;
		__entry->target_offset = target_offset;
	),

	TP_printk("name=%s, cmdbuf_mem=%08x, cmdbuf_offset=%04x, target=%08x, target_offset=%04x",
	  __entry->name, __entry->cmdbuf_mem, __entry->cmdbuf_offset,
	  __entry->target, __entry->target_offset)
);

TRACE_EVENT(nvhost_channel_write_waitchks,
	TP_PROTO(const char *name, u32 waitchks),

	TP_ARGS(name, waitchks),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, waitchks)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->waitchks = waitchks;
	),

	TP_printk("name=%s, waitchks=%u",
	  __entry->name, __entry->waitchks)
);

TRACE_EVENT(nvhost_channel_context_save,
	TP_PROTO(const char *name, void *ctx),

	TP_ARGS(name, ctx),

	TP_STRUCT__entry(
	    __field(const char *, name)
	    __field(void*, ctx)
	),

	TP_fast_assign(
	    __entry->name = name;
	    __entry->ctx = ctx;
	),

	TP_printk("name=%s, ctx=%p",
	  __entry->name, __entry->ctx)
);

TRACE_EVENT(nvhost_channel_context_restore,
	TP_PROTO(const char *name, void *ctx),

	TP_ARGS(name, ctx),

	TP_STRUCT__entry(
	    __field(const char *, name)
	    __field(void*, ctx)
	),

	TP_fast_assign(
	    __entry->name = name;
	    __entry->ctx = ctx;
	),

	TP_printk("name=%s, ctx=%p",
	  __entry->name, __entry->ctx)
);

TRACE_EVENT(nvhost_ioctl_channel_set_ctxswitch,
	TP_PROTO(const char *name, void *ctx,
		u32 save_mem, u32 save_offset, u32 save_words,
		u32 restore_mem, u32 restore_offset, u32 restore_words,
		u32 syncpt_id, u32 waitbase, u32 save_incrs, u32 restore_incrs),

	TP_ARGS(name, ctx,
		save_mem, save_offset, save_words,
		restore_mem, restore_offset, restore_words,
		syncpt_id, waitbase, save_incrs, restore_incrs),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(void *, ctx)
		__field(u32, save_mem)
		__field(u32, save_offset)
		__field(u32, save_words)
		__field(u32, restore_mem)
		__field(u32, restore_offset)
		__field(u32, restore_words)
		__field(u32, syncpt_id)
		__field(u32, waitbase)
		__field(u32, save_incrs)
		__field(u32, restore_incrs)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->ctx = ctx;
		__entry->save_mem = save_mem;
		__entry->save_offset = save_offset;
		__entry->save_words = save_words;
		__entry->restore_mem = restore_mem;
		__entry->restore_offset = restore_offset;
		__entry->restore_words = restore_words;
		__entry->syncpt_id = syncpt_id;
		__entry->waitbase = waitbase;
		__entry->save_incrs = save_incrs;
		__entry->restore_incrs = restore_incrs;
	),

	TP_printk("name=%s, ctx=%p, save_mem=%08x, save_offset=%d, save_words=%d, restore_mem=%08x, restore_offset=%d, restore_words=%d, syncpt_id=%d, waitbase=%d, save_incrs=%d, restore_incrs=%d",
	  __entry->name, __entry->ctx,
	  __entry->save_mem, __entry->save_offset, __entry->save_words,
	  __entry->restore_mem, __entry->restore_offset, __entry->restore_words,
	  __entry->syncpt_id, __entry->waitbase,
	  __entry->save_incrs, __entry->restore_incrs)
);
TRACE_EVENT(nvhost_ctrlopen,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
	    __field(const char *, name)
	),
	TP_fast_assign(
	    __entry->name = name
	),
	TP_printk("name=%s", __entry->name)
);

TRACE_EVENT(nvhost_ctrlrelease,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
	    __field(const char *, name)
	),
	TP_fast_assign(
	    __entry->name = name
	),
	TP_printk("name=%s", __entry->name)
);

TRACE_EVENT(nvhost_ioctl_ctrl_module_mutex,
	TP_PROTO(u32 lock, u32 id),

	TP_ARGS(lock, id),

	TP_STRUCT__entry(
	    __field(u32, lock);
	    __field(u32, id);
	),

	TP_fast_assign(
		__entry->lock = lock;
		__entry->id = id;
	),

	TP_printk("lock=%u, id=%d",
		__entry->lock, __entry->id)
	);

TRACE_EVENT(nvhost_ioctl_ctrl_syncpt_incr,
	TP_PROTO(u32 id),

	TP_ARGS(id),

	TP_STRUCT__entry(
	    __field(u32, id);
	),

	TP_fast_assign(
	   __entry->id = id;
	),

	TP_printk("id=%d", __entry->id)
);

TRACE_EVENT(nvhost_ioctl_ctrl_syncpt_read,
	TP_PROTO(u32 id, u32 value),

	TP_ARGS(id, value),

	TP_STRUCT__entry(
	    __field(u32, id);
		__field(u32, value);
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->value = value;
	),

	TP_printk("id=%d, value=%d", __entry->id, __entry->value)
);

TRACE_EVENT(nvhost_ioctl_ctrl_syncpt_wait,
	TP_PROTO(u32 id, u32 threshold, s32 timeout, u32 value, int err),

	TP_ARGS(id, threshold, timeout, value, err),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, threshold)
		__field(s32, timeout)
		__field(u32, value)
		__field(int, err)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->threshold = threshold;
		__entry->timeout = timeout;
		__entry->value = value;
		__entry->err = err;
	),

	TP_printk("id=%u, threshold=%u, timeout=%d, value=%u, err=%d",
	  __entry->id, __entry->threshold, __entry->timeout,
	  __entry->value, __entry->err)
);

TRACE_EVENT(nvhost_ioctl_channel_module_regrdwr,
	TP_PROTO(u32 id, u32 num_offsets, bool write),

	TP_ARGS(id, num_offsets, write),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, num_offsets)
		__field(bool, write)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->num_offsets = num_offsets;
		__entry->write = write;
	),

	TP_printk("id=%u, num_offsets=%u, write=%d",
	  __entry->id, __entry->num_offsets, __entry->write)
);

TRACE_EVENT(nvhost_ioctl_ctrl_module_regrdwr,
	TP_PROTO(u32 id, u32 num_offsets, bool write),

	TP_ARGS(id, num_offsets, write),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, num_offsets)
		__field(bool, write)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->num_offsets = num_offsets;
		__entry->write = write;
	),

	TP_printk("id=%u, num_offsets=%u, write=%d",
	  __entry->id, __entry->num_offsets, __entry->write)
);

TRACE_EVENT(nvhost_channel_submitted,
	TP_PROTO(const char *name, u32 syncpt_base, u32 syncpt_max),

	TP_ARGS(name, syncpt_base, syncpt_max),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, syncpt_base)
		__field(u32, syncpt_max)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->syncpt_base = syncpt_base;
		__entry->syncpt_max = syncpt_max;
	),

	TP_printk("name=%s, syncpt_base=%d, syncpt_max=%d",
		__entry->name, __entry->syncpt_base, __entry->syncpt_max)
);

TRACE_EVENT(nvhost_channel_submit_complete,
	TP_PROTO(const char *name, int count, u32 thresh,
		int hi_count, int med_count, int low_count),

	TP_ARGS(name, count, thresh, hi_count, med_count, low_count),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(int, count)
		__field(u32, thresh)
		__field(int, hi_count)
		__field(int, med_count)
		__field(int, low_count)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->count = count;
		__entry->thresh = thresh;
		__entry->hi_count = hi_count;
		__entry->med_count = med_count;
		__entry->low_count = low_count;
	),

	TP_printk("name=%s, count=%d, thresh=%d, hi=%d, med=%d, low=%d",
		__entry->name, __entry->count, __entry->thresh,
		__entry->hi_count, __entry->med_count, __entry->low_count)
);

TRACE_EVENT(nvhost_wait_cdma,
	TP_PROTO(const char *name, u32 eventid),

	TP_ARGS(name, eventid),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, eventid)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->eventid = eventid;
	),

	TP_printk("name=%s, event=%d", __entry->name, __entry->eventid)
);

TRACE_EVENT(nvhost_syncpt_update_min,
	TP_PROTO(u32 id, u32 val),

	TP_ARGS(id, val),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, val)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->val = val;
	),

	TP_printk("id=%d, val=%d", __entry->id, __entry->val)
);

TRACE_EVENT(nvhost_syncpt_wait_check,
	TP_PROTO(u32 mem_id, u32 offset, u32 syncpt_id, u32 thresh, u32 min),

	TP_ARGS(mem_id, offset, syncpt_id, thresh, min),

	TP_STRUCT__entry(
		__field(u32, mem_id)
		__field(u32, offset)
		__field(u32, syncpt_id)
		__field(u32, thresh)
		__field(u32, min)
	),

	TP_fast_assign(
		__entry->mem_id = mem_id;
		__entry->offset = offset;
		__entry->syncpt_id = syncpt_id;
		__entry->thresh = thresh;
		__entry->min = min;
	),

	TP_printk("mem_id=%08x, offset=%05x, id=%d, thresh=%d, current=%d",
		__entry->mem_id, __entry->offset,
		__entry->syncpt_id, __entry->thresh,
		__entry->min)
);

TRACE_EVENT(nvhost_module_set_devfreq_rate,
	TP_PROTO(const char *devname, const char *clockname,
		unsigned long rate),

	TP_ARGS(devname, clockname, rate),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(const char *, clockname)
		__field(unsigned long, rate)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->clockname = clockname;
		__entry->rate = rate;
	),

	TP_printk("dev=%s, clock=%s, rate=%ld",
		__entry->devname, __entry->clockname,
		__entry->rate)
);

TRACE_EVENT(nvhost_module_update_rate,
	TP_PROTO(const char *devname, const char *clockname,
		unsigned long rate),

	TP_ARGS(devname, clockname, rate),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(const char *, clockname)
		__field(unsigned long, rate)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->clockname = clockname;
		__entry->rate = rate;
	),

	TP_printk("dev=%s, clock=%s, rate=%ld",
		__entry->devname, __entry->clockname,
		__entry->rate)
);

TRACE_EVENT(nvhost_channel_submit_gpfifo,
		TP_PROTO(const char *name, u32 hw_chid, u32 num_entries,
		u32 flags, u32 wait_id, u32 wait_value,
		u32 incr_id),

		TP_ARGS(name, hw_chid, num_entries, flags, wait_id, wait_value,
			incr_id),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, hw_chid)
		__field(u32, num_entries)
		__field(u32, flags)
		__field(u32, wait_id)
		__field(u32, wait_value)
		__field(u32, incr_id)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->hw_chid = hw_chid;
		__entry->num_entries = num_entries;
		__entry->flags = flags;
		__entry->wait_id = wait_id;
		__entry->wait_value = wait_value;
		__entry->incr_id = incr_id;
	),

	TP_printk("name=%s, hw_chid=%d, num_entries=%u, flags=%u, wait_id=%d,"
		" wait_value=%u, incr_id=%u",
		__entry->name, __entry->hw_chid, __entry->num_entries,
		__entry->flags, __entry->wait_id, __entry->wait_value,
		__entry->incr_id)
);

TRACE_EVENT(nvhost_channel_submitted_gpfifo,
		TP_PROTO(const char *name, u32 hw_chid, u32 num_entries,
		u32 flags, u32 wait_id, u32 wait_value,
		u32 incr_id, u32 incr_value),

		TP_ARGS(name, hw_chid, num_entries, flags, wait_id, wait_value,
			incr_id, incr_value),

	TP_STRUCT__entry(
		__field(const char *, name)
		__field(u32, hw_chid)
		__field(u32, num_entries)
		__field(u32, flags)
		__field(u32, wait_id)
		__field(u32, wait_value)
		__field(u32, incr_id)
		__field(u32, incr_value)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->hw_chid = hw_chid;
		__entry->num_entries = num_entries;
		__entry->flags = flags;
		__entry->wait_id = wait_id;
		__entry->wait_value = wait_value;
		__entry->incr_id = incr_id;
		__entry->incr_value = incr_value;
	),

	TP_printk("name=%s, hw_chid=%d, num_entries=%u, flags=%u, wait_id=%d,"
		" wait_value=%u, incr_id=%u, incr_value=%u",
		__entry->name, __entry->hw_chid, __entry->num_entries,
		__entry->flags, __entry->wait_id, __entry->wait_value,
		__entry->incr_id, __entry->incr_value)
);


TRACE_EVENT(nvhost_as_dev_open,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(nvhost_as_dev_release,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);


TRACE_EVENT(nvhost_as_ioctl_bind_channel,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);


TRACE_EVENT(nvhost_as_ioctl_alloc_space,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(nvhost_as_ioctl_free_space,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(nvhost_as_ioctl_map_buffer,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(nvhost_as_ioctl_unmap_buffer,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
			 __field(const char *, name)
			 ),
	TP_fast_assign(
		       __entry->name = name;
		       ),
	TP_printk("name=%s ",  __entry->name)
);

TRACE_EVENT(nvhost_gk20a_mmu_fault,
	    TP_PROTO(u32 fault_hi, u32 fault_lo,
		     u32 fault_info,
		     u32 instance,
		     u32 engine_id,
		     const char *engine,
		     const char *client,
		     const char *fault_type),
	    TP_ARGS(fault_hi, fault_lo, fault_info,
		    instance, engine_id, engine, client, fault_type),
	    TP_STRUCT__entry(
			 __field(u32, fault_hi)
			 __field(u32, fault_lo)
			 __field(u32, fault_info)
			 __field(u32, instance)
			 __field(u32, engine_id)
			 __field(const char *, engine)
			 __field(const char *, client)
			 __field(const char *, fault_type)
			 ),
	    TP_fast_assign(
		       __entry->fault_hi = fault_hi;
		       __entry->fault_lo = fault_lo;
		       __entry->fault_info = fault_info;
		       __entry->instance = instance;
		       __entry->engine_id = engine_id;
		       __entry->engine = engine;
		       __entry->client = client;
		       __entry->fault_type = fault_type;
		       ),
	    TP_printk("fault=0x%x,%08x info=0x%x instance=0x%x engine_id=%d engine=%s client=%s type=%s",
		      __entry->fault_hi, __entry->fault_lo,
		      __entry->fault_info, __entry->instance, __entry->engine_id,
		      __entry->engine, __entry->client, __entry->fault_type)
);

DECLARE_EVENT_CLASS(nvhost_map,
	TP_PROTO(const char *devname, void *handle, int size,
		dma_addr_t iova),

	TP_ARGS(devname, handle, size, iova),

	TP_STRUCT__entry(
		__field(const char *, devname)
		__field(void *, handle)
		__field(int, size)
		__field(dma_addr_t, iova)
	),

	TP_fast_assign(
		__entry->devname = devname;
		__entry->handle = handle;
		__entry->size = size;
		__entry->iova = iova;
	),

	TP_printk("dev=%s, handle=%p, size=%d, iova=%08llx",
		__entry->devname, __entry->handle, __entry->size,
		(u64)__entry->iova)
);

DEFINE_EVENT(nvhost_map, nvhost_nvmap_pin,
	TP_PROTO(const char *devname, void *handle, int size,
		dma_addr_t iova),

	TP_ARGS(devname, handle, size, iova)
);

DEFINE_EVENT(nvhost_map, nvhost_nvmap_unpin,
	TP_PROTO(const char *devname, void *handle, int size,
		dma_addr_t iova),

	TP_ARGS(devname, handle, size, iova)
);

#endif /*  _TRACE_NVHOST_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
