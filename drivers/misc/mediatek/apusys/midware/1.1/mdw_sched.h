/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __APUSYS_MDW_SCHED_H__
#define __APUSYS_MDW_SCHED_H__

#ifdef CONFIG_MTK_APUSYS_RT_SUPPORT
#define preemption_support (1)
#else
#define preemption_support (0)
#endif

int mdw_sched_dev_routine(void *arg);
int mdw_sched(struct mdw_apu_sc *sc);
void mdw_sched_set_thd_group(void);
int mdw_sched_init(void);
void mdw_sched_exit(void);

int mdw_sched_pause(void);
void mdw_sched_restart(void);

#endif
