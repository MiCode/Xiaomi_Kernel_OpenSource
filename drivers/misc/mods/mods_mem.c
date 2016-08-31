/*
 * mods_mem.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"

#include <linux/pagemap.h>
#include <linux/rbtree.h>

#ifdef CONFIG_BIGPHYS_AREA
#include <linux/bigphysarea.h>
#endif

#define P2M(x) ((x) >> (20 - PAGE_SHIFT))

static spinlock_t km_lock;
static NvU32 km_usage;

static struct rb_root km_root;
static int mods_post_alloc(NvU64 vaddr, NvU64 paddr, NvU64 pages,
						   NvU32 cachetype);

struct mem_tracker {
	void	*addr;
	NvU32	 size;
	const char *file;
	NvU32	 line;
	struct rb_node node;
};

/************************************************************************ */
/************************************************************************ */
/**  Kernel memory allocation tracker					  */
/**  Register all the allocation from the beginning and inform		  */
/**  about the memory leakage at unload time				  */
/************************************************************************ */
/************************************************************************ */

/*********************
 * PRIVATE FUNCTIONS *
 *********************/

/* Insert pmem_t into the indicated red-black tree, using pmem_t->addr
 * as the sort key.  Return 1 on success, or 0 if the insert failed
 * because there is already a node in the tree with that addr.
 */
static int mods_insert_mem_tracker(struct rb_root     *root,
				   struct mem_tracker *pmem_t)

{
	struct rb_node *parent_node = NULL;
	struct mem_tracker *parent_data = NULL;
	struct rb_node **child_ptr = &root->rb_node;

	while (*child_ptr != NULL) {
		parent_node = *child_ptr;
		parent_data = rb_entry(parent_node, struct mem_tracker, node);
		if (pmem_t->addr < parent_data->addr)
			child_ptr = &parent_node->rb_left;
		else if (pmem_t->addr > parent_data->addr)
			child_ptr = &parent_node->rb_right;
		else
			return 0;
	}

	rb_link_node(&pmem_t->node, parent_node, child_ptr);
	rb_insert_color(&pmem_t->node, root);
	return 1;
}

/* Search the red-black tree at root for a mem_tracker with the
 * indicated address.  Return the node on success, or NULL on failure.
 */
static struct mem_tracker *mods_find_mem_tracker(struct rb_root *root,
						 void		*addr)
{
	struct rb_node *node = root->rb_node;
	struct mem_tracker *pmem_t;

	while (node != NULL) {
		pmem_t = rb_entry(node, struct mem_tracker, node);
		if (addr < pmem_t->addr)
			node = node->rb_left;
		else if (addr > pmem_t->addr)
			node = node->rb_right;
		else
			return pmem_t;
	}
	return NULL;
}

static void mods_list_mem(void)
{
	struct rb_root		*root = &km_root;
	struct rb_node		*iter;
	struct mem_tracker	*pmem_t;

	for (iter = rb_first(root); iter; iter = rb_next(iter)) {
		pmem_t = rb_entry(iter, struct mem_tracker, node);

		mods_debug_printk(DEBUG_MEM,
				  "leak: virt %p, size 0x%x, "
				  "alloc'd by %s:%d\n",
				  pmem_t->addr,
				  (unsigned int) pmem_t->size,
				  pmem_t->file,
				  (unsigned int) pmem_t->line);
	}
}

static void mods_del_list_mem(void)
{
	struct rb_root		*root = &km_root;
	struct rb_node		*node;
	struct mem_tracker	*pmem_t;

	while (!RB_EMPTY_ROOT(root)) {
		node = rb_first(root);
		pmem_t = rb_entry(node, struct mem_tracker, node);

		/* free the memory */
		rb_erase(node, root);
		MODS_FORCE_KFREE(pmem_t->addr);
		MEMDBG_FREE(pmem_t);
	}
}

#if !defined(CONFIG_ARCH_TEGRA) || defined(CONFIG_CPA) ||\
	defined(CONFIG_ARCH_TEGRA_3x_SOC)
static int mods_set_mem_type(NvU64 virt_addr, NvU64 pages, NvU32 type)
{
	if (type == MODS_MEMORY_UNCACHED)
		return MODS_SET_MEMORY_UC(virt_addr, pages);
	else if (type == MODS_MEMORY_WRITECOMBINE)
		return MODS_SET_MEMORY_WC(virt_addr, pages);
	return 0;
}
#endif

static int mods_restore_mem_type(NvU64 virt_addr,
				 NvU64 pages,
				 NvU32 type_override)
{
	if ((type_override == MODS_MEMORY_UNCACHED) ||
			(type_override == MODS_MEMORY_WRITECOMBINE)) {
		return MODS_SET_MEMORY_WB(virt_addr, pages);
	}
	return 0;
}

