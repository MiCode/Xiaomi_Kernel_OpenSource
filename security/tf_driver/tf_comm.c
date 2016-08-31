/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <asm/div64.h>
#include <asm/system.h>
#include <linux/version.h>
#include <asm/cputype.h>
#include <linux/interrupt.h>
#include <linux/page-flags.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/jiffies.h>
#include <linux/freezer.h>

#include "tf_defs.h"
#include "tf_comm.h"
#include "tf_protocol.h"
#include "tf_util.h"
#include "tf_conn.h"

#ifdef CONFIG_TF_ZEBRA
#include "tf_zebra.h"
#endif

/*---------------------------------------------------------------------------
 * Internal Constants
 *---------------------------------------------------------------------------*/

/*
 * shared memories descriptor constants
 */
#define DESCRIPTOR_B_MASK           (1 << 2)
#define DESCRIPTOR_C_MASK           (1 << 3)
#define DESCRIPTOR_S_MASK           (1 << 10)

#define L1_COARSE_DESCRIPTOR_BASE         (0x00000001)
#define L1_COARSE_DESCRIPTOR_ADDR_MASK    (0xFFFFFC00)
#define L1_COARSE_DESCRIPTOR_V13_12_SHIFT (5)

#define L2_PAGE_DESCRIPTOR_BASE              (0x00000003)
#define L2_PAGE_DESCRIPTOR_AP_APX_READ       (0x220)
#define L2_PAGE_DESCRIPTOR_AP_APX_READ_WRITE (0x30)

#define L2_INIT_DESCRIPTOR_BASE           (0x00000003)
#define L2_INIT_DESCRIPTOR_V13_12_SHIFT   (4)

/*
 * Reject an attempt to share a strongly-Ordered or Device memory
 * Strongly-Ordered:  TEX=0b000, C=0, B=0
 * Shared Device:     TEX=0b000, C=0, B=1
 * Non-Shared Device: TEX=0b010, C=0, B=0
 */
#define L2_TEX_C_B_MASK \
	((1<<8) | (1<<7) | (1<<6) | (1<<3) | (1<<2))
#define L2_TEX_C_B_STRONGLY_ORDERED \
	((0<<8) | (0<<7) | (0<<6) | (0<<3) | (0<<2))
#define L2_TEX_C_B_SHARED_DEVICE \
	((0<<8) | (0<<7) | (0<<6) | (0<<3) | (1<<2))
#define L2_TEX_C_B_NON_SHARED_DEVICE \
	((0<<8) | (1<<7) | (0<<6) | (0<<3) | (0<<2))

#define CACHE_S(x)      ((x) & (1 << 24))
#define CACHE_DSIZE(x)  (((x) >> 12) & 4095)

#define TIME_IMMEDIATE ((u64) 0x0000000000000000ULL)
#define TIME_INFINITE  ((u64) 0xFFFFFFFFFFFFFFFFULL)

/*---------------------------------------------------------------------------
 * atomic operation definitions
 *---------------------------------------------------------------------------*/

/*
 * Atomically updates the sync_serial_n and time_n register
 * sync_serial_n and time_n modifications are thread safe
 */
void tf_set_current_time(struct tf_comm *comm)
{
	u32 new_sync_serial;
	struct timeval now;
	u64 time64;

	/*
	 * lock the structure while updating the L1 shared memory fields
	 */
	spin_lock(&comm->lock);

	/* read sync_serial_n and change the TimeSlot bit field */
	new_sync_serial =
		tf_read_reg32(&comm->l1_buffer->sync_serial_n) + 1;

	do_gettimeofday(&now);
	time64 = now.tv_sec;
	time64 = (time64 * 1000) + (now.tv_usec / 1000);

	/* Write the new time64 and nSyncSerial into shared memory */
	tf_write_reg64(&comm->l1_buffer->time_n[new_sync_serial &
		TF_SYNC_SERIAL_TIMESLOT_N], time64);
	tf_write_reg32(&comm->l1_buffer->sync_serial_n,
		new_sync_serial);

	spin_unlock(&comm->lock);
}

/*
 * Performs the specific read timeout operation
 * The difficulty here is to read atomically 2 u32
 * values from the L1 shared buffer.
 * This is guaranteed by reading before and after the operation
 * the timeslot given by the Secure World
 */
static inline void tf_read_timeout(struct tf_comm *comm, u64 *time)
{
	u32 sync_serial_s_initial = 0;
	u32 sync_serial_s_final = 1;
	u64 time64;

	spin_lock(&comm->lock);

	while (sync_serial_s_initial != sync_serial_s_final) {
		sync_serial_s_initial = tf_read_reg32(
			&comm->l1_buffer->sync_serial_s);
		time64 = tf_read_reg64(
			&comm->l1_buffer->timeout_s[sync_serial_s_initial&1]);

		sync_serial_s_final = tf_read_reg32(
			&comm->l1_buffer->sync_serial_s);
	}

	spin_unlock(&comm->lock);

	*time = time64;
}

/*----------------------------------------------------------------------------
 * SIGKILL signal handling
 *----------------------------------------------------------------------------*/

static bool sigkill_pending(void)
{
	if (signal_pending(current)) {
		dprintk(KERN_INFO "A signal is pending\n");
		if (sigismember(&current->pending.signal, SIGKILL)) {
			dprintk(KERN_INFO "A SIGKILL is pending\n");
			return true;
		} else if (sigismember(
			&current->signal->shared_pending.signal, SIGKILL)) {
			dprintk(KERN_INFO "A SIGKILL is pending (shared)\n");
			return true;
		}
	}
	return false;
}

/*----------------------------------------------------------------------------
 * Shared memory related operations
 *----------------------------------------------------------------------------*/

