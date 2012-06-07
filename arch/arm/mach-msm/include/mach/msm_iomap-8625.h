/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * The MSM peripherals are spread all over across 768MB of physical
 * space, which makes just having a simple IO_ADDRESS macro to slide
 * them into the right virtual location rough.  Instead, we will
 * provide a master phys->virt mapping for peripherals here.
 *
 */

#ifndef __ASM_ARCH_MSM_IOMAP_8625_H
#define __ASM_ARCH_MSM_IOMAP_8625_H

/* Physical base address and size of peripherals.
 * Ordered by the virtual base addresses they will be mapped at.
 *
 * If you add or remove entries here, you'll want to edit the
 * msm_io_desc array in arch/arm/mach-msm/io.c to reflect your
 * changes.
 *
 */

#define MSM8625_TMR_PHYS		0xC0800000
#define MSM8625_TMR_SIZE		SZ_4K

#define MSM8625_TMR0_PHYS		0xC0100000
#define MSM8625_TMR0_SIZE		SZ_4K

#define MSM8625_CLK_CTL_PHYS		0xA8600000
#define MSM8625_CLK_CTL_SIZE		SZ_4K

#define MSM8625_QGIC_DIST_PHYS		0xC0000000
#define MSM8625_QGIC_DIST_SIZE		SZ_4K

#define MSM8625_QGIC_CPU_PHYS		0xC0002000
#define MSM8625_QGIC_CPU_SIZE		SZ_4K

#define MSM8625_SCU_PHYS		0xC0600000
#define MSM8625_SCU_SIZE		SZ_256

#define MSM8625_SAW0_PHYS		0xC0200000
#define MSM8625_SAW0_SIZE		SZ_4K

#define MSM8625_SAW1_PHYS		0xC0700000
#define MSM8625_SAW1_SIZE		SZ_4K

#define MSM8625_CFG_CTL_PHYS		0xA9800000
#define MSM8625_CFG_CTL_SIZE		SZ_4K

#endif