static void mods_free_contig_pages(struct SYS_MEM_MODS_INFO *p_mem_info)
{
#ifdef CONFIG_BIGPHYS_AREA
	if (p_mem_info->alloc_type == MODS_ALLOC_TYPE_BIGPHYS_AREA) {
		bigphysarea_free_pages((void *)p_mem_info->logical_addr);
	} else
#endif
		__MODS_FREE_PAGES(p_mem_info->p_page, p_mem_info->order);
}

static void mods_alloc_contig_sys_pages(struct SYS_MEM_MODS_INFO *p_mem_info)
{
	NvU32 order = 0;
	NvU64 phys_addr;
	NvU32 num_pages = 0;
	NvU32 i_page = 0;
	LOG_ENT();

	while ((1 << order) < p_mem_info->num_pages)
		order++;
	p_mem_info->order = order;
	num_pages = 1 << order;

	__MODS_ALLOC_PAGES(p_mem_info->p_page, order,
		GFP_KERNEL | __GFP_COMP
		| (((p_mem_info->addr_bits & 0xff) == 32)
			? __GFP_DMA32 : __GFP_HIGHMEM),
		p_mem_info->numa_node
		);

#ifdef CONFIG_BIGPHYS_AREA
	if (p_mem_info->p_page == NULL) {
		mods_debug_printk(DEBUG_MEM,
	"failed to allocate %u contiguous pages, falling back to bigphysarea\n",
				  num_pages);
		p_mem_info->logical_addr = (NvU64)
			bigphysarea_alloc_pages(num_pages, 0, GFP_KERNEL);
		p_mem_info->alloc_type = MODS_ALLOC_TYPE_BIGPHYS_AREA;
	}
#endif

	if (p_mem_info->p_page == NULL && p_mem_info->logical_addr == 0) {
		LOG_EXT();
		return;
	}

#ifdef CONFIG_BIGPHYS_AREA
	if (p_mem_info->alloc_type == MODS_ALLOC_TYPE_BIGPHYS_AREA) {
		phys_addr = __pa(p_mem_info->logical_addr);
	} else
#endif
		phys_addr = page_to_phys(p_mem_info->p_page);
	if (phys_addr == 0) {
		mods_error_printk(
		"alloc_contig_sys_pages: failed to lookup physical address\n");
		mods_free_contig_pages(p_mem_info);
		p_mem_info->logical_addr = 0;
		LOG_EXT();
		return;
	}
	p_mem_info->dma_addr = MODS_PHYS_TO_DMA(phys_addr);

	mods_debug_printk(DEBUG_MEM_DETAILED,
	"alloc_contig_sys_pages: alloc'd %u contig pages @ dma addr 0x%llu\n",
			  num_pages, p_mem_info->dma_addr);

	if (((p_mem_info->addr_bits & 0xFF) == 32) &&
		(p_mem_info->dma_addr + p_mem_info->length > 0x100000000ULL)) {

		mods_error_printk(
	"alloc_contig_sys_pages: alloc'd memory exceeds 32-bit addressing\n");
		mods_free_contig_pages(p_mem_info);
		p_mem_info->logical_addr = 0;
		LOG_EXT();
		return;
	}

	for (i_page = 0; i_page < num_pages; i_page++) {
		NvU64 ptr = 0;
#ifdef CONFIG_BIGPHYS_AREA
		if (p_mem_info->alloc_type == MODS_ALLOC_TYPE_BIGPHYS_AREA) {
			ptr = p_mem_info->logical_addr + i_page * PAGE_SIZE;
		} else
#endif
			ptr = (NvU64)(size_t)kmap(p_mem_info->p_page + i_page);
		if (ptr == 0) {
			mods_error_printk(
			    "alloc_contig_sys_pages: unable to map pages\n");
			mods_free_contig_pages(p_mem_info);
			p_mem_info->logical_addr = 0;
			LOG_EXT();
			return;
		}
		if (mods_post_alloc(ptr,
				    phys_addr,
				    1,
				    p_mem_info->cache_type)) {
			mods_error_printk(
		"alloc_contig_sys_pages: failed to set caching type\n");
			mods_free_contig_pages(p_mem_info);
			p_mem_info->logical_addr = 0;
			LOG_EXT();
			return;
		}
#ifdef CONFIG_BIGPHYS_AREA
		if (p_mem_info->alloc_type != MODS_ALLOC_TYPE_BIGPHYS_AREA)
#endif
			kunmap(p_mem_info->p_page + i_page);
	}
	LOG_EXT();
}

