/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __ASM_ARCH_MSM_IOMAP_FSM9XXX_H
#define __ASM_ARCH_MSM_IOMAP_FSM9XXX_H

/* Physical base address and size of peripherals.
 * Ordered by the virtual base addresses they will be mapped at.
 *
 * If you add or remove entries here, you'll want to edit the
 * msm_io_desc array in arch/arm/mach-msm/io.c to reflect your
 * changes.
 *
 */

#define MSM_VIC_BASE          IOMEM(0xFA000000)
#define MSM_VIC_PHYS          0x9C080000
#define MSM_VIC_SIZE          SZ_4K

#define MSM_SIRC_BASE         IOMEM(0xFA001000)
#define MSM_SIRC_PHYS         0x94190000
#define MSM_SIRC_SIZE         SZ_4K

#define MSM_CSR_BASE          IOMEM(0xFA002000)
#define MSM_CSR_PHYS          0x9C000000
#define MSM_CSR_SIZE          SZ_4K

#define MSM_TMR_BASE          MSM_CSR_BASE

#define MSM_TLMM_BASE         IOMEM(0xFA003000)
#define MSM_TLMM_PHYS         0x94040000
#define MSM_TLMM_SIZE         SZ_4K

#define MSM_TCSR_BASE	      IOMEM(0xFA004000)
#define MSM_TCSR_PHYS	      0x94030000
#define MSM_TCSR_SIZE	      SZ_4K

#define MSM_CLK_CTL_BASE      IOMEM(0xFA005000)
#define MSM_CLK_CTL_PHYS      0x94020000
#define MSM_CLK_CTL_SIZE      SZ_4K

#define MSM_ACC_BASE          IOMEM(0xFA006000)
#define MSM_ACC_PHYS          0x9C001000
#define MSM_ACC_SIZE          SZ_4K

#define MSM_SAW_BASE          IOMEM(0xFA007000)
#define MSM_SAW_PHYS          0x9C002000
#define MSM_SAW_SIZE          SZ_4K

#define MSM_GCC_BASE	      IOMEM(0xFA008000)
#define MSM_GCC_PHYS	      0x9C082000
#define MSM_GCC_SIZE	      SZ_4K

#define MSM_GRFC_BASE	      IOMEM(0xFA009000)
#define MSM_GRFC_PHYS	      0x94038000
#define MSM_GRFC_SIZE	      SZ_4K

#define MSM_QFP_FUSE_BASE     IOMEM(0xFA010000)
#define MSM_QFP_FUSE_PHYS     0x80000000
#define MSM_QFP_FUSE_SIZE     SZ_32K

#define MSM_HH_BASE	      IOMEM(0xFA100000)
#define MSM_HH_PHYS	      0x94200000
#define MSM_HH_SIZE	      SZ_1M

#define MSM_SHARED_RAM_BASE   IOMEM(0xFA200000)
#define MSM_SHARED_RAM_SIZE   SZ_1M

#define MSM_UART1_PHYS        0x94000000
#define MSM_UART1_SIZE        SZ_4K

#define MSM_UART2_PHYS        0x94100000
#define MSM_UART2_SIZE        SZ_4K

#endif