struct tf_coarse_page_table *tf_alloc_coarse_page_table(
	struct tf_coarse_page_table_allocation_context *alloc_context,
	u32 type)
{
	struct tf_coarse_page_table *coarse_pg_table = NULL;

	spin_lock(&(alloc_context->lock));

	if (!(list_empty(&(alloc_context->free_coarse_page_tables)))) {
		/*
		 * The free list can provide us a coarse page table
		 * descriptor
		 */
		coarse_pg_table = list_first_entry(
				&alloc_context->free_coarse_page_tables,
				struct tf_coarse_page_table, list);
		list_del(&(coarse_pg_table->list));

		coarse_pg_table->parent->ref_count++;
	} else {
		/* no array of coarse page tables, create a new one */
		struct tf_coarse_page_table_array *array;
		void *page;
		int i;

		spin_unlock(&(alloc_context->lock));

		/* first allocate a new page descriptor */
		array = internal_kmalloc(sizeof(*array), GFP_KERNEL);
		if (array == NULL) {
			dprintk(KERN_ERR "tf_alloc_coarse_page_table(%p):"
					" failed to allocate a table array\n",
					alloc_context);
			return NULL;
		}

		array->type = type;
		array->ref_count = 0;
		INIT_LIST_HEAD(&(array->list));

		/* now allocate the actual page the page descriptor describes */
		page = (void *) internal_get_zeroed_page(GFP_KERNEL);
		if (page == NULL) {
			dprintk(KERN_ERR "tf_alloc_coarse_page_table(%p):"
					" failed allocate a page\n",
					alloc_context);
			internal_kfree(array);
			return NULL;
		}

		spin_lock(&(alloc_context->lock));

		/* initialize the coarse page table descriptors */
		for (i = 0; i < 4; i++) {
			INIT_LIST_HEAD(&(array->coarse_page_tables[i].list));
			array->coarse_page_tables[i].descriptors =
				page + (i * SIZE_1KB);
			array->coarse_page_tables[i].parent = array;

			if (i == 0) {
				/*
				 * the first element is kept for the current
				 * coarse page table allocation
				 */
				coarse_pg_table =
					&(array->coarse_page_tables[i]);
				array->ref_count++;
			} else {
				/*
				 * The other elements are added to the free list
				 */
				list_add(&(array->coarse_page_tables[i].list),
					&(alloc_context->
						free_coarse_page_tables));
			}
		}

		list_add(&(array->list),
			&(alloc_context->coarse_page_table_arrays));
	}
	spin_unlock(&(alloc_context->lock));

	return coarse_pg_table;
}


void tf_free_coarse_page_table(
	struct tf_coarse_page_table_allocation_context *alloc_context,
	struct tf_coarse_page_table *coarse_pg_table,
	int force)
{
	struct tf_coarse_page_table_array *array;

	spin_lock(&(alloc_context->lock));

	array = coarse_pg_table->parent;

	(array->ref_count)--;

	if (array->ref_count == 0) {
		/*
		 * no coarse page table descriptor is used
		 * check if we should free the whole page
		 */

		if ((array->type == TF_PAGE_DESCRIPTOR_TYPE_PREALLOCATED)
			&& (force == 0))
			/*
			 * This is a preallocated page,
			 * add the page back to the free list
			 */
			list_add(&(coarse_pg_table->list),
				&(alloc_context->free_coarse_page_tables));
		else {
			/*
			 * None of the page's coarse page table descriptors
			 * are in use, free the whole page
			 */
			int i;
			u32 *descriptors;

			/*
			 * remove the page's associated coarse page table
			 * descriptors from the free list
			 */
			for (i = 0; i < 4; i++)
				if (&(array->coarse_page_tables[i]) !=
						coarse_pg_table)
					list_del(&(array->
						coarse_page_tables[i].list));

			descriptors =
				array->coarse_page_tables[0].descriptors;
			array->coarse_page_tables[0].descriptors = NULL;

			/* remove the coarse page table from the array  */
			list_del(&(array->list));

			spin_unlock(&(alloc_context->lock));
			/*
			 * Free the page.
			 * The address of the page is contained in the first
			 * element
			 */
			internal_free_page((unsigned long) descriptors);
			/* finaly free the array */
			internal_kfree(array);

			spin_lock(&(alloc_context->lock));
		}
	} else {
		/*
		 * Some coarse page table descriptors are in use.
		 * Add the descriptor to the free list
		 */
		list_add(&(coarse_pg_table->list),
			&(alloc_context->free_coarse_page_tables));
	}

	spin_unlock(&(alloc_context->lock));
}


void tf_init_coarse_page_table_allocator(
	struct tf_coarse_page_table_allocation_context *alloc_context)
{
	spin_lock_init(&(alloc_context->lock));
	INIT_LIST_HEAD(&(alloc_context->coarse_page_table_arrays));
	INIT_LIST_HEAD(&(alloc_context->free_coarse_page_tables));
}

void tf_release_coarse_page_table_allocator(
	struct tf_coarse_page_table_allocation_context *alloc_context)
{
	spin_lock(&(alloc_context->lock));

	/* now clean up the list of page descriptors */
	while (!list_empty(&(alloc_context->coarse_page_table_arrays))) {
		struct tf_coarse_page_table_array *page_desc;
		u32 *descriptors;

		page_desc = list_first_entry(
			&alloc_context->coarse_page_table_arrays,
			struct tf_coarse_page_table_array, list);

		descriptors = page_desc->coarse_page_tables[0].descriptors;
		list_del(&(page_desc->list));

		spin_unlock(&(alloc_context->lock));

		if (descriptors != NULL)
			internal_free_page((unsigned long)descriptors);

		internal_kfree(page_desc);

		spin_lock(&(alloc_context->lock));
	}

	spin_unlock(&(alloc_context->lock));
}

/*
 * Returns the L1 coarse page descriptor for
 * a coarse page table located at address coarse_pg_table_descriptors
 */
u32 tf_get_l1_coarse_descriptor(
	u32 coarse_pg_table_descriptors[256])
{
	u32 descriptor = L1_COARSE_DESCRIPTOR_BASE;
	unsigned int info = read_cpuid(CPUID_CACHETYPE);

	descriptor |= (virt_to_phys((void *) coarse_pg_table_descriptors)
		& L1_COARSE_DESCRIPTOR_ADDR_MASK);

	if (CACHE_S(info) && (CACHE_DSIZE(info) & (1 << 11))) {
		dprintk(KERN_DEBUG "tf_get_l1_coarse_descriptor "
			"V31-12 added to descriptor\n");
		/* the 16k alignment restriction applies */
		descriptor |= (DESCRIPTOR_V13_12_GET(
			(u32)coarse_pg_table_descriptors) <<
				L1_COARSE_DESCRIPTOR_V13_12_SHIFT);
	}

	return descriptor;
}


#define dprintk_desc(...)
/*
 * Returns the L2 descriptor for the specified user page.
 */