static void mods_free_contig_sys_mem(struct SYS_MEM_MODS_INFO *p_mem_info)
{
	NvU32 num_pages = 1 << p_mem_info->order;
	NvU32 i_page = 0;

	for (i_page = 0; i_page < num_pages; i_page++) {
		NvU64 ptr = 0;
#ifdef CONFIG_BIGPHYS_AREA
		if (p_mem_info->alloc_type == MODS_ALLOC_TYPE_BIGPHYS_AREA) {
			ptr = p_mem_info->logical_addr + i_page * PAGE_SIZE;
		} else
#endif
			ptr = (NvU64)(size_t)kmap(p_mem_info->p_page + i_page);
		mods_restore_mem_type(ptr, 1, p_mem_info->cache_type);
#ifdef CONFIG_BIGPHYS_AREA
		if (p_mem_info->alloc_type != MODS_ALLOC_TYPE_BIGPHYS_AREA)
#endif
			kunmap(p_mem_info->p_page + i_page);
	}
	mods_free_contig_pages(p_mem_info);
}

static void mods_free_noncontig_sys_mem(struct SYS_MEM_MODS_INFO *p_mem_info)
{
	int i;
	int pta_size;
	struct SYS_PAGE_TABLE *pt;

	pta_size = p_mem_info->num_pages * sizeof(pt);

	if (p_mem_info->p_page_tbl) {
		for (i = 0; i < p_mem_info->num_pages; i++) {
			void *ptr;
			pt = p_mem_info->p_page_tbl[i];
			if (!pt)
				continue;
			if (!pt->p_page) {
				MODS_KFREE(pt, sizeof(*pt));
				continue;
			}
			ptr = kmap(pt->p_page);
			if (ptr != NULL) {
				mods_restore_mem_type((NvU64)(size_t)ptr,
						      1,
						      p_mem_info->cache_type);
				kunmap(pt->p_page);
			}
			__MODS_FREE_PAGES(pt->p_page, 0);
			MODS_KFREE(pt, sizeof(*pt));
		}
		MODS_KFREE(p_mem_info->p_page_tbl, pta_size);
		p_mem_info->p_page_tbl = 0;
	}
}

static void mods_alloc_noncontig_sys_pages(struct SYS_MEM_MODS_INFO *p_mem_info)
{
	int pta_size;
	int i;
	struct SYS_PAGE_TABLE *pt;

	LOG_ENT();

	pta_size = p_mem_info->num_pages * sizeof(pt);

	MODS_KMALLOC(p_mem_info->p_page_tbl, pta_size);
	if (unlikely(!p_mem_info->p_page_tbl))
		goto failed;
	memset(p_mem_info->p_page_tbl, 0, pta_size);

	/* allocate resources */
	for (i = 0; i < p_mem_info->num_pages; i++) {
		MODS_KMALLOC(p_mem_info->p_page_tbl[i], sizeof(*pt));
		if (unlikely(!p_mem_info->p_page_tbl[i]))
			goto failed;
		memset(p_mem_info->p_page_tbl[i], 0, sizeof(*pt));
	}

	/* alloc pages */
	for (i = 0; i < p_mem_info->num_pages; i++) {
		NvU64 phys_addr = 0;
		pt = p_mem_info->p_page_tbl[i];

		__MODS_ALLOC_PAGES(pt->p_page, 0, GFP_KERNEL
			| (((p_mem_info->addr_bits & 0xff) == 32)
				? __GFP_DMA32 : __GFP_HIGHMEM),
			p_mem_info->numa_node
			);
		if (pt->p_page == NULL) {
			mods_error_printk(
			"can't allocate single page with alloc_pages\n");
			goto failed;
		}
		phys_addr = page_to_phys(pt->p_page);
		if (phys_addr == 0) {
			mods_error_printk(
		"alloc_noncontig_sys_pages: failed to lookup phys addr\n");
			goto failed;
		}
		pt->dma_addr = MODS_PHYS_TO_DMA(phys_addr);
		mods_debug_printk(DEBUG_MEM_DETAILED,
				  "%d-th page is alloc'd, dma_addr=0x%llx\n",
				  i,
				  pt->dma_addr);

		{
			void *ptr = kmap(pt->p_page);
			int ret;
			if (ptr == NULL) {
				mods_error_printk(
			"alloc_noncontig_sys_pages: unable to map page\n");
				goto failed;
			}
			ret = mods_post_alloc((NvU64)(size_t)ptr,
					      phys_addr,
					      1,
					      p_mem_info->cache_type);
			kunmap(pt->p_page);
			if (ret) {
				mods_error_printk(
	"alloc_noncontig_sys_pages: failed to set caching type to uncached\n");
				goto failed;
			}
		}
	}

	return;

failed:
	mods_free_noncontig_sys_mem(p_mem_info);
}

static void mods_register_alloc(struct file *fp,
				struct SYS_MEM_MODS_INFO *p_mem_info)
{
	MODS_PRIVATE_DATA(private_data, fp);
	spin_lock(&private_data->lock);
	list_add(&p_mem_info->list, private_data->mods_alloc_list);
	spin_unlock(&private_data->lock);
}

