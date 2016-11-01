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
#ifndef __SOC_QCOM_SYS_PM_H__
#define __SOC_QCOM_SYS_PM_H__

#ifdef CONFIG_QTI_SYSTEM_PM
int system_sleep_enter(uint64_t sleep_val);

void system_sleep_exit(void);
#else
static inline int system_sleep_enter(uint64_t sleep_val)
{ return -ENODEV; }

static inline void system_sleep_exit(void)
{ }
#endif /* CONFIG_QTI_SYSTEM_PM */

#endif /* __SOC_QCOM_SYS_PM_H__ */