u32 tf_get_l2_descriptor_common(u32 vaddr, struct mm_struct *mm)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep;
	u32  *hwpte;
	u32   tex = 0;
	u32 descriptor = 0;

	dprintk_desc(KERN_INFO "VirtAddr = %x\n", vaddr);
	pgd = pgd_offset(mm, vaddr);
	dprintk_desc(KERN_INFO "pgd = %x, value=%x\n", (unsigned int) pgd,
		(unsigned int) *pgd);
	if (pgd_none(*pgd))
		goto error;
	pud = pud_offset(pgd, vaddr);
	dprintk_desc(KERN_INFO "pud = %x, value=%x\n", (unsigned int) pud,
		(unsigned int) *pud);
	if (pud_none(*pud))
		goto error;
	pmd = pmd_offset(pud, vaddr);
	dprintk_desc(KERN_INFO "pmd = %x, value=%x\n", (unsigned int) pmd,
		(unsigned int) *pmd);
	if (pmd_none(*pmd))
		goto error;

	if (PMD_TYPE_SECT&(*pmd)) {
		/* We have a section */
		dprintk_desc(KERN_INFO "Section descr=%x\n",
			(unsigned int)*pmd);
		if ((*pmd) & PMD_SECT_BUFFERABLE)
			descriptor |= DESCRIPTOR_B_MASK;
		if ((*pmd) & PMD_SECT_CACHEABLE)
			descriptor |= DESCRIPTOR_C_MASK;
		if ((*pmd) & PMD_SECT_S)
			descriptor |= DESCRIPTOR_S_MASK;
		tex = ((*pmd) >> 12) & 7;
	} else {
		/* We have a table */
		ptep = pte_offset_map(pmd, vaddr);
		if (pte_present(*ptep)) {
			dprintk_desc(KERN_INFO "L2 descr=%x\n",
				(unsigned int) *ptep);
			if ((*ptep) & L_PTE_MT_BUFFERABLE)
				descriptor |= DESCRIPTOR_B_MASK;
			if ((*ptep) & L_PTE_MT_WRITETHROUGH)
				descriptor |= DESCRIPTOR_C_MASK;
			if ((*ptep) & L_PTE_MT_DEV_SHARED)
				descriptor |= DESCRIPTOR_S_MASK;

			/*
			 * Linux's pte doesn't keep track of TEX value.
			 * Have to jump to hwpte see include/asm/pgtable.h
			 */
#ifdef PTE_HWTABLE_SIZE
			hwpte = (u32 *) (ptep + PTE_HWTABLE_PTRS);
#else
			hwpte = (u32 *) (ptep - PTRS_PER_PTE);
#endif
			if (((*hwpte) & L2_DESCRIPTOR_ADDR_MASK) !=
					((*ptep) & L2_DESCRIPTOR_ADDR_MASK))
				goto error;
			dprintk_desc(KERN_INFO "hw descr=%x\n", *hwpte);
			tex = ((*hwpte) >> 6) & 7;
			pte_unmap(ptep);
		} else {
			pte_unmap(ptep);
			goto error;
		}
	}

	descriptor |= (tex << 6);

	return descriptor;

error:
	dprintk(KERN_ERR "Error occured in %s\n", __func__);
	return 0;
}


/*
 * Changes an L2 page descriptor back to a pointer to a physical page
 */
inline struct page *tf_l2_page_descriptor_to_page(u32 l2_page_descriptor)
{
	return pte_page(l2_page_descriptor & L2_DESCRIPTOR_ADDR_MASK);
}


/*
 * Returns the L1 descriptor for the 1KB-aligned coarse page table. The address
 * must be in the kernel address space.
 */
static void tf_get_l2_page_descriptor(
	u32 *l2_page_descriptor,
	u32 flags, struct mm_struct *mm)
{
	unsigned long page_vaddr;
	u32 descriptor;
	struct page *page;
	bool unmap_page = false;

#if 0
	dprintk(KERN_INFO
		"tf_get_l2_page_descriptor():"
		"*l2_page_descriptor=%x\n",
		*l2_page_descriptor);
#endif

	if (*l2_page_descriptor == L2_DESCRIPTOR_FAULT)
		return;

	page = (struct page *) (*l2_page_descriptor);

	page_vaddr = (unsigned long) page_address(page);
	if (page_vaddr == 0) {
		dprintk(KERN_INFO "page_address returned 0\n");
		/* Should we use kmap_atomic(page, KM_USER0) instead ? */
		page_vaddr = (unsigned long) kmap(page);
		if (page_vaddr == 0) {
			*l2_page_descriptor = L2_DESCRIPTOR_FAULT;
			dprintk(KERN_ERR "kmap returned 0\n");
			return;
		}
		unmap_page = true;
	}

	descriptor = tf_get_l2_descriptor_common(page_vaddr, mm);
	if (descriptor == 0) {
		*l2_page_descriptor = L2_DESCRIPTOR_FAULT;
		return;
	}
	descriptor |= L2_PAGE_DESCRIPTOR_BASE;

	descriptor |= (page_to_phys(page) & L2_DESCRIPTOR_ADDR_MASK);

	if (!(flags & TF_SHMEM_TYPE_WRITE))
		/* only read access */
		descriptor |= L2_PAGE_DESCRIPTOR_AP_APX_READ;
	else
		/* read and write access */
		descriptor |= L2_PAGE_DESCRIPTOR_AP_APX_READ_WRITE;

	if (unmap_page)
		kunmap(page);

	*l2_page_descriptor = descriptor;
}


/*
 * Unlocks the physical memory pages
 * and frees the coarse pages that need to
 */
void tf_cleanup_shared_memory(
	struct tf_coarse_page_table_allocation_context *alloc_context,
	struct tf_shmem_desc *shmem_desc,
	u32 full_cleanup)
{
	u32 coarse_page_index;

	dprintk(KERN_INFO "tf_cleanup_shared_memory(%p)\n",
			shmem_desc);

#ifdef DEBUG_COARSE_TABLES
	printk(KERN_DEBUG "tf_cleanup_shared_memory "
		"- number of coarse page tables=%d\n",
		shmem_desc->coarse_pg_table_count);

	for (coarse_page_index = 0;
	     coarse_page_index < shmem_desc->coarse_pg_table_count;
	     coarse_page_index++) {
		u32 j;

		printk(KERN_DEBUG "  Descriptor=%p address=%p index=%d\n",
			shmem_desc->coarse_pg_table[coarse_page_index],
			shmem_desc->coarse_pg_table[coarse_page_index]->
				descriptors,
			coarse_page_index);
		if (shmem_desc->coarse_pg_table[coarse_page_index] != NULL) {
			for (j = 0;
			     j < TF_DESCRIPTOR_TABLE_CAPACITY;
			     j += 8) {
				int k;
				printk(KERN_DEBUG "    ");
				for (k = j; k < j + 8; k++)
					printk(KERN_DEBUG "%p ",
						shmem_desc->coarse_pg_table[
							coarse_page_index]->
								descriptors);
				printk(KERN_DEBUG "\n");
			}
		}
	}
	printk(KERN_DEBUG "tf_cleanup_shared_memory() - done\n\n");
#endif

	/* Parse the coarse page descriptors */
	for (coarse_page_index = 0;
	     coarse_page_index < shmem_desc->coarse_pg_table_count;
	     coarse_page_index++) {
		u32 j;
		u32 found = 0;

		/* parse the page descriptors of the coarse page */
		for (j = 0; j < TF_DESCRIPTOR_TABLE_CAPACITY; j++) {
			u32 l2_page_descriptor = (u32) (shmem_desc->
				coarse_pg_table[coarse_page_index]->
					descriptors[j]);

			if (l2_page_descriptor != L2_DESCRIPTOR_FAULT) {
				struct page *page =
					tf_l2_page_descriptor_to_page(
						l2_page_descriptor);

				if (!PageReserved(page))
					SetPageDirty(page);
				internal_page_cache_release(page);

				found = 1;
			} else if (found == 1) {
				break;
			}
		}

		/*
		 * Only free the coarse pages of descriptors not preallocated
		 */
		if ((shmem_desc->type == TF_SHMEM_TYPE_REGISTERED_SHMEM) ||
			(full_cleanup != 0))
			tf_free_coarse_page_table(alloc_context,
				shmem_desc->coarse_pg_table[coarse_page_index],
				0);
	}

	shmem_desc->coarse_pg_table_count = 0;
	dprintk(KERN_INFO "tf_cleanup_shared_memory(%p) done\n",
			shmem_desc);
}