static void mods_unregister_and_free(struct file *fp,
				     struct SYS_MEM_MODS_INFO *p_del_mem)
{
	struct SYS_MEM_MODS_INFO	*p_mem_info;

	MODS_PRIVATE_DATA(private_data, fp);
	struct list_head  *head;
	struct list_head  *iter;

	spin_lock(&private_data->lock);

	head = private_data->mods_alloc_list;

	list_for_each(iter, head) {
		p_mem_info = list_entry(iter, struct SYS_MEM_MODS_INFO, list);

		if (p_del_mem == p_mem_info) {
			/* remove from the list */
			list_del(iter);

			spin_unlock(&private_data->lock);

			if (p_mem_info->alloc_type !=
					MODS_ALLOC_TYPE_NON_CONTIG) {
				/* was a contiguous alloc */
				mods_free_contig_sys_mem(p_mem_info);
			} else {
				/* was a normal, noncontiguous alloc */
				mods_free_noncontig_sys_mem(p_mem_info);
			}

			/* free our data struct that keeps track of this
			 * allocation */
			MODS_KFREE(p_mem_info, sizeof(*p_mem_info));

			return;
		}
	}

	spin_unlock(&private_data->lock);

	mods_error_printk(
		"mods_unregister_and_free: can't unregister allocation");
}

/********************
 * PUBLIC FUNCTIONS *
 ********************/
void mods_init_mem(void)
{
	km_root = RB_ROOT;
	spin_lock_init(&km_lock);
	km_usage = 0;
}

/* implements mods kmalloc */
void mods_add_mem(void *addr, NvU32 size, const char *file, NvU32 line)
{
	struct mem_tracker *mem_t;
	unsigned long __eflags;

	spin_lock_irqsave(&km_lock, __eflags);

	km_usage += size;

	MEMDBG_ALLOC(mem_t, sizeof(struct mem_tracker));
	if (mem_t == NULL) {
		spin_unlock_irqrestore(&km_lock, __eflags);
		return;
	}
	mem_t->addr = addr;
	mem_t->size = size;
	mem_t->file = file;
	mem_t->line = line;

	if (!mods_insert_mem_tracker(&km_root, mem_t)) {
		mods_error_printk(
			"mods_add_mem already alloc'd the address\n");
	}

	spin_unlock_irqrestore(&km_lock, __eflags);
}

/* implements mods kfree */
void mods_del_mem(void *addr, NvU32 size, const char *file, NvU32 line)
{
	struct rb_root	   *root = &km_root;
	struct mem_tracker *pmem_t;
	unsigned long __eflags;

	spin_lock_irqsave(&km_lock, __eflags);

	km_usage -= size;

	pmem_t = mods_find_mem_tracker(root, addr);
	if (pmem_t) {
		if (pmem_t->size != size)
			mods_error_printk(
				"mods_del_mem size mismatch on free\n");
		rb_erase(&pmem_t->node, root);
		MEMDBG_FREE(pmem_t);
	} else {
		/* no allocation with given address */
		mods_error_printk(
			"mods_del_mem no allocation with given address\n");
	}

	spin_unlock_irqrestore(&km_lock, __eflags);
}

void mods_check_mem(void)
{
	if (km_usage != 0) {
		mods_warning_printk("memory leaks detected: 0x%x bytes\n",
				    km_usage);
		mods_list_mem();
		mods_del_list_mem();
	}
}

void mods_unregister_all_alloc(struct file *fp)
{
	struct SYS_MEM_MODS_INFO	*p_mem_info;

	MODS_PRIVATE_DATA(private_data, fp);
	struct list_head  *head = private_data->mods_alloc_list;
	struct list_head  *iter;
	struct list_head  *tmp;

	list_for_each_safe(iter, tmp, head) {
		p_mem_info = list_entry(iter, struct SYS_MEM_MODS_INFO, list);
		mods_unregister_and_free(fp, p_mem_info);
	}
}

/* Returns an offset of given dma address
 * If dma address doesn't belong to the allocation, returns ERROR
 */
int mods_get_alloc_offset(struct SYS_MEM_MODS_INFO *p_mem_info,
			  NvU64 dma_addr,
			  NvU32 *ret_offs)
{
	int i;
	int offset = 0;

	if (p_mem_info->alloc_type != MODS_ALLOC_TYPE_NON_CONTIG) {
		if (p_mem_info->dma_addr <= dma_addr &&
		    p_mem_info->dma_addr + p_mem_info->length > dma_addr) {

			*ret_offs = dma_addr - p_mem_info->dma_addr;
			return OK;
		}
	} else {
		/* Non-contiguous: one page at a time */
		for (i = 0; i < p_mem_info->num_pages; i++) {
			NvU64 start_addr = p_mem_info->p_page_tbl[i]->dma_addr;
			if (start_addr <= dma_addr &&
			    start_addr + PAGE_SIZE > dma_addr) {

				offset = offset + dma_addr - start_addr;
				*ret_offs = offset;
				return OK;
			}
			offset += PAGE_SIZE;
		}
	}

	/* Physical address doesn't belong to the allocation */
	return ERROR;
}

