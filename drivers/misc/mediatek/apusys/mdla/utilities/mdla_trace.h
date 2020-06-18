/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_TRACE_H__
#define __MDLA_TRACE_H__

#include <linux/types.h>

struct command_entry;

enum {
	MDLA_TRACE_MODE_CMD = 0,
	MDLA_TRACE_MODE_INT = 1
};


#ifndef TRACE_LEN
#define TRACE_LEN 255
#endif

#if IS_ENABLED(CONFIG_FTRACE)
void mdla_trace_begin(u32 core_id, struct command_entry *ce);
void mdla_trace_end(u32 core_id, int preempt, struct command_entry *ce);
void mdla_trace_reset(u32 core_id, const char *str);
void mdla_trace_pmu_polling(u32 core_id, u32 *c);
#else
static inline void mdla_trace_begin(u32 core_id,
					struct command_entry *ce) {}
static inline void mdla_trace_end(u32 core_id, int preempt,
					struct command_entry *ce) {}
static inline void mdla_trace_reset(u32 core_id, const char *str) {}
static inline void mdla_trace_pmu_polling(u32 core_id, u32 *c) {}
#endif

bool mdla_trace_enable(void);

void mdla_trace_set_cfg_pmu_tmr_en(int enable);
bool mdla_trace_get_cfg_pmu_tmr_en(void);
void mdla_trace_register_cfg_pmu_tmr(int *timer_en);

#endif /* __MDLA_TRACE_H__ */