/*
 * Make sure the coarse pages are allocated. If not allocated, do it.
 * Locks down the physical memory pages.
 * Verifies the memory attributes depending on flags.
 */
int tf_fill_descriptor_table(
	struct tf_coarse_page_table_allocation_context *alloc_context,
	struct tf_shmem_desc *shmem_desc,
	u32 buffer,
	struct vm_area_struct **vmas,
	u32 descriptors[TF_MAX_COARSE_PAGES],
	u32 buffer_size,
	u32 *buffer_start_offset,
	bool in_user_space,
	u32 flags,
	u32 *descriptor_count)
{
	u32 coarse_page_index;
	u32 coarse_page_count;
	u32 page_count;
	u32 page_shift = 0;
	int ret = 0;
	unsigned int info = read_cpuid(CPUID_CACHETYPE);

	dprintk(KERN_INFO "tf_fill_descriptor_table"
		"(%p, buffer=0x%08X, size=0x%08X, user=%01x "
		"flags = 0x%08x)\n",
		shmem_desc,
		buffer,
		buffer_size,
		in_user_space,
		flags);

	/*
	 * Compute the number of pages
	 * Compute the number of coarse pages
	 * Compute the page offset
	 */
	page_count = ((buffer & ~PAGE_MASK) +
		buffer_size + ~PAGE_MASK) >> PAGE_SHIFT;

	/* check whether the 16k alignment restriction applies */
	if (CACHE_S(info) && (CACHE_DSIZE(info) & (1 << 11)))
		/*
		 * The 16k alignment restriction applies.
		 * Shift data to get them 16k aligned
		 */
		page_shift = DESCRIPTOR_V13_12_GET(buffer);
	page_count += page_shift;


	/*
	 * Check the number of pages fit in the coarse pages
	 */
	if (page_count > (TF_DESCRIPTOR_TABLE_CAPACITY *
			TF_MAX_COARSE_PAGES)) {
		dprintk(KERN_ERR "tf_fill_descriptor_table(%p): "
			"%u pages required to map shared memory!\n",
			shmem_desc, page_count);
		ret = -ENOMEM;
		goto error;
	}

	/* coarse page describe 256 pages */
	coarse_page_count = ((page_count +
		TF_DESCRIPTOR_TABLE_CAPACITY_MASK) >>
			TF_DESCRIPTOR_TABLE_CAPACITY_BIT_SHIFT);

	/*
	 * Compute the buffer offset
	 */
	*buffer_start_offset = (buffer & ~PAGE_MASK) |
		(page_shift << PAGE_SHIFT);

	/* map each coarse page */
	for (coarse_page_index = 0;
	     coarse_page_index < coarse_page_count;
	     coarse_page_index++) {
		u32 j;
		struct tf_coarse_page_table *coarse_pg_table;

		/* compute a virtual address with appropriate offset */
		u32 buffer_offset_vaddr = buffer +
			(coarse_page_index * TF_MAX_COARSE_PAGE_MAPPED_SIZE);
		u32 pages_to_get;

		/*
		 * Compute the number of pages left for this coarse page.
		 * Decrement page_count each time
		 */
		pages_to_get = (page_count >>
			TF_DESCRIPTOR_TABLE_CAPACITY_BIT_SHIFT) ?
				TF_DESCRIPTOR_TABLE_CAPACITY : page_count;
		page_count -= pages_to_get;

		/*
		 * Check if the coarse page has already been allocated
		 * If not, do it now
		 */
		if ((shmem_desc->type == TF_SHMEM_TYPE_REGISTERED_SHMEM)
			|| (shmem_desc->type ==
				TF_SHMEM_TYPE_PM_HIBERNATE)) {
			coarse_pg_table = tf_alloc_coarse_page_table(
				alloc_context,
				TF_PAGE_DESCRIPTOR_TYPE_NORMAL);

			if (coarse_pg_table == NULL) {
				dprintk(KERN_ERR
					"tf_fill_descriptor_table(%p): "
					"tf_alloc_coarse_page_table "
					"failed for coarse page %d\n",
					shmem_desc, coarse_page_index);
				ret = -ENOMEM;
				goto error;
			}

			shmem_desc->coarse_pg_table[coarse_page_index] =
				coarse_pg_table;
		} else {
			coarse_pg_table =
				shmem_desc->coarse_pg_table[coarse_page_index];
		}

		/*
		 * The page is not necessarily filled with zeroes.
		 * Set the fault descriptors ( each descriptor is 4 bytes long)
		 */
		memset(coarse_pg_table->descriptors, 0x00,
			TF_DESCRIPTOR_TABLE_CAPACITY * sizeof(u32));

		if (in_user_space) {
			int pages;

			/*
			 * TRICK: use pCoarsePageDescriptor->descriptors to
			 * hold the (struct page*) items before getting their
			 * physical address
			 */
			down_read(&(current->mm->mmap_sem));
			pages = internal_get_user_pages(
				current,
				current->mm,
				buffer_offset_vaddr,
				/*
				 * page_shift is cleared after retrieving first
				 * coarse page
				 */
				(pages_to_get - page_shift),
				(flags & TF_SHMEM_TYPE_WRITE) ? 1 : 0,
				0,
				(struct page **) (coarse_pg_table->descriptors
					+ page_shift),
				vmas);
			up_read(&(current->mm->mmap_sem));

			if ((pages <= 0) ||
				(pages != (pages_to_get - page_shift))) {
				dprintk(KERN_ERR "tf_fill_descriptor_table:"
					" get_user_pages got %d pages while "
					"trying to get %d pages!\n",
					pages, pages_to_get - page_shift);
				ret = -EFAULT;
				goto error;
			}

			for (j = page_shift;
				  j < page_shift + pages;
				  j++) {
				/* Get the actual L2 descriptors */
				tf_get_l2_page_descriptor(
					&coarse_pg_table->descriptors[j],
					flags,
					current->mm);
				/*
				 * Reject Strongly-Ordered or Device Memory
				 */
#define IS_STRONGLY_ORDERED_OR_DEVICE_MEM(x) \
	((((x) & L2_TEX_C_B_MASK) == L2_TEX_C_B_STRONGLY_ORDERED) || \
	 (((x) & L2_TEX_C_B_MASK) == L2_TEX_C_B_SHARED_DEVICE) || \
	 (((x) & L2_TEX_C_B_MASK) == L2_TEX_C_B_NON_SHARED_DEVICE))

				if (IS_STRONGLY_ORDERED_OR_DEVICE_MEM(
					coarse_pg_table->
						descriptors[j])) {
					dprintk(KERN_ERR
						"tf_fill_descriptor_table:"
						" descriptor 0x%08X use "
						"strongly-ordered or device "
						"memory. Rejecting!\n",
						coarse_pg_table->
							descriptors[j]);
					ret = -EFAULT;
					goto error;
				}
			}
		} else {
			/* Kernel-space memory */
			dprintk(KERN_INFO
				"tf_fill_descriptor_table: "
				"buffer starting at %p\n",
			       (void *)buffer_offset_vaddr);
			for (j = page_shift; j < pages_to_get; j++) {
				struct page *page;
				void *addr =
					(void *)(buffer_offset_vaddr +
						(j - page_shift) * PAGE_SIZE);

				if (is_vmalloc_addr(
						(void *) buffer_offset_vaddr))
					page = vmalloc_to_page(addr);
				else
					page = virt_to_page(addr);

				if (page == NULL) {
					dprintk(KERN_ERR
						"tf_fill_descriptor_table: "
						"cannot map %p (vmalloc) "
						"to page\n",
						addr);
					ret = -EFAULT;
					goto error;
				}
				coarse_pg_table->descriptors[j] = (u32)page;
				get_page(page);

				/* change coarse page "page address" */
				tf_get_l2_page_descriptor(
					&coarse_pg_table->descriptors[j],
					flags,
					&init_mm);
			}
		}

		dmac_flush_range((void *)coarse_pg_table->descriptors,
		   (void *)(((u32)(coarse_pg_table->descriptors)) +
		   TF_DESCRIPTOR_TABLE_CAPACITY * sizeof(u32)));

		outer_clean_range(
			__pa(coarse_pg_table->descriptors),
			__pa(coarse_pg_table->descriptors) +
			TF_DESCRIPTOR_TABLE_CAPACITY * sizeof(u32));
		wmb();

		/* Update the coarse page table address */
		descriptors[coarse_page_index] =
			tf_get_l1_coarse_descriptor(
				coarse_pg_table->descriptors);

		/*
		 * The next coarse page has no page shift, reset the
		 * page_shift
		 */
		page_shift = 0;
	}

	*descriptor_count = coarse_page_count;
	shmem_desc->coarse_pg_table_count = coarse_page_count;

#ifdef DEBUG_COARSE_TABLES
	printk(KERN_DEBUG "ntf_fill_descriptor_table - size=0x%08X "
		"numberOfCoarsePages=%d\n", buffer_size,
		shmem_desc->coarse_pg_table_count);
	for (coarse_page_index = 0;
	     coarse_page_index < shmem_desc->coarse_pg_table_count;
	     coarse_page_index++) {
		u32 j;
		struct tf_coarse_page_table *coarse_page_table =
			shmem_desc->coarse_pg_table[coarse_page_index];

		printk(KERN_DEBUG "  Descriptor=%p address=%p index=%d\n",
			coarse_page_table,
			coarse_page_table->descriptors,
			coarse_page_index);
		for (j = 0;
		     j < TF_DESCRIPTOR_TABLE_CAPACITY;
		     j += 8) {
			int k;
			printk(KERN_DEBUG "    ");
			for (k = j; k < j + 8; k++)
				printk(KERN_DEBUG "0x%08X ",
					coarse_page_table->descriptors[k]);
			printk(KERN_DEBUG "\n");
		}
	}
	printk(KERN_DEBUG "ntf_fill_descriptor_table() - done\n\n");
#endif

	return 0;

error:
	tf_cleanup_shared_memory(
			alloc_context,
			shmem_desc,
			0);

	return ret;
}


