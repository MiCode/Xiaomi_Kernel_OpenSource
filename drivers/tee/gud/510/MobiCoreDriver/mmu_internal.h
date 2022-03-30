/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2020 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MC_MMU_INTERNAL_H
#define MC_MMU_INTERNAL_H

#include <linux/kref.h>

/*
 * This represents the maximum number of entries in a Page Table Entries
 * array which maps one 4KiB page. Each entry is 64 bits long physical
 * address with some possible flags. With 512 entries it is possible
 * to map 2MiB memory block.
 */
#define PTE_ENTRIES_MAX	512

/*
 * This represents the maximum number of entries in a Page Middle Directory
 * which maps one 4KiB page. Each entry is a 64 bits physical address that
 * points to a PTE. With 512 entries t is possible to map 1GB memory block.
 */
#define PMD_ENTRIES_MAX	512

struct tee_deleter {
	void *object;
	void (*delete)(void *object);
};

/*
 * A table that could be either a pmd or pte
 */
union mmu_table {
	u64		*entries;	/* Array of PTEs */
	/* Array of pages */
	struct page	**pages;
	/* Array of VAs */
	uintptr_t	*vas;
	/* Address of table */
	void		*addr;
	/* Page for table */
	unsigned long	page;
};

/*
 * MMU table allocated to the Daemon or a TLC describing a world shared
 * buffer.
 * When users map a malloc()ed area into SWd, a MMU table is allocated.
 * In addition, the area of maximum 1MB virtual address space is mapped into
 * the MMU table and a handle for this table is returned to the user.
 */
struct tee_mmu {
	struct kref			kref;
	/* Array of pages that hold buffer ptes*/
	union mmu_table			pte_tables[PMD_ENTRIES_MAX];
	/* Actual number of ptes tables */
	size_t				nr_pmd_entries;
	/* Contains phys @ of ptes tables */
	union mmu_table			pmd_table;
	struct tee_deleter		*deleter;	/* Xen map to free */
	unsigned long			nr_pages;
	int				pages_created;	/* Leak check */
	int				pages_locked;	/* Leak check */
	u32				offset;
	u32				length;
	u32				flags;
	/* Pages are from user space */
	int				user;
	/* Some front-ends need access to pages */
	int				use_pages_and_vas;
	/* ION case only */
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attach;
	struct sg_table			*sgt;
};

/*
 * Allocate MMU table and initialize it
 */
struct tee_mmu *tee_mmu_create_and_init(void);

/*
 * Puts a reference on a MMU table.
 */
void tee_mmu_put(struct tee_mmu *mmu);

/*
 * Fill in buffer info for MMU table.
 */
void tee_mmu_buffer(struct tee_mmu *mmu, struct mcp_buffer_map *map);

/*
 * Give the MMU an object to release when released
 */
void tee_mmu_set_deleter(struct tee_mmu *mmu, struct tee_deleter *deleter);

/*
 * Allocate MMU table and map pages into it.
 * This is for Xen Dom0 to re-create a buffer with existing pages.
 */
struct tee_mmu *tee_mmu_wrap(struct tee_deleter *deleter, struct page **pages,
			     unsigned long nr_pages,
			     const struct mcp_buffer_map *buf);

#endif /* MC_MMU_INTERNAL_H */
