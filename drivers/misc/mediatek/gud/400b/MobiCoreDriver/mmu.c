/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
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
#include <linux/version.h>

#include "public/mc_user.h"

#include "mci/mcimcp.h"

#include "platform.h"	/* CONFIG_TRUSTONIC_TEE_LPAE */
#include "main.h"
#include "mcp.h"	/* mcp_buffer_map */
#include "mmu.h"

/* Common */
#define MMU_BUFFERABLE		BIT(2)		/* AttrIndx[0] */
#define MMU_CACHEABLE		BIT(3)		/* AttrIndx[1] */
#define MMU_EXT_NG		BIT(11)		/* ARMv6 and higher */

/* LPAE */
#define MMU_TYPE_PAGE		(3 << 0)
#define MMU_NS			BIT(5)
#define MMU_AP_RW_ALL		BIT(6) /* AP[2:1], RW, at any privilege level */
#define	MMU_AP2_RO		BIT(7)
#define MMU_EXT_SHARED_64	(3 << 8)	/* SH[1:0], inner shareable */
#define MMU_EXT_AF		BIT(10)		/* Access Flag */
#define MMU_EXT_XN		(((u64)1) << 54) /* XN */

/* Non-LPAE */
#define MMU_TYPE_EXT		(3 << 0)	/* v5 */
#define MMU_TYPE_SMALL		(2 << 0)
#define MMU_EXT_AP0		BIT(4)
#define MMU_EXT_AP1		(2 << 4)
#define MMU_EXT_AP2		BIT(9)
#define MMU_EXT_TEX(x)		((x) << 6)	/* v5 */
#define MMU_EXT_SHARED_32	BIT(10)		/* ARMv6 and higher */

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

#if KERNEL_VERSION(4, 6, 0) > LINUX_VERSION_CODE
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	return get_user_pages(NULL, mm, start, nr_pages, write, 0, pages, NULL);
}
#elif KERNEL_VERSION(4, 9, 0) > LINUX_VERSION_CODE
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int flags = 0;

	if (write)
		flags |= FOLL_WRITE;

	return get_user_pages_remote(NULL, mm, start, nr_pages, write, 0, pages,
				     NULL);
}
#elif KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int flags = 0;

	if (write)
		flags |= FOLL_WRITE;

	return get_user_pages_remote(NULL, mm, start, nr_pages, flags, pages,
				     NULL);
}
#else
static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int flags = 0;

	if (write)
		flags |= FOLL_WRITE;

	return get_user_pages_remote(NULL, mm, start, nr_pages, flags, pages,
				     NULL, NULL);
}
#endif

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
	union {				/* Array of PTEs */
		u32	*ptes_32;
		u64	*ptes_64;
	};
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
	union l2_table	l2_tables[L1_ENTRIES_MAX];	/* L2 tables */
	size_t		l2_tables_nr;	/* Actual number of L2 tables */
	union l1_table	l1_table;	/* Fake L1 table */
	union l2_table	l1_l2_table;	/* L2 table for the L1 table */
	u32		offset;
	u32		length;
	bool		user;		/* Pages are from user space */
	int		pages_created;	/* Leak check */
	int		pages_locked;	/* Leak check */
};

/*
 * Linux uses different mappings for SMP systems(the sharing flag is set for the
 * pte. In order not to confuse things too much in Mobicore make sure the shared
 * buffers have the same flags.  This should also be done in SWD side.
 */

static u64 pte_flags_64 = MMU_BUFFERABLE | MMU_CACHEABLE | MMU_EXT_NG |
#ifdef CONFIG_SMP
			  MMU_EXT_SHARED_64 |
#endif /* CONFIG_SMP */
			  MMU_EXT_XN | MMU_EXT_AF | MMU_AP_RW_ALL |
			  MMU_NS | MMU_TYPE_PAGE;

static u32 pte_flags_32 = MMU_BUFFERABLE | MMU_CACHEABLE | MMU_EXT_NG |
#ifdef CONFIG_SMP
			  MMU_EXT_SHARED_32 | MMU_EXT_TEX(1) |
