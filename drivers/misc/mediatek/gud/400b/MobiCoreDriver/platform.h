/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
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
#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

/* MobiCore Interrupt. */
/* Uncomment to override dts SSIQ setting */
#if 0
#if defined(CONFIG_ARCH_MT6580)
#define MC_INTR_SSIQ	183
#elif defined(CONFIG_ARCH_MT6752)
#define MC_INTR_SSIQ	280
#elif defined(CONFIG_ARCH_MT6797)
#define MC_INTR_SSIQ    324
#else
#define MC_INTR_SSIQ	80
#endif
#endif

/* Enable Runtime Power Management */
#if 0
#ifdef CONFIG_PM_RUNTIME
#define MC_PM_RUNTIME
#endif
#endif

#define TBASE_CORE_SWITCHER
/* Values of MPIDR regs in  cpu0, cpu1, cpu2, cpu3*/
#if defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
#define CPU_IDS {0x81000000, 0x81000100, 0x81000200, 0x81000300, 0x81000400, \
		0x81000500, 0x81000600, 0x81000700}
#else
#define CPU_IDS {0x0000, 0x0001, 0x0002, 0x0003, \
		 0x0100, 0x0101, 0x0102, 0x0103, 0x0200, 0x0201}
#endif
#define COUNT_OF_CPUS (CONFIG_NR_CPUS)

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD

#if defined(CONFIG_ARM64)
/* Enable LPAE on 64-bit platforms */
#ifndef CONFIG_TRUSTONIC_TEE_LPAE
#define CONFIG_TRUSTONIC_TEE_LPAE
#endif
#endif

/* For retrieving SSIQ from dts */
#define MC_DEVICE_PROPNAME	"trustonic,mobicore"

#endif /* _MC_DRV_PLATFORM_H_ */
