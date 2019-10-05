/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MC_SCHEDULER_H__
#define __MC_SCHEDULER_H__

int mc_scheduler_init(void);
static inline void mc_scheduler_exit(void) {}
int mc_scheduler_start(void);
void mc_scheduler_stop(void);
int mc_scheduler_suspend(void);
int mc_scheduler_resume(void);

#endif /* __MC_SCHEDULER_H__ */
