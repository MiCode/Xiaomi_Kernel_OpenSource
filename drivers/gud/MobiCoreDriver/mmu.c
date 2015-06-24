/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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

#include <asm/pgtable.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/pagemap.h>
#include <linux/device.h>

#include "public/mc_linux.h"

#include "mci/mcimcp.h"

#include "platform.h"	/* CONFIG_TRUSTONIC_TEE_LPAE */
#include "main.h"
#include "debug.h"
#include "mcp.h"	/* mcp_buffer_map */
#include "mmu.h"

#ifdef CONFIG_TRUSTONIC_TEE_LPAE
#define MMU_TYPE_PAGE	(3 << 0)
#define MMU_BUFFERABLE	BIT(2) /* AttrIndx[0] */
#define MMU_CACHEABLE	BIT(3) /* AttrIndx[1] */
#define MMU_NS		BIT(5)
#define MMU_AP_RW_ALL	BIT(6) /* AP[2:1], RW, at any privilege level */
#define MMU_EXT_SHARED	(3 << 8) /* SH[1:0], inner shareable */
#define MMU_EXT_AF	BIT(10) /* Access Flag */
#define MMU_EXT_NG	BIT(11)
#define MMU_EXT_XN      (((uint64_t)1) << 54) /* XN */
#else
#define MMU_TYPE_EXT	(3 << 0)	/* v5 */
#define MMU_TYPE_SMALL	(2 << 0)
#define MMU_BUFFERABLE	BIT(2)
#define MMU_CACHEABLE	BIT(3)
#define MMU_EXT_AP0	BIT(4)
#define MMU_EXT_AP1	(2 << 4)
#define MMU_EXT_TEX(x)	((x) << 6)	/* v5 */
#define MMU_EXT_SHARED	BIT(10)	/* v6 */
#define MMU_EXT_NG	BIT(11)	/* v6 */
#endif

/*
 * MobiCore specific page tables for world shared memory.
 * Linux uses shadow page tables, see arch/arm/include/asm/pgtable-2level.
 * MobiCore uses the default ARM format.
 *
 * Number of page table entries in one L2 MMU table. This is ARM specific, an
 * MMU table covers 1 MiB by using 256 entries referring to 4KiB pages each.
 */
#define L2_ENTRIES_MAX	256

/*
 * Small buffers (below 1MiB) are mapped using the legacy L2 table, but bigger
 * buffers now use a fake L1 table that holds 64-bit pointers to L2 tables. As
 * this must be exactly one page, we can hold up to 512 entries.
 */
#define L1_ENTRIES_MAX	512

#ifdef CONFIG_TRUSTONIC_TEE_LPAE

/*
 * Secure world uses 64-bit physical addresses
 */
typedef u64 tbase_pte_t;

/*
 * Linux uses different mappings for SMP systems(the sharing flag is set for
 * the pte. In order not to confuse things too much in Mobicore make sure the
 * shared buffers have the same flags.  This should also be done in SWD side.
 */
static tbase_pte_t pte_flags = MMU_BUFFERABLE | MMU_CACHEABLE | MMU_EXT_NG |
#ifdef CONFIG_SMP
			       MMU_EXT_SHARED |
#endif /* CONFIG_SMP */
			       MMU_EXT_XN | MMU_EXT_AF | MMU_AP_RW_ALL |
			       MMU_NS | MMU_TYPE_PAGE;

#else /* CONFIG_TRUSTONIC_TEE_LPAE */

/*
 * Secure world uses 32-bit physical addresses
 */
typedef u32 tbase_pte_t;

/*
 * Linux uses different mappings for SMP systems(the sharing flag is set for
 * the pte. In order not to confuse things too much in Mobicore make sure the
 * shared buffers have the same flags.  This should also be done in SWD side.
 */
static tbase_pte_t pte_flags = MMU_BUFFERABLE | MMU_CACHEABLE | MMU_EXT_NG |
#ifdef CONFIG_SMP
			       MMU_EXT_SHARED | MMU_EXT_TEX(1) |
