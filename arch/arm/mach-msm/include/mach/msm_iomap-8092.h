/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_IOMAP_8092_H
#define __ASM_ARCH_MSM_IOMAP_8092_H

/* Physical base address and size of peripherals.
 * Ordered by the virtual base addresses they will be mapped at.
 *
 * If you add or remove entries here, you'll want to edit the
 * io desc array in arch/arm/mach-msm/io.c to reflect your
 * changes.
 *
 */

#define MPQ8092_MSM_SHARED_RAM_PHYS	0x0FA00000

#define MPQ8092_TLMM_PHYS	0xFD510000
#define MPQ8092_TLMM_SIZE	SZ_16K

#define MPQ8092_MPM2_PSHOLD_PHYS	0xFC4AB000
#define MPQ8092_MPM2_PSHOLD_SIZE	SZ_4K

#ifdef CONFIG_DEBUG_MPQ8092_UART
#define MSM_DEBUG_UART_BASE	IOMEM(0xFA721000)
#define MSM_DEBUG_UART_PHYS	0xF9921000
#endif

#endif
