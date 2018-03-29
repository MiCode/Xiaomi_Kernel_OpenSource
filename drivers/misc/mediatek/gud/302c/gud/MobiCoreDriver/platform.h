/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
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
/*
 * Header file of MobiCore Driver Kernel Module Platform
 * specific structures
 *
 * Internal structures of the McDrvModule
 *
 * Header file the MobiCore Driver Kernel Module,
 * its internal structures and defines.
 */
#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

/* MobiCore Interrupt. */
#define MC_INTR_SSIQ 280

/* Enable mobicore mem traces */
#define MC_MEM_TRACES

#define TBASE_CORE_SWITCHER
/* Values of MPIDR regs in CPUs */
#define CPU_IDS {0x0000, 0x0001, 0x0002, 0x0003, 0x0100, 0x0101, 0x0102, 0x0103, 0x0200, 0x0201}
#define COUNT_OF_CPUS (CONFIG_NR_CPUS)

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD

#if !defined(CONFIG_ARCH_MT6580)
/* Enable LPAE */
#define LPAE_SUPPORT

/* Enable AARCH32 Fast call IDs */
#define MC_AARCH32_FC
#endif /* !CONFIG_ARCH_MT6580 */

#endif /* _MC_DRV_PLATFORM_H_ */
