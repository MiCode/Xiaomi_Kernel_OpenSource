/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _L3CC_COMMON_H_
#define _L3CC_COMMON_H_
#ifdef CONFIG_MTK_CACHE_CONTROL
extern void hook_ca_context_switch(struct rq *rq,
		struct task_struct *prev,
	    struct task_struct *next);
extern int ca_force_stop_set_in_kernel(int val);
extern void hook_ca_scheduler_tick(int cpu);
#else

inline void hook_ca_context_switch(struct rq *rq,
		struct task_struct *prev,
	    struct task_struct *next) {}
inline void hook_ca_scheduler_tick(int cpu) {}
int ca_force_stop_set_in_kernel(int val) {return -EINVAL; }
#endif
#endif
