// SPDX-License-Identifier: GPL-2.0
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
#include <linux/dma-buf.h>
#ifdef CONFIG_DMA_SHARED_BUFFER
#include <ion.h>
#ifdef CONFIG_MTK_ION
/* for mtk_ion, struct ion_buffer is decleared here */
#include <ion_priv.h>
#endif
#endif

#ifdef CONFIG_XEN
/* To get the MFN */
#include <linux/pfn.h>
#include <xen/page.h>
#endif

#include "public/mc_user.h"

#include "mci/mcimcp.h"

#include "main.h"
#include "mcp.h"	/* mcp_buffer_map */
#include "protocol.h"	/* protocol_fe_uses_pages_and_vas */
#include "mmu.h"

#define PHYS_48BIT_MASK (BIT(48) - 1)

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

/* ION */
/* Trustonic Specific flag to detect ION mem */
#define MMU_ION_BUF		BIT(24)

static inline long gup_local(struct mm_struct *mm, uintptr_t start,
			     unsigned long nr_pages, int write,
			     struct page **pages)
{
	unsigned int gup_flags = 0;

	if (write)
		gup_flags |= FOLL_WRITE;

	return get_user_pages_remote(NULL, mm, start, nr_pages, gup_flags,
				    pages, NULL, NULL);
}

static inline long gup_local_repeat(struct mm_struct *mm, uintptr_t start,
				    unsigned long nr_pages, int write,
				    struct page **pages)
{
	int retries = 10;
	long ret = 0;

	while (retries--) {
		ret = gup_local(mm, start, nr_pages, write, pages);

		if (-EBUSY != ret)
			break;
	}

	return ret;
}

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
	bool				user;
	bool				use_pages_and_vas;
	/* ION case only */
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attach;
	struct sg_table			*sgt;
};

static void tee_mmu_delete(struct tee_mmu *mmu)
{
	unsigned long chunk, nr_pages_left = mmu->nr_pages;

#ifdef CONFIG_DMA_SHARED_BUFFER
	if (mmu->dma_buf) {
		dma_buf_unmap_attachment(mmu->attach, mmu->sgt,
					 DMA_BIDIRECTIONAL);
		dma_buf_detach(mmu->dma_buf, mmu->attach);
		dma_buf_put(mmu->dma_buf);
	}
#endif

	/* Release all locked user space pages */
	for (chunk = 0; chunk < mmu->nr_pmd_entries; chunk++) {
		union mmu_table *pte_table = &mmu->pte_tables[chunk];
		unsigned long nr_pages = nr_pages_left;

		if (nr_pages > PTE_ENTRIES_MAX)
			nr_pages = PTE_ENTRIES_MAX;

		nr_pages_left -= nr_pages;

		if (!pte_table->page)
			break;

		if (mmu->user && mmu->use_pages_and_vas) {
			struct page **page = pte_table->pages;
			int i;

			for (i = 0; i < nr_pages; i++, page++)
				put_page(*page);

			mmu->pages_locked -= nr_pages;
		} else if (mmu->user) {
			u64 *pte64 = pte_table->entries;
			pte_t pte;
			int i;

			for (i = 0; i < nr_pages; i++) {
#if defined(CONFIG_ARM)
				{
					pte = *pte64++;
					/* Unused entries are 0 */
					if (!pte)
						break;
				}
#else
				{
					pte.pte = *pte64++;
					/* Unused entries are 0 */
					if (!pte.pte)
						break;
				}
#endif

				/* pte_page() cannot return NULL */
				put_page(pte_page(pte));
			}

			mmu->pages_locked -= nr_pages;
		}

		free_page(pte_table->page);
		mmu->pages_created--;
	}

	if (mmu->pmd_table.page) {
		free_page(mmu->pmd_table.page);
		mmu->pages_created--;
	}

	if (mmu->pages_created || mmu->pages_locked)
		mc_dev_err(-EUCLEAN,
			   "leak detected: still in use %d, still locked %d",
			   mmu->pages_created, mmu->pages_locked);

	if (mmu->deleter)
		mmu->deleter->delete(mmu->deleter->object);

	kfree(mmu);

	/* Decrement debug counter */
	atomic_dec(&g_ctx.c_mmus);
}

static struct tee_mmu *tee_mmu_create_common(const struct mcp_buffer_map *b_map)
{
	struct tee_mmu *mmu;
	int ret = -ENOMEM;