struct SYS_MEM_MODS_INFO *mods_find_alloc(struct file *fp, NvU64 phys_addr)
{
	MODS_PRIVATE_DATA(private_data, fp);
	struct list_head	  *plist_head = private_data->mods_alloc_list;
	struct list_head	  *plist_iter;
	struct SYS_MEM_MODS_INFO  *p_mem_info;
	NvU32			   offset;

	list_for_each(plist_iter, plist_head) {
		int ret;
		p_mem_info = list_entry(plist_iter,
					struct SYS_MEM_MODS_INFO,
					list);
		ret = mods_get_alloc_offset(p_mem_info, phys_addr, &offset);
		if (ret == OK)
			return p_mem_info;
	}
	/* physical address doesn't belong to any memory allocation */
	return NULL;
}

/************************
 * ESCAPE CALL FUNCTONS *
 ************************/

int esc_mods_device_alloc_pages(struct file *fp,
				struct MODS_DEVICE_ALLOC_PAGES *p)
{
	struct SYS_MEM_MODS_INFO *p_mem_info;

	LOG_ENT();

	switch (p->attrib) {
	case MODS_MEMORY_CACHED:
	case MODS_MEMORY_UNCACHED:
	case MODS_MEMORY_WRITECOMBINE:
		break;

	default:
		mods_error_printk("invalid memory type: %u\n", p->attrib);
		return -EINVAL;
	}

	MODS_KMALLOC(p_mem_info, sizeof(*p_mem_info));
	if (unlikely(!p_mem_info)) {
		LOG_EXT();
		return -ENOMEM;
	}

	p_mem_info->alloc_type = p->contiguous ? MODS_ALLOC_TYPE_CONTIG
					       : MODS_ALLOC_TYPE_NON_CONTIG;
	p_mem_info->cache_type = p->attrib;
	p_mem_info->length = p->num_bytes;
	p_mem_info->order = 0;
	p_mem_info->k_mapping_ref_cnt = 0;
	p_mem_info->logical_addr = 0;
	p_mem_info->p_page_tbl = NULL;
	p_mem_info->addr_bits = p->address_bits;
	p_mem_info->p_page = NULL;
	p_mem_info->num_pages =
		(p->num_bytes >> PAGE_SHIFT) + ((p->num_bytes & ~PAGE_MASK) ? 1
									  : 0);

	p_mem_info->numa_node = numa_node_id();
#ifdef MODS_HAS_DEV_TO_NUMA_NODE
	if (p->pci_device.bus || p->pci_device.device) {
		unsigned int devfn = PCI_DEVFN(p->pci_device.device,
					       p->pci_device.function);
		struct pci_dev *dev = MODS_PCI_GET_SLOT(p->pci_device.bus,
							devfn);

		if (dev == NULL)
			return -EINVAL;
		p_mem_info->numa_node = dev_to_node(&dev->dev);
		mods_debug_printk(DEBUG_MEM_DETAILED,
			"esc_mods_alloc_pages affinity %x:%x.%x node %d\n",
			p->pci_device.bus,
			p->pci_device.device,
			p->pci_device.function,
			p_mem_info->numa_node);
	}
#endif

	mods_debug_printk(
		DEBUG_MEM_DETAILED,
		"esc_mods_alloc_pages - alloc %d %s pages on node %d\n",
		(int)p_mem_info->num_pages,
		p->contiguous ? "contiguous" : "noncontiguous",
		p_mem_info->numa_node);

	p->memory_handle = 0;

	if (p->contiguous) {
		mods_alloc_contig_sys_pages(p_mem_info);
		if ((p_mem_info->logical_addr == 0) &&
		    (p_mem_info->p_page == NULL)) {

			mods_error_printk(
				"failed to allocate %u contiguous bytes\n",
				1 << p_mem_info->length);
			MODS_KFREE(p_mem_info, sizeof(*p_mem_info));
			LOG_EXT();
			return -ENOMEM;
		}
	} else {
		mods_alloc_noncontig_sys_pages(p_mem_info);
		if (p_mem_info->p_page_tbl == NULL) {
			mods_error_printk(
			    "failed to alloc noncontiguous system pages\n");
			MODS_KFREE(p_mem_info, sizeof(*p_mem_info));
			LOG_EXT();
			return -ENOMEM;
		}
	}

	p->memory_handle = (NvU64) (long) p_mem_info;

	/* Register the allocation of the memory */
	mods_register_alloc(fp, p_mem_info);
	LOG_EXT();
	return OK;
}

