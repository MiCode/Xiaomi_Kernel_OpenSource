/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __M4U_PGTABLE_H__
#define __M4U_PGTABLE_H__

#include "m4u_reg.h"

/* ================================================================= */
/* 2 level pagetable: pgd -> pte */

#define F_PTE_TYPE_MSK F_MSK(1, 0)
#define F_PTE_TYPE_SET(val) F_VAL(val, 1, 0)
#define F_PTE_TYPE_GET(regval) F_MSK_SHIFT(regval, 1, 0)
#define F_PTE_TYPE_LARGE (0x1)
#define F_PTE_TYPE_SMALL (0x2)
#define F_PTE_B_BIT F_BIT_SET(2)
#define F_PTE_C_BIT F_BIT_SET(3)
#define F_PTE_BIT33_BIT F_BIT_SET(4)
#define F_PTE_TEX_MSK F_MSK(8, 6)
#define F_PTE_TEX_SET(val) F_VAL(val, 8, 6)
#define F_PTE_TEX_GET(regval) F_MSK_SHIFT(regval, 8, 6)
#define F_PTE_BIT32_BIT F_BIT_SET(9)
#define F_PTE_S_BIT F_BIT_SET(10)
#define F_PTE_NG_BIT F_BIT_SET(11)

#define F_PTE_PA_LARGE_MSK F_MSK(31, 16)
#define F_PTE_PA_LARGE_SET(val) F_VAL(val, 31, 16)
#define F_PTE_PA_LARGE_GET(regval) F_MSK_SHIFT(regval, 31, 16)
#define F_PTE_PA_SMALL_MSK F_MSK(31, 12)
#define F_PTE_PA_SMALL_SET(val) F_VAL(val, 31, 12)
#define F_PTE_PA_SMALL_GET(regval) F_MSK_SHIFT(regval, 31, 12)

#define F_PTE_TYPE_IS_LARGE_PAGE(pte)                                          \
	((imu_pte_val(pte) & 0x3) == F_PTE_TYPE_LARGE)
#define F_PTE_TYPE_IS_SMALL_PAGE(pte)                                          \
	((imu_pte_val(pte) & 0x3) == F_PTE_TYPE_SMALL)

#define F_PGD_TYPE_PAGE (0x1)
#define F_PGD_TYPE_PAGE_MSK (0x3)
#define F_PGD_TYPE_SECTION (0x2)
#define F_PGD_TYPE_SUPERSECTION (0x2 | (1 << 18))
#define F_PGD_TYPE_SECTION_MSK (0x3 | (1 << 18))
#define F_PGD_TYPE_IS_PAGE(pgd) ((imu_pgd_val(pgd) & 3) == 1)
#define F_PGD_TYPE_IS_SECTION(pgd)                                             \
	(F_PGD_TYPE_IS_PAGE(pgd)                                               \
		 ? 0                                                           \
		 : ((imu_pgd_val(pgd) & F_PGD_TYPE_SECTION_MSK) ==             \
		    F_PGD_TYPE_SECTION))
#define F_PGD_TYPE_IS_SUPERSECTION(pgd)                                        \
	(F_PGD_TYPE_IS_PAGE(pgd)                                               \
		 ? 0                                                           \
		 : ((imu_pgd_val(pgd) & F_PGD_TYPE_SECTION_MSK) ==             \
		    F_PGD_TYPE_SUPERSECTION))

#define F_PGD_B_BIT F_BIT_SET(2)
#define F_PGD_C_BIT F_BIT_SET(3)
#define F_PGD_BIT33_BIT F_BIT_SET(4)
#define F_PGD_AP_MSK F_MSK(11, 10)
#define F_PGD_AP_SET(val) F_VAL(val, 11, 10)
#define F_PGD_AP_GET(regval) F_MSK_SHIFT(regval, 11, 10)
#define F_PGD_TEX_MSK F_MSK(14, 12)
#define F_PGD_TEX_SET(val) F_VAL(val, 14, 12)
#define F_PGD_TEX_GET(regval) F_MSK_SHIFT(regval, 14, 12)
#define F_PGD_BIT32_BIT F_BIT_SET(9)
#define F_PGD_S_BIT F_BIT_SET(16)
#define F_PGD_NG_BIT F_BIT_SET(17)
#define F_PGD_NS_BIT_PAGE(ns) F_BIT_VAL(ns, 3)
#define F_PGD_NS_BIT_SECTION(ns) F_BIT_VAL(ns, 19)
#define F_PGD_NS_BIT_SUPERSECTION(ns) F_BIT_VAL(ns, 19)

