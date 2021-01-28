/*
 * Copyright (C) 2017 MediaTek Inc.
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
extern void unthrottle_offline_rt_rqs(struct rq *rq);
DECLARE_PER_CPU(struct hmp_domain *, hmp_cpu_domain);
#include "../../drivers/misc/mediatek/base/power/include/mtk_upower.h"
extern int l_plus_cpu;
extern unsigned long get_cpu_util(int cpu);
#ifdef CONFIG_SMP
#ifdef CONFIG_ARM64
extern unsigned long arch_scale_get_max_freq(int cpu);
extern unsigned long arch_scale_get_min_freq(int cpu);
#else
static inline unsigned long arch_scale_get_max_freq(int cpu) { return 0; }
static inline unsigned long arch_scale_get_min_freq(int cpu) { return 0; }
#endif
#endif
