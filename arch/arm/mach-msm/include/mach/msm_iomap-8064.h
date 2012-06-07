/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_IOMAP_8064_H
#define __ASM_ARCH_MSM_IOMAP_8064_H

/* Physical base address and size of peripherals.
 * Ordered by the virtual base addresses they will be mapped at.
 *
 * If you add or remove entries here, you'll want to edit the
 * msm_io_desc array in arch/arm/mach-msm/io.c to reflect your
 * changes.
 *
 */

#define APQ8064_TMR_PHYS		0x0200A000
#define APQ8064_TMR_SIZE		SZ_4K

#define APQ8064_TMR0_PHYS		0x0208A000
#define APQ8064_TMR0_SIZE		SZ_4K

#define APQ8064_QGIC_DIST_PHYS		0x02000000
#define APQ8064_QGIC_DIST_SIZE		SZ_4K

#define APQ8064_QGIC_CPU_PHYS		0x02002000
#define APQ8064_QGIC_CPU_SIZE		SZ_4K

#define APQ8064_TLMM_PHYS		0x00800000
#define APQ8064_TLMM_SIZE		SZ_16K

#define APQ8064_ACC0_PHYS		0x02088000
#define APQ8064_ACC0_SIZE		SZ_4K

#define APQ8064_ACC1_PHYS		0x02098000
#define APQ8064_ACC1_SIZE		SZ_4K

#define APQ8064_ACC2_PHYS		0x020A8000
#define APQ8064_ACC2_SIZE		SZ_4K

#define APQ8064_ACC3_PHYS		0x020B8000
#define APQ8064_ACC3_SIZE		SZ_4K

#define APQ8064_APCS_GCC_PHYS		0x02011000
#define APQ8064_APCS_GCC_SIZE		SZ_4K

#define APQ8064_CLK_CTL_PHYS		0x00900000
#define APQ8064_CLK_CTL_SIZE		SZ_16K

#define APQ8064_MMSS_CLK_CTL_PHYS	0x04000000
#define APQ8064_MMSS_CLK_CTL_SIZE	SZ_4K

#define APQ8064_LPASS_CLK_CTL_PHYS	0x28000000
#define APQ8064_LPASS_CLK_CTL_SIZE	SZ_4K

#define APQ8064_HFPLL_PHYS		0x00903000
#define APQ8064_HFPLL_SIZE		SZ_4K

#define APQ8064_IMEM_PHYS		0x2A03F000
#define APQ8064_IMEM_SIZE		SZ_4K

#define APQ8064_RPM_PHYS		0x00108000
#define APQ8064_RPM_SIZE		SZ_4K

#define APQ8064_RPM_MPM_PHYS		0x00200000
#define APQ8064_RPM_MPM_SIZE		SZ_4K

#define APQ8064_SAW0_PHYS		0x02089000
#define APQ8064_SAW0_SIZE		SZ_4K

#define APQ8064_SAW1_PHYS		0x02099000
#define APQ8064_SAW1_SIZE		SZ_4K

#define APQ8064_SAW2_PHYS		0x020A9000
#define APQ8064_SAW2_SIZE		SZ_4K

#define APQ8064_SAW3_PHYS		0x020B9000
#define APQ8064_SAW3_SIZE		SZ_4K

#define APQ8064_SAW_L2_PHYS		0x02012000
#define APQ8064_SAW_L2_SIZE		SZ_4K
#define APQ8064_QFPROM_PHYS		0x00700000
#define APQ8064_QFPROM_SIZE		SZ_4K

#define APQ8064_SIC_NON_SECURE_PHYS	0x12100000
#define APQ8064_SIC_NON_SECURE_SIZE	SZ_64K

#define APQ8064_HDMI_PHYS		0x04A00000
#define APQ8064_HDMI_SIZE		SZ_4K

#ifdef CONFIG_DEBUG_APQ8064_UART
#define MSM_DEBUG_UART_BASE		IOMEM(0xFA740000)
#define MSM_DEBUG_UART_PHYS		0x16640000
#endif

#endif