#endif /* CONFIG_SMP */
			  MMU_EXT_AP1 | MMU_EXT_AP0 |
			  MMU_TYPE_SMALL | MMU_TYPE_EXT;

static inline u32 get_pte_flags_32(bool is_writable)
{
	return is_writable ? pte_flags_32 : pte_flags_32 | MMU_EXT_AP2;
}

static inline u64 get_pte_flags_64(bool is_writable)
{
	return is_writable ? pte_flags_64 : pte_flags_64 | MMU_AP2_RO;
}

static uintptr_t mmu_table_pointer(const struct tee_mmu *mmu)
{
	if (mmu->l1_table.page) {
		return g_ctx.f_lpae ?
			(uintptr_t)mmu->l1_l2_table.ptes_64 :
			(uintptr_t)mmu->l1_l2_table.ptes_32;
	} else {
		return g_ctx.f_lpae ?
			(uintptr_t)mmu->l2_tables[0].ptes_64 :
			(uintptr_t)mmu->l2_tables[0].ptes_32;
	}
}

static void mmu_release(struct tee_mmu *mmu)
{
	size_t t;

	/* Release all locked user space pages */
	for (t = 0; t < mmu->l2_tables_nr; t++) {
		union l2_table *l2_table = &mmu->l2_tables[t];

		if (!l2_table->page)
			break;

		if (mmu->user) {
			u64 *pte64 = l2_table->ptes_64;
			u32 *pte32 = l2_table->ptes_32;
			pte_t pte;
			int i;

			for (i = 0; i < L2_ENTRIES_MAX; i++) {
#if (KERNEL_VERSION(4, 7, 0) > LINUX_VERSION_CODE) || defined(CONFIG_ARM)
				{
					if (g_ctx.f_lpae)
						pte = *pte64++;
					else
						pte = *pte32++;
				}

				/* Unused entries are 0 */
				if (!pte)
					break;
#else
				{
					if (g_ctx.f_lpae)
						pte.pte = *pte64++;
					else
						pte.pte = *pte32++;
				}

				/* Unused entries are 0 */
				if (!pte.pte)
					break;
#endif

				/* pte_page() cannot return NULL */
				put_page(pte_page(pte));
				mmu->pages_locked--;
			}
		}

		free_page(l2_table->page);
		mmu->pages_created--;
	}

	if (mmu->l1_l2_table.page) {
		free_page(mmu->l1_l2_table.page);
		mmu->pages_created--;
	}

	if (mmu->l1_table.page) {
		free_page(mmu->l1_table.page);
		mmu->pages_created--;
	}

	if (mmu->pages_created || mmu->pages_locked)
		mc_dev_notice("leak detected: still in use %d, still locked %d",
			   mmu->pages_created, mmu->pages_locked);

	kfree(mmu);

	/* Decrement debug counter */
	atomic_dec(&g_ctx.c_mmus);
}

/*
 * Allocate MMU table and map buffer into it.
 * That is, create respective table entries.
 */
