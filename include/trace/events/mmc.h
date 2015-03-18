/*
 * Copyright (C) 2013 Google, Inc.
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmc

#if !defined(_TRACE_MMC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMC_H

#include <linux/tracepoint.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/core.h>

/*
 * Unconditional logging of mmc block erase operations,
 * including cmd, address, size
 */
DECLARE_EVENT_CLASS(mmc_blk_erase_class,
	TP_PROTO(unsigned int cmd, unsigned int addr, unsigned int size),
	TP_ARGS(cmd, addr, size),
	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, addr)
		__field(unsigned int, size)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->addr = addr;
		__entry->size = size;
	),
	TP_printk("cmd=%u,addr=0x%08x,size=0x%08x",
		  __entry->cmd, __entry->addr, __entry->size)
);

DEFINE_EVENT(mmc_blk_erase_class, mmc_blk_erase_start,
	TP_PROTO(unsigned int cmd, unsigned int addr, unsigned int size),
	TP_ARGS(cmd, addr, size));

DEFINE_EVENT(mmc_blk_erase_class, mmc_blk_erase_end,
	TP_PROTO(unsigned int cmd, unsigned int addr, unsigned int size),
	TP_ARGS(cmd, addr, size));

/*
 * Logging of start of read or write mmc block operation,
 * including cmd, address, size
 */
DECLARE_EVENT_CLASS(mmc_blk_rw_class,
	TP_PROTO(unsigned int cmd, unsigned int addr, struct mmc_data *data),
	TP_ARGS(cmd, addr, data),
	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, addr)
		__field(unsigned int, size)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->addr = addr;
		__entry->size = data->blocks;
	),
	TP_printk("cmd=%u,addr=0x%08x,size=0x%08x",
		  __entry->cmd, __entry->addr, __entry->size)
);

DEFINE_EVENT_CONDITION(mmc_blk_rw_class, mmc_blk_rw_start,
	TP_PROTO(unsigned int cmd, unsigned int addr, struct mmc_data *data),
	TP_ARGS(cmd, addr, data),
	TP_CONDITION(((cmd == MMC_READ_MULTIPLE_BLOCK) ||
		      (cmd == MMC_WRITE_MULTIPLE_BLOCK)) &&
		      data));

DEFINE_EVENT_CONDITION(mmc_blk_rw_class, mmc_blk_rw_end,
	TP_PROTO(unsigned int cmd, unsigned int addr, struct mmc_data *data),
	TP_ARGS(cmd, addr, data),
	TP_CONDITION(((cmd == MMC_READ_MULTIPLE_BLOCK) ||
		      (cmd == MMC_WRITE_MULTIPLE_BLOCK)) &&
		      data));

TRACE_EVENT(mmc_cmd_rw_start,
	TP_PROTO(unsigned int cmd, unsigned int arg, unsigned int flags),
	TP_ARGS(cmd, arg, flags),
	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, arg)
		__field(unsigned int, flags)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->arg = arg;
		__entry->flags = flags;
	),
	TP_printk("cmd=%u,arg=0x%08x,flags=0x%08x",
		  __entry->cmd, __entry->arg, __entry->flags)
);

TRACE_EVENT(mmc_cmd_rw_end,
	TP_PROTO(unsigned int cmd, unsigned int status, unsigned int resp),
	TP_ARGS(cmd, status, resp),
	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, status)
		__field(unsigned int, resp)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->status = status;
		__entry->resp = resp;
	),
	TP_printk("cmd=%u,int_status=0x%08x,response=0x%08x",
		  __entry->cmd, __entry->status, __entry->resp)
);

TRACE_EVENT(mmc_data_rw_end,
	TP_PROTO(unsigned int cmd, unsigned int status),
	TP_ARGS(cmd, status),
	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, status)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->status = status;
	),
	TP_printk("cmd=%u,int_status=0x%08x",
		  __entry->cmd, __entry->status)
);

DECLARE_EVENT_CLASS(mmc_adma_class,
	TP_PROTO(unsigned int cmd, unsigned int len),
	TP_ARGS(cmd, len),
	TP_STRUCT__entry(
		__field(unsigned int, cmd)
		__field(unsigned int, len)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->len = len;
	),
	TP_printk("cmd=%u,sg_len=0x%08x", __entry->cmd, __entry->len)
);

DEFINE_EVENT(mmc_adma_class, mmc_adma_table_pre,
	TP_PROTO(unsigned int cmd, unsigned int len),
	TP_ARGS(cmd, len));