/*----------------------------------------------------------------------------
 * Standard communication operations
 *----------------------------------------------------------------------------*/

u8 *tf_get_description(struct tf_comm *comm)
{
	if (test_bit(TF_COMM_FLAG_L1_SHARED_ALLOCATED, &(comm->flags)))
		return comm->l1_buffer->version_description;

	return NULL;
}

/*
 * Returns a non-zero value if the specified S-timeout has expired, zero
 * otherwise.
 *
 * The placeholder referenced to by relative_timeout_jiffies gives the relative
 * timeout from now in jiffies. It is set to zero if the S-timeout has expired,
 * or to MAX_SCHEDULE_TIMEOUT if the S-timeout is infinite.
 */
static int tf_test_s_timeout(
		u64 timeout,
		signed long *relative_timeout_jiffies)
{
	struct timeval now;
	u64 time64;

	*relative_timeout_jiffies = 0;

	/* immediate timeout */
	if (timeout == TIME_IMMEDIATE)
		return 1;

	/* infinite timeout */
	if (timeout == TIME_INFINITE) {
		dprintk(KERN_DEBUG "tf_test_s_timeout: "
			"timeout is infinite\n");
		*relative_timeout_jiffies = MAX_SCHEDULE_TIMEOUT;
		return 0;
	}

	do_gettimeofday(&now);
	time64 = now.tv_sec;
	/* will not overflow as operations are done on 64bit values */
	time64 = (time64 * 1000) + (now.tv_usec / 1000);

	/* timeout expired */
	if (time64 >= timeout) {
		dprintk(KERN_DEBUG "tf_test_s_timeout: timeout expired\n");
		return 1;
	}

	/*
	 * finite timeout, compute relative_timeout_jiffies
	 */
	/* will not overflow as time64 < timeout */
	timeout -= time64;

	/* guarantee *relative_timeout_jiffies is a valid timeout */
	if ((timeout >> 32) != 0)
		*relative_timeout_jiffies = MAX_JIFFY_OFFSET;
	else
		*relative_timeout_jiffies =
			msecs_to_jiffies((unsigned int) timeout);

	dprintk(KERN_DEBUG "tf_test_s_timeout: timeout is 0x%lx\n",
		*relative_timeout_jiffies);
	return 0;
}

