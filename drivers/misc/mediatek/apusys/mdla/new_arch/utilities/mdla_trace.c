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

static u32 *cfg_pmu_timer_en;

bool mdla_trace_enable(void)
{
	return cfg_apusys_trace;
}

void mdla_trace_begin(u32 core_id, struct command_entry *ce)
{
	char buf[TRACE_LEN] = {0};

	if (!mdla_trace_enable())
		return;

	if (snprintf(buf, sizeof(buf),
			"mdla-%d|fin_cid:%d,total_cmd_num:%d",
			core_id, ce->fin_cid, ce->count) > 0) {
		mdla_perf_debug("%s\n", __func__);
		trace_async_tag(1, buf);
	}
}

void mdla_trace_end(u32 core_id, int preempt, struct command_entry *ce)
{
	char buf[TRACE_LEN] = {0};

	if (!ce->req_end_t || !mdla_trace_enable())
		return;

	if (snprintf(buf, sizeof(buf),
			"mdla-%d|fin_id:%d,preempted:%d",
			core_id, ce->fin_cid, preempt) > 0) {
		mdla_perf_debug("%s\n", __func__);
		trace_async_tag(0, buf);
	}
}

/* restore trace settings after reset */
void mdla_trace_reset(u32 core_id, const char *str)
{
	if (!mdla_trace_enable())
		return;

//	trace_tag_customer("C|%d|mdla-%d,reset:%s|0",
//			  task_pid_nr(current), core_id, str);
}

void mdla_trace_pmu_polling(u32 core_id, u32 *c)
{
	trace_mdla_polling(core_id, c);
}

void mdla_trace_set_cfg_pmu_tmr_en(int enable)
{
	if (cfg_pmu_timer_en)
		*cfg_pmu_timer_en = enable;
}

bool mdla_trace_get_cfg_pmu_tmr_en(void)
{
	return cfg_pmu_timer_en ? *cfg_pmu_timer_en : false;
}

void mdla_trace_register_cfg_pmu_tmr(int *timer_en)
{
	cfg_pmu_timer_en = timer_en;
}
