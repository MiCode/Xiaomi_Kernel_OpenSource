/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufs_mtk

#if !defined(_TRACE_EVENT_UFS_MEDIATEK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_UFS_MEDIATEK_H

#include <linux/tracepoint.h>

TRACE_EVENT(ufs_mtk_event,
	TP_PROTO(unsigned int type, unsigned int data),
	TP_ARGS(type, data),

	TP_STRUCT__entry(
		__field(unsigned int, type)
		__field(unsigned int, data)
	),

	TP_fast_assign(
		__entry->type = type;
		__entry->data = data;
	),

	TP_printk("ufs: event=%u data=%u",
		  __entry->type, __entry->data)
);

TRACE_EVENT(ufs_mtk_clk_scale,
	TP_PROTO(const char *name, bool scale_up, uint64_t clk_rate),
	TP_ARGS(name, scale_up, clk_rate),

	TP_STRUCT__entry(
		__field(const char*, name)
		__field(bool, scale_up)
		__field(uint64_t, clk_rate)
	),

	TP_fast_assign(
		__entry->name = name;
		__entry->scale_up = scale_up;
		__entry->clk_rate = clk_rate;
	),

	TP_printk("ufs: clk (%s) scaled %s @ %lld",
		  __entry->name,
		  __entry->scale_up ? "up" : "down",
		  __entry->clk_rate)
);


TRACE_EVENT(ufs_mtk_mcq_command,
	TP_PROTO(const char *dev_name, enum ufs_trace_str_t str_t,
		 unsigned int tag, u32 doorbell, int transfer_len, u32 intr,
		 u64 lba, u8 opcode, u8 group_id),

	TP_ARGS(dev_name, str_t, tag, doorbell, transfer_len,
				intr, lba, opcode, group_id),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__field(enum ufs_trace_str_t, str_t)
		__field(unsigned int, tag)
		__field(u32, doorbell)
		__field(int, transfer_len)
		__field(u32, intr)
		__field(u64, lba)
		__field(u8, opcode)
		__field(u8, group_id)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__entry->str_t = str_t;
		__entry->tag = tag;
		__entry->doorbell = doorbell;
		__entry->transfer_len = transfer_len;
		__entry->intr = intr;
		__entry->lba = lba;
		__entry->opcode = opcode;
		__entry->group_id = group_id;
	),

	TP_printk(
		"%s: %s: tag: %u, DB: 0x%x, size: %d, IS: %u, LBA: %llu, opcode: 0x%x (%s), group_id: 0x%x",
		show_ufs_cmd_trace_str(__entry->str_t), __get_str(dev_name),
		__entry->tag, __entry->doorbell, __entry->transfer_len,
		__entry->intr, __entry->lba, (u32)__entry->opcode,
		str_opcode(__entry->opcode), (u32)__entry->group_id
	)
);


#endif
#include <trace/define_trace.h>