int esc_mods_alloc_pages(struct file *fp, struct MODS_ALLOC_PAGES *p)
{
	struct MODS_DEVICE_ALLOC_PAGES dev_alloc_pages;
	int ret;
	LOG_ENT();
	dev_alloc_pages.num_bytes	   = p->num_bytes;
	dev_alloc_pages.contiguous	   = p->contiguous;
	dev_alloc_pages.address_bits	   = p->address_bits;
	dev_alloc_pages.attrib		   = p->attrib;
	dev_alloc_pages.pci_device.bus	   = 0;
	dev_alloc_pages.pci_device.device   = 0;
	dev_alloc_pages.pci_device.function = 0;
	ret = esc_mods_device_alloc_pages(fp, &dev_alloc_pages);
	if (!ret)
		p->memory_handle = dev_alloc_pages.memory_handle;
	LOG_EXT();
	return ret;
}

int esc_mods_free_pages(struct file *fp, struct MODS_FREE_PAGES *p)
{
	LOG_ENT();

	/* unregister and free the allocation of the memory */
	mods_unregister_and_free(fp,
				 (struct SYS_MEM_MODS_INFO *) (long)
					p->memory_handle);

	LOG_EXT();

	return OK;
}

int esc_mods_set_mem_type(struct file *fp, struct MODS_MEMORY_TYPE *p)
{
	struct SYS_MEM_MODS_INFO *p_mem_info;
	MODS_PRIVATE_DATA(private_data, fp);

	LOG_ENT();

	spin_lock(&private_data->lock);

	p_mem_info = mods_find_alloc(fp, p->physical_address);
	if (p_mem_info != NULL) {
		spin_unlock(&private_data->lock);
		mods_error_printk(
	"unable to change mem type of an addr which was already alloc'd!\n");
		LOG_EXT();
		return -EINVAL;
	}

	switch (p->type) {
	case MODS_MEMORY_CACHED:
	case MODS_MEMORY_UNCACHED:
	case MODS_MEMORY_WRITECOMBINE:
		break;

	default:
		spin_unlock(&private_data->lock);
		mods_error_printk("invalid memory type: %u\n", p->type);
		LOG_EXT();
		return -EINVAL;
	}

	private_data->mem_type.dma_addr = p->physical_address;
	private_data->mem_type.size = p->size;
	private_data->mem_type.type = p->type;

	spin_unlock(&private_data->lock);

	LOG_EXT();
	return OK;
}

int esc_mods_get_phys_addr(struct file *fp, struct MODS_GET_PHYSICAL_ADDRESS *p)
{
	struct SYS_MEM_MODS_INFO *p_mem_info
		= (struct SYS_MEM_MODS_INFO *)(long)p->memory_handle;
	NvU32	page_n;
	NvU32	page_offs;

	LOG_ENT();

	if (p_mem_info->alloc_type != MODS_ALLOC_TYPE_NON_CONTIG) {
		p->physical_address = p_mem_info->dma_addr + p->offset;
	} else {
		page_n = p->offset >> PAGE_SHIFT;
		page_offs = p->offset % PAGE_SIZE;

		if (page_n >= p_mem_info->num_pages) {
			mods_error_printk(
			"get_phys_addr query exceeds allocation's boundary!\n");
			LOG_EXT();
			return -EINVAL;
		}
		mods_debug_printk(DEBUG_MEM_DETAILED,
	"esc_mods_get_phys_addr with offs=0x%x => page_n=%d, page_offs=0x%x\n",
				  (int) p->offset,
				  (int) page_n,
				  (int) page_offs);

		p->physical_address =
			p_mem_info->p_page_tbl[page_n]->dma_addr + page_offs;

		mods_debug_printk(DEBUG_MEM_DETAILED,
	"esc_mods_get_phys_addr: dma_addr 0x%llx, returned phys_addr 0x%llx\n",
				  p_mem_info->p_page_tbl[page_n]->dma_addr,
				  p->physical_address);
	}
	LOG_EXT();
	return OK;
}

int esc_mods_virtual_to_phys(struct file *fp,
			     struct MODS_VIRTUAL_TO_PHYSICAL *p)
{
	struct MODS_GET_PHYSICAL_ADDRESS get_phys_addr;
	struct SYS_MAP_MEMORY *p_map_mem;
	MODS_PRIVATE_DATA(private_data, fp);
	struct list_head *head;
	struct list_head *iter;
	NvU32	phys_offset;
	NvU32	virt_offset;
	NvU32	rc;

	LOG_ENT_C("virt_addr=0x%llx\n", p->virtual_address);

	spin_lock(&private_data->lock);

	head = private_data->mods_mapping_list;

