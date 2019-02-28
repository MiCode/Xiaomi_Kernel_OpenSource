/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 * Add a system-wide over-utilization indicator which
 * is updated in load-balance.
 */
#include "../../drivers/misc/mediatek/base/power/include/mtk_upower.h"
static bool system_overutil;
extern int cpu_eff_tp;

inline bool system_overutilized(int cpu);

static inline unsigned long task_util(struct task_struct *p);
static int select_max_spare_capacity(struct task_struct *p, int target);
static int init_cpu_info(void);
static unsigned int aggressive_idle_pull(int this_cpu);
bool idle_lb_enhance(struct task_struct *p, int cpu);
static bool is_intra_domain(int prev, int target);
#if 0
static int is_tiny_task(struct task_struct *p);
#endif
static int
___select_idle_sibling(struct task_struct *p, int prev_cpu, int new_cpu);
extern int find_best_idle_cpu(struct task_struct *p, bool prefer_idle);

#ifdef CONFIG_MTK_UNIFY_POWER
extern int
mtk_idle_power(int cpu_idx, int idle_state, int cpu, void *argu, int sd_level);

extern
int mtk_busy_power(int cpu_idx, int cpu, void *argu, int sd_level);

#endif