struct tee_mmu *tee_mmu_create(struct mm_struct *mm,
			       const struct mc_ioctl_buffer *buf)
{
	struct tee_mmu	*mmu;
	const void	*data = (const void *)(uintptr_t)buf->va;
	const void	*reader = (const void *)((uintptr_t)data & PAGE_MASK);
	struct page	**pages;	/* Same as above, conveniently typed */
	unsigned long	pages_page = 0;	/* Page to contain the page pointers */
	size_t		chunk;
	unsigned long	total_pages_nr;
	int		l1_entries_max;
	int		ret = 0;
	int		write = (buf->flags & MC_IO_MAP_OUTPUT) != 0;

	/* Check input arguments */
	if (!buf->va)
		return ERR_PTR(-EINVAL);

	/* Allocate the struct */
	mmu = kzalloc(sizeof(*mmu), GFP_KERNEL);
	if (!mmu)
		return ERR_PTR(-ENOMEM);

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_mmus);

	/* Check that we have enough space to map data */
	mmu->length = buf->len;
	mmu->offset = (u32)(buf->va & ~PAGE_MASK);
	total_pages_nr = PAGE_ALIGN(mmu->offset + buf->len) / PAGE_SIZE;
	if (g_ctx.f_mem_ext)
		l1_entries_max = L1_ENTRIES_MAX;
	else
		l1_entries_max = 1;

	if (total_pages_nr > (l1_entries_max * L2_ENTRIES_MAX)) {
		mc_dev_notice("data mapping exceeds %d pages",
			   l1_entries_max * L2_ENTRIES_MAX);
		ret = -EINVAL;
		goto end;
	}

	/* Get number of L2 tables needed */
	mmu->l2_tables_nr = (total_pages_nr + L2_ENTRIES_MAX - 1) /
			    L2_ENTRIES_MAX;
	mc_dev_devel("total_pages_nr %lu l2_tables_nr %zu",
		     total_pages_nr, mmu->l2_tables_nr);

	/* Get a page to store page pointers */
	pages_page = get_zeroed_page(GFP_KERNEL);
	if (!pages_page) {
		ret = -ENOMEM;
		goto end;
	}
	mmu->pages_created++;

	pages = (struct page **)pages_page;

	/* Allocate a page for the L1 table */
	if (mmu->l2_tables_nr > 1) {
		mmu->l1_table.page = get_zeroed_page(GFP_KERNEL);
		if (!mmu->l1_table.page) {
			ret = -ENOMEM;
			goto end;
		}
		mmu->pages_created++;

		mmu->l1_l2_table.page = get_zeroed_page(GFP_KERNEL);
		if (!mmu->l1_l2_table.page) {
			ret = -ENOMEM;
			goto end;
		}
		mmu->pages_created++;

		/* Map it */
		if (g_ctx.f_lpae) {
			u64 *pte;

			pte = &mmu->l1_l2_table.ptes_64[0];
			*pte = virt_to_phys(mmu->l1_table.pages_phys);
			*pte |= get_pte_flags_64(write);
		} else {
			u32 *pte;

			pte = &mmu->l1_l2_table.ptes_32[0];
			*pte = virt_to_phys(mmu->l1_table.pages_phys);
			*pte |= get_pte_flags_32(write);
		}
	}

	for (chunk = 0; chunk < mmu->l2_tables_nr; chunk++) {
		unsigned long pages_nr, i;
		struct page **page_ptr;

		/* Size to map for this chunk */
		if (chunk == (mmu->l2_tables_nr - 1))
			pages_nr = ((total_pages_nr - 1) % L2_ENTRIES_MAX) + 1;
		else
			pages_nr = L2_ENTRIES_MAX;

		/* Allocate a page for the MMU descriptor */
		mmu->l2_tables[chunk].page = get_zeroed_page(GFP_KERNEL);
		if (!mmu->l2_tables[chunk].page) {
			ret = -ENOMEM;
			goto end;
		}
		mmu->pages_created++;

		/* Add page address to L1 table if needed */
		if (mmu->l1_table.page) {
			void *table;

			if (g_ctx.f_lpae)
				table = mmu->l2_tables[chunk].ptes_64;
			else
				table = mmu->l2_tables[chunk].ptes_32;

			mmu->l1_table.pages_phys[chunk] = virt_to_phys(table);
		}

		/* Get pages */
		if (mm) {
			long gup_ret;

			/* Buffer was allocated in user space */
			down_read(&mm->mmap_sem);
			gup_ret = gup_local(mm, (uintptr_t)reader,
					    pages_nr, 1, pages);
			if ((gup_ret == -EFAULT) && !write) {
				gup_ret = gup_local(mm, (uintptr_t)reader,
						    pages_nr, 0, pages);
			}
			up_read(&mm->mmap_sem);
			if (gup_ret < 0) {
				ret = gup_ret;
				mc_dev_notice("failed to get user pages @%p: %d",
					   reader, ret);
				goto end;
			}

			/* check if we could lock all pages. */
			if (gup_ret != pages_nr) {
				mc_dev_notice("failed to get user pages: %ld",
					   gup_ret);
				release_pages(pages, gup_ret, 0);
				ret = -EINVAL;
				goto end;
			}

			reader += pages_nr * PAGE_SIZE;
			mmu->user = true;
			mmu->pages_locked += pages_nr;
		} else if (is_vmalloc_addr(data)) {
			/* Buffer vmalloc'ed in kernel space */
			page_ptr = &pages[0];
			for (i = 0; i < pages_nr; i++) {
				struct page *page = vmalloc_to_page(reader);

				if (!page) {
					mc_dev_notice("failed to map address");
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

		/*
		 * Create MMU table entry, see ARM MMU docu for details about
		 * flags stored in the lowest 12 bits.  As a side reference, the
		 * Article "ARM's multiply-mapped memory mess" found in the
		 * collection at http://lwn.net/Articles/409032/ is also worth
		 * reading.
		 */
		if (g_ctx.f_lpae) {
			u64 *pte = &mmu->l2_tables[chunk].ptes_64[0];

			for (i = 0; i < pages_nr; i++, page_ptr++, pte++) {
				*pte = page_to_phys(*page_ptr);
				*pte |= get_pte_flags_64(write);
			}
		} else {
			u32 *pte = &mmu->l2_tables[chunk].ptes_32[0];

			for (i = 0; i < pages_nr; i++, page_ptr++, pte++) {
				unsigned long phys = page_to_phys(*page_ptr);
#if defined CONFIG_ARM64
				if (phys & 0xffffffff00000000UL) {
					mc_dev_notice("64-bit pointer: 0x%16lx",
						   phys);
					ret = -EFAULT;
					goto end;
				}
#endif
				*pte = (u32)phys;
				*pte |= get_pte_flags_32(write);
			}
		}
	}

end:
	if (pages_page) {
		free_page(pages_page);
		mmu->pages_created--;
	}

	if (ret) {
		mmu_release(mmu);
		return ERR_PTR(ret);
	}

	mc_dev_devel("created mmu %p: %s va %llx len %u off %u L%d table %lx",
		     mmu, mmu->user ? "user" : "kernel", buf->va, mmu->length,
		     mmu->offset, mmu->l1_table.page ? 1 : 2,
		     mmu_table_pointer(mmu));
	return mmu;
}

void tee_mmu_delete(struct tee_mmu *mmu)
{
	if (!mmu)
		return;

	mc_dev_devel("free mmu %p: %s len %u off %u L%d table %lx",
		     mmu, mmu->user ? "user" : "kernel", mmu->length,
		     mmu->offset, mmu->l1_table.page ? 1 : 2,
		     mmu_table_pointer(mmu));
	mmu_release(mmu);
}

bool client_mmu_matches(const struct tee_mmu *left,
			const struct tee_mmu *right)
{
	const void *left_page = left->l2_tables[0].ptes_32;
	const void *right_page = right->l2_tables[0].ptes_32;
	bool ret;

	/* L1 not supported */
	if (left->l1_table.page || right->l1_table.page)
		return false;

	/* Only need to compare contents of L2 page */
	ret = !memcmp(left_page, right_page, PAGE_SIZE);
	mc_dev_devel("MMU tables virt %p and %p %smatch", left, right,
		     ret ? "" : "do not ");
	return ret;
}

void tee_mmu_buffer(const struct tee_mmu *mmu, struct mcp_buffer_map *map)
{
	uintptr_t table = mmu_table_pointer(mmu);

	map->phys_addr = virt_to_phys((void *)table);
	map->secure_va = 0;
	map->offset = mmu->offset;
	map->length = mmu->length;
	map->flags = MC_IO_MAP_INPUT | MC_IO_MAP_OUTPUT;
	if (mmu->l1_table.page)
		map->type = WSM_L1;
	else
		map->type = WSM_L2;
}

int tee_mmu_debug_structs(struct kasnprintf_buf *buf, const struct tee_mmu *mmu)
{
	return kasnprintf(buf,
			  "\t\t\tmmu %p: %s len %u off %u table %lx type L%d\n",
			  mmu, mmu->user ? "user" : "kernel", mmu->length,
			  mmu->offset, mmu_table_pointer(mmu),
			  mmu->l1_table.page ? 1 : 2);
}
