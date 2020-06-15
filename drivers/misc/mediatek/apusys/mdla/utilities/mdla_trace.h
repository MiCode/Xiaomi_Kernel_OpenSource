/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_TRACE_H__
#define __MDLA_TRACE_H__

struct command_entry;

enum {
	MDLA_TRACE_MODE_CMD = 0,
	MDLA_TRACE_MODE_INT = 1
};


#ifndef TRACE_LEN
#define TRACE_LEN 255
#endif

#if IS_ENABLED(CONFIG_FTRACE)
void mdla_trace_begin(int core_id, struct command_entry *ce);
void mdla_trace_end(int core_id, int preempt, struct command_entry *ce);
void mdla_trace_reset(int core_id, const char *str);
void mdla_trace_pmu_polling(int core_id, unsigned int *c);
#else
static inline void mdla_trace_begin(int core_id,
					struct command_entry *ce) {}
static inline void mdla_trace_end(int core_id, int preempt,
					struct command_entry *ce) {}
static inline void mdla_trace_reset(int core_id, const char *str) {}
static inline void mdla_trace_pmu_polling(int core_id, unsigned int *c) {}
#endif

#endif /* __MDLA_TRACE_H__ */