	if (b_map->nr_pages > (PMD_ENTRIES_MAX * PTE_ENTRIES_MAX)) {
		ret = -EINVAL;
		mc_dev_err(ret, "data mapping exceeds %d pages: %lu",
			   PMD_ENTRIES_MAX * PTE_ENTRIES_MAX, b_map->nr_pages);
		return ERR_PTR(ret);
	}

	/* Allocate the struct */
	mmu = kzalloc(sizeof(*mmu), GFP_KERNEL);
	if (!mmu)
		return ERR_PTR(-ENOMEM);

	/* Increment debug counter */
	atomic_inc(&g_ctx.c_mmus);
	kref_init(&mmu->kref);

	/* The Xen front-end does not use PTEs */
	if (protocol_fe_uses_pages_and_vas())
		mmu->use_pages_and_vas = true;

	/* Buffer info */
	mmu->offset = b_map->offset;
	mmu->length = b_map->length;
	mmu->flags = b_map->flags;

	/* Pages info */
	mmu->nr_pages = b_map->nr_pages;
	mmu->nr_pmd_entries = (mmu->nr_pages + PTE_ENTRIES_MAX - 1) /
			    PTE_ENTRIES_MAX;
	mc_dev_devel("mmu->nr_pages %lu num_ptes_pages %zu",
		     mmu->nr_pages, mmu->nr_pmd_entries);

	/* Allocate a page for the L1 table, always used for DomU */
	mmu->pmd_table.page = get_zeroed_page(GFP_KERNEL);
	if (!mmu->pmd_table.page)
		goto end;

	mmu->pages_created++;

	return mmu;

end:
	tee_mmu_delete(mmu);
	return ERR_PTR(ret);
}

