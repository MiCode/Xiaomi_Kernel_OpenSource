/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __EDMC_DVFS_H__
#define __EDMC_DVFS_H__


#ifdef CONFIG_MTK_MDLA_SUPPORT
extern void mdla_put_power(int core);
extern int mdla_get_power(int core);
extern void mdla_opp_check(int core, uint8_t vmdla_index, uint8_t freq_index);
#else
inline void mdla_put_power(int core)
{
}

inline int mdla_get_power(int core)
{
	//It mean no define
	return -1;
}

inline void mdla_opp_check(int core, uint8_t vmdla_index, uint8_t freq_index)
{
}
#endif


#endif