#endif /* CONFIG_SMP */
			       MMU_EXT_AP1 | MMU_EXT_AP0 |
			       MMU_TYPE_SMALL | MMU_TYPE_EXT;

#endif /* !CONFIG_TRUSTONIC_TEE_LPAE */

/*
 * Fake L1 MMU table.
 */
union l1_table {
	u64		*pages_phys;	/* Array of physical page addresses */
	unsigned long	page;
};

/*
 * L2 MMU table, which is more a L3 table in the LPAE case.
 */
union l2_table {
	tbase_pte_t	*ptes;		/* Array of PTEs */
	unsigned long	page;
};

/*
 * MMU table allocated to the Daemon or a TLC describing a world shared
 * buffer.
 * When users map a malloc()ed area into SWd, a MMU table is allocated.
 * In addition, the area of maximum 1MB virtual address space is mapped into
 * the MMU table and a handle for this table is returned to the user.
 */
struct tbase_mmu {
	union l2_table	l2_tables[L1_ENTRIES_MAX];	/* L2 tables */
	size_t		l2_tables_nr;	/* Actual number of L2 tables */
	union l1_table	l1_table;	/* Fake L1 table */
	union l2_table	l1_l2_table;	/* L2 table for the L1 table */
	uint32_t	offset;
	uint32_t	length;
	bool		user;		/* Pages are from user space */
};

static void free_all_pages(struct tbase_mmu *mmu_table)
{
	union l2_table *l2_table = &mmu_table->l2_tables[0];
	size_t i;

	for (i = 0; i < mmu_table->l2_tables_nr; i++, l2_table++) {
		if (!l2_table->page)
			break;

		free_page(l2_table->page);
	}

	if (mmu_table->l1_l2_table.page)
		free_page(mmu_table->l1_l2_table.page);

	if (mmu_table->l1_table.page)
		free_page(mmu_table->l1_table.page);
}

/*
 * Create a MMU table for a buffer or trustlet.
 */
