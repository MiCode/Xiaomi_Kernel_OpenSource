/* Copyright (c) 2012 The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_IOMMU_PAGETABLE_H
#define __ARCH_ARM_MACH_MSM_IOMMU_PAGETABLE_H

#define NUM_FL_PTE      4096
#define NUM_SL_PTE      256
#define NUM_TEX_CLASS   8

/* First-level page table bits */
#define FL_BASE_MASK            0xFFFFFC00
#define FL_TYPE_TABLE           (1 << 0)
#define FL_TYPE_SECT            (2 << 0)
#define FL_SUPERSECTION         (1 << 18)
#define FL_AP0                  (1 << 10)
#define FL_AP1                  (1 << 11)
#define FL_AP2                  (1 << 15)
#define FL_SHARED               (1 << 16)
#define FL_BUFFERABLE           (1 << 2)
#define FL_CACHEABLE            (1 << 3)
#define FL_TEX0                 (1 << 12)
#define FL_OFFSET(va)           (((va) & 0xFFF00000) >> 20)
#define FL_NG                   (1 << 17)

/* Second-level page table bits */
#define SL_BASE_MASK_LARGE      0xFFFF0000
#define SL_BASE_MASK_SMALL      0xFFFFF000
#define SL_TYPE_LARGE           (1 << 0)
#define SL_TYPE_SMALL           (2 << 0)
#define SL_AP0                  (1 << 4)
#define SL_AP1                  (2 << 4)
#define SL_AP2                  (1 << 9)
#define SL_SHARED               (1 << 10)
#define SL_BUFFERABLE           (1 << 2)
#define SL_CACHEABLE            (1 << 3)
#define SL_TEX0                 (1 << 6)
#define SL_OFFSET(va)           (((va) & 0xFF000) >> 12)
#define SL_NG                   (1 << 11)

/* Memory type and cache policy attributes */
#define MT_SO                   0
#define MT_DEV                  1
#define MT_NORMAL               2
#define CP_NONCACHED            0
#define CP_WB_WA                1
#define CP_WT                   2
#define CP_WB_NWA               3

/* TEX Remap Registers */
#define NMRR_ICP(nmrr, n) (((nmrr) & (3 << ((n) * 2))) >> ((n) * 2))
#define NMRR_OCP(nmrr, n) (((nmrr) & (3 << ((n) * 2 + 16))) >> ((n) * 2 + 16))

#define PRRR_NOS(prrr, n) ((prrr) & (1 << ((n) + 24)) ? 1 : 0)
#define PRRR_MT(prrr, n)  ((((prrr) & (3 << ((n) * 2))) >> ((n) * 2)))

#define MRC(reg, processor, op1, crn, crm, op2)                         \
__asm__ __volatile__ (                                                  \
"   mrc   "   #processor "," #op1 ", %0,"  #crn "," #crm "," #op2 "\n"  \
: "=r" (reg))

#define RCP15_PRRR(reg)   MRC(reg, p15, 0, c10, c2, 0)
#define RCP15_NMRR(reg)   MRC(reg, p15, 0, c10, c2, 1)

struct iommu_pt {
	unsigned long *fl_table;
	int redirect;
};

void msm_iommu_pagetable_init(void);
int msm_iommu_pagetable_alloc(struct iommu_pt *pt);
void msm_iommu_pagetable_free(struct iommu_pt *pt);
int msm_iommu_pagetable_map(struct iommu_pt *pt, unsigned long va,
			phys_addr_t pa, size_t len, int prot);
size_t msm_iommu_pagetable_unmap(struct iommu_pt *pt, unsigned long va,
				size_t len);
int msm_iommu_pagetable_map_range(struct iommu_pt *pt, unsigned int va,
			struct scatterlist *sg, unsigned int len, int prot);
void msm_iommu_pagetable_unmap_range(struct iommu_pt *pt, unsigned int va,
				unsigned int len);
#endif