	list_for_each(iter, head) {
		p_map_mem = list_entry(iter, struct SYS_MAP_MEMORY, list);

		if (p_map_mem->virtual_addr <= p->virtual_address &&
			p_map_mem->virtual_addr + p_map_mem->mapping_length
			> p->virtual_address) {

			virt_offset = p->virtual_address
				      - p_map_mem->virtual_addr;

			if (p_map_mem->contiguous) {
				p->physical_address = p_map_mem->dma_addr
						      + virt_offset;
				spin_unlock(&private_data->lock);
				LOG_EXT_C("phys: 0x%llx\n",
					  p->physical_address);
				return OK;
			}

			/* non-contiguous */
			if (mods_get_alloc_offset(p_map_mem->p_mem_info,
						  p_map_mem->dma_addr,
						  &phys_offset) != OK) {
				spin_unlock(&private_data->lock);
				return -EINVAL;
			}

			get_phys_addr.memory_handle =
				(NvU64)(long)p_map_mem->p_mem_info;
			get_phys_addr.offset = virt_offset + phys_offset;

			spin_unlock(&private_data->lock);

			rc = esc_mods_get_phys_addr(fp, &get_phys_addr);
			if (rc != OK)
				return rc;

			p->physical_address = get_phys_addr.physical_address;
			LOG_EXT_C("phys: 0x%llx\n", p->physical_address);
			return OK;
		}
	}

	spin_unlock(&private_data->lock);

	mods_error_printk(
		"esc_mods_virtual_to_phys query has invalid virt addr\n");
	return -EINVAL;
}

int esc_mods_phys_to_virtual(struct file *fp,
			     struct MODS_PHYSICAL_TO_VIRTUAL *p)
{
	struct SYS_MAP_MEMORY *p_map_mem;
	MODS_PRIVATE_DATA(private_data, fp);
	struct list_head *head;
	struct list_head *iter;
	NvU32	offset;
	NvU32	map_offset;

	LOG_ENT_C("physAddr=0x%llx\n", p->physical_address);

	spin_lock(&private_data->lock);

	head = private_data->mods_mapping_list;

	list_for_each(iter, head) {
		p_map_mem = list_entry(iter, struct SYS_MAP_MEMORY, list);

		if (p_map_mem->contiguous) {
			if (p_map_mem->dma_addr <= p->physical_address &&
			    p_map_mem->dma_addr + p_map_mem->mapping_length
			    > p->physical_address) {

				offset = p->physical_address
					 - p_map_mem->dma_addr;
				p->virtual_address = p_map_mem->virtual_addr
						     + offset;
				spin_unlock(&private_data->lock);
				LOG_EXT_C("virt:0x%llx\n", p->virtual_address);
				return OK;
			}
			continue;
		}

		/* non-contiguous */
		if (mods_get_alloc_offset(p_map_mem->p_mem_info,
					  p->physical_address,
					  &offset))
			continue;

		/* offset the mapping starts from */
		if (mods_get_alloc_offset(p_map_mem->p_mem_info,
					  p_map_mem->dma_addr,
					  &map_offset))
			continue;

		if ((map_offset <= offset) &&
			(map_offset + p_map_mem->mapping_length > offset)) {

			p->virtual_address = p_map_mem->virtual_addr + offset
								- map_offset;
			spin_unlock(&private_data->lock);
			LOG_EXT_C("virt:0x%llx\n", p->virtual_address);
			return OK;
		}
	}
	spin_unlock(&private_data->lock);
	mods_error_printk(
		"esc_mods_virtual_to_phys query has invalid phys_addr\n");
	return -EINVAL;
}

int esc_mods_memory_barrier(struct file *fp)
{
	wmb();
	return OK;
}

#ifdef CONFIG_ARCH_TEGRA

static void clear_contiguous_cache
(
	NvU64 virt_start,
	NvU64 virt_end,
	NvU64 phys_start,
	NvU64 phys_end
)
{
	/* We are expecting virt_end and phys_end to point to the first address
	 * of the next range */
	NvU32 size = virt_end - virt_start;
	size += (~virt_end + 1) % PAGE_SIZE;  /* Align up to page boundary */

#ifdef CONFIG_ARM64
	/* Flush L1 cache */
	__flush_dcache_area((void *)(size_t)(virt_start), size);
#else
	/* Flush L1 cache */
	__cpuc_flush_dcache_area((void *)(size_t)(virt_start), size);

	/* Now flush L2 cache. */
	outer_flush_range(phys_start, phys_end);
#endif
}

