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

#ifdef CONFIG_MTK_UNIFY_POWER
extern int
mtk_idle_power(int cpu_idx, int idle_state, int cpu, void *argu, int sd_level);

extern
int mtk_busy_power(int cpu_idx, int cpu, void *argu, int sd_level);
#endif