#define F_PGD_PA_PAGETABLE_MSK F_MSK(31, 10)
#define F_PGD_PA_PAGETABLE_SET(val) F_VAL(val, 31, 10)
#define F_PGD_PA_SECTION_MSK F_MSK(31, 20)
#define F_PGD_PA_SECTION_SET(val) F_VAL(val, 31, 20)
#define F_PGD_PA_SUPERSECTION_MSK F_MSK(31, 24)
#define F_PGD_PA_SUPERSECTION_SET(val) F_VAL(val, 31, 24)

/* pagetable walk */
#define IMU_PGDIR_SHIFT 20
#define IMU_PAGE_SHIFT 12
#define IMU_PTRS_PER_PGD 4096
#define IMU_PTRS_PER_PTE 256
#define IMU_BYTES_PER_PTE (IMU_PTRS_PER_PTE * sizeof(imu_pteval_t))

#define MMU_PT_TYPE_SUPERSECTION (1 << 4)
#define MMU_PT_TYPE_SECTION (1 << 3)
#define MMU_PT_TYPE_LARGE_PAGE (1 << 2)
#define MMU_PT_TYPE_SMALL_PAGE (1 << 1)

#define MMU_SMALL_PAGE_SIZE (SZ_4K)
#define MMU_LARGE_PAGE_SIZE (SZ_64K)
#define MMU_SECTION_SIZE (SZ_1M)
#define MMU_SUPERSECTION_SIZE (SZ_16M)

typedef unsigned int imu_pteval_t;
struct imu_pte_t {imu_pteval_t imu_pte; };
struct imu_pgd_t {imu_pteval_t imu_pgd; };

#define imu_pte_val(x) ((x).imu_pte)
#define imu_pgd_val(x) ((x).imu_pgd)

#define __imu_pte(x) ((struct imu_pte_t){(x)})
#define __imu_pgd(x) ((struct imu_pgd_t){(x)})

#define imu_pte_none(pte) (!imu_pte_val(pte))
#define imu_pte_type(pte) (imu_pte_val(pte) & 0x3)

#define imu_pgd_index(addr) ((addr) >> IMU_PGDIR_SHIFT)
#define imu_pgd_offset(domain, addr) ((domain)->pgd + imu_pgd_index(addr))

#define imu_pte_index(addr)                                                    \
	(((addr) >> IMU_PAGE_SHIFT) & (IMU_PTRS_PER_PTE - 1))
#define imu_pte_offset_map(pgd, addr) (imu_pte_map(pgd) + imu_pte_index(addr))

extern int gM4U_4G_DRAM_Mode;

static inline struct imu_pte_t *imu_pte_map(struct imu_pgd_t *pgd)
{
	imu_pteval_t pte_pa = imu_pgd_val(*pgd);

	if (gM4U_4G_DRAM_Mode) {
		if (pte_pa < 0x40000000)
			return (struct imu_pte_t *)(__va(
				(pte_pa & F_PGD_PA_PAGETABLE_MSK) +
				0x100000000L));
		else
			return (struct imu_pte_t *)(__va(pte_pa &
						  F_PGD_PA_PAGETABLE_MSK));
	} else {
		phys_addr_t pte_pa_new = pte_pa & F_PGD_PA_PAGETABLE_MSK;

		if (pte_pa & F_PGD_BIT32_BIT)
			pte_pa_new |= 0x100000000L;
		if (pte_pa & F_PGD_BIT33_BIT)
			pte_pa_new |= 0x200000000L;
		return (struct imu_pte_t *)(__va(pte_pa_new));
	}
}

static inline int imu_pte_unmap(struct imu_pte_t *pte)
{
	return 0;
}

static inline unsigned int imu_pgd_entry_pa(struct imu_pgd_t pgd)
{
	if (F_PGD_TYPE_IS_PAGE(pgd))
		return imu_pgd_val(pgd) & F_PGD_PA_PAGETABLE_MSK;
	else if (F_PGD_TYPE_IS_SECTION(pgd))
		return imu_pgd_val(pgd) & F_PGD_PA_SECTION_MSK;
	else if (F_PGD_TYPE_IS_SUPERSECTION(pgd))
		return imu_pgd_val(pgd) & F_PGD_PA_SUPERSECTION_MSK;
	else
		return 0;
}

static inline struct imu_pgd_t *imu_supersection_start(struct imu_pgd_t *pgd)
{
	return (struct imu_pgd_t *)(round_down((unsigned long)pgd, (16 * 4)));
}
static inline struct imu_pte_t *imu_largepage_start(struct imu_pte_t *pte)
{
	return (struct imu_pte_t *)(round_down((unsigned long)pte, (16 * 4)));
}

static inline unsigned long long m4u_calc_next_mva(unsigned long long addr,
						   unsigned long long end,
						   unsigned int size)
{
	/* addr + size may equal 0x100000000*/
	unsigned long long __boundary = (addr + (unsigned long long)size) &
					(~((unsigned long long)size - 1));
	unsigned long long min = min_t(unsigned long long, __boundary, end);

	return min;
}

#endif
