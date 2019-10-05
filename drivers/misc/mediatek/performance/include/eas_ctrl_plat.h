/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _EAS_CTRL_PLAT_H_
#define _EAS_CTRL_PLAT_H_

/* control migration cost */
extern unsigned int sysctl_sched_migration_cost;
extern unsigned int sysctl_sched_sync_hint_enable;

extern int schedutil_set_down_rate_limit_us(int cpu,
	unsigned int rate_limit_us);
extern int schedutil_set_up_rate_limit_us(int cpu,
	unsigned int rate_limit_us);

/* EAS */
extern int uclamp_min_pct_for_perf_idx(int group_idx, int min_value);
extern void set_sched_rotation_enable(bool enable);

#endif /* _EAS_CTRL_PLAT_H_ */
