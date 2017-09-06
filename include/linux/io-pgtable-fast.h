/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef __LINUX_IO_PGTABLE_FAST_H
#define __LINUX_IO_PGTABLE_FAST_H

#include <linux/notifier.h>

typedef u64 av8l_fast_iopte;

#define iopte_pmd_offset(pmds, iova) (pmds + (iova >> 12))

int av8l_fast_map_public(av8l_fast_iopte *ptep, phys_addr_t paddr, size_t size,
			 int prot);
void av8l_fast_unmap_public(av8l_fast_iopte *ptep, size_t size);

/* events for notifiers passed to av8l_register_notify */
#define MAPPED_OVER_STALE_TLB 1


#ifdef CONFIG_IOMMU_IO_PGTABLE_FAST_PROVE_TLB
/*
 * Doesn't matter what we use as long as bit 0 is unset.  The reason why we
 * need a different value at all is that there are certain hardware
 * platforms with erratum that require that a PTE actually be zero'd out
 * and not just have its valid bit unset.
 */
#define AV8L_FAST_PTE_UNMAPPED_NEED_TLBI 0xa

void av8l_fast_clear_stale_ptes(av8l_fast_iopte *puds, bool skip_sync);
void av8l_register_notify(struct notifier_block *nb);

#else  /* !CONFIG_IOMMU_IO_PGTABLE_FAST_PROVE_TLB */

#define AV8L_FAST_PTE_UNMAPPED_NEED_TLBI 0

static inline void av8l_fast_clear_stale_ptes(av8l_fast_iopte *puds,
					      bool skip_sync)
{
}

static inline void av8l_register_notify(struct notifier_block *nb)
{
}

#endif	/* CONFIG_IOMMU_IO_PGTABLE_FAST_PROVE_TLB */

#endif /* __LINUX_IO_PGTABLE_FAST_H */
