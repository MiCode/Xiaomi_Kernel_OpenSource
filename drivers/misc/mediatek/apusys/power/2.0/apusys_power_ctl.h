/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef _APUSYS_POWER_CTL_H_
#define _APUSYS_POWER_CTL_H_

#include "apusys_power_cust.h"

#define MAX(a, b)	((a) > (b) ? (a) : (b))
#define MIN(a, b)	((a) < (b) ? (a) : (b))

extern struct apusys_dvfs_opps apusys_opps;
extern bool is_power_debug_lock;

extern void apusys_dvfs_policy(uint64_t round_id);
extern void apusys_set_opp(enum DVFS_USER user, uint8_t opp);
extern bool apusys_check_opp_change(void);
extern void apusys_power_init(enum DVFS_USER user, void *init_power_data);
extern void apusys_power_uninit(enum DVFS_USER user);
extern void apusys_power_on(enum DVFS_USER user);
extern void apusys_power_off(enum DVFS_USER user);
extern uint8_t apusys_boost_value_to_opp
	(enum DVFS_USER user, uint8_t boost_value);
#endif
