/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ASM_ARCH_MSM_WATCHDOG_H_
#define _ASM_ARCH_MSM_WATCHDOG_H_

#ifdef CONFIG_QCOM_FORCE_WDOG_BITE_ON_PANIC
#define WDOG_BITE_ON_PANIC 1
#else
#define WDOG_BITE_ON_PANIC 0
#endif

#ifdef CONFIG_QCOM_WATCHDOG_V2
void msm_trigger_wdog_bite(void);
#else
static inline void msm_trigger_wdog_bite(void) { }
#endif

#endif
