/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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


#ifndef __ASM_ARCH_MSM_IOMAP_8084_H
#define __ASM_ARCH_MSM_IOMAP_8084_H

/* Physical base address and size of peripherals.
 * Ordered by the virtual base addresses they will be mapped at.
 *
 * If you add or remove entries here, you'll want to edit the
 * io desc array in arch/arm/mach-msm/io.c to reflect your
 * changes.
 *
 */

#define APQ8084_SHARED_RAM_PHYS     0x0FA00000

#define APQ8084_QGIC_DIST_PHYS	0xF9000000
#define APQ8084_QGIC_DIST_SIZE	SZ_4K

#define APQ8084_TLMM_PHYS	0xFD510000
#define APQ8084_TLMM_SIZE	SZ_16K

#define APQ8084_QGIC_CPU_PHYS	0xF9002000
#define APQ8084_QGIC_CPU_SIZE	SZ_4K

#ifdef CONFIG_DEBUG_APQ8084_UART
#define MSM_DEBUG_UART_BASE	IOMEM(0xFA71E000)
#define MSM_DEBUG_UART_PHYS	0xF991E000
#endif

#endif