static void clear_entry_cache_mappings
(
	struct SYS_MAP_MEMORY *p_map_mem,
	NvU64 virt_start,
	NvU64 virt_end
)
{
	struct SYS_MEM_MODS_INFO *p_mem_info = p_map_mem->p_mem_info;
	NvU64 original_virt_end = virt_end;
	NvU64 phys_start;
	NvU64 phys_end;
	NvU64 v_start_offset;
	NvU64 v_end_offset;
	NvU64 start_offset;
	NvU64 start_page;
	NvU64 end_offset;
	NvU64 end_page;
	NvU64 i;

	if (NULL == p_mem_info || NULL == p_mem_info->p_page_tbl) {
		mods_debug_printk(DEBUG_MEM_DETAILED,
				  "Skipping unmapped region\n");
		return;
	}

	if (p_mem_info->cache_type != MODS_MEMORY_CACHED) {
		mods_debug_printk(DEBUG_MEM_DETAILED,
				  "Skipping uncached region\n");
		return;
	}

	v_start_offset = (virt_start - p_map_mem->virtual_addr);
	v_end_offset   = (virt_end - p_map_mem->virtual_addr);
	if (p_map_mem->contiguous) {
		NvU64 start_addr = MODS_DMA_TO_PHYS(p_map_mem->dma_addr);
		phys_start = start_addr + v_start_offset;
		phys_end   = start_addr + v_end_offset;

		clear_contiguous_cache(virt_start,
				       virt_end,
				       phys_start,
				       phys_end);
		return;
	}

	/* If not contiguous, go page by page clearing each page */
	start_page   = v_start_offset >> PAGE_SHIFT;
	start_offset = v_start_offset % PAGE_SIZE;
	end_page     = v_end_offset >> PAGE_SHIFT;
	end_offset   = v_end_offset % PAGE_SIZE;

	for (i = start_page; i <= end_page && i < p_mem_info->num_pages; i++) {
		NvU64 start_addr = MODS_DMA_TO_PHYS(
				p_mem_info->p_page_tbl[i]->dma_addr);
		if (i == start_page) {
			phys_start = start_addr + start_offset;
		} else {
			virt_start = p_map_mem->virtual_addr + (i * PAGE_SIZE);
			phys_start = start_addr;
		}

		if (i == end_page) {
			virt_end = original_virt_end;
			phys_end = start_addr + end_offset;
		} else {
			virt_end = p_map_mem->virtual_addr
				   + ((i + 1) * PAGE_SIZE);
			phys_end = start_addr + PAGE_SIZE;
		}

		clear_contiguous_cache(virt_start,
				       virt_end,
				       phys_start,
				       phys_end);
	}
}

int esc_mods_flush_cpu_cache_range(struct file *fp,
				   struct MODS_FLUSH_CPU_CACHE_RANGE *p)
{
	MODS_PRIVATE_DATA(private_data, fp);
	struct list_head *head;
	struct list_head *iter;

	if (irqs_disabled() || in_interrupt() ||
		p->virt_addr_start > p->virt_addr_end ||
		MODS_INVALIDATE_CPU_CACHE == p->flags) {

		mods_debug_printk(DEBUG_MEM_DETAILED, "cannot clear cache\n");
		return ~EINVAL;
	}

	spin_lock(&private_data->lock);

	head = private_data->mods_mapping_list;

	list_for_each(iter, head) {
		struct SYS_MAP_MEMORY *p_map_mem
			= list_entry(iter, struct SYS_MAP_MEMORY, list);

		NvU64 mapped_va = p_map_mem->virtual_addr;

		/* Note: mapping end points to the first address of next range*/
		NvU64 mapping_end = mapped_va + p_map_mem->mapping_length;

		int start_on_page = p->virt_addr_start >= mapped_va
				    && p->virt_addr_start < mapping_end;
		int start_before_page = p->virt_addr_start < mapped_va;
		int end_on_page = p->virt_addr_end >= mapped_va
				  && p->virt_addr_end < mapping_end;
		int end_after_page = p->virt_addr_end >= mapping_end;
		NvU64 virt_start = p->virt_addr_start;

		/* Kernel expects end to point to the first address of next
		 * range */
		NvU64 virt_end = p->virt_addr_end + 1;

		if ((start_on_page || start_before_page)
			&& (end_on_page || end_after_page)) {

			if (!start_on_page)
				virt_start = p_map_mem->virtual_addr;
			if (!end_on_page)
				virt_end = mapping_end;
			clear_entry_cache_mappings(p_map_mem,
						   virt_start,
						   virt_end);
		}
	}
	spin_unlock(&private_data->lock);
	return OK;
}

#endif

static int mods_post_alloc(NvU64 vaddr,
			   NvU64 paddr,
			   NvU64 pages,
			   NvU32 cachetype)
{
#if defined(CONFIG_ARCH_TEGRA) && !defined(CONFIG_CPA) &&\
	!defined(CONFIG_ARCH_TEGRA_3x_SOC)
	NvU64 size = pages * PAGE_SIZE;
	clear_contiguous_cache(vaddr,
			       vaddr + size,
			       paddr,
			       paddr + size);
	return 0;
#else
	return mods_set_mem_type(vaddr, pages, cachetype);
#endif
}
