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

#ifndef __ASM_ARCH_MSM_IOMAP_8909_H
#define __ASM_ARCH_MSM_IOMAP_8909_H

/* Physical base address and size of peripherals.
 * Ordered by the virtual base addresses they will be mapped at.
 *
 * If you add or remove entries here, you'll want to edit the
 * io desc array in arch/arm/mach-msm/io.c to reflect your
 * changes.
 *
 */

#define MSM8909_APCS_GCC_PHYS	0xB011000
#define MSM8909_APCS_GCC_SIZE	SZ_4K

#if defined(CONFIG_DEBUG_MSM8909_UART) || defined(CONFIG_DEBUG_MDMFERRUM_UART)
#define MSM_DEBUG_UART_BASE	IOMEM(0xFA0AF000)
#define MSM_DEBUG_UART_PHYS	0x78AF000
#endif

#endif
