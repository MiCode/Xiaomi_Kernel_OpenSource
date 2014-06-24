/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_IOMAP_FERRUM_H
#define __ASM_ARCH_MSM_IOMAP_FERRUM_H

/* Physical base address and size of peripherals.
 * Ordered by the virtual base addresses they will be mapped at.
 *
 * If you add or remove entries here, you'll want to edit the
 * io desc array in arch/arm/mach-msm/io.c to reflect your
 * changes.
 *
 */

#define MSMFERRUM_APCS_GCC_PHYS	0xB011000
#define MSMFERRUM_APCS_GCC_SIZE	SZ_4K

#ifdef CONFIG_DEBUG_MSMFERRUM_UART
#define MSM_DEBUG_UART_BASE	IOMEM(0xFA0B0000)
#define MSM_DEBUG_UART_PHYS	0x78B0000
#endif

#endif
