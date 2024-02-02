/* Copyright (c) 2010, 2012, 2014, The Linux Foundation. All rights reserved.
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
#ifndef __MACH_SCM_BOOT_H
#define __MACH_SCM_BOOT_H

#define SCM_BOOT_ADDR			0x1
#define SCM_FLAG_COLDBOOT_CPU1		0x01
#define SCM_FLAG_COLDBOOT_CPU2		0x08
#define SCM_FLAG_COLDBOOT_CPU3		0x20
#define SCM_FLAG_WARMBOOT_CPU1		0x02
#define SCM_FLAG_WARMBOOT_CPU0		0x04
#define SCM_FLAG_WARMBOOT_CPU2		0x10
#define SCM_FLAG_WARMBOOT_CPU3		0x40

/* Multicluster Variants */
#define SCM_BOOT_ADDR_MC		0x11
#define SCM_FLAG_COLDBOOT_MC		0x02
#define SCM_FLAG_WARMBOOT_MC		0x04

#ifdef CONFIG_ARM64
#define SCM_FLAG_HLOS			0x01
#else
#define SCM_FLAG_HLOS			0x0
#endif

#ifdef CONFIG_QCOM_SCM
int scm_set_boot_addr(phys_addr_t addr, unsigned int flags);
int scm_set_boot_addr_mc(phys_addr_t addr, u32 aff0,
		u32 aff1, u32 aff2, u32 flags);
int scm_set_warm_boot_addr_mc_for_all(phys_addr_t addr);
int scm_is_mc_boot_available(void);
#else
static inline int scm_set_boot_addr(phys_addr_t addr, unsigned int flags)
{
	WARN_ONCE(1, "CONFIG_QCOM_SCM disabled, SCM call will fail silently\n");
	return 0;
}
static inline int scm_set_boot_addr_mc(phys_addr_t addr, u32 aff0,
		u32 aff1, u32 aff2, u32 flags)
{
	WARN_ONCE(1, "CONFIG_QCOM_SCM disabled, SCM call will fail silently\n");
	return 0;
}
static inline int scm_set_warm_boot_addr_mc_for_all(phys_addr_t addr)
{
	WARN_ONCE(1, "CONFIG_QCOM_SCM disabled, SCM call will fail silently\n");
	return 0;
}
static inline int scm_is_mc_boot_available(void)
{
	WARN_ONCE(1, "CONFIG_QCOM_SCM disabled, SCM call will fail silently\n");
	return 0;
}
#endif

#endif
