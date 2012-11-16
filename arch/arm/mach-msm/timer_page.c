/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/mm.h>
#include <linux/export.h>
#include <asm/user_accessible_timer.h>
#include "mach/socinfo.h"
#include "mach/msm_iomap.h"

#include "timer.h"

inline int get_timer_page_address(void)
{
	if (!use_user_accessible_timers())
		return ARM_USER_ACCESSIBLE_TIMERS_INVALID_PAGE;

	if (cpu_is_msm8960())
		return MSM8960_TMR0_PHYS;
	else if (cpu_is_msm8930())
		return MSM8930_TMR0_PHYS;
	else if (cpu_is_apq8064())
		return APQ8064_TMR0_PHYS;
	else
		return ARM_USER_ACCESSIBLE_TIMERS_INVALID_PAGE;
}
EXPORT_SYMBOL(get_timer_page_address);