static inline int map_buffer(struct task_struct *task, const void *data,
			     unsigned int length, struct tbase_mmu *mmu_table)
{
	const void      *reader = (const void *)((uintptr_t)data & PAGE_MASK);
	struct page	**pages;	/* Same as above, conveniently typed */
	unsigned long	pages_page;	/* Page to contain the page pointers */
	size_t		chunk;
	unsigned long	total_pages_nr;
	int		l1_entries_max;
	int		ret = 0;

	/* Check that we have enough space to map data */
	mmu_table->length = length;
	mmu_table->offset = (uintptr_t)data & ~PAGE_MASK;
	total_pages_nr = PAGE_ALIGN(mmu_table->offset + length) / PAGE_SIZE;
	if (g_ctx.f_mem_ext)
		l1_entries_max = L1_ENTRIES_MAX;
	 else
		l1_entries_max = 1;

	if (total_pages_nr > (l1_entries_max * L2_ENTRIES_MAX)) {
		dev_err(g_ctx.mcd, "data mapping exceeds %d pages",
			l1_entries_max * L2_ENTRIES_MAX);
		return -EINVAL;
	}

	/* Get number of L2 tables needed */
	mmu_table->l2_tables_nr = (total_pages_nr + L2_ENTRIES_MAX - 1) /
				  L2_ENTRIES_MAX;
	dev_dbg(g_ctx.mcd, "total_pages_nr %lu l2_tables_nr %zu",
		total_pages_nr, mmu_table->l2_tables_nr);

	/* Get a page to store page pointers */
	pages_page = get_zeroed_page(GFP_KERNEL);
	if (!pages_page)
		return -ENOMEM;

	pages = (struct page **)pages_page;

	/* Allocate a page for the L1 table */
	if (mmu_table->l2_tables_nr > 1) {
		tbase_pte_t *pte;

		mmu_table->l1_table.page = get_zeroed_page(GFP_KERNEL);
		mmu_table->l1_l2_table.page = get_zeroed_page(GFP_KERNEL);
		if (!mmu_table->l1_table.page || !mmu_table->l1_l2_table.page) {
			ret = -ENOMEM;
			goto end;
		}

		/* Map it */
		pte = &mmu_table->l1_l2_table.ptes[0];
		*pte = virt_to_phys(mmu_table->l1_table.pages_phys);
		*pte |= pte_flags;
	}

	for (chunk = 0; chunk < mmu_table->l2_tables_nr; chunk++) {
		unsigned long pages_nr, i;
		tbase_pte_t *pte;
		struct page **page_ptr;

		/* Size to map for this chunk */
		if (chunk == (mmu_table->l2_tables_nr - 1))
			pages_nr = ((total_pages_nr - 1) % L2_ENTRIES_MAX) + 1;
		else
			pages_nr = L2_ENTRIES_MAX;

		/* Allocate a page for the MMU descriptor */
		mmu_table->l2_tables[chunk].page = get_zeroed_page(GFP_KERNEL);
		if (!mmu_table->l2_tables[chunk].page) {
			ret = -ENOMEM;
			goto end;
		}

		/* Add page address to L1 table if needed */
		if (mmu_table->l1_table.page)
			mmu_table->l1_table.pages_phys[chunk] =
				virt_to_phys(mmu_table->l2_tables[chunk].ptes);

		/* Get pages */
		if (task) {
			long gup_ret;

			/* Buffer was allocated in user space */
			down_read(&task->mm->mmap_sem);
			gup_ret = get_user_pages(task, task->mm,
						 (uintptr_t)reader, pages_nr,
						 1, 0, pages, 0);
			reader += pages_nr * PAGE_SIZE;
			up_read(&task->mm->mmap_sem);
			if (gup_ret < 0) {
				ret = gup_ret;
				dev_err(g_ctx.mcd,
					"failed to get user pages: %d", ret);
				goto end;
			}

			/* check if we could lock all pages. */
			if (gup_ret != pages_nr) {
				dev_err(g_ctx.mcd,
					"get_user_pages() failed, ret: %ld",
					gup_ret);
				release_pages(pages, gup_ret, 0);
				ret = -ENOMEM;
				goto end;
			}

			mmu_table->user = true;
		} else if (is_vmalloc_addr(data)) {
			/* Buffer vmalloc'ed in kernel space */
			page_ptr = &pages[0];
			for (i = 0; i < pages_nr; i++) {
				struct page *page = vmalloc_to_page(reader);

				if (!page) {
					dev_err(g_ctx.mcd,
						"failed to map address");
					ret = -EINVAL;
					goto end;
				}

				*page_ptr++ = page;
				reader += PAGE_SIZE;
			}
		} else {
			/* Buffer kmalloc'ed in kernel space */
			struct page *page = virt_to_page(reader);

			reader += pages_nr * PAGE_SIZE;
			page_ptr = &pages[0];
			for (i = 0; i < pages_nr; i++)
				*page_ptr++ = page++;
		}

		/* Create MMU Table entries */
		page_ptr = &pages[0];
		pte = &mmu_table->l2_tables[chunk].ptes[0];
		for (i = 0; i < pages_nr; i++, page_ptr++, pte++) {
			/*
			* Create MMU table entry, see ARM MMU docu for details
			* about flags stored in the lowest 12 bits.  As a side
			* reference, the Article "ARM's multiply-mapped memory
			* mess" found in the collection at
			* http://lwn.net/Articles/409032/ is also worth reading.
			*/
			unsigned long phys = page_to_phys(*page_ptr);
#if defined CONFIG_ARM64 && !defined CONFIG_TRUSTONIC_TEE_LPAE
			if (phys & 0xffffffff00000000) {
				dev_err(g_ctx.mcd,
					"Pointer too big for non-LPAE: 0x%16lx",
					phys);
				ret = -EFAULT;
				goto end;
			}
#endif
			*pte = phys;
			*pte |= pte_flags;
		}
	}

end:
	if (ret)
		free_all_pages(mmu_table);

	free_page(pages_page);
	return ret;
}

