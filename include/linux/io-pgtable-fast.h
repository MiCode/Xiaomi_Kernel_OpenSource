/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

typedef u64 av8l_fast_iopte;

#define iopte_pmd_offset(pmds, iova) (pmds + (iova >> 12))

int av8l_fast_map_public(av8l_fast_iopte *ptep, phys_addr_t paddr, size_t size,
			 int prot);
void av8l_fast_unmap_public(av8l_fast_iopte *ptep, size_t size);

#endif /* __LINUX_IO_PGTABLE_FAST_H */
