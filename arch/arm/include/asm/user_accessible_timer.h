/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ARM_KERNEL_USER_ACCESSIBLE_TIMER_H_
#define _ARM_KERNEL_USER_ACCESSIBLE_TIMER_H_

#define ARM_USER_ACCESSIBLE_TIMERS_INVALID_PAGE -1

extern unsigned long zero_pfn;

#define CONFIG_ARM_USER_ACCESSIBLE_TIMER_BASE 0
static inline void setup_user_timer_offset(unsigned long addr)
{
}
static inline int get_timer_page_address(void)
{
	return ARM_USER_ACCESSIBLE_TIMERS_INVALID_PAGE;
}
static inline int get_user_accessible_timers_base(void)
{
	return 0;
}
static inline void set_user_accessible_timer_flag(bool flag)
{
}

#endif
