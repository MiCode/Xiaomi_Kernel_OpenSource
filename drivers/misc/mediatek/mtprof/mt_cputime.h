/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/sched.h>

bool mtsched_is_enabled(void);
int mtproc_counts(void);

#ifdef CONFIG_MTPROF_CPUTIME
extern void save_mtproc_info(struct task_struct *p, unsigned long long ts);
extern void end_mtproc_info(struct task_struct *p);
extern void mt_cputime_switch(int on);
#else
static inline void
save_mtproc_info(struct task_struct *p, unsigned long long ts) {};
static inline void end_mtproc_info(struct task_struct *p) {};
static inline void mt_cputime_switch(int on) {};
#endif

