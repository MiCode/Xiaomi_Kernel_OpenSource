/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
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
#ifndef _MC_MEM_H_
#define _MC_MEM_H_

#ifdef LPAE_SUPPORT
/*
 * Number of page table entries in one MMU table. This is ARM specific, an
 * MMU table covers 2 MiB by using 512 entries referring to 4KiB pages each.
 */
#define MC_ARM_MMU_TABLE_ENTRIES		512

/* ARM level 3 (MMU) table with 512 entries. Size: 4k */
struct mmutable {
	uint64_t	table_entries[MC_ARM_MMU_TABLE_ENTRIES];
};

/* There is 1 table in each page. */
#define MMU_TABLES_PER_PAGE		1
#else
/*
 * MobiCore specific page tables for world shared memory.
 * Linux uses shadow page tables, see arch/arm/include/asm/pgtable-2level.
 * MobiCore uses the default ARM format.
 *
 * Number of page table entries in one MMU table. This is ARM specific, an
 * MMU table covers 1 MiB by using 256 entries referring to 4KiB pages each.
 */
#define MC_ARM_MMU_TABLE_ENTRIES		256

/* ARM level 2 (MMU) table with 256 entries. Size: 1k */
struct mmutable {
	uint32_t	table_entries[MC_ARM_MMU_TABLE_ENTRIES];
};

/* There are 4 tables in each page. */
#define MMU_TABLES_PER_PAGE		4
#endif

/* Store for four MMU tables in one 4kb page*/
struct mc_mmu_table_store {
	struct mmutable table[MMU_TABLES_PER_PAGE];
};

/* Usage and maintenance information about mc_mmu_table_store */
struct mc_mmu_tables_set {
	struct list_head		list;
	/* kernel virtual address */
	struct mc_mmu_table_store	*kernel_virt;
	/* physical address */
	phys_addr_t			phys;
	/* pointer to page struct */
	struct page			*page;
	/* How many pages from this set are used */
	atomic_t			used_tables;
};

/*
 * MMU table allocated to the Daemon or a TLC describing a world shared
 * buffer.
 * When users map a malloc()ed area into SWd, a MMU table is allocated.
 * In addition, the area of maximum 1MB virtual address space is mapped into
 * the MMU table and a handle for this table is returned to the user.
 */
struct mc_mmu_table {
	struct list_head	list;
	/* Table lock */
	struct mutex		lock;
	/* handle as communicated to user mode */
	unsigned int		handle;
	/* Number of references kept to this MMU table */
	atomic_t		usage;
	/* owner of this MMU table */
	struct mc_instance	*owner;
	/* set describing where our MMU table is stored */
	struct mc_mmu_tables_set	*set;
	/* index into MMU table set */
	unsigned int		idx;
	/* size of buffer */
	unsigned int		pages;
	/* virtual address*/
	void			*virt;
	/* physical address */
	phys_addr_t		phys;
};

/* MobiCore Driver Memory context data. */
struct mc_mem_context {
	struct mc_instance	*daemon_inst;
	/* Backing store for MMU tables */
	struct list_head	mmu_tables_sets;
	/* Bookkeeping for used MMU tables */
	struct list_head	mmu_tables;
	/* Bookkeeping for free MMU tables */
	struct list_head	free_mmu_tables;
	/* semaphore to synchronize access to above lists */
	struct mutex		table_lock;
	atomic_t		table_counter;
};

/*
 * Allocate MMU table and map buffer into it.
 * That is, create respective table entries.
 */
struct mc_mmu_table *mc_alloc_mmu_table(struct mc_instance *instance,
	struct task_struct *task, void *wsm_buffer, unsigned int wsm_len);

/* Delete all the MMU tables associated with an instance */
void mc_clear_mmu_tables(struct mc_instance *instance);

/* Release all orphaned MMU tables */
void mc_clean_mmu_tables(void);

/* Delete a used MMU table. */
int mc_free_mmu_table(struct mc_instance *instance, uint32_t handle);

/*
 * Lock a MMU table - the daemon adds +1 to refcount of the MMU table
 * marking it in use by SWD so it doesn't get released when the TLC dies.
 */
int mc_lock_mmu_table(struct mc_instance *instance, uint32_t handle);

/* Return the phys address of MMU table. */
phys_addr_t mc_find_mmu_table(uint32_t handle, int32_t fd);
/* Release all used MMU tables to Linux memory space */
void mc_release_mmu_tables(void);

/* Initialize all MMU tables structure */
int mc_init_mmu_tables(void);

#endif /* _MC_MEM_H_ */
