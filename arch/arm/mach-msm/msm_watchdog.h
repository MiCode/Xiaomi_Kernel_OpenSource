/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_MSM_WATCHDOG_H
#define __ARCH_ARM_MACH_MSM_MSM_WATCHDOG_H

struct msm_watchdog_pdata {
	/* pet interval period in ms */
	unsigned int pet_time;
	/* bark timeout in ms */
	unsigned int bark_time;
	bool has_secure;
};

#ifdef CONFIG_MSM_WATCHDOG
void pet_watchdog(void);
#else
static inline void pet_watchdog(void) { }
#endif

#endif
