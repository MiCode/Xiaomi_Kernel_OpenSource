/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_IOMAP_FSM9010_H
#define __ASM_ARCH_MSM_IOMAP_FSM9010_H

/* Physical base address and size of peripherals.
 * Ordered by the virtual base addresses they will be mapped at.
 *
 * If you add or remove entries here, you'll want to edit the
 * io desc array in arch/arm/mach-msm/io.c to reflect your
 * changes.
 *
 */

#define FSM9010_APCS_GCC_PHYS	0xF9011000
#define FSM9010_APCS_GCC_SIZE	SZ_4K

#define FSM9010_MPM2_PSHOLD_PHYS	0xFC4AB000
#define FSM9010_MPM2_PSHOLD_SIZE	SZ_4K

#ifdef CONFIG_DEBUG_FSM9010_UART
#define MSM_DEBUG_UART_BASE	IOMEM(0xFA720000)
#define MSM_DEBUG_UART_PHYS	0xF9920000
#endif

#endif
