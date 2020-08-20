/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_V1_X_SCHED_H__
#define __MDLA_V1_X_SCHED_H__


void mdla_sched_set_smp_deadline(int priority, u64 deadline);
u64 mdla_sched_get_smp_deadline(int priority);

int mdla_v1_x_sched_init(void);
void mdla_v1_x_sched_deinit(void);

#endif /* __MDLA_V1_X_SCHED_H__ */