static void tf_copy_answers(struct tf_comm *comm)
{
	u32 first_answer;
	u32 first_free_answer;
	struct tf_answer_struct *answerStructureTemp;

	if (test_bit(TF_COMM_FLAG_L1_SHARED_ALLOCATED, &(comm->flags))) {
		spin_lock(&comm->lock);
		first_free_answer = tf_read_reg32(
			&comm->l1_buffer->first_free_answer);
		first_answer = tf_read_reg32(
			&comm->l1_buffer->first_answer);

		while (first_answer != first_free_answer) {
			/* answer queue not empty */
			union tf_answer sComAnswer;
			struct tf_answer_header  header;

			/*
			 * the size of the command in words of 32bit, not in
			 * bytes
			 */
			u32 command_size;
			u32 i;
			u32 *temp = (uint32_t *) &header;

			dprintk(KERN_INFO
				"[pid=%d] tf_copy_answers(%p): "
				"Read answers from L1\n",
				current->pid, comm);

			/* Read the answer header */
			for (i = 0;
			     i < sizeof(struct tf_answer_header)/sizeof(u32);
			       i++)
				temp[i] = comm->l1_buffer->answer_queue[
					(first_answer + i) %
						TF_S_ANSWER_QUEUE_CAPACITY];

			/* Read the answer from the L1_Buffer*/
			command_size = header.message_size +
				sizeof(struct tf_answer_header)/sizeof(u32);
			temp = (uint32_t *) &sComAnswer;
			for (i = 0; i < command_size; i++)
				temp[i] = comm->l1_buffer->answer_queue[
					(first_answer + i) %
						TF_S_ANSWER_QUEUE_CAPACITY];

			answerStructureTemp = (struct tf_answer_struct *)
				sComAnswer.header.operation_id;

			tf_dump_answer(&sComAnswer);

			memcpy(answerStructureTemp->answer, &sComAnswer,
				command_size * sizeof(u32));
			answerStructureTemp->answer_copied = true;

			first_answer += command_size;
			tf_write_reg32(&comm->l1_buffer->first_answer,
				first_answer);
		}
		spin_unlock(&(comm->lock));
	}
}

static void tf_copy_command(
	struct tf_comm *comm,
	union tf_command *command,
	struct tf_connection *connection,
	enum TF_COMMAND_STATE *command_status)
{
	if ((test_bit(TF_COMM_FLAG_L1_SHARED_ALLOCATED, &(comm->flags)))
		&& (command != NULL)) {
		/*
		 * Write the message in the message queue.
		 */

		if (*command_status == TF_COMMAND_STATE_PENDING) {
			u32 command_size;
			u32 queue_words_count;
			u32 i;
			u32 first_free_command;
			u32 first_command;

			spin_lock(&comm->lock);

			first_command = tf_read_reg32(
				&comm->l1_buffer->first_command);
			first_free_command = tf_read_reg32(
				&comm->l1_buffer->first_free_command);

			queue_words_count = first_free_command - first_command;
			command_size     = command->header.message_size +
				sizeof(struct tf_command_header)/sizeof(u32);
			if ((queue_words_count + command_size) <
				TF_N_MESSAGE_QUEUE_CAPACITY) {
				/*
				* Command queue is not full.
				* If the Command queue is full,
				* the command will be copied at
				* another iteration
				* of the current function.
				*/

				/*
				* Change the conn state
				*/
				if (connection == NULL)
					goto copy;

				spin_lock(&(connection->state_lock));

				if ((connection->state ==
				TF_CONN_STATE_NO_DEVICE_CONTEXT)
				&&
				(command->header.message_type ==
				TF_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT)) {

					dprintk(KERN_INFO
				"tf_copy_command(%p):"
				"Conn state is DEVICE_CONTEXT_SENT\n",
				 connection);
					connection->state =
			TF_CONN_STATE_CREATE_DEVICE_CONTEXT_SENT;
				} else if ((connection->state !=
				TF_CONN_STATE_VALID_DEVICE_CONTEXT)
				&&
				(command->header.message_type !=
				TF_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT)) {
					/* The connection
					* is no longer valid.
					* We may not send any command on it,
					* not even another
					* DESTROY_DEVICE_CONTEXT.
					*/
					dprintk(KERN_INFO
						"[pid=%d] tf_copy_command(%p): "
						"Connection no longer valid."
						"ABORT\n",
						current->pid, connection);
					*command_status =
						TF_COMMAND_STATE_ABORTED;
					spin_unlock(
						&(connection->state_lock));
					spin_unlock(
						&comm->lock);
					return;
				} else if (
					(command->header.message_type ==
				TF_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT) &&
				(connection->state ==
				TF_CONN_STATE_VALID_DEVICE_CONTEXT)
						) {
					dprintk(KERN_INFO
					"[pid=%d] tf_copy_command(%p): "
					"Conn state is "
					"DESTROY_DEVICE_CONTEXT_SENT\n",
					current->pid, connection);
					connection->state =
			TF_CONN_STATE_DESTROY_DEVICE_CONTEXT_SENT;
					}
					spin_unlock(&(connection->state_lock));
copy:
					/*
					* Copy the command to L1 Buffer
					*/
					dprintk(KERN_INFO
				"[pid=%d] tf_copy_command(%p): "
				"Write Message in the queue\n",
				current->pid, command);
					tf_dump_command(command);

					for (i = 0; i < command_size; i++)
						comm->l1_buffer->command_queue[
						(first_free_command + i) %
						TF_N_MESSAGE_QUEUE_CAPACITY] =
						((uint32_t *) command)[i];

					*command_status =
						TF_COMMAND_STATE_SENT;
					first_free_command += command_size;

					tf_write_reg32(
						&comm->
						l1_buffer->first_free_command,
						first_free_command);
			}
			spin_unlock(&comm->lock);
		}
	}
}

/*
 * Sends the specified message through the specified communication channel.
 *
 * This function sends the command and waits for the answer
 *
 * Returns zero upon successful completion, or an appropriate error code upon
 * failure.
 */
