// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/kernel.h>

#include <apusys_trace.h>

#include <common/mdla_device.h>

#include <utilities/mdla_debug.h>

#define CREATE_TRACE_POINTS
#include <utilities/met_mdlasys_events.h>

static inline bool mdla_trace_enable(void)
{
	return cfg_apusys_trace;
}

void mdla_trace_begin(int core_id, struct command_entry *ce)
{
	char buf[TRACE_LEN];

	if (!mdla_trace_enable())
		return;

	snprintf(buf, sizeof(buf),
		"mdla-%d|fin_cid:%d,total_cmd_num:%d",
		core_id,
		ce->fin_cid,
		ce->count);

	mdla_perf_debug("%s\n", __func__);
	trace_async_tag(1, buf);
}

void mdla_trace_end(int core_id, int preempt, struct command_entry *ce)
{
	char buf[TRACE_LEN];

	if (!ce->req_end_t || !mdla_trace_enable())
		return;

	snprintf(buf, sizeof(buf),
		"mdla-%d|fin_id:%d,preempted:%d",
		core_id, ce->fin_cid, preempt);

	mdla_perf_debug("%s\n", __func__);
	trace_async_tag(0, buf);
}

/* restore trace settings after reset */
void mdla_trace_reset(int core_id, const char *str)
{
	if (!mdla_trace_enable())
		return;

	trace_tag_customer("C|%d|mdla-%d,reset:%s|0",
			  task_pid_nr(current), core_id, str);
}

void mdla_trace_pmu_polling(int core_id, unsigned int *c)
{
	trace_mdla_polling(core_id, c);
}
