/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

/*
 * Power management for TrustZone
 */

#ifndef __KREE_TZ_PM_H__
#define __KREE_TZ_PM_H__

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) || defined(CONFIG_TRUSTY)

void kree_pm_init(void);
void kree_pm_cpu_lowpower(int *ppen_release, int logical_cpuid);
int kree_pm_cpu_dormant(int mode);
int kree_pm_device_ops(int state);
int kree_pm_cpu_dormant_workaround_wake(int workaround_wake);

#else

#define kree_pm_cpu_lowpower(ppen, cpuid)
#define kree_pm_cpu_dormant(mode)    1

#endif	/* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT || CONFIG_TRUSTY */

#endif	/* __KREE_TZ_PM_H__ */