static bool mmu_get_dma_buffer(struct tee_mmu *mmu, int va)
{
#ifdef CONFIG_DMA_SHARED_BUFFER
	struct dma_buf *buf;

	buf = dma_buf_get(va);
	if (IS_ERR(buf))
		return false;

	mmu->dma_buf = buf;
	mmu->attach = dma_buf_attach(mmu->dma_buf, g_ctx.mcd);
	if (IS_ERR(mmu->attach))
		goto err_attach;

	mmu->sgt = dma_buf_map_attachment(mmu->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(mmu->sgt))
		goto err_map;

	return true;

err_map:
	dma_buf_detach(mmu->dma_buf, mmu->attach);

err_attach:
	dma_buf_put(mmu->dma_buf);
#endif
	return false;
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
	struct page	**pages;	/* Same as below, conveniently typed */
	unsigned long	pages_page = 0;	/* Page to contain the page pointers */
	unsigned long	chunk;
	struct mcp_buffer_map b_map = {
		.offset = (u32)(buf->va & ~PAGE_MASK),
		.length = buf->len,
		.flags = buf->flags,
	};
	bool		writeable = buf->flags & MC_IO_MAP_OUTPUT;
	int		ret = 0;

#ifndef CONFIG_DMA_SHARED_BUFFER
	if (buf->flags & MMU_ION_BUF) {
		mc_dev_err(-EINVAL, "ION buffers not supported by kernel");
		return ERR_PTR(-EINVAL);
	}
#endif

	/* Check input arguments */
	if (!(buf->flags & MMU_ION_BUF) && !buf->va)
		return ERR_PTR(-EINVAL);

	if (buf->flags & MMU_ION_BUF)
		/* buf->va is not a valid address. ION buffers are aligned */
		b_map.offset = 0;

	/* Allocate the struct */
	b_map.nr_pages = PAGE_ALIGN(b_map.offset + b_map.length) / PAGE_SIZE;
	/* Allow Registered Shared mem with valid pointer and zero size. */
	if (!b_map.nr_pages)
		b_map.nr_pages = 1;

	mmu = tee_mmu_create_common(&b_map);
	if (IS_ERR(mmu))
		return mmu;

	if (buf->flags & MMU_ION_BUF) {
		mc_dev_devel("Buffer is ION");
		/* Buffer is ION -
		 * va is the client's dma_buf fd, which should be converted
		 * to a struct sg_table * directly.
		 */
		if (!mmu_get_dma_buffer(mmu, buf->va)) {
			mc_dev_err(ret, "mmu_get_dma_buffer failed");
			ret = -EINVAL;
			goto end;
		}
	}
	/* Get a page to store page pointers */
	pages_page = get_zeroed_page(GFP_KERNEL);
	if (!pages_page) {
		ret = -ENOMEM;
		goto end;
	}
	mmu->pages_created++;

	pages = (struct page **)pages_page;
	for (chunk = 0; chunk < mmu->nr_pmd_entries; chunk++) {
		unsigned long nr_pages;
		int i;

		/* Size to map for this chunk */
		if (chunk == (mmu->nr_pmd_entries - 1))
			nr_pages = ((mmu->nr_pages - 1) % PTE_ENTRIES_MAX) + 1;
		else
			nr_pages = PTE_ENTRIES_MAX;

		/* Allocate a page to hold ptes that describe buffer pages */
		mmu->pte_tables[chunk].page = get_zeroed_page(GFP_KERNEL);
		if (!mmu->pte_tables[chunk].page) {
			ret = -ENOMEM;
			goto end;
		}
		mmu->pages_created++;

		/* Add page address to pmd table if needed */
		if (mmu->use_pages_and_vas)
			mmu->pmd_table.vas[chunk] =
				mmu->pte_tables[chunk].page;
		else
			mmu->pmd_table.entries[chunk] =
			       virt_to_phys(mmu->pte_tables[chunk].addr);

		/* Get pages */
		if (mmu->dma_buf) {
			/* Buffer is ION */
			struct sg_mapping_iter miter;
			struct page **page_ptr;
			unsigned int cnt = 0;
			unsigned int global_cnt = 0;

			page_ptr = pages;
			sg_miter_start(&miter, mmu->sgt->sgl,
				       mmu->sgt->nents,
				       SG_MITER_FROM_SG);

			while (sg_miter_next(&miter)) {
				if (((global_cnt) >=
				    (PTE_ENTRIES_MAX * chunk)) &&
				    cnt < nr_pages) {
					page_ptr[cnt] = miter.page;
					cnt++;
				}
				global_cnt++;
			}
			sg_miter_stop(&miter);
		} else if (mm) {
			long gup_ret;

			/* Buffer was allocated in user space */
			down_read(&mm->mmap_sem);
			/*
			 * Always try to map read/write from a Linux PoV, so
			 * Linux creates (page faults) the underlying pages if
			 * missing.
			 */
			gup_ret = gup_local_repeat(mm, (uintptr_t)reader,
						   nr_pages, 1, pages);
			if ((gup_ret == -EFAULT) && !writeable) {
				/*
				 * If mapping read/write fails, and the buffer
				 * is to be shared as input only, try to map
				 * again read-only.
				 */
				gup_ret = gup_local_repeat(mm,
							   (uintptr_t)reader,
							   nr_pages, 0, pages);
			}
			up_read(&mm->mmap_sem);
			if (gup_ret < 0) {
				ret = gup_ret;
				mc_dev_err(ret, "failed to get user pages @%p",
					   reader);
				goto end;
			}

			/* check if we could lock all pages. */
			if (gup_ret != nr_pages) {
				mc_dev_err((int)gup_ret,
					   "failed to get user pages");
				release_pages(pages, gup_ret);
				ret = -EINVAL;
				goto end;
			}

			reader += nr_pages * PAGE_SIZE;
			mmu->user = true;
			mmu->pages_locked += nr_pages;
		} else if (is_vmalloc_addr(data)) {
			/* Buffer vmalloc'ed in kernel space */
			for (i = 0; i < nr_pages; i++) {
				struct page *page = vmalloc_to_page(reader);

				if (!page) {
					ret = -EINVAL;
					mc_dev_err(ret,
						   "failed to map address");
					goto end;
				}

				pages[i] = page;
				reader += PAGE_SIZE;
			}
		} else {
			/* Buffer kmalloc'ed in kernel space */
			struct page *page = virt_to_page(reader);

			reader += nr_pages * PAGE_SIZE;
			for (i = 0; i < nr_pages; i++)
				pages[i] = page++;
		}

		/* Create Table of physical addresses*/
		if (mmu->use_pages_and_vas) {
			memcpy(mmu->pte_tables[chunk].pages, pages,
			       nr_pages * sizeof(*pages));
		} else {
			for (i = 0; i < nr_pages; i++) {
				mmu->pte_tables[chunk].entries[i] =
						page_to_phys(pages[i]);
			}
		}
	}

end:
	if (pages_page) {
		free_page(pages_page);
		mmu->pages_created--;
	}

	if (ret) {
		tee_mmu_delete(mmu);
		return ERR_PTR(ret);
	}

	mc_dev_devel(
		"created mmu %p: %s va %llx len %u off %u flg %x pmd table %lx",
		mmu, mmu->user ? "user" : "kernel", buf->va, mmu->length,
		mmu->offset, mmu->flags, mmu->pmd_table.page);
	return mmu;
}