static int tf_send_recv(struct tf_comm *comm,
	union tf_command *command,
	struct tf_answer_struct *answerStruct,
	struct tf_connection *connection,
	int bKillable
	)
{
	int result;
	u64 timeout;
	signed long nRelativeTimeoutJiffies;
	bool wait_prepared = false;
	enum TF_COMMAND_STATE command_status = TF_COMMAND_STATE_PENDING;
	DEFINE_WAIT(wait);
#ifdef CONFIG_FREEZER
	unsigned long saved_flags;
#endif
	dprintk(KERN_INFO "[pid=%d] tf_send_recv(%p)\n",
		 current->pid, command);

#ifdef CONFIG_FREEZER
	saved_flags = current->flags;
	current->flags |= PF_KTHREAD;
#endif

	/*
	 * Read all answers from the answer queue
	 */
copy_answers:
	tf_copy_answers(comm);

	tf_copy_command(comm, command, connection, &command_status);

	/*
	 * Notify all waiting threads
	 */
	wake_up(&(comm->wait_queue));

#ifdef CONFIG_FREEZER
	if (unlikely(freezing(current))) {

		dprintk(KERN_INFO
			"Entering refrigerator.\n");
		try_to_freeze();
		dprintk(KERN_INFO
			"Left refrigerator.\n");
		goto copy_answers;
	}
#endif

#ifndef CONFIG_PREEMPT
	if (need_resched())
		schedule();
#endif

#ifdef CONFIG_TF_ZEBRA
	/*
	 * Handle RPC (if any)
	 */
	if (tf_rpc_execute(comm) == RPC_NON_YIELD)
		goto schedule_secure_world;
#endif

	/*
	 * Join wait queue
	 */
	/*dprintk(KERN_INFO "[pid=%d] tf_send_recv(%p): Prepare to wait\n",
		current->pid, command);*/
	prepare_to_wait(&comm->wait_queue, &wait,
			bKillable ? TASK_INTERRUPTIBLE : TASK_UNINTERRUPTIBLE);
	wait_prepared = true;

	/*
	 * Check if our answer is available
	 */
	if (command_status == TF_COMMAND_STATE_ABORTED) {
		/* Not waiting for an answer, return error code */
		result = -EINTR;
		dprintk(KERN_ERR "[pid=%d] tf_send_recv: "
			"Command status is ABORTED."
			"Exit with 0x%x\n",
			current->pid, result);
		goto exit;
	}
	if (answerStruct->answer_copied) {
		dprintk(KERN_INFO "[pid=%d] tf_send_recv: "
			"Received answer (type 0x%02X)\n",
			current->pid,
			answerStruct->answer->header.message_type);
		result = 0;
		goto exit;
	}

	/*
	 * Check if a signal is pending
	 */
	if (bKillable && (sigkill_pending())) {
		if (command_status == TF_COMMAND_STATE_PENDING)
			/*Command was not sent. */
			result = -EINTR;
		else
			/* Command was sent but no answer was received yet. */
			result = -EIO;

		dprintk(KERN_ERR "[pid=%d] tf_send_recv: "
			"Signal Pending. Return error %d\n",
			current->pid, result);
		goto exit;
	}

	/*
	 * Check if secure world is schedulable. It is schedulable if at
	 * least one of the following conditions holds:
	 * + it is still initializing (TF_COMM_FLAG_L1_SHARED_ALLOCATED
	 *   is not set);
	 * + there is a command in the queue;
	 * + the secure world timeout is zero.
	 */
	if (test_bit(TF_COMM_FLAG_L1_SHARED_ALLOCATED, &(comm->flags))) {
		u32 first_free_command;
		u32 first_command;
		spin_lock(&comm->lock);
		first_command = tf_read_reg32(
			&comm->l1_buffer->first_command);
		first_free_command = tf_read_reg32(
			&comm->l1_buffer->first_free_command);
		spin_unlock(&comm->lock);
		tf_read_timeout(comm, &timeout);
		if ((first_free_command == first_command) &&
			 (tf_test_s_timeout(timeout,
			&nRelativeTimeoutJiffies) == 0))
			/*
			 * If command queue is empty and if timeout has not
			 * expired secure world is not schedulable
			 */
			goto wait;
	}

	finish_wait(&comm->wait_queue, &wait);
	wait_prepared = false;

	/*
	 * Yield to the Secure World
	 */
#ifdef CONFIG_TF_ZEBRA
schedule_secure_world:
#endif

	result = tf_schedule_secure_world(comm);
	if (result < 0)
		goto exit;
	goto copy_answers;

wait:
	if (bKillable && (sigkill_pending())) {
		if (command_status == TF_COMMAND_STATE_PENDING)
			result = -EINTR; /* Command was not sent. */
		else
			/* Command was sent but no answer was received yet. */
			result = -EIO;

		dprintk(KERN_ERR "[pid=%d] tf_send_recv: "
			"Signal Pending while waiting. Return error %d\n",
			current->pid, result);
		goto exit;
	}

	if (nRelativeTimeoutJiffies == MAX_SCHEDULE_TIMEOUT)
		dprintk(KERN_INFO "[pid=%d] tf_send_recv: "
			"prepare to sleep infinitely\n", current->pid);
	else
		dprintk(KERN_INFO "tf_send_recv: "
			"prepare to sleep 0x%lx jiffies\n",
			nRelativeTimeoutJiffies);

	/* go to sleep */
	if (schedule_timeout(nRelativeTimeoutJiffies) == 0)
		dprintk(KERN_INFO
			"tf_send_recv: timeout expired\n");
	else
		dprintk(KERN_INFO
			"tf_send_recv: signal delivered\n");

	finish_wait(&comm->wait_queue, &wait);
	wait_prepared = false;
	goto copy_answers;

exit:
	if (wait_prepared) {
		finish_wait(&comm->wait_queue, &wait);
		wait_prepared = false;
	}

#ifdef CONFIG_FREEZER
	current->flags &= ~(PF_KTHREAD);
	current->flags |= (saved_flags & PF_KTHREAD);
#endif

	return result;
}

/*
 * Sends the specified message through the specified communication channel.
 *
 * This function sends the message and waits for the corresponding answer
 * It may return if a signal needs to be delivered.
 *
 * Returns zero upon successful completion, or an appropriate error code upon
 * failure.
 */
