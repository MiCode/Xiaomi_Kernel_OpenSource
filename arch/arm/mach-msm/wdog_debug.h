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

#ifndef __WDOG_DEBUG_H
#define __WDOG_DEBUG_H

#ifdef CONFIG_MSM_ENABLE_WDOG_DEBUG_CONTROL
void msm_enable_wdog_debug(void);
void msm_disable_wdog_debug(void);
#else
void msm_enable_wdog_debug(void) { }
void msm_disable_wdog_debug(void) { }
#endif

#endif