struct tee_mmu *tee_mmu_wrap(struct tee_deleter *deleter, struct page **pages,
			     const struct mcp_buffer_map *b_map)
{
	int ret = -EINVAL;
#ifdef CONFIG_XEN
	struct tee_mmu *mmu;
	unsigned long chunk, nr_pages_left;

	/* Allocate the struct */
	mmu = tee_mmu_create_common(b_map);
	if (IS_ERR(mmu))
		return mmu;

	nr_pages_left = mmu->nr_pages;
	for (chunk = 0; chunk < mmu->nr_pmd_entries; chunk++) {
		unsigned long nr_pages = nr_pages_left;
		u64 *pte;
		int i;

		if (nr_pages > PTE_ENTRIES_MAX)
			nr_pages = PTE_ENTRIES_MAX;

		nr_pages_left -= nr_pages;

		/* Allocate a page to hold ptes that describe buffer pages */
		mmu->pte_tables[chunk].page = get_zeroed_page(GFP_KERNEL);
		if (!mmu->pte_tables[chunk].page) {
			ret = -ENOMEM;
			goto err;
		}
		mmu->pages_created++;

		/* Add page address to pmd table if needed */
		mmu->pmd_table.entries[chunk] =
			virt_to_phys(mmu->pte_tables[chunk].addr);

		/* Convert to PTEs */
		pte = &mmu->pte_tables[chunk].entries[0];

		for (i = 0; i < nr_pages; i++, pages++, pte++) {
			unsigned long phys;
			unsigned long pfn;

			phys = page_to_phys(*pages);
#if defined CONFIG_ARM64
			phys &= PHYS_48BIT_MASK;
#endif
			pfn = PFN_DOWN(phys);
			*pte = __pfn_to_mfn(pfn) << PAGE_SHIFT;
		}
	}

	mmu->deleter = deleter;
	mc_dev_devel("wrapped mmu %p: len %u off %u flg %x pmd table %lx",
		     mmu, mmu->length, mmu->offset, mmu->flags,
		     mmu->pmd_table.page);
	return mmu;

err:
	tee_mmu_delete(mmu);
#endif
	return ERR_PTR(ret);
}

void tee_mmu_set_deleter(struct tee_mmu *mmu, struct tee_deleter *deleter)
{
	mmu->deleter = deleter;
}

static void tee_mmu_release(struct kref *kref)
{
	struct tee_mmu *mmu = container_of(kref, struct tee_mmu, kref);

	mc_dev_devel("free mmu %p: %s len %u off %u pmd table %lx",
		     mmu, mmu->user ? "user" : "kernel", mmu->length,
		     mmu->offset, mmu->pmd_table.page);
	tee_mmu_delete(mmu);
}

void tee_mmu_get(struct tee_mmu *mmu)
{
	kref_get(&mmu->kref);
}

void tee_mmu_put(struct tee_mmu *mmu)
{
	kref_put(&mmu->kref, tee_mmu_release);
}

void tee_mmu_buffer(struct tee_mmu *mmu, struct mcp_buffer_map *map)
{
	if (mmu->use_pages_and_vas)
		map->addr = mmu->pmd_table.page;
	else
		map->addr = virt_to_phys(mmu->pmd_table.addr);

	map->secure_va = 0;
	map->offset = mmu->offset;
	map->length = mmu->length;
	map->nr_pages = mmu->nr_pages;
	map->flags = mmu->flags;
	map->type = WSM_L1;
#ifdef CONFIG_DMA_SHARED_BUFFER
	if (mmu->dma_buf) {
		/* ION */
		if (!(((struct ion_buffer *)mmu->dma_buf->priv)->flags
		   & ION_FLAG_CACHED)) {
			map->type |= WSM_UNCACHED;
			mc_dev_devel("ION buffer Non cacheable");
		} else {
			mc_dev_devel("ION buffer cacheable");
		}
	}
#endif
	map->mmu = mmu;
}

int tee_mmu_debug_structs(struct kasnprintf_buf *buf, const struct tee_mmu *mmu)
{
	return kasnprintf(buf,
			  "\t\t\tmmu %pK: %s len %u off %u table %pK\n",
			  mmu, mmu->user ? "user" : "kernel", mmu->length,
			  mmu->offset, (void *)mmu->pmd_table.page);
}
