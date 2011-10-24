/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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

#ifndef __ASM_ARCH_MSM_IOMAP_9615_H
#define __ASM_ARCH_MSM_IOMAP_9615_H

/* Physical base address and size of peripherals.
 * Ordered by the virtual base addresses they will be mapped at.
 *
 * If you add or remove entries here, you'll want to edit the
 * msm_io_desc array in arch/arm/mach-msm/io.c to reflect your
 * changes.
 *
 */

#define MSM9615_TMR_PHYS		0x0200A000
#define MSM9615_TMR_SIZE		SZ_4K

#define MSM9615_QGIC_DIST_PHYS		0x02000000
#define MSM9615_QGIC_DIST_SIZE		SZ_4K

#define MSM9615_QGIC_CPU_PHYS		0x02002000
#define MSM9615_QGIC_CPU_SIZE		SZ_4K

#define MSM9615_TLMM_PHYS		0x00800000
#define MSM9615_TLMM_SIZE		SZ_1M

#define MSM9615_ACC0_PHYS		0x02008000
#define MSM9615_ACC0_SIZE		SZ_4K

#define MSM9615_APCS_GCC_PHYS		0x02011000
#define MSM9615_APCS_GCC_SIZE		SZ_4K

#define MSM9615_SAW0_PHYS		0x02009000
#define MSM9615_SAW0_SIZE		SZ_4K

#define MSM9615_TCSR_PHYS		0x1A400000
#define MSM9615_TCSR_SIZE		SZ_4K

#define MSM9615_L2CC_PHYS		0x02040000
#define MSM9615_L2CC_SIZE		SZ_4K

#define MSM9615_CLK_CTL_PHYS            0x00900000
#define MSM9615_CLK_CTL_SIZE            SZ_16K

#define MSM9615_LPASS_CLK_CTL_PHYS      0x28000000
#define MSM9615_LPASS_CLK_CTL_SIZE      SZ_4K

#define MSM9615_RPM_PHYS		0x00108000
#define MSM9615_RPM_SIZE		SZ_4K

#define MSM9615_RPM_MPM_PHYS		0x00200000
#define MSM9615_RPM_MPM_SIZE		SZ_4K

#define MSM9615_APCS_GLB_PHYS		0x02010000
#define MSM9615_APCS_GLB_SIZE		SZ_4K

#define MSM9615_HSUSB_PHYS		0x12500000
#define MSM9615_HSUSB_SIZE		SZ_4K

#define MSM9615_QFPROM_PHYS		0x00700000
#define MSM9615_QFPROM_SIZE		SZ_4K

#define MSM9615_IMEM_PHYS		0x2B000000
#define MSM9615_IMEM_SIZE		SZ_4K

#endif