static inline void unmap_buffer(struct tbase_mmu *mmu_table)
{
	int t;

	dev_dbg(g_ctx.mcd, "clear MMU table, virt %p", mmu_table);
	if (!mmu_table->user)
		goto end;

	/* Release all locked user space pages */
	for (t = 0; t < mmu_table->l2_tables_nr; t++) {
		tbase_pte_t *pte = mmu_table->l2_tables[t].ptes;
		int i;

		for (i = 0; i < L2_ENTRIES_MAX; i++, pte++) {
			struct page *page;

			/* If not all entries are used, unused ones are 0 */
			if (!*pte)
				break;

			/* pte_page() cannot return NULL */
			page = pte_page(*pte);
			dev_dbg(g_ctx.mcd, "MMU entry %d: 0x%llx, virt %p",
				i, (u64)*pte, page);

			page_cache_release(page);
		}
	}

end:
	free_all_pages(mmu_table);
}

/*
 * Delete a MMU table.
 */
void tbase_mmu_delete(struct tbase_mmu *mmu)
{
	if (WARN(!mmu, "NULL mmu pointer given"))
		return;

	unmap_buffer(mmu);
	MCDRV_DBG("freed mmu %p: %s len %u off %u table %lx type L%d",
		  mmu, mmu->user ? "user" : "kernel", mmu->length, mmu->offset,
		  (uintptr_t)(mmu->l1_table.page ? mmu->l1_l2_table.ptes :
						   mmu->l2_tables[0].ptes),
		  mmu->l1_table.page ? 1 : 2);
	kfree(mmu);
}

/*
 * Allocate MMU table and map buffer into it.
 * That is, create respective table entries.
 */
struct tbase_mmu *tbase_mmu_create(struct task_struct *task,
				   const void *addr,
				   unsigned int length)
{
	struct tbase_mmu *mmu;
	int ret;

	/* Check input arguments */
	if (WARN(!addr, "data address is NULL"))
		return ERR_PTR(-EINVAL);

	if (WARN(!length, "data length is 0"))
		return ERR_PTR(-EINVAL);

	/* Allocate the struct */
	mmu = kmalloc(sizeof(*mmu), GFP_KERNEL | __GFP_ZERO);
	if (!mmu)
		return ERR_PTR(-ENOMEM);

	/* Create the MMU mapping for the data */
	ret = map_buffer(task, addr, length, mmu);
	if (ret) {
		kfree(mmu);
		return ERR_PTR(ret);
	}

	MCDRV_DBG("created mmu %p: %s addr %p len %u off %u table %lx type L%d",
		  mmu, mmu->user ? "user" : "kernel", addr, mmu->length,
		  mmu->offset,
		  (uintptr_t)(mmu->l1_table.page ? mmu->l1_l2_table.ptes :
						   mmu->l2_tables[0].ptes),
		  mmu->l1_table.page ? 1 : 2);
	return mmu;
}

void tbase_mmu_buffer(const struct tbase_mmu *mmu, struct mcp_buffer_map *map)
{
	if (mmu->l1_table.page) {
		map->phys_addr = virt_to_phys(mmu->l1_l2_table.ptes);
		map->type = WSM_L1;
	} else {
		map->phys_addr = virt_to_phys(mmu->l2_tables[0].ptes);
		map->type = WSM_L2;
	}

	map->secure_va = 0;
	map->offset = mmu->offset;
	map->length = mmu->length;
}

int tbase_mmu_info(const struct tbase_mmu *mmu, struct kasnprintf_buf *buf)
{
	return kasnprintf(buf,
			  "\t\t\tmmu %p: %s len %u off %u table %lx type L%d\n",
			  mmu, mmu->user ? "user" : "kernel", mmu->length,
			  mmu->offset,
			  (uintptr_t)(mmu->l1_table.page ?
				mmu->l1_l2_table.ptes : mmu->l2_tables[0].ptes),
			  mmu->l1_table.page ? 1 : 2);
}