int tf_send_receive(struct tf_comm *comm,
	  union tf_command *command,
	  union tf_answer *answer,
	  struct tf_connection *connection,
	  bool bKillable)
{
	int error;
	struct tf_answer_struct answerStructure;
#ifdef CONFIG_SMP
	long ret_affinity;
	cpumask_t saved_cpu_mask;
	cpumask_t local_cpu_mask = CPU_MASK_NONE;
#endif

	answerStructure.answer = answer;
	answerStructure.answer_copied = false;

	if (command != NULL)
		command->header.operation_id = (u32) &answerStructure;

	dprintk(KERN_INFO "tf_send_receive\n");

#ifdef CONFIG_TF_ZEBRA
	if (!test_bit(TF_COMM_FLAG_PA_AVAILABLE, &comm->flags)) {
		dprintk(KERN_ERR "tf_send_receive(%p): "
			"Secure world not started\n", comm);

		return -EFAULT;
	}
#endif

	if (test_bit(TF_COMM_FLAG_TERMINATING, &(comm->flags)) != 0) {
		dprintk(KERN_DEBUG
			"tf_send_receive: Flag Terminating is set\n");
		return 0;
	}

#ifdef CONFIG_SMP
	cpu_set(0, local_cpu_mask);
	cpumask_copy(&saved_cpu_mask, tsk_cpus_allowed(current));
	ret_affinity = sched_setaffinity(0, &local_cpu_mask);
	if (ret_affinity != 0)
		dprintk(KERN_ERR "sched_setaffinity #1 -> 0x%lX", ret_affinity);
#endif


	/*
	 * Send the command
	 */
	error = tf_send_recv(comm,
		command, &answerStructure, connection, bKillable);

	if (!bKillable && sigkill_pending()) {
		if ((command->header.message_type ==
			TF_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT) &&
			(answer->create_device_context.error_code ==
				S_SUCCESS)) {

			/*
			 * CREATE_DEVICE_CONTEXT was interrupted.
			 */
			dprintk(KERN_INFO "tf_send_receive: "
				"sending DESTROY_DEVICE_CONTEXT\n");
			answerStructure.answer =  answer;
			answerStructure.answer_copied = false;

			command->header.message_type =
				TF_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT;
			command->header.message_size =
				(sizeof(struct
					tf_command_destroy_device_context) -
				 sizeof(struct tf_command_header))/sizeof(u32);
			command->header.operation_id =
				(u32) &answerStructure;
			command->destroy_device_context.device_context =
				answer->create_device_context.
					device_context;

			goto destroy_context;
		}
	}

	if (error == 0) {
		/*
		 * tf_send_recv returned Success.
		 */
		if (command->header.message_type ==
		TF_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT) {
			spin_lock(&(connection->state_lock));
			connection->state = TF_CONN_STATE_VALID_DEVICE_CONTEXT;
			spin_unlock(&(connection->state_lock));
		} else if (command->header.message_type ==
		TF_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT) {
			spin_lock(&(connection->state_lock));
			connection->state = TF_CONN_STATE_NO_DEVICE_CONTEXT;
			spin_unlock(&(connection->state_lock));
		}
	} else if (error  == -EINTR) {
		/*
		* No command was sent, return failure.
		*/
		dprintk(KERN_ERR
			"tf_send_receive: "
			"tf_send_recv failed (error %d) !\n",
			error);
	} else if (error  == -EIO) {
		/*
		* A command was sent but its answer is still pending.
		*/

		/* means bKillable is true */
		dprintk(KERN_ERR
			"tf_send_receive: "
			"tf_send_recv interrupted (error %d)."
			"Send DESTROY_DEVICE_CONTEXT.\n", error);

		/* Send the DESTROY_DEVICE_CONTEXT. */
		answerStructure.answer =  answer;
		answerStructure.answer_copied = false;

		command->header.message_type =
			TF_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT;
		command->header.message_size =
			(sizeof(struct tf_command_destroy_device_context) -
				sizeof(struct tf_command_header))/sizeof(u32);
		command->header.operation_id =
			(u32) &answerStructure;
		command->destroy_device_context.device_context =
			connection->device_context;

		error = tf_send_recv(comm,
			command, &answerStructure, connection, false);
		if (error == -EINTR) {
			/*
			* Another thread already sent
			* DESTROY_DEVICE_CONTEXT.
			* We must still wait for the answer
			* to the original command.
			*/
			command = NULL;
			goto destroy_context;
		} else {
			 /* An answer was received.
			 * Check if it is the answer
			 * to the DESTROY_DEVICE_CONTEXT.
			 */
			 spin_lock(&comm->lock);
			 if (answer->header.message_type !=
			 TF_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT) {
				answerStructure.answer_copied = false;
			 }
			 spin_unlock(&comm->lock);
			 if (!answerStructure.answer_copied) {
				/* Answer to DESTROY_DEVICE_CONTEXT
				* was not yet received.
				* Wait for the answer.
				*/
				dprintk(KERN_INFO
					"[pid=%d] tf_send_receive:"
					"Answer to DESTROY_DEVICE_CONTEXT"
					"not yet received.Retry\n",
					current->pid);
				command = NULL;
				goto destroy_context;
			 }
		}
	}

	dprintk(KERN_INFO "tf_send_receive(): Message answer ready\n");
	goto exit;

destroy_context:
	error = tf_send_recv(comm,
	command, &answerStructure, connection, false);

	/*
	 * tf_send_recv cannot return an error because
	 * it's not killable and not within a connection
	 */
	BUG_ON(error != 0);

	/* Reset the state, so a new CREATE DEVICE CONTEXT can be sent */
	spin_lock(&(connection->state_lock));
	connection->state = TF_CONN_STATE_NO_DEVICE_CONTEXT;
	spin_unlock(&(connection->state_lock));

exit:

#ifdef CONFIG_SMP
	ret_affinity = sched_setaffinity(0, &saved_cpu_mask);
	if (ret_affinity != 0)
		dprintk(KERN_ERR "sched_setaffinity #2 -> 0x%lX", ret_affinity);
#endif
	return error;
}

/*----------------------------------------------------------------------------
 * Power management
 *----------------------------------------------------------------------------*/


/*
 * Handles all the power management calls.
 * The operation is the type of power management
 * operation to be performed.
 *
 * This routine will only return if a failure occured or if
 * the required opwer management is of type "resume".
 * "Hibernate" and "Shutdown" should lock when doing the
 * corresponding SMC to the Secure World
 */
int tf_power_management(struct tf_comm *comm,
	enum TF_POWER_OPERATION operation)
{
	u32 status;
	int error = 0;

	dprintk(KERN_INFO "tf_power_management(%d)\n", operation);

#ifdef CONFIG_TF_ZEBRA
	if (!test_bit(TF_COMM_FLAG_PA_AVAILABLE, &comm->flags)) {
		dprintk(KERN_INFO "tf_power_management(%p): "
			"succeeded (not started)\n", comm);

		return 0;
	}
#endif

	status = ((tf_read_reg32(&(comm->l1_buffer->status_s))
		& TF_STATUS_POWER_STATE_MASK)
		>> TF_STATUS_POWER_STATE_SHIFT);

	switch (operation) {
	case TF_POWER_OPERATION_SHUTDOWN:
		switch (status) {
		case TF_POWER_MODE_ACTIVE:
			error = tf_pm_shutdown(comm);

			if (error) {
				dprintk(KERN_ERR "tf_power_management(): "
					"Failed with error code 0x%08x\n",
					error);
				goto error;
			}
			break;

		default:
			goto not_allowed;
		}
		break;

	case TF_POWER_OPERATION_HIBERNATE:
		switch (status) {
		case TF_POWER_MODE_ACTIVE:
			error = tf_pm_hibernate(comm);

			if (error) {
				dprintk(KERN_ERR "tf_power_management(): "
					"Failed with error code 0x%08x\n",
					error);
				goto error;
			}
			break;

		default:
			goto not_allowed;
		}
		break;

	case TF_POWER_OPERATION_RESUME:
		error = tf_pm_resume(comm);

		if (error != 0) {
			dprintk(KERN_ERR "tf_power_management(): "
				"Failed with error code 0x%08x\n",
				error);
			goto error;
		}
		break;
	}

	dprintk(KERN_INFO "tf_power_management(): succeeded\n");
	return 0;

not_allowed:
	dprintk(KERN_ERR "tf_power_management(): "
		"Power command not allowed in current "
		"Secure World state %d\n", status);
	error = -ENOTTY;
error:
	return error;
}

