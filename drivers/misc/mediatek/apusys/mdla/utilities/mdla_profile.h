/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_PROFILE_H__
#define __MDLA_PROFILE_H__

void mdla_prof_info_show(struct seq_file *s);

void mdla_prof_start(unsigned int core_id);
void mdla_prof_stop(unsigned int core_id, int wait);

void mdla_prof_pmu_timer_start(void);
void mdla_prof_pmu_timer_stop(void);
bool mdla_prof_pmu_timer_is_running(unsigned int core_id);

bool mdla_prof_use_dbgfs_pmu_event(void);

void mdla_prof_iter(int core_id);

void mdla_prof_init(void);
void mdla_prof_deinit(void);

#endif /* __MDLA_PROFILE_H__ */

