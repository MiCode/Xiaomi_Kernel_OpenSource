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

#ifndef __MC_DEVICE_H__
#define __MC_DEVICE_H__

void mc_dev_schedule(void);
int mc_dev_yield(void);
int mc_dev_nsiq(void);
uint32_t mc_dev_get_version(void);
void mc_dev_dump_mobicore_status(void);
int mc_dev_sched_init(void);
void mc_dev_sched_cleanup(void);
int mc_dev_notify(uint32_t session_id);

#endif /* __MC_DEVICE_H__ */
