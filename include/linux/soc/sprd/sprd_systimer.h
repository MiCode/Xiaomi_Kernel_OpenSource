/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef __SPRD_SYSFRT_H__
#define __SPRD_SYSFRT_H__

enum {
	SYSTEM_TIMER,
	SYSTEM_FRT,
};

struct cnter_to_boottime {
	u64 last_boottime;
	u64 last_systimer_counter;
	u64 last_sysfrt_counter;
	u32 systimer_mult;
	u32 systimer_shift;
	u32 sysfrt_mult;
	u32 sysfrt_shift;
};

#if IS_ENABLED(CONFIG_SPRD_SYSTIMER)
extern void get_convert_para(struct cnter_to_boottime *convert_para);
extern u64 sprd_systimer_read(void);
extern u64 sprd_sysfrt_read(void);
extern u64 sprd_systimer_to_boottime(u64 counter, int src);
#else
static inline void get_convert_para(struct cnter_to_boottime *convert_para) {};
static inline u64 sprd_sysfrt_read(void) { return 0; }
static inline u64 sprd_sysfrt_to_boottime(u64 counter) { return 0; }
static inline u64 sprd_systimer_to_boottime(u64 counter, int src) { return 0; }
#endif

#endif