DEFINE_EVENT(mmc_adma_class, mmc_adma_table_post,
	TP_PROTO(unsigned int cmd, unsigned int len),
	TP_ARGS(cmd, len));

TRACE_EVENT(mmc_clk,
	TP_PROTO(char *print_info),

	TP_ARGS(print_info),

	TP_STRUCT__entry(
		__string(print_info, print_info)
	),

	TP_fast_assign(
		__assign_str(print_info, print_info);
	),

	TP_printk("%s",
		__get_str(print_info)
	)
);

DECLARE_EVENT_CLASS(mmc_pm_template,
	TP_PROTO(const char *dev_name, int err, s64 usecs),

	TP_ARGS(dev_name, err, usecs),

	TP_STRUCT__entry(
		__field(s64, usecs)
		__field(int, err)
		__string(dev_name, dev_name)
	),

	TP_fast_assign(
		__entry->usecs = usecs;
		__entry->err = err;
		__assign_str(dev_name, dev_name);
	),

	TP_printk(
		"took %lld usecs, %s err %d",
		__entry->usecs,
		__get_str(dev_name),
		__entry->err
	)
);

DEFINE_EVENT(mmc_pm_template, mmc_suspend_host,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DEFINE_EVENT(mmc_pm_template, mmc_resume_host,
	     TP_PROTO(const char *dev_name, int err, s64 usecs),
	     TP_ARGS(dev_name, err, usecs));

DECLARE_EVENT_CLASS(mmc_pm_qos_template,
	TP_PROTO(const char *dev_name, s32 latency, int type,
		cpumask_t *cpumask),

	TP_ARGS(dev_name, latency, type, cpumask),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__field(s32, latency)
		__field(int, type)
		__field(u32, cpumask)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__entry->latency = latency;
		__entry->type = type;
		__entry->cpumask = cpumask->bits[0];
	),

	TP_printk(
		"%s: latency=%d for req_type=%d with cpu mask=0x%08X",
		__get_str(dev_name),
		__entry->latency,
		__entry->type,
		__entry->cpumask
	)
);

DEFINE_EVENT(mmc_pm_qos_template, mmc_pm_qos_add,
	TP_PROTO(const char *dev_name, s32 latency, int type,
		cpumask_t *cpumask),
	TP_ARGS(dev_name, latency, type, cpumask));

DEFINE_EVENT(mmc_pm_qos_template, mmc_pm_qos_update,
	TP_PROTO(const char *dev_name, s32 latency, int type,
		cpumask_t *cpumask),
	TP_ARGS(dev_name, latency, type, cpumask));

DEFINE_EVENT(mmc_pm_qos_template, mmc_pm_qos_update_timeout,
	TP_PROTO(const char *dev_name, s32 latency, int type,
		cpumask_t *cpumask),
	TP_ARGS(dev_name, latency, type, cpumask));

DEFINE_EVENT(mmc_pm_qos_template, mmc_pm_qos_remove,
	TP_PROTO(const char *dev_name, s32 latency, int type,
		cpumask_t *cpumask),
	TP_ARGS(dev_name, latency, type, cpumask));

TRACE_EVENT(mmc_pm_qos_config,

	TP_PROTO(const char *dev_name, int type, cpumask_t *cpumask,
		 int policy, u32 *latencies, unsigned num),

	TP_ARGS(dev_name, type, cpumask, policy, latencies, num),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__field(int, type)
		__field(u32, cpumask)
		__field(int, policy)
		__field(unsigned, num)
		__dynamic_array(u32, _latencies, num)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__entry->type = type;
		__entry->cpumask = cpumask->bits[0];
		__entry->policy = policy;
		__entry->num = num;
		memcpy(__get_dynamic_array(_latencies), latencies, num);
	),

	TP_printk(
		"%s: req_type=%d, cpu mask=0x%08X, policy=%d, tbl_sz=%d, latencies=%s",
		__get_str(dev_name), __entry->type,
		__entry->cpumask, __entry->policy, __entry->num,
		__print_hex(__get_dynamic_array(_latencies), __entry->num)
	)
);

TRACE_EVENT(mmc_pm_qos_skip,

	TP_PROTO(const char *dev_name, const char *msg, int type),

	TP_ARGS(dev_name, msg, type),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__string(msg, msg)
		__field(int, type)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__assign_str(msg, msg);
		__entry->type = type;
	),

	TP_printk("%s: %s for req_type=%d\n",
		__get_str(dev_name), __get_str(msg), __entry->type
	)
);

#endif /* if !defined(_TRACE_MMC_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
