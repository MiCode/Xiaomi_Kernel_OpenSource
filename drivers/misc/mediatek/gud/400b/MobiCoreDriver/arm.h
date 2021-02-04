/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MC_ARM_H_
#define _MC_ARM_H_

#include "main.h"

#ifdef CONFIG_ARM64
static inline bool has_security_extensions(void)
{
	return true;
}

static inline bool is_secure_mode(void)
{
	return false;
}
#else
/*
 * ARM Trustzone specific masks and modes
 * Vanilla Linux is unaware of TrustZone extension.
 * I.e. arch/arm/include/asm/ptrace.h does not define monitor mode.
 * Also TZ bits in cpuid are not defined, ARM port uses magic numbers,
 * see arch/arm/kernel/setup.c
 */
#define ARM_MONITOR_MODE		(0x16) /*(0b10110)*/
#define ARM_SECURITY_EXTENSION_MASK	(0x30)

/* check if CPU supports the ARM TrustZone Security Extensions */
static inline bool has_security_extensions(void)
{
	u32 fea = 0;

	asm volatile(
		"mrc p15, 0, %[fea], cr0, cr1, 0" :
		[fea]"=r" (fea));

	mc_dev_devel("CPU Features: 0x%X", fea);

	/*
	 * If the CPU features ID has 0 for security features then the CPU
	 * doesn't support TrustZone at all!
	 */
	if ((fea & ARM_SECURITY_EXTENSION_MASK) == 0)
		return false;

	return true;
}

/* check if running in secure mode */
static inline bool is_secure_mode(void)
{
	u32 cpsr = 0;
	u32 nsacr = 0;

	asm volatile(
		"mrc	p15, 0, %[nsacr], cr1, cr1, 2\n"
		"mrs %[cpsr], cpsr\n" :
		[nsacr]"=r" (nsacr),
		[cpsr]"=r"(cpsr));

	mc_dev_devel("CPRS.M = set to 0x%X", cpsr & MODE_MASK);
	mc_dev_devel("SCR.NS = set to 0x%X", nsacr);

	/*
	 * If the NSACR contains the reset value(=0) then most likely we are
	 * running in Secure MODE.
	 * If the cpsr mode is set to monitor mode then we cannot load!
	 */
	if (nsacr == 0 || ((cpsr & MODE_MASK) == ARM_MONITOR_MODE))
		return true;

	return false;
}
#endif

#endif /* _MC_ARM_H_ */
