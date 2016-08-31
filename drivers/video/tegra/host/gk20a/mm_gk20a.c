/*
 * drivers/video/tegra/host/gk20a/mm_gk20a.c
 *
 * GK20A memory management
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/log2.h>
#include <linux/nvhost.h>
#include <linux/scatterlist.h>
#include <linux/nvmap.h>
#include <linux/tegra-soc.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>

#include "dev.h"
#include "nvhost_as.h"
#include "gk20a.h"
#include "mm_gk20a.h"
#include "hw_gmmu_gk20a.h"
#include "hw_fb_gk20a.h"
#include "hw_bus_gk20a.h"
#include "hw_ram_gk20a.h"
#include "hw_mc_gk20a.h"
#include "hw_flush_gk20a.h"
#include "hw_ltc_gk20a.h"

#include "kind_gk20a.h"

/*
 * GPU mapping life cycle
 * ======================
 *
 * Kernel mappings
 * ---------------
 *
 * Kernel mappings are created through vm.map(..., false):
 *
 *  - Mappings to the same allocations are reused and refcounted.
 *  - This path does not support deferred unmapping (i.e. kernel must wait for
 *    all hw operations on the buffer to complete before unmapping).
 *  - References to memmgr and mem_handle are owned and managed by the (kernel)
 *    clients of the gk20a_vm layer.
 *
 *
 * User space mappings
 * -------------------
 *
 * User space mappings are created through as.map_buffer -> vm.map(..., true):
 *
 *  - Mappings to the same allocations are reused and refcounted.
 *  - This path supports deferred unmapping (i.e. we delay the actual unmapping
 *    until all hw operations have completed).
 *  - References to memmgr and mem_handle are owned and managed by the vm_gk20a
 *    layer itself. vm.map acquires these refs, and sets
 *    mapped_buffer->own_mem_ref to record that we must release the refs when we
 *    actually unmap.
 *
 */

static inline int vm_aspace_id(struct vm_gk20a *vm)
{
	/* -1 is bar1 or pmu, etc. */
	return vm->as_share ? vm->as_share->id : -1;
}
static inline u32 hi32(u64 f)
{
	return (u32)(f >> 32);
}
static inline u32 lo32(u64 f)
{
	return (u32)(f & 0xffffffff);
}

#define FLUSH_CPU_DCACHE(va, pa, size)	\
	do {	\
		__cpuc_flush_dcache_area((void *)(va), (size_t)(size));	\
		outer_flush_range(pa, pa + (size_t)(size));		\
	} while (0)

static void gk20a_vm_unmap_locked(struct mapped_buffer_node *mapped_buffer);
static struct mapped_buffer_node *find_mapped_buffer_locked(
					struct rb_root *root, u64 addr);
static struct mapped_buffer_node *find_mapped_buffer_reverse_locked(
				struct rb_root *root, struct mem_handle *r);
static int update_gmmu_ptes_locked(struct vm_gk20a *vm,
				   enum gmmu_pgsz_gk20a pgsz_idx,
				   struct sg_table *sgt,
				   u64 first_vaddr, u64 last_vaddr,
				   u8 kind_v, u32 ctag_offset, bool cacheable,
				   int rw_flag);
static void update_gmmu_pde_locked(struct vm_gk20a *vm, u32 i);
static void gk20a_vm_remove_support(struct vm_gk20a *vm);


/* note: keep the page sizes sorted lowest to highest here */
static const u32 gmmu_page_sizes[gmmu_nr_page_sizes] = { SZ_4K, SZ_128K };
static const u32 gmmu_page_shifts[gmmu_nr_page_sizes] = { 12, 17 };
static const u64 gmmu_page_offset_masks[gmmu_nr_page_sizes] = { 0xfffLL,
								0x1ffffLL };
static const u64 gmmu_page_masks[gmmu_nr_page_sizes] = { ~0xfffLL, ~0x1ffffLL };

static int gk20a_init_mm_reset_enable_hw(struct gk20a *g)
{
	nvhost_dbg_fn("");
	gk20a_reset(g, mc_enable_pfb_enabled_f()
			| mc_enable_l2_enabled_f()
			| mc_enable_ce2_enabled_f()
			| mc_enable_xbar_enabled_f()
			| mc_enable_hub_enabled_f());

	return 0;
}

void gk20a_remove_mm_support(struct mm_gk20a *mm)
{
	struct gk20a *g = mm->g;
	struct device *d = dev_from_gk20a(g);
	struct vm_gk20a *vm = &mm->bar1.vm;
	struct inst_desc *inst_block = &mm->bar1.inst_block;

	nvhost_dbg_fn("");

	if (inst_block->cpuva)
		dma_free_coherent(d, inst_block->size,
			inst_block->cpuva, inst_block->iova);
	inst_block->cpuva = NULL;
	inst_block->iova = 0;

	gk20a_vm_remove_support(vm);
}

int gk20a_init_mm_setup_sw(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	int i;

	nvhost_dbg_fn("");

	if (mm->sw_ready) {
		nvhost_dbg_fn("skip init");
		return 0;
	}

	mm->g = g;
	mutex_init(&mm->tlb_lock);
	mutex_init(&mm->l2_op_lock);
	mm->big_page_size = gmmu_page_sizes[gmmu_page_size_big];
	mm->pde_stride    = mm->big_page_size << 10;
	mm->pde_stride_shift = ilog2(mm->pde_stride);
	BUG_ON(mm->pde_stride_shift > 31); /* we have assumptions about this */

	for (i = 0; i < ARRAY_SIZE(gmmu_page_sizes); i++) {

		u32 num_ptes, pte_space, num_pages;

		/* assuming "full" page tables */
		num_ptes = mm->pde_stride / gmmu_page_sizes[i];

		pte_space = num_ptes * gmmu_pte__size_v();
		/* allocate whole pages */
		pte_space = roundup(pte_space, PAGE_SIZE);

		num_pages = pte_space / PAGE_SIZE;
		/* make sure "order" is viable */
		BUG_ON(!is_power_of_2(num_pages));

		mm->page_table_sizing[i].num_ptes = num_ptes;
		mm->page_table_sizing[i].order = ilog2(num_pages);
	}

	/*TBD: make channel vm size configurable */
	mm->channel.size = 1ULL << NV_GMMU_VA_RANGE;

	nvhost_dbg_info("channel vm size: %dMB", (int)(mm->channel.size >> 20));

	nvhost_dbg_info("small page-size (%dKB) pte array: %dKB",
			gmmu_page_sizes[gmmu_page_size_small] >> 10,
			(mm->page_table_sizing[gmmu_page_size_small].num_ptes *
			 gmmu_pte__size_v()) >> 10);

	nvhost_dbg_info("big page-size (%dKB) pte array: %dKB",
			gmmu_page_sizes[gmmu_page_size_big] >> 10,
			(mm->page_table_sizing[gmmu_page_size_big].num_ptes *
			 gmmu_pte__size_v()) >> 10);


	gk20a_init_bar1_vm(mm);

	gk20a_init_uncompressed_kind_map();
	gk20a_init_kind_attr();

	mm->remove_support = gk20a_remove_mm_support;
	mm->sw_ready = true;

	nvhost_dbg_fn("done");
	return 0;
}

/* make sure gk20a_init_mm_support is called before */
static int gk20a_init_mm_setup_hw(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	struct inst_desc *inst_block = &mm->bar1.inst_block;
	phys_addr_t inst_pa = inst_block->cpu_pa;

	nvhost_dbg_fn("");

	/* set large page size in fb
	 * note this is very early on, can we defer it ? */
	{
		u32 fb_mmu_ctrl = gk20a_readl(g, fb_mmu_ctrl_r());

		if (gmmu_page_sizes[gmmu_page_size_big] == SZ_128K)
			fb_mmu_ctrl = (fb_mmu_ctrl &
				       ~fb_mmu_ctrl_vm_pg_size_f(~0x0)) |
				fb_mmu_ctrl_vm_pg_size_128kb_f();
		else
			BUG_ON(1); /* no support/testing for larger ones yet */

		gk20a_writel(g, fb_mmu_ctrl_r(), fb_mmu_ctrl);
	}

	inst_pa = (u32)(inst_pa >> bar1_instance_block_shift_gk20a());
	nvhost_dbg_info("bar1 inst block ptr: 0x%08x",  (u32)inst_pa);

	/* this is very early in init... can we defer this? */
	{
		gk20a_writel(g, bus_bar1_block_r(),
			     bus_bar1_block_target_vid_mem_f() |
			     bus_bar1_block_mode_virtual_f() |
			     bus_bar1_block_ptr_f(inst_pa));
	}

	nvhost_dbg_fn("done");
	return 0;
}

int gk20a_init_mm_support(struct gk20a *g)
{
	u32 err;

	err = gk20a_init_mm_reset_enable_hw(g);
	if (err)
		return err;

	err = gk20a_init_mm_setup_sw(g);
	if (err)
		return err;

	err = gk20a_init_mm_setup_hw(g);
	if (err)
		return err;

	return err;
}

#ifdef TEGRA_GRHOST_GK20A_PHYS_PAGE_TABLES
static int alloc_gmmu_pages(struct vm_gk20a *vm, u32 order,
			    void **handle,
			    struct sg_table **sgt,
			    size_t *size)
{
	u32 num_pages = 1 << order;
	u32 len = num_pages * PAGE_SIZE;
	int err;
	struct page *pages;

	nvhost_dbg_fn("");

	pages = alloc_pages(GFP_KERNEL, order);
	if (!pages) {
		nvhost_dbg(dbg_pte, "alloc_pages failed\n");
		goto err_out;
	}
	*sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		nvhost_dbg(dbg_pte, "cannot allocate sg table");
		goto err_alloced;
	}
	err = sg_alloc_table(*sgt, 1, GFP_KERNEL);
	if (err) {
		nvhost_dbg(dbg_pte, "sg_alloc_table failed\n");
		goto err_sg_table;
	}
	sg_set_page((*sgt)->sgl, pages, len, 0);
	*handle = page_address(pages);
	memset(*handle, 0, len);
	*size = len;
	FLUSH_CPU_DCACHE(*handle, sg_phys((*sgt)->sgl), len);

	return 0;

err_sg_table:
	kfree(*sgt);
err_alloced:
	__free_pages(pages, order);
err_out:
	return -ENOMEM;
}

static void free_gmmu_pages(struct vm_gk20a *vm, void *handle,
			    struct sg_table *sgt, u32 order,
			    size_t size)
{
	nvhost_dbg_fn("");
	BUG_ON(sgt == NULL);
	free_pages((unsigned long)handle, order);
	sg_free_table(sgt);
	kfree(sgt);
}

static int map_gmmu_pages(void *handle, struct sg_table *sgt,
			  void **va, size_t size)
{
	FLUSH_CPU_DCACHE(handle, sg_phys(sgt->sgl), sgt->sgl->length);
	*va = handle;
	return 0;
}

static void unmap_gmmu_pages(void *handle, struct sg_table *sgt, void *va)
{
	FLUSH_CPU_DCACHE(handle, sg_phys(sgt->sgl), sgt->sgl->length);
}
#else
static int alloc_gmmu_pages(struct vm_gk20a *vm, u32 order,
			    void **handle,
			    struct sg_table **sgt,
			    size_t *size)
{
	struct device *d = dev_from_vm(vm);
	u32 num_pages = 1 << order;
	u32 len = num_pages * PAGE_SIZE;
	u64 iova;
	DEFINE_DMA_ATTRS(attrs);
	struct page **pages;
	int err = 0;

	nvhost_dbg_fn("");

	*size = len;
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
	pages = dma_alloc_attrs(d, len, &iova, GFP_KERNEL, &attrs);
	if (!pages) {
		nvhost_err(d, "memory allocation failed\n");
		goto err_out;
	}

	err = gk20a_get_sgtable_from_pages(d, sgt, pages,
				iova, len);
	if (err) {
		nvhost_err(d, "sgt allocation failed\n");
		goto err_free;
	}

	*handle = (void *)pages;

	return 0;

err_free:
	dma_free_attrs(d, len, pages, iova, &attrs);
	pages = NULL;
	iova = 0;
err_out:
	return -ENOMEM;
}

static void free_gmmu_pages(struct vm_gk20a *vm, void *handle,
			    struct sg_table *sgt, u32 order,
			    size_t size)
{
	struct device *d = dev_from_vm(vm);
	u64 iova;
	DEFINE_DMA_ATTRS(attrs);
	struct page **pages = (struct page **)handle;

	nvhost_dbg_fn("");
	BUG_ON(sgt == NULL);

	iova = sg_dma_address(sgt->sgl);

	gk20a_free_sgtable(&sgt);

	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
	dma_free_attrs(d, size, pages, iova, &attrs);
	pages = NULL;
	iova = 0;
}

static int map_gmmu_pages(void *handle, struct sg_table *sgt,
			  void **kva, size_t size)
{
	int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	struct page **pages = (struct page **)handle;
	nvhost_dbg_fn("");

	*kva = vmap(pages, count, 0, pgprot_dmacoherent(PAGE_KERNEL));
	if (!(*kva))
		return -ENOMEM;

	return 0;
}

static void unmap_gmmu_pages(void *handle, struct sg_table *sgt, void *va)
{
	nvhost_dbg_fn("");
	vunmap(va);
}
#endif

/* allocate a phys contig region big enough for a full
 * sized gmmu page table for the given gmmu_page_size.
 * the whole range is zeroed so it's "invalid"/will fault
 */

static int zalloc_gmmu_page_table_gk20a(struct vm_gk20a *vm,
					enum gmmu_pgsz_gk20a gmmu_pgsz_idx,
					struct page_table_gk20a *pte)
{
	int err;
	u32 pte_order;
	void *handle = NULL;
	struct sg_table *sgt;
	size_t size;

	nvhost_dbg_fn("");

	/* allocate enough pages for the table */
	pte_order = vm->mm->page_table_sizing[gmmu_pgsz_idx].order;

	err = alloc_gmmu_pages(vm, pte_order, &handle, &sgt, &size);
	if (err)
		return err;

	nvhost_dbg(dbg_pte, "pte = 0x%p, addr=%08llx, size %d",
			pte, gk20a_mm_iova_addr(sgt->sgl), pte_order);

	pte->ref = handle;
	pte->sgt = sgt;
	pte->size = size;

	return 0;
}

/* given address range (inclusive) determine the pdes crossed */
static inline void pde_range_from_vaddr_range(struct vm_gk20a *vm,
					      u64 addr_lo, u64 addr_hi,
					      u32 *pde_lo, u32 *pde_hi)
{
	*pde_lo = (u32)(addr_lo >> vm->mm->pde_stride_shift);
	*pde_hi = (u32)(addr_hi >> vm->mm->pde_stride_shift);
	nvhost_dbg(dbg_pte, "addr_lo=0x%llx addr_hi=0x%llx pde_ss=%d",
		   addr_lo, addr_hi, vm->mm->pde_stride_shift);
	nvhost_dbg(dbg_pte, "pde_lo=%d pde_hi=%d",
		   *pde_lo, *pde_hi);
}

static inline u32 *pde_from_index(struct vm_gk20a *vm, u32 i)
{
	return (u32 *) (((u8 *)vm->pdes.kv) + i*gmmu_pde__size_v());
}

static inline u32 pte_index_from_vaddr(struct vm_gk20a *vm,
				       u64 addr, enum gmmu_pgsz_gk20a pgsz_idx)
{
	u32 ret;
	/* mask off pde part */
	addr = addr & ((((u64)1) << vm->mm->pde_stride_shift) - ((u64)1));
	/* shift over to get pte index. note assumption that pte index
	 * doesn't leak over into the high 32b */
	ret = (u32)(addr >> gmmu_page_shifts[pgsz_idx]);

	nvhost_dbg(dbg_pte, "addr=0x%llx pte_i=0x%x", addr, ret);
	return ret;
}

static inline void pte_space_page_offset_from_index(u32 i, u32 *pte_page,
						    u32 *pte_offset)
{
	/* ptes are 8B regardless of pagesize */
	/* pte space pages are 4KB. so 512 ptes per 4KB page*/
	*pte_page = i >> 9;

	/* this offset is a pte offset, not a byte offset */
	*pte_offset = i & ((1<<9)-1);

	nvhost_dbg(dbg_pte, "i=0x%x pte_page=0x%x pte_offset=0x%x",
		   i, *pte_page, *pte_offset);
}


/*
 * given a pde index/page table number make sure it has
 * backing store and if not go ahead allocate it and
 * record it in the appropriate pde
 */
static int validate_gmmu_page_table_gk20a_locked(struct vm_gk20a *vm,
				u32 i, enum gmmu_pgsz_gk20a gmmu_pgsz_idx)
{
	int err;
	struct page_table_gk20a *pte =
		vm->pdes.ptes[gmmu_pgsz_idx] + i;

	nvhost_dbg_fn("");

	/* if it's already in place it's valid */
	if (pte->ref)
		return 0;

	nvhost_dbg(dbg_pte, "alloc %dKB ptes for pde %d",
		   gmmu_page_sizes[gmmu_pgsz_idx]/1024, i);

	err = zalloc_gmmu_page_table_gk20a(vm, gmmu_pgsz_idx, pte);
	if (err)
		return err;

	/* rewrite pde */
	update_gmmu_pde_locked(vm, i);

	return 0;
}

static struct vm_reserved_va_node *addr_to_reservation(struct vm_gk20a *vm,
						       u64 addr)
{
	struct vm_reserved_va_node *va_node;
	list_for_each_entry(va_node, &vm->reserved_va_list, reserved_va_list)
		if (addr >= va_node->vaddr_start &&
		    addr < (u64)va_node->vaddr_start + (u64)va_node->size)
			return va_node;

	return NULL;
}

int gk20a_vm_get_buffers(struct vm_gk20a *vm,
			 struct mapped_buffer_node ***mapped_buffers,
			 int *num_buffers)
{
	struct mapped_buffer_node *mapped_buffer;
	struct mapped_buffer_node **buffer_list;
	struct rb_node *node;
	int i = 0;

	mutex_lock(&vm->update_gmmu_lock);

	buffer_list = kzalloc(sizeof(*buffer_list) *
			      vm->num_user_mapped_buffers, GFP_KERNEL);
	if (!buffer_list) {
		mutex_unlock(&vm->update_gmmu_lock);
		return -ENOMEM;
	}

	node = rb_first(&vm->mapped_buffers);
	while (node) {
		mapped_buffer =
			container_of(node, struct mapped_buffer_node, node);
		if (mapped_buffer->user_mapped) {
			buffer_list[i] = mapped_buffer;
			kref_get(&mapped_buffer->ref);
			i++;
		}
		node = rb_next(&mapped_buffer->node);
	}

	BUG_ON(i != vm->num_user_mapped_buffers);

	*num_buffers = vm->num_user_mapped_buffers;
	*mapped_buffers = buffer_list;

	mutex_unlock(&vm->update_gmmu_lock);

	return 0;
}

static void gk20a_vm_unmap_locked_kref(struct kref *ref)
{
	struct mapped_buffer_node *mapped_buffer =
		container_of(ref, struct mapped_buffer_node, ref);
	gk20a_vm_unmap_locked(mapped_buffer);
}

void gk20a_vm_put_buffers(struct vm_gk20a *vm,
				 struct mapped_buffer_node **mapped_buffers,
				 int num_buffers)
{
	int i;

	mutex_lock(&vm->update_gmmu_lock);

	for (i = 0; i < num_buffers; ++i)
		kref_put(&mapped_buffers[i]->ref,
			 gk20a_vm_unmap_locked_kref);

	mutex_unlock(&vm->update_gmmu_lock);

	kfree(mapped_buffers);
}

static void gk20a_vm_unmap_user(struct vm_gk20a *vm, u64 offset)
{
	struct device *d = dev_from_vm(vm);
	int retries;
	struct mapped_buffer_node *mapped_buffer;

	mutex_lock(&vm->update_gmmu_lock);

	mapped_buffer = find_mapped_buffer_locked(&vm->mapped_buffers, offset);
	if (!mapped_buffer) {
		mutex_unlock(&vm->update_gmmu_lock);
		nvhost_err(d, "invalid addr to unmap 0x%llx", offset);
		return;
	}

	if (mapped_buffer->flags & NVHOST_AS_MAP_BUFFER_FLAGS_FIXED_OFFSET) {
		mutex_unlock(&vm->update_gmmu_lock);

		retries = 1000;
		while (retries) {
			if (atomic_read(&mapped_buffer->ref.refcount) == 1)
				break;
			retries--;
			udelay(50);
		}
		if (!retries)
			nvhost_err(d, "sync-unmap failed on 0x%llx",
								offset);
		mutex_lock(&vm->update_gmmu_lock);
	}

	mapped_buffer->user_mapped--;
	if (mapped_buffer->user_mapped == 0)
		vm->num_user_mapped_buffers--;
	kref_put(&mapped_buffer->ref, gk20a_vm_unmap_locked_kref);

	mutex_unlock(&vm->update_gmmu_lock);
}

static u64 gk20a_vm_alloc_va(struct vm_gk20a *vm,
			     u64 size,
			     enum gmmu_pgsz_gk20a gmmu_pgsz_idx)

{
	struct nvhost_allocator *vma = &vm->vma[gmmu_pgsz_idx];
	int err;
	u64 offset;
	u32 start_page_nr = 0, num_pages;
	u64 gmmu_page_size = gmmu_page_sizes[gmmu_pgsz_idx];

	if (gmmu_pgsz_idx >= ARRAY_SIZE(gmmu_page_sizes)) {
		dev_warn(dev_from_vm(vm),
			 "invalid page size requested in gk20a vm alloc");
		return -EINVAL;
	}

	if ((gmmu_pgsz_idx == gmmu_page_size_big) && !vm->big_pages) {
		dev_warn(dev_from_vm(vm),
			 "unsupportd page size requested");
		return -EINVAL;

	}

	/* be certain we round up to gmmu_page_size if needed */
	/* TBD: DIV_ROUND_UP -> undefined reference to __aeabi_uldivmod */
	size = (size + ((u64)gmmu_page_size - 1)) & ~((u64)gmmu_page_size - 1);

	nvhost_dbg_info("size=0x%llx @ pgsz=%dKB", size,
			gmmu_page_sizes[gmmu_pgsz_idx]>>10);

	/* The vma allocator represents page accounting. */
	num_pages = size >> gmmu_page_shifts[gmmu_pgsz_idx];

	err = vma->alloc(vma, &start_page_nr, num_pages);

	if (err) {
		nvhost_err(dev_from_vm(vm),
			   "%s oom: sz=0x%llx", vma->name, size);
		return 0;
	}

	offset = (u64)start_page_nr << gmmu_page_shifts[gmmu_pgsz_idx];
	nvhost_dbg_fn("%s found addr: 0x%llx", vma->name, offset);

	return offset;
}

static int gk20a_vm_free_va(struct vm_gk20a *vm,
			     u64 offset, u64 size,
			     enum gmmu_pgsz_gk20a pgsz_idx)
{
	struct nvhost_allocator *vma = &vm->vma[pgsz_idx];
	u32 page_size = gmmu_page_sizes[pgsz_idx];
	u32 page_shift = gmmu_page_shifts[pgsz_idx];
	u32 start_page_nr, num_pages;
	int err;

	nvhost_dbg_info("%s free addr=0x%llx, size=0x%llx",
			vma->name, offset, size);

	start_page_nr = (u32)(offset >> page_shift);
	num_pages = (u32)((size + page_size - 1) >> page_shift);

	err = vma->free(vma, start_page_nr, num_pages);
	if (err) {
		nvhost_err(dev_from_vm(vm),
			   "not found: offset=0x%llx, sz=0x%llx",
			   offset, size);
	}

	return err;
}

static int insert_mapped_buffer(struct rb_root *root,
				struct mapped_buffer_node *mapped_buffer)
{
	struct rb_node **new_node = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new_node) {
		struct mapped_buffer_node *cmp_with =
			container_of(*new_node, struct mapped_buffer_node,
				     node);

		parent = *new_node;

		if (cmp_with->addr > mapped_buffer->addr) /* u64 cmp */
			new_node = &((*new_node)->rb_left);
		else if (cmp_with->addr != mapped_buffer->addr) /* u64 cmp */
			new_node = &((*new_node)->rb_right);
		else
			return -EINVAL; /* no fair dup'ing */
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&mapped_buffer->node, parent, new_node);
	rb_insert_color(&mapped_buffer->node, root);

	return 0;
}

static struct mapped_buffer_node *find_mapped_buffer_reverse_locked(
				struct rb_root *root, struct mem_handle *r)
{
	struct rb_node *node = rb_first(root);
	while (node) {
		struct mapped_buffer_node *mapped_buffer =
			container_of(node, struct mapped_buffer_node, node);
		if (mapped_buffer->handle_ref == r)
			return mapped_buffer;
		node = rb_next(&mapped_buffer->node);
	}
	return 0;
}

static struct mapped_buffer_node *find_mapped_buffer_locked(
					struct rb_root *root, u64 addr)
{

	struct rb_node *node = root->rb_node;
	while (node) {
		struct mapped_buffer_node *mapped_buffer =
			container_of(node, struct mapped_buffer_node, node);
		if (mapped_buffer->addr > addr) /* u64 cmp */
			node = node->rb_left;
		else if (mapped_buffer->addr != addr) /* u64 cmp */
			node = node->rb_right;
		else
			return mapped_buffer;
	}
	return 0;
}

static struct mapped_buffer_node *find_mapped_buffer_range_locked(
					struct rb_root *root, u64 addr)
{
	struct rb_node *node = root->rb_node;
	while (node) {
		struct mapped_buffer_node *m =
			container_of(node, struct mapped_buffer_node, node);
		if (m->addr <= addr && m->addr + m->size > addr)
			return m;
		else if (m->addr > addr) /* u64 cmp */
			node = node->rb_left;
		else
			node = node->rb_right;
	}
	return 0;
}

/* convenience setup for nvmap buffer attr queries */
struct bfr_attr_query {
	int err;
	u64 v;
};
static u32 nvmap_bfr_param[] = {
#define BFR_SIZE   0
	NVMAP_HANDLE_PARAM_SIZE,
#define BFR_ALIGN  1
	NVMAP_HANDLE_PARAM_ALIGNMENT,
#define BFR_HEAP   2
	NVMAP_HANDLE_PARAM_HEAP,
#define BFR_KIND   3
	NVMAP_HANDLE_PARAM_KIND,
};
#define BFR_ATTRS (sizeof(nvmap_bfr_param)/sizeof(nvmap_bfr_param[0]))

struct buffer_attrs {
	struct sg_table *sgt;
	u64 size;
	u64 align;
	u32 ctag_offset;
	u32 ctag_lines;
	int pgsz_idx;
	u8 kind_v;
	u8 uc_kind_v;
};

static void gmmu_select_page_size(struct buffer_attrs *bfr)
{
	int i;
	/*  choose the biggest first (top->bottom) */
	for (i = (gmmu_nr_page_sizes-1); i >= 0; i--)
		if (!(gmmu_page_offset_masks[i] & bfr->align)) {
			/* would like to add this too but nvmap returns the
			 * original requested size not the allocated size.
			 * (!(gmmu_page_offset_masks[i] & bfr->size)) */
			bfr->pgsz_idx = i;
			break;
		}
}

static int setup_buffer_size_and_align(struct device *d,
				       struct buffer_attrs *bfr,
				       struct bfr_attr_query *query,
				       u64 offset, u32 flags)
{
	/* buffer allocation size and alignment must be a multiple
	   of one of the supported page sizes.*/
	bfr->size = query[BFR_SIZE].v;
	bfr->align = query[BFR_ALIGN].v;
	bfr->pgsz_idx = -1;

	/* If FIX_OFFSET is set, pgsz is determined. Otherwise, select
	 * page size according to memory alignment */
	if (flags & NVHOST_AS_MAP_BUFFER_FLAGS_FIXED_OFFSET) {
		bfr->pgsz_idx = NV_GMMU_VA_IS_UPPER(offset) ?
				gmmu_page_size_big : gmmu_page_size_small;
	} else {
		gmmu_select_page_size(bfr);
	}

	if (unlikely(bfr->pgsz_idx == -1)) {
		nvhost_warn(d, "unsupported buffer alignment: 0x%llx",
			   bfr->align);
		return -EINVAL;
	}

	bfr->kind_v = query[BFR_KIND].v;

	return 0;
}


static int setup_buffer_kind_and_compression(struct device *d,
					     u32 flags,
					     struct buffer_attrs *bfr,
					     enum gmmu_pgsz_gk20a pgsz_idx)
{
	bool kind_compressible;

	if (unlikely(bfr->kind_v == gmmu_pte_kind_invalid_v()))
		bfr->kind_v = gmmu_pte_kind_pitch_v();

	if (unlikely(!gk20a_kind_is_supported(bfr->kind_v))) {
		nvhost_err(d, "kind 0x%x not supported", bfr->kind_v);
		return -EINVAL;
	}

	bfr->uc_kind_v = gmmu_pte_kind_invalid_v();
	/* find a suitable uncompressed kind if it becomes necessary later */
	kind_compressible = gk20a_kind_is_compressible(bfr->kind_v);
	if (kind_compressible) {
		bfr->uc_kind_v = gk20a_get_uncompressed_kind(bfr->kind_v);
		if (unlikely(bfr->uc_kind_v == gmmu_pte_kind_invalid_v())) {
			/* shouldn't happen, but it is worth cross-checking */
			nvhost_err(d, "comptag kind 0x%x can't be"
				   " downgraded to uncompressed kind",
				   bfr->kind_v);
			return -EINVAL;
		}
	}
	/* comptags only supported for suitable kinds, 128KB pagesize */
	if (unlikely(kind_compressible &&
		     (gmmu_page_sizes[pgsz_idx] != 128*1024))) {
		/*
		nvhost_warn(d, "comptags specified"
		" but pagesize being used doesn't support it");*/
		/* it is safe to fall back to uncompressed as
		   functionality is not harmed */
		bfr->kind_v = bfr->uc_kind_v;
		kind_compressible = false;
	}
	if (kind_compressible)
		bfr->ctag_lines = ALIGN(bfr->size, COMP_TAG_LINE_SIZE) >>
			COMP_TAG_LINE_SIZE_SHIFT;
	else
		bfr->ctag_lines = 0;

	return 0;
}

static int validate_fixed_buffer(struct vm_gk20a *vm,
				 struct buffer_attrs *bfr,
				 u64 map_offset)
{
	struct device *dev = dev_from_vm(vm);
	struct vm_reserved_va_node *va_node;
	struct mapped_buffer_node *buffer;

	if (map_offset & gmmu_page_offset_masks[bfr->pgsz_idx]) {
		nvhost_err(dev, "map offset must be buffer page size aligned 0x%llx",
			   map_offset);
		return -EINVAL;
	}

	/* find the space reservation */
	va_node = addr_to_reservation(vm, map_offset);
	if (!va_node) {
		nvhost_warn(dev, "fixed offset mapping without space allocation");
		return -EINVAL;
	}

	/* check that this mappings does not collide with existing
	 * mappings by checking the overlapping area between the current
	 * buffer and all other mapped buffers */

	list_for_each_entry(buffer,
		&va_node->va_buffers_list, va_buffers_list) {
		s64 begin = max(buffer->addr, map_offset);
		s64 end = min(buffer->addr +
			buffer->size, map_offset + bfr->size);
		if (end - begin > 0) {
			nvhost_warn(dev, "overlapping buffer map requested");
			return -EINVAL;
		}
	}

	return 0;
}

static u64 __locked_gmmu_map(struct vm_gk20a *vm,
				u64 map_offset,
				struct sg_table *sgt,
				u64 size,
				int pgsz_idx,
				u8 kind_v,
				u32 ctag_offset,
				u32 flags,
				int rw_flag)
{
	int err = 0, i = 0;
	u32 pde_lo, pde_hi;
	struct device *d = dev_from_vm(vm);

	/* Allocate (or validate when map_offset != 0) the virtual address. */
	if (!map_offset) {
		map_offset = gk20a_vm_alloc_va(vm, size,
					  pgsz_idx);
		if (!map_offset) {
			nvhost_err(d, "failed to allocate va space");
			err = -ENOMEM;
			goto fail;
		}
	}

	pde_range_from_vaddr_range(vm,
				   map_offset,
				   map_offset + size - 1,
				   &pde_lo, &pde_hi);

	/* mark the addr range valid (but with 0 phys addr, which will fault) */
	for (i = pde_lo; i <= pde_hi; i++) {
		err = validate_gmmu_page_table_gk20a_locked(vm, i,
							    pgsz_idx);
		if (err) {
			nvhost_err(d, "failed to validate page table %d: %d",
							   i, err);
			goto fail;
		}
	}

	err = update_gmmu_ptes_locked(vm, pgsz_idx,
				      sgt,
				      map_offset, map_offset + size - 1,
				      kind_v,
				      ctag_offset,
				      flags &
				      NVHOST_MAP_BUFFER_FLAGS_CACHEABLE_TRUE,
				      rw_flag);
	if (err) {
		nvhost_err(d, "failed to update ptes on map");
		goto fail;
	}

	return map_offset;
 fail:
	nvhost_err(d, "%s: failed with err=%d\n", __func__, err);
	return 0;
}

static void __locked_gmmu_unmap(struct vm_gk20a *vm,
				u64 vaddr,
				u64 size,
				int pgsz_idx,
				bool va_allocated,
				int rw_flag)
{
	int err = 0;
	struct gk20a *g = gk20a_from_vm(vm);

	if (va_allocated) {
		err = gk20a_vm_free_va(vm, vaddr, size, pgsz_idx);
		if (err) {
			dev_err(dev_from_vm(vm),
				"failed to free va");
			return;
		}
	}

	/* unmap here needs to know the page size we assigned at mapping */
	err = update_gmmu_ptes_locked(vm,
				pgsz_idx,
				0, /* n/a for unmap */
				vaddr,
				vaddr + size - 1,
				0, 0, false /* n/a for unmap */,
				rw_flag);
	if (err)
		dev_err(dev_from_vm(vm),
			"failed to update gmmu ptes on unmap");

	/* detect which if any pdes/ptes can now be released */

	/* flush l2 so any dirty lines are written out *now*.
	 *  also as we could potentially be switching this buffer
	 * from nonvolatile (l2 cacheable) to volatile (l2 non-cacheable) at
	 * some point in the future we need to invalidate l2.  e.g. switching
	 * from a render buffer unmap (here) to later using the same memory
	 * for gmmu ptes.  note the positioning of this relative to any smmu
	 * unmapping (below). */

	gk20a_mm_l2_flush(g, true);
}

static u64 gk20a_vm_map_duplicate_locked(struct vm_gk20a *vm,
					 struct mem_mgr *memmgr,
					 struct mem_handle *r,
					 u64 offset_align,
					 u32 flags,
					 u32 kind,
					 struct sg_table **sgt,
					 bool user_mapped,
					 int rw_flag)
{
	struct mapped_buffer_node *mapped_buffer = 0;

	mapped_buffer = find_mapped_buffer_reverse_locked(
						&vm->mapped_buffers, r);
	if (!mapped_buffer)
		return 0;

	if (mapped_buffer->flags != flags)
		return 0;

	if (flags & NVHOST_AS_MAP_BUFFER_FLAGS_FIXED_OFFSET &&
	    mapped_buffer->addr != offset_align)
		return 0;

	WARN_ON(mapped_buffer->memmgr != memmgr);
	BUG_ON(mapped_buffer->vm != vm);

	/* mark the buffer as used */
	if (user_mapped) {
		if (mapped_buffer->user_mapped == 0)
			vm->num_user_mapped_buffers++;
		mapped_buffer->user_mapped++;

		/* If the mapping comes from user space, we own
		 * the memmgr and handle refs. Since we reuse an
		 * existing mapping here, we need to give back those
		 * refs once in order not to leak.
		 */
		if (mapped_buffer->own_mem_ref) {
			nvhost_memmgr_put(mapped_buffer->memmgr,
					  mapped_buffer->handle_ref);
			nvhost_memmgr_put_mgr(mapped_buffer->memmgr);
		} else
			mapped_buffer->own_mem_ref = true;

		mapped_buffer->memmgr = memmgr;
	}
	kref_get(&mapped_buffer->ref);

	nvhost_dbg(dbg_map,
		   "reusing as=%d pgsz=%d flags=0x%x ctags=%d "
		   "start=%d gv=0x%x,%08x -> 0x%x,%08x -> 0x%x,%08x "
		   "own_mem_ref=%d user_mapped=%d",
		   vm_aspace_id(vm), mapped_buffer->pgsz_idx,
		   mapped_buffer->flags,
		   mapped_buffer->ctag_lines,
		   mapped_buffer->ctag_offset,
		   hi32(mapped_buffer->addr), lo32(mapped_buffer->addr),
		   hi32((u64)sg_dma_address(mapped_buffer->sgt->sgl)),
		   lo32((u64)sg_dma_address(mapped_buffer->sgt->sgl)),
		   hi32((u64)sg_phys(mapped_buffer->sgt->sgl)),
		   lo32((u64)sg_phys(mapped_buffer->sgt->sgl)),
		   mapped_buffer->own_mem_ref, user_mapped);

	if (sgt)
		*sgt = mapped_buffer->sgt;
	return mapped_buffer->addr;
}

u64 gk20a_vm_map(struct vm_gk20a *vm,
			struct mem_mgr *memmgr,
			struct mem_handle *r,
			u64 offset_align,
			u32 flags /*NVHOST_AS_MAP_BUFFER_FLAGS_*/,
			u32 kind,
			struct sg_table **sgt,
			bool user_mapped,
			int rw_flag)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvhost_allocator *ctag_allocator = &g->gr.comp_tags;
	struct device *d = dev_from_vm(vm);
	struct mapped_buffer_node *mapped_buffer = 0;
	bool inserted = false, va_allocated = false;
	u32 gmmu_page_size = 0;
	u64 map_offset = 0;
	int attr, err = 0;
	struct buffer_attrs bfr = {0};
	struct bfr_attr_query query[BFR_ATTRS];
	struct nvhost_comptags comptags;

	mutex_lock(&vm->update_gmmu_lock);

	/* check if this buffer is already mapped */
	map_offset = gk20a_vm_map_duplicate_locked(vm, memmgr, r, offset_align,
						   flags, kind, sgt,
						   user_mapped, rw_flag);
	if (map_offset) {
		mutex_unlock(&vm->update_gmmu_lock);
		return map_offset;
	}

	/* pin buffer to get phys/iovmm addr */
	bfr.sgt = nvhost_memmgr_pin(memmgr, r, d, rw_flag);
	if (IS_ERR(bfr.sgt)) {
		/* Falling back to physical is actually possible
		 * here in many cases if we use 4K phys pages in the
		 * gmmu.  However we have some regions which require
		 * contig regions to work properly (either phys-contig
		 * or contig through smmu io_vaspace).  Until we can
		 * track the difference between those two cases we have
		 * to fail the mapping when we run out of SMMU space.
		 */
		nvhost_warn(d, "oom allocating tracking buffer");
		goto clean_up;
	}

	if (sgt)
		*sgt = bfr.sgt;

	/* query bfr attributes: size, align, heap, kind */
	for (attr = 0; attr < BFR_ATTRS; attr++) {
		query[attr].err =
			nvhost_memmgr_get_param(memmgr, r,
						nvmap_bfr_param[attr],
						&query[attr].v);
		if (unlikely(query[attr].err != 0)) {
			nvhost_err(d,
				   "failed to get nvmap buffer param %d: %d\n",
				   nvmap_bfr_param[attr],
				   query[attr].err);
			err = query[attr].err;
			goto clean_up;
		}
	}

	/* validate/adjust bfr attributes */
	err = setup_buffer_size_and_align(d, &bfr, query, offset_align, flags);
	if (unlikely(err))
		goto clean_up;
	if (unlikely(bfr.pgsz_idx < gmmu_page_size_small ||
		     bfr.pgsz_idx > gmmu_page_size_big)) {
		BUG_ON(1);
		err = -EINVAL;
		goto clean_up;
	}
	gmmu_page_size = gmmu_page_sizes[bfr.pgsz_idx];

	/* Check if we should use a fixed offset for mapping this buffer */
	if (flags & NVHOST_AS_MAP_BUFFER_FLAGS_FIXED_OFFSET)  {
		err = validate_fixed_buffer(vm, &bfr, offset_align);
		if (err)
			goto clean_up;

		map_offset = offset_align;
		va_allocated = false;
	} else
		va_allocated = true;

	if (sgt)
		*sgt = bfr.sgt;

	err = setup_buffer_kind_and_compression(d, flags, &bfr, bfr.pgsz_idx);
	if (unlikely(err)) {
		nvhost_err(d, "failure setting up kind and compression");
		goto clean_up;
	}

	/* bar1 and pmu vm don't need ctag */
	if (!vm->enable_ctag)
		bfr.ctag_lines = 0;

	nvhost_memmgr_get_comptags(r, &comptags);

	if (bfr.ctag_lines && !comptags.lines) {
		/* allocate compression resources if needed */
		err = nvhost_memmgr_alloc_comptags(r,
				ctag_allocator, bfr.ctag_lines);
		if (err) {
			/* ok to fall back here if we ran out */
			/* TBD: we can partially alloc ctags as well... */
			bfr.ctag_lines = bfr.ctag_offset = 0;
			bfr.kind_v = bfr.uc_kind_v;
		} else {
			nvhost_memmgr_get_comptags(r, &comptags);

			/* init/clear the ctag buffer */
			gk20a_gr_clear_comptags(g,
				comptags.offset,
				comptags.offset + comptags.lines - 1);
		}
	}

	/* store the comptag info */
	WARN_ON(bfr.ctag_lines != comptags.lines);
	bfr.ctag_offset = comptags.offset;

	/* update gmmu ptes */
	map_offset = __locked_gmmu_map(vm, map_offset,
					bfr.sgt,
					bfr.size,
					bfr.pgsz_idx,
					bfr.kind_v,
					bfr.ctag_offset,
					flags, rw_flag);
	if (!map_offset)
		goto clean_up;

	nvhost_dbg(dbg_map,
	   "as=%d pgsz=%d "
	   "kind=0x%x kind_uc=0x%x flags=0x%x "
	   "ctags=%d start=%d gv=0x%x,%08x -> 0x%x,%08x -> 0x%x,%08x",
	   vm_aspace_id(vm), gmmu_page_size,
	   bfr.kind_v, bfr.uc_kind_v, flags,
	   bfr.ctag_lines, bfr.ctag_offset,
	   hi32(map_offset), lo32(map_offset),
	   hi32((u64)sg_dma_address(bfr.sgt->sgl)),
	   lo32((u64)sg_dma_address(bfr.sgt->sgl)),
	   hi32((u64)sg_phys(bfr.sgt->sgl)),
	   lo32((u64)sg_phys(bfr.sgt->sgl)));

#if defined(NVHOST_DEBUG)
	{
		int i;
		struct scatterlist *sg = NULL;
		nvhost_dbg(dbg_pte, "for_each_sg(bfr.sgt->sgl, sg, bfr.sgt->nents, i)");
		for_each_sg(bfr.sgt->sgl, sg, bfr.sgt->nents, i ) {
			u64 da = sg_dma_address(sg);
			u64 pa = sg_phys(sg);
			u64 len = sg->length;
			nvhost_dbg(dbg_pte, "i=%d pa=0x%x,%08x da=0x%x,%08x len=0x%x,%08x",
				   i, hi32(pa), lo32(pa), hi32(da), lo32(da),
				   hi32(len), lo32(len));
		}
	}
#endif

	/* keep track of the buffer for unmapping */
	/* TBD: check for multiple mapping of same buffer */
	mapped_buffer = kzalloc(sizeof(*mapped_buffer), GFP_KERNEL);
	if (!mapped_buffer) {
		nvhost_warn(d, "oom allocating tracking buffer");
		goto clean_up;
	}
	mapped_buffer->memmgr      = memmgr;
	mapped_buffer->handle_ref  = r;
	mapped_buffer->sgt         = bfr.sgt;
	mapped_buffer->addr        = map_offset;
	mapped_buffer->size        = bfr.size;
	mapped_buffer->pgsz_idx    = bfr.pgsz_idx;
	mapped_buffer->ctag_offset = bfr.ctag_offset;
	mapped_buffer->ctag_lines  = bfr.ctag_lines;
	mapped_buffer->vm          = vm;
	mapped_buffer->flags       = flags;
	mapped_buffer->va_allocated = va_allocated;
	mapped_buffer->user_mapped = user_mapped ? 1 : 0;
	mapped_buffer->own_mem_ref = user_mapped;
	INIT_LIST_HEAD(&mapped_buffer->unmap_list);
	INIT_LIST_HEAD(&mapped_buffer->va_buffers_list);
	kref_init(&mapped_buffer->ref);

	err = insert_mapped_buffer(&vm->mapped_buffers, mapped_buffer);
	if (err) {
		nvhost_err(d, "failed to insert into mapped buffer tree");
		goto clean_up;
	}
	inserted = true;
	if (user_mapped)
		vm->num_user_mapped_buffers++;

	nvhost_dbg_info("allocated va @ 0x%llx", map_offset);

	if (!va_allocated) {
		struct vm_reserved_va_node *va_node;

		/* find the space reservation */
		va_node = addr_to_reservation(vm, map_offset);
		list_add_tail(&mapped_buffer->va_buffers_list,
			      &va_node->va_buffers_list);
		mapped_buffer->va_node = va_node;
	}

	mutex_unlock(&vm->update_gmmu_lock);

	/* Invalidate kernel mappings immediately */
	if (vm_aspace_id(vm) == -1)
		gk20a_mm_tlb_invalidate(vm);

	return map_offset;

clean_up:
	if (inserted) {
		rb_erase(&mapped_buffer->node, &vm->mapped_buffers);
		if (user_mapped)
			vm->num_user_mapped_buffers--;
	}
	kfree(mapped_buffer);
	if (va_allocated)
		gk20a_vm_free_va(vm, map_offset, bfr.size, bfr.pgsz_idx);
	if (!IS_ERR(bfr.sgt))
		nvhost_memmgr_unpin(memmgr, r, d, bfr.sgt);

	mutex_unlock(&vm->update_gmmu_lock);
	nvhost_dbg_info("err=%d\n", err);
	return 0;
}

u64 gk20a_gmmu_map(struct vm_gk20a *vm,
		struct sg_table **sgt,
		u64 size,
		u32 flags,
		int rw_flag)
{
	u64 vaddr;

	mutex_lock(&vm->update_gmmu_lock);
	vaddr = __locked_gmmu_map(vm, 0, /* already mapped? - No */
				*sgt, /* sg table */
				size,
				0, /* page size index = 0 i.e. SZ_4K */
				0, /* kind */
				0, /* ctag_offset */
				flags, rw_flag);
	mutex_unlock(&vm->update_gmmu_lock);
	if (!vaddr) {
		nvhost_err(dev_from_vm(vm), "failed to allocate va space");
		return 0;
	}

	/* Invalidate kernel mappings immediately */
	gk20a_mm_tlb_invalidate(vm);

	return vaddr;
}

void gk20a_gmmu_unmap(struct vm_gk20a *vm,
		u64 vaddr,
		u64 size,
		int rw_flag)
{
	mutex_lock(&vm->update_gmmu_lock);
	__locked_gmmu_unmap(vm,
			vaddr,
			size,
			0, /* page size 4K */
			true, /*va_allocated */
			rw_flag);
	mutex_unlock(&vm->update_gmmu_lock);
}

phys_addr_t gk20a_get_phys_from_iova(struct device *d,
				u64 dma_addr)
{
	phys_addr_t phys;
	u64 iova;

	struct dma_iommu_mapping *mapping = to_dma_iommu_mapping(d);
	if (!mapping)
		return dma_addr;

	iova = dma_addr & PAGE_MASK;
	phys = iommu_iova_to_phys(mapping->domain, iova);
	return phys;
}

/* get sg_table from already allocated buffer */
int gk20a_get_sgtable(struct device *d, struct sg_table **sgt,
			void *cpuva, u64 iova,
			size_t size)
{
	int err = 0;
	*sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!(*sgt)) {
		dev_err(d, "failed to allocate memory\n");
		err = -ENOMEM;
		goto fail;
	}
	err = dma_get_sgtable(d, *sgt,
			cpuva, iova,
			size);
	if (err) {
		dev_err(d, "failed to create sg table\n");
		goto fail;
	}
	sg_dma_address((*sgt)->sgl) = iova;

	return 0;
 fail:
	if (*sgt) {
		kfree(*sgt);
		*sgt = NULL;
	}
	return err;
}

int gk20a_get_sgtable_from_pages(struct device *d, struct sg_table **sgt,
			struct page **pages, u64 iova,
			size_t size)
{
	int err = 0;
	*sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!(*sgt)) {
		dev_err(d, "failed to allocate memory\n");
		err = -ENOMEM;
		goto fail;
	}
	err = sg_alloc_table(*sgt, 1, GFP_KERNEL);
	if (err) {
		dev_err(d, "failed to allocate sg_table\n");
		goto fail;
	}
	sg_set_page((*sgt)->sgl, *pages, size, 0);
	sg_dma_address((*sgt)->sgl) = iova;

	return 0;
 fail:
	if (*sgt) {
		kfree(*sgt);
		*sgt = NULL;
	}
	return err;
}

void gk20a_free_sgtable(struct sg_table **sgt)
{
	sg_free_table(*sgt);
	kfree(*sgt);
	*sgt = NULL;
}

u64 gk20a_mm_iova_addr(struct scatterlist *sgl)
{
	u64 result = sg_phys(sgl);
#ifdef CONFIG_TEGRA_IOMMU_SMMU
	if (sg_dma_address(sgl) == DMA_ERROR_CODE)
		result = 0;
	else if (sg_dma_address(sgl)) {
		result = sg_dma_address(sgl) |
			1ULL << NV_MC_SMMU_VADDR_TRANSLATION_BIT;
	}
#endif
	return result;
}

static int update_gmmu_ptes_locked(struct vm_gk20a *vm,
				   enum gmmu_pgsz_gk20a pgsz_idx,
				   struct sg_table *sgt,
				   u64 first_vaddr, u64 last_vaddr,
				   u8 kind_v, u32 ctag_offset,
				   bool cacheable,
				   int rw_flag)
{
	int err;
	u32 pde_lo, pde_hi, pde_i;
	struct scatterlist *cur_chunk;
	unsigned int cur_offset;
	u32 pte_w[2] = {0, 0}; /* invalid pte */
	u32 ctag = ctag_offset;
	u32 ctag_incr;
	u32 page_size  = gmmu_page_sizes[pgsz_idx];
	u64 addr = 0;

	pde_range_from_vaddr_range(vm, first_vaddr, last_vaddr,
				   &pde_lo, &pde_hi);

	nvhost_dbg(dbg_pte, "size_idx=%d, pde_lo=%d, pde_hi=%d",
		   pgsz_idx, pde_lo, pde_hi);

	/* If ctag_offset !=0 add 1 else add 0.  The idea is to avoid a branch
	 * below (per-pte). Note: this doesn't work unless page size (when
	 * comptags are active) is 128KB. We have checks elsewhere for that. */
	ctag_incr = !!ctag_offset;

	if (sgt)
		cur_chunk = sgt->sgl;
	else
		cur_chunk = NULL;

	cur_offset = 0;

	for (pde_i = pde_lo; pde_i <= pde_hi; pde_i++) {
		u32 pte_lo, pte_hi;
		u32 pte_cur;
		void *pte_kv_cur;

		struct page_table_gk20a *pte = vm->pdes.ptes[pgsz_idx] + pde_i;

		if (pde_i == pde_lo)
			pte_lo = pte_index_from_vaddr(vm, first_vaddr,
						      pgsz_idx);
		else
			pte_lo = 0;

		if ((pde_i != pde_hi) && (pde_hi != pde_lo))
			pte_hi = vm->mm->page_table_sizing[pgsz_idx].num_ptes-1;
		else
			pte_hi = pte_index_from_vaddr(vm, last_vaddr,
						      pgsz_idx);

		/* get cpu access to the ptes */
		err = map_gmmu_pages(pte->ref, pte->sgt, &pte_kv_cur,
				     pte->size);
		if (err) {
			nvhost_err(dev_from_vm(vm),
				   "couldn't map ptes for update as=%d pte_ref_cnt=%d",
				   vm_aspace_id(vm), pte->ref_cnt);
			goto clean_up;
		}

		nvhost_dbg(dbg_pte, "pte_lo=%d, pte_hi=%d", pte_lo, pte_hi);
		for (pte_cur = pte_lo; pte_cur <= pte_hi; pte_cur++) {

			if (likely(sgt)) {
				u64 new_addr = gk20a_mm_iova_addr(cur_chunk);
				if (new_addr) {
					addr = new_addr;
					addr += cur_offset;
				}

				pte_w[0] = gmmu_pte_valid_true_f() |
					gmmu_pte_address_sys_f(addr
						>> gmmu_pte_address_shift_v());
				pte_w[1] = gmmu_pte_aperture_video_memory_f() |
					gmmu_pte_kind_f(kind_v) |
					gmmu_pte_comptagline_f(ctag);

				if (rw_flag == mem_flag_read_only) {
					pte_w[0] |= gmmu_pte_read_only_true_f();
					pte_w[1] |=
						gmmu_pte_write_disable_true_f();
				} else if (rw_flag == mem_flag_write_only) {
					pte_w[1] |=
						gmmu_pte_read_disable_true_f();
				}

				if (!cacheable)
					pte_w[1] |= gmmu_pte_vol_true_f();

				pte->ref_cnt++;

				nvhost_dbg(dbg_pte,
					   "pte_cur=%d addr=0x%x,%08x kind=%d"
					   " ctag=%d vol=%d refs=%d"
					   " [0x%08x,0x%08x]",
					   pte_cur, hi32(addr), lo32(addr),
					   kind_v, ctag, !cacheable,
					   pte->ref_cnt, pte_w[1], pte_w[0]);

				ctag += ctag_incr;
				cur_offset += page_size;
				addr += page_size;
				while (cur_chunk &&
					cur_offset >= cur_chunk->length) {
					cur_offset -= cur_chunk->length;
					cur_chunk = sg_next(cur_chunk);
				}

			} else {
				pte->ref_cnt--;
				nvhost_dbg(dbg_pte,
					   "pte_cur=%d ref=%d [0x0,0x0]",
					   pte_cur, pte->ref_cnt);
			}

			mem_wr32(pte_kv_cur + pte_cur*8, 0, pte_w[0]);
			mem_wr32(pte_kv_cur + pte_cur*8, 1, pte_w[1]);
		}

		unmap_gmmu_pages(pte->ref, pte->sgt, pte_kv_cur);

		if (pte->ref_cnt == 0) {
			/* It can make sense to keep around one page table for
			 * each flavor (empty)... in case a new map is coming
			 * right back to alloc (and fill it in) again.
			 * But: deferring unmapping should help with pathologic
			 * unmap/map/unmap/map cases where we'd trigger pte
			 * free/alloc/free/alloc.
			 */
			free_gmmu_pages(vm, pte->ref, pte->sgt,
				vm->mm->page_table_sizing[pgsz_idx].order,
				pte->size);
			pte->ref = NULL;

			/* rewrite pde */
			update_gmmu_pde_locked(vm, pde_i);
		}

	}

	smp_mb();
	vm->tlb_dirty = true;
	nvhost_dbg_fn("set tlb dirty");

	return 0;

clean_up:
	/*TBD: potentially rewrite above to pre-map everything it needs to
	 * as that's the only way it can fail */
	return err;

}


/* for gk20a the "video memory" apertures here are misnomers. */
static inline u32 big_valid_pde0_bits(u64 pte_addr)
{
	u32 pde0_bits =
		gmmu_pde_aperture_big_video_memory_f() |
		gmmu_pde_address_big_sys_f(
			   (u32)(pte_addr >> gmmu_pde_address_shift_v()));
	return  pde0_bits;
}
static inline u32 small_valid_pde1_bits(u64 pte_addr)
{
	u32 pde1_bits =
		gmmu_pde_aperture_small_video_memory_f() |
		gmmu_pde_vol_small_true_f() | /* tbd: why? */
		gmmu_pde_address_small_sys_f(
			   (u32)(pte_addr >> gmmu_pde_address_shift_v()));
	return pde1_bits;
}

/* Given the current state of the ptes associated with a pde,
   determine value and write it out.  There's no checking
   here to determine whether or not a change was actually
   made.  So, superfluous updates will cause unnecessary
   pde invalidations.
*/
static void update_gmmu_pde_locked(struct vm_gk20a *vm, u32 i)
{
	bool small_valid, big_valid;
	u64 pte_addr[2] = {0, 0};
	struct page_table_gk20a *small_pte =
		vm->pdes.ptes[gmmu_page_size_small] + i;
	struct page_table_gk20a *big_pte =
		vm->pdes.ptes[gmmu_page_size_big] + i;
	u32 pde_v[2] = {0, 0};
	u32 *pde;

	small_valid = small_pte && small_pte->ref;
	big_valid   = big_pte && big_pte->ref;

	if (small_valid)
		pte_addr[gmmu_page_size_small] =
			gk20a_mm_iova_addr(small_pte->sgt->sgl);
	if (big_valid)
		pte_addr[gmmu_page_size_big] =
			gk20a_mm_iova_addr(big_pte->sgt->sgl);

	pde_v[0] = gmmu_pde_size_full_f();
	pde_v[0] |= big_valid ?
		big_valid_pde0_bits(pte_addr[gmmu_page_size_big])
		:
		(gmmu_pde_aperture_big_invalid_f());

	pde_v[1] |= (small_valid ?
		     small_valid_pde1_bits(pte_addr[gmmu_page_size_small])
		     :
		     (gmmu_pde_aperture_small_invalid_f() |
		      gmmu_pde_vol_small_false_f())
		     )
		|
		(big_valid ? (gmmu_pde_vol_big_true_f()) :
		 gmmu_pde_vol_big_false_f());

	pde = pde_from_index(vm, i);

	mem_wr32(pde, 0, pde_v[0]);
	mem_wr32(pde, 1, pde_v[1]);

	smp_mb();

	FLUSH_CPU_DCACHE(pde,
			 sg_phys(vm->pdes.sgt->sgl) + (i*gmmu_pde__size_v()),
			 sizeof(u32)*2);

	gk20a_mm_l2_invalidate(vm->mm->g);

	nvhost_dbg(dbg_pte, "pde:%d = 0x%x,0x%08x\n", i, pde_v[1], pde_v[0]);

	vm->tlb_dirty  = true;
}


static int gk20a_vm_put_empty(struct vm_gk20a *vm, u64 vaddr,
			       u32 num_pages, u32 pgsz_idx)
{
	struct mm_gk20a *mm = vm->mm;
	struct gk20a *g = mm->g;
	u32 pgsz = gmmu_page_sizes[pgsz_idx];
	u32 i;

	/* allocate the zero page if the va does not already have one */
	if (!vm->zero_page_cpuva) {
		int err = 0;
		vm->zero_page_cpuva = dma_alloc_coherent(&g->dev->dev,
							 mm->big_page_size,
							 &vm->zero_page_iova,
							 GFP_KERNEL);
		if (!vm->zero_page_cpuva) {
			dev_err(&g->dev->dev, "failed to allocate zero page\n");
			return -ENOMEM;
		}

		err = gk20a_get_sgtable(&g->dev->dev, &vm->zero_page_sgt,
					vm->zero_page_cpuva, vm->zero_page_iova,
					mm->big_page_size);
		if (err) {
			dma_free_coherent(&g->dev->dev, mm->big_page_size,
					  vm->zero_page_cpuva,
					  vm->zero_page_iova);
			vm->zero_page_iova = 0;
			vm->zero_page_cpuva = NULL;

			dev_err(&g->dev->dev, "failed to create sg table for zero page\n");
			return -ENOMEM;
		}
	}

	for (i = 0; i < num_pages; i++) {
		u64 page_vaddr = __locked_gmmu_map(vm, vaddr,
			vm->zero_page_sgt, pgsz, pgsz_idx, 0, 0,
			NVHOST_AS_ALLOC_SPACE_FLAGS_FIXED_OFFSET,
			mem_flag_none);

		if (!page_vaddr) {
			nvhost_err(dev_from_vm(vm), "failed to remap clean buffers!");
			goto err_unmap;
		}
		vaddr += pgsz;
	}

	gk20a_mm_l2_flush(mm->g, true);

	return 0;

err_unmap:

	WARN_ON(1);
	/* something went wrong. unmap pages */
	while (i--) {
		vaddr -= pgsz;
		__locked_gmmu_unmap(vm, vaddr, pgsz, pgsz_idx, 0,
				    mem_flag_none);
	}

	return -EINVAL;
}

/* return mem_mgr and mem_handle to caller. If the mem_handle is a kernel dup
   from user space (as_ioctl), caller releases the kernel duplicated handle */
/* NOTE! mapped_buffers lock must be held */
static void gk20a_vm_unmap_locked(struct mapped_buffer_node *mapped_buffer)
{
	struct vm_gk20a *vm = mapped_buffer->vm;

	if (mapped_buffer->va_node &&
	    mapped_buffer->va_node->sparse) {
		u64 vaddr = mapped_buffer->addr;
		u32 pgsz_idx = mapped_buffer->pgsz_idx;
		u32 num_pages = mapped_buffer->size >>
			gmmu_page_shifts[pgsz_idx];

		/* there is little we can do if this fails... */
		gk20a_vm_put_empty(vm, vaddr, num_pages, pgsz_idx);

	} else
		__locked_gmmu_unmap(vm,
				mapped_buffer->addr,
				mapped_buffer->size,
				mapped_buffer->pgsz_idx,
				mapped_buffer->va_allocated,
				mem_flag_none);

	nvhost_dbg(dbg_map, "as=%d pgsz=%d gv=0x%x,%08x own_mem_ref=%d",
		   vm_aspace_id(vm), gmmu_page_sizes[mapped_buffer->pgsz_idx],
		   hi32(mapped_buffer->addr), lo32(mapped_buffer->addr),
		   mapped_buffer->own_mem_ref);

	nvhost_memmgr_unpin(mapped_buffer->memmgr,
			    mapped_buffer->handle_ref,
			    dev_from_vm(vm),
			    mapped_buffer->sgt);

	/* remove from mapped buffer tree and remove list, free */
	rb_erase(&mapped_buffer->node, &vm->mapped_buffers);
	if (!list_empty(&mapped_buffer->va_buffers_list))
		list_del(&mapped_buffer->va_buffers_list);

	/* keep track of mapped buffers */
	if (mapped_buffer->user_mapped)
		vm->num_user_mapped_buffers--;

	if (mapped_buffer->own_mem_ref) {
		nvhost_memmgr_put(mapped_buffer->memmgr,
				  mapped_buffer->handle_ref);
		nvhost_memmgr_put_mgr(mapped_buffer->memmgr);
	}

	kfree(mapped_buffer);

	return;
}

void gk20a_vm_unmap(struct vm_gk20a *vm, u64 offset)
{
	struct device *d = dev_from_vm(vm);
	struct mapped_buffer_node *mapped_buffer;

	mutex_lock(&vm->update_gmmu_lock);
	mapped_buffer = find_mapped_buffer_locked(&vm->mapped_buffers, offset);
	if (!mapped_buffer) {
		mutex_unlock(&vm->update_gmmu_lock);
		nvhost_err(d, "invalid addr to unmap 0x%llx", offset);
		return;
	}
	kref_put(&mapped_buffer->ref, gk20a_vm_unmap_locked_kref);
	mutex_unlock(&vm->update_gmmu_lock);
}

static void gk20a_vm_remove_support(struct vm_gk20a *vm)
{
	struct gk20a *g = vm->mm->g;
	struct mapped_buffer_node *mapped_buffer;
	struct vm_reserved_va_node *va_node, *va_node_tmp;
	struct rb_node *node;

	nvhost_dbg_fn("");
	mutex_lock(&vm->update_gmmu_lock);

	/* TBD: add a flag here for the unmap code to recognize teardown
	 * and short-circuit any otherwise expensive operations. */

	node = rb_first(&vm->mapped_buffers);
	while (node) {
		mapped_buffer =
			container_of(node, struct mapped_buffer_node, node);
		gk20a_vm_unmap_locked(mapped_buffer);
		node = rb_first(&vm->mapped_buffers);
	}

	/* destroy remaining reserved memory areas */
	list_for_each_entry_safe(va_node, va_node_tmp, &vm->reserved_va_list,
		reserved_va_list) {
		list_del(&va_node->reserved_va_list);
		kfree(va_node);
	}

	/* TBD: unmapping all buffers above may not actually free
	 * all vm ptes.  jettison them here for certain... */

	unmap_gmmu_pages(vm->pdes.ref, vm->pdes.sgt, vm->pdes.kv);
	free_gmmu_pages(vm, vm->pdes.ref, vm->pdes.sgt, 0, vm->pdes.size);

	kfree(vm->pdes.ptes[gmmu_page_size_small]);
	kfree(vm->pdes.ptes[gmmu_page_size_big]);
	nvhost_allocator_destroy(&vm->vma[gmmu_page_size_small]);
	nvhost_allocator_destroy(&vm->vma[gmmu_page_size_big]);

	mutex_unlock(&vm->update_gmmu_lock);

	/* release zero page if used */
	if (vm->zero_page_cpuva)
		dma_free_coherent(&g->dev->dev, vm->mm->big_page_size,
				  vm->zero_page_cpuva, vm->zero_page_iova);

	/* vm is not used anymore. release it. */
	kfree(vm);
}

static void gk20a_vm_remove_support_kref(struct kref *ref)
{
	struct vm_gk20a *vm = container_of(ref, struct vm_gk20a, ref);
	gk20a_vm_remove_support(vm);
}

void gk20a_vm_get(struct vm_gk20a *vm)
{
	kref_get(&vm->ref);
}

void gk20a_vm_put(struct vm_gk20a *vm)
{
	kref_put(&vm->ref, gk20a_vm_remove_support_kref);
}

/* address space interfaces for the gk20a module */
static int gk20a_as_alloc_share(struct nvhost_as_share *as_share)
{
	struct nvhost_as *as = as_share->as;
	struct gk20a *gk20a = get_gk20a(as->ch->dev);
	struct mm_gk20a *mm = &gk20a->mm;
	struct vm_gk20a *vm;
	u64 vma_size;
	u32 num_pages, low_hole_pages;
	char name[32];
	int err;

	nvhost_dbg_fn("");

	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return -ENOMEM;

	as_share->priv = (void *)vm;

	vm->mm = mm;
	vm->as_share = as_share;

	vm->big_pages = true;

	vm->va_start  = mm->pde_stride;   /* create a one pde hole */
	vm->va_limit  = mm->channel.size; /* note this means channel.size is
					     really just the max */
	{
		u32 pde_lo, pde_hi;
		pde_range_from_vaddr_range(vm,
					   0, vm->va_limit-1,
					   &pde_lo, &pde_hi);
		vm->pdes.num_pdes = pde_hi + 1;
	}

	vm->pdes.ptes[gmmu_page_size_small] =
		kzalloc(sizeof(struct page_table_gk20a) *
			vm->pdes.num_pdes, GFP_KERNEL);

	vm->pdes.ptes[gmmu_page_size_big] =
		kzalloc(sizeof(struct page_table_gk20a) *
			vm->pdes.num_pdes, GFP_KERNEL);

	if (!(vm->pdes.ptes[gmmu_page_size_small] &&
	      vm->pdes.ptes[gmmu_page_size_big]))
		return -ENOMEM;

	nvhost_dbg_info("init space for va_limit=0x%llx num_pdes=%d",
		   vm->va_limit, vm->pdes.num_pdes);

	/* allocate the page table directory */
	err = alloc_gmmu_pages(vm, 0, &vm->pdes.ref,
			       &vm->pdes.sgt, &vm->pdes.size);
	if (err)
		return -ENOMEM;

	err = map_gmmu_pages(vm->pdes.ref, vm->pdes.sgt, &vm->pdes.kv,
			     vm->pdes.size);
	if (err) {
		free_gmmu_pages(vm, vm->pdes.ref, vm->pdes.sgt, 0,
					vm->pdes.size);
		return -ENOMEM;
	}
	nvhost_dbg(dbg_pte, "pdes.kv = 0x%p, pdes.phys = 0x%llx",
			vm->pdes.kv,
			gk20a_mm_iova_addr(vm->pdes.sgt->sgl));
	/* we could release vm->pdes.kv but it's only one page... */


	/* low-half: alloc small pages */
	/* high-half: alloc big pages */
	vma_size = mm->channel.size >> 1;

	snprintf(name, sizeof(name), "gk20a_as_%d-%dKB", as_share->id,
		 gmmu_page_sizes[gmmu_page_size_small]>>10);
	num_pages = (u32)(vma_size >> gmmu_page_shifts[gmmu_page_size_small]);

	/* num_pages above is without regard to the low-side hole. */
	low_hole_pages = (vm->va_start >>
			  gmmu_page_shifts[gmmu_page_size_small]);

	nvhost_allocator_init(&vm->vma[gmmu_page_size_small], name,
	      low_hole_pages,             /* start */
	      num_pages - low_hole_pages, /* length */
	      1);                         /* align */

	snprintf(name, sizeof(name), "gk20a_as_%d-%dKB", as_share->id,
		 gmmu_page_sizes[gmmu_page_size_big]>>10);

	num_pages = (u32)(vma_size >> gmmu_page_shifts[gmmu_page_size_big]);
	nvhost_allocator_init(&vm->vma[gmmu_page_size_big], name,
			      num_pages, /* start */
			      num_pages, /* length */
			      1); /* align */

	vm->mapped_buffers = RB_ROOT;

	mutex_init(&vm->update_gmmu_lock);
	kref_init(&vm->ref);
	INIT_LIST_HEAD(&vm->reserved_va_list);

	vm->enable_ctag = true;

	return 0;
}


static int gk20a_as_release_share(struct nvhost_as_share *as_share)
{
	struct vm_gk20a *vm = (struct vm_gk20a *)as_share->priv;

	nvhost_dbg_fn("");

	vm->as_share = NULL;

	/* put as reference to vm */
	gk20a_vm_put(vm);

	as_share->priv = NULL;

	return 0;
}


static int gk20a_as_alloc_space(struct nvhost_as_share *as_share,
				struct nvhost_as_alloc_space_args *args)

{	int err = -ENOMEM;
	int pgsz_idx;
	u32 start_page_nr;
	struct nvhost_allocator *vma;
	struct vm_gk20a *vm = (struct vm_gk20a *)as_share->priv;
	struct vm_reserved_va_node *va_node;
	u64 vaddr_start = 0;

	nvhost_dbg_fn("flags=0x%x pgsz=0x%x nr_pages=0x%x o/a=0x%llx",
			args->flags, args->page_size, args->pages,
			args->o_a.offset);

	/* determine pagesz idx */
	for (pgsz_idx = gmmu_page_size_small;
	     pgsz_idx < gmmu_nr_page_sizes;
	     pgsz_idx++) {
		if (gmmu_page_sizes[pgsz_idx] == args->page_size)
			break;
	}

	if (pgsz_idx >= gmmu_nr_page_sizes) {
		err = -EINVAL;
		goto clean_up;
	}

	va_node = kzalloc(sizeof(*va_node), GFP_KERNEL);
	if (!va_node) {
		err = -ENOMEM;
		goto clean_up;
	}

	if (args->flags & NVHOST_AS_ALLOC_SPACE_FLAGS_SPARSE &&
	    pgsz_idx != gmmu_page_size_big) {
		err = -ENOSYS;
		goto clean_up;
	}

	start_page_nr = 0;
	if (args->flags & NVHOST_AS_ALLOC_SPACE_FLAGS_FIXED_OFFSET)
		start_page_nr = (u32)(args->o_a.offset >>
				      gmmu_page_shifts[pgsz_idx]);

	vma = &vm->vma[pgsz_idx];
	err = vma->alloc(vma, &start_page_nr, args->pages);
	if (err) {
		kfree(va_node);
		goto clean_up;
	}

	vaddr_start = (u64)start_page_nr << gmmu_page_shifts[pgsz_idx];

	va_node->vaddr_start = vaddr_start;
	va_node->size = (u64)args->page_size * (u64)args->pages;
	va_node->pgsz_idx = args->page_size;
	INIT_LIST_HEAD(&va_node->va_buffers_list);
	INIT_LIST_HEAD(&va_node->reserved_va_list);

	mutex_lock(&vm->update_gmmu_lock);

	/* mark that we need to use sparse mappings here */
	if (args->flags & NVHOST_AS_ALLOC_SPACE_FLAGS_SPARSE) {
		err = gk20a_vm_put_empty(vm, vaddr_start, args->pages,
					 pgsz_idx);
		if (err) {
			mutex_unlock(&vm->update_gmmu_lock);
			vma->free(vma, start_page_nr, args->pages);
			kfree(va_node);
			goto clean_up;
		}

		va_node->sparse = true;
	}

	list_add_tail(&va_node->reserved_va_list, &vm->reserved_va_list);

	mutex_unlock(&vm->update_gmmu_lock);

	args->o_a.offset = vaddr_start;

clean_up:
	return err;
}

static int gk20a_as_free_space(struct nvhost_as_share *as_share,
			       struct nvhost_as_free_space_args *args)
{
	int err = -ENOMEM;
	int pgsz_idx;
	u32 start_page_nr;
	struct nvhost_allocator *vma;
	struct vm_gk20a *vm = (struct vm_gk20a *)as_share->priv;
	struct vm_reserved_va_node *va_node;

	nvhost_dbg_fn("pgsz=0x%x nr_pages=0x%x o/a=0x%llx", args->page_size,
			args->pages, args->offset);

	/* determine pagesz idx */
	for (pgsz_idx = gmmu_page_size_small;
	     pgsz_idx < gmmu_nr_page_sizes;
	     pgsz_idx++) {
		if (gmmu_page_sizes[pgsz_idx] == args->page_size)
			break;
	}

	if (pgsz_idx >= gmmu_nr_page_sizes) {
		err = -EINVAL;
		goto clean_up;
	}

	start_page_nr = (u32)(args->offset >>
			      gmmu_page_shifts[pgsz_idx]);

	vma = &vm->vma[pgsz_idx];
	err = vma->free(vma, start_page_nr, args->pages);

	if (err)
		goto clean_up;

	mutex_lock(&vm->update_gmmu_lock);
	va_node = addr_to_reservation(vm, args->offset);
	if (va_node) {
		struct mapped_buffer_node *buffer;

		/* there is no need to unallocate the buffers in va. Just
		 * convert them into normal buffers */

		list_for_each_entry(buffer,
			&va_node->va_buffers_list, va_buffers_list)
			list_del_init(&buffer->va_buffers_list);

		list_del(&va_node->reserved_va_list);

		/* if this was a sparse mapping, free the va */
		if (va_node->sparse)
			__locked_gmmu_unmap(vm,
				va_node->vaddr_start,
				va_node->size,
				va_node->pgsz_idx,
				false,
				mem_flag_none);
		kfree(va_node);
	}
	mutex_unlock(&vm->update_gmmu_lock);

clean_up:
	return err;
}

static int gk20a_as_bind_hwctx(struct nvhost_as_share *as_share,
			       struct nvhost_hwctx *hwctx)
{
	int err = 0;
	struct vm_gk20a *vm = (struct vm_gk20a *)as_share->priv;
	struct channel_gk20a *c = hwctx->priv;

	nvhost_dbg_fn("");

	c->vm = vm;
	err = channel_gk20a_commit_va(c);
	if (err)
		c->vm = 0;

	return err;
}

static int gk20a_as_map_buffer(struct nvhost_as_share *as_share,
			       int memmgr_fd,
			       ulong mem_id,
			       u64 *offset_align,
			       u32 flags /*NVHOST_AS_MAP_BUFFER_FLAGS_*/)
{
	int err = 0;
	struct vm_gk20a *vm = (struct vm_gk20a *)as_share->priv;
	struct gk20a *g = gk20a_from_vm(vm);
	struct mem_mgr *memmgr;
	struct mem_handle *r;
	u64 ret_va;

	nvhost_dbg_fn("");

	/* get ref to the memmgr (released on unmap_locked) */
	memmgr = nvhost_memmgr_get_mgr_file(memmgr_fd);
	if (IS_ERR(memmgr))
		return 0;

	/* get ref to the mem handle (released on unmap_locked) */
	r = nvhost_memmgr_get(memmgr, mem_id, g->dev);
	if (!r) {
		nvhost_memmgr_put_mgr(memmgr);
		return 0;
	}

	ret_va = gk20a_vm_map(vm, memmgr, r, *offset_align,
			flags, 0/*no kind here, to be removed*/, NULL, true,
			mem_flag_none);
	*offset_align = ret_va;
	if (!ret_va) {
		nvhost_memmgr_put(memmgr, r);
		nvhost_memmgr_put_mgr(memmgr);
		err = -EINVAL;
	}

	return err;
}

static int gk20a_as_unmap_buffer(struct nvhost_as_share *as_share, u64 offset)
{
	struct vm_gk20a *vm = (struct vm_gk20a *)as_share->priv;

	nvhost_dbg_fn("");

	gk20a_vm_unmap_user(vm, offset);
	return 0;
}


const struct nvhost_as_moduleops tegra_gk20a_as_ops = {
	.alloc_share   = gk20a_as_alloc_share,
	.release_share = gk20a_as_release_share,
	.alloc_space   = gk20a_as_alloc_space,
	.free_space    = gk20a_as_free_space,
	.bind_hwctx    = gk20a_as_bind_hwctx,
	.map_buffer    = gk20a_as_map_buffer,
	.unmap_buffer  = gk20a_as_unmap_buffer,
};

int gk20a_init_bar1_vm(struct mm_gk20a *mm)
{
	int err;
	phys_addr_t inst_pa;
	void *inst_ptr;
	struct vm_gk20a *vm = &mm->bar1.vm;
	struct gk20a *g = gk20a_from_mm(mm);
	struct device *d = dev_from_gk20a(g);
	struct inst_desc *inst_block = &mm->bar1.inst_block;
	u64 pde_addr;
	u32 pde_addr_lo;
	u32 pde_addr_hi;

	vm->mm = mm;

	mm->bar1.aperture_size = bar1_aperture_size_mb_gk20a() << 20;

	nvhost_dbg_info("bar1 vm size = 0x%x", mm->bar1.aperture_size);

	vm->va_start = mm->pde_stride * 1;
	vm->va_limit = mm->bar1.aperture_size;

	{
		u32 pde_lo, pde_hi;
		pde_range_from_vaddr_range(vm,
					   0, vm->va_limit-1,
					   &pde_lo, &pde_hi);
		vm->pdes.num_pdes = pde_hi + 1;
	}

	/* bar1 is likely only to ever use/need small page sizes. */
	/* But just in case, for now... arrange for both.*/
	vm->pdes.ptes[gmmu_page_size_small] =
		kzalloc(sizeof(struct page_table_gk20a) *
			vm->pdes.num_pdes, GFP_KERNEL);

	vm->pdes.ptes[gmmu_page_size_big] =
		kzalloc(sizeof(struct page_table_gk20a) *
			vm->pdes.num_pdes, GFP_KERNEL);

	if (!(vm->pdes.ptes[gmmu_page_size_small] &&
	      vm->pdes.ptes[gmmu_page_size_big]))
		return -ENOMEM;

	nvhost_dbg_info("init space for bar1 va_limit=0x%llx num_pdes=%d",
		   vm->va_limit, vm->pdes.num_pdes);


	/* allocate the page table directory */
	err = alloc_gmmu_pages(vm, 0, &vm->pdes.ref,
			       &vm->pdes.sgt, &vm->pdes.size);
	if (err)
		goto clean_up;

	err = map_gmmu_pages(vm->pdes.ref, vm->pdes.sgt, &vm->pdes.kv,
			     vm->pdes.size);
	if (err) {
		free_gmmu_pages(vm, vm->pdes.ref, vm->pdes.sgt, 0,
					vm->pdes.size);
		goto clean_up;
	}
	nvhost_dbg(dbg_pte, "bar 1 pdes.kv = 0x%p, pdes.phys = 0x%llx",
			vm->pdes.kv, gk20a_mm_iova_addr(vm->pdes.sgt->sgl));
	/* we could release vm->pdes.kv but it's only one page... */

	pde_addr = gk20a_mm_iova_addr(vm->pdes.sgt->sgl);
	pde_addr_lo = u64_lo32(pde_addr >> 12);
	pde_addr_hi = u64_hi32(pde_addr);

	nvhost_dbg_info("pde pa=0x%llx pde_addr_lo=0x%x pde_addr_hi=0x%x",
		(u64)gk20a_mm_iova_addr(vm->pdes.sgt->sgl),
		pde_addr_lo, pde_addr_hi);

	/* allocate instance mem for bar1 */
	inst_block->size = ram_in_alloc_size_v();
	inst_block->cpuva = dma_alloc_coherent(d, inst_block->size,
				&inst_block->iova, GFP_KERNEL);
	if (!inst_block->cpuva) {
		nvhost_err(d, "%s: memory allocation failed\n", __func__);
		err = -ENOMEM;
		goto clean_up;
	}

	inst_block->cpu_pa = gk20a_get_phys_from_iova(d, inst_block->iova);
	if (!inst_block->cpu_pa) {
		nvhost_err(d, "%s: failed to get phys address\n", __func__);
		err = -ENOMEM;
		goto clean_up;
	}

	inst_pa = inst_block->cpu_pa;
	inst_ptr = inst_block->cpuva;

	nvhost_dbg_info("bar1 inst block physical phys = 0x%llx, kv = 0x%p",
		(u64)inst_pa, inst_ptr);

	memset(inst_ptr, 0, ram_fc_size_val_v());

	mem_wr32(inst_ptr, ram_in_page_dir_base_lo_w(),
		ram_in_page_dir_base_target_vid_mem_f() |
		ram_in_page_dir_base_vol_true_f() |
		ram_in_page_dir_base_lo_f(pde_addr_lo));

	mem_wr32(inst_ptr, ram_in_page_dir_base_hi_w(),
		ram_in_page_dir_base_hi_f(pde_addr_hi));

	mem_wr32(inst_ptr, ram_in_adr_limit_lo_w(),
		 u64_lo32(vm->va_limit) | 0xFFF);

	mem_wr32(inst_ptr, ram_in_adr_limit_hi_w(),
		ram_in_adr_limit_hi_f(u64_hi32(vm->va_limit)));

	nvhost_dbg_info("bar1 inst block ptr: %08llx",  (u64)inst_pa);
	nvhost_allocator_init(&vm->vma[gmmu_page_size_small], "gk20a_bar1",
			      1,/*start*/
			      (vm->va_limit >> 12) - 1 /* length*/,
			      1); /* align */
	/* initialize just in case we try to use it anyway */
	nvhost_allocator_init(&vm->vma[gmmu_page_size_big], "gk20a_bar1-unused",
			      0x0badc0de, /* start */
			      1, /* length */
			      1); /* align */

	vm->mapped_buffers = RB_ROOT;

	mutex_init(&vm->update_gmmu_lock);
	kref_init(&vm->ref);
	INIT_LIST_HEAD(&vm->reserved_va_list);

	return 0;

clean_up:
	/* free, etc */
	if (inst_block->cpuva)
		dma_free_coherent(d, inst_block->size,
			inst_block->cpuva, inst_block->iova);
	inst_block->cpuva = NULL;
	inst_block->iova = 0;
	return err;
}

/* pmu vm, share channel_vm interfaces */
int gk20a_init_pmu_vm(struct mm_gk20a *mm)
{
	int err;
	phys_addr_t inst_pa;
	void *inst_ptr;
	struct vm_gk20a *vm = &mm->pmu.vm;
	struct gk20a *g = gk20a_from_mm(mm);
	struct device *d = dev_from_gk20a(g);
	struct inst_desc *inst_block = &mm->pmu.inst_block;
	u64 pde_addr;
	u32 pde_addr_lo;
	u32 pde_addr_hi;

	vm->mm = mm;

	mm->pmu.aperture_size = GK20A_PMU_VA_SIZE;

	nvhost_dbg_info("pmu vm size = 0x%x", mm->pmu.aperture_size);

	vm->va_start  = GK20A_PMU_VA_START;
	vm->va_limit  = vm->va_start + mm->pmu.aperture_size;

	{
		u32 pde_lo, pde_hi;
		pde_range_from_vaddr_range(vm,
					   0, vm->va_limit-1,
					   &pde_lo, &pde_hi);
		vm->pdes.num_pdes = pde_hi + 1;
	}

	/* The pmu is likely only to ever use/need small page sizes. */
	/* But just in case, for now... arrange for both.*/
	vm->pdes.ptes[gmmu_page_size_small] =
		kzalloc(sizeof(struct page_table_gk20a) *
			vm->pdes.num_pdes, GFP_KERNEL);

	vm->pdes.ptes[gmmu_page_size_big] =
		kzalloc(sizeof(struct page_table_gk20a) *
			vm->pdes.num_pdes, GFP_KERNEL);

	if (!(vm->pdes.ptes[gmmu_page_size_small] &&
	      vm->pdes.ptes[gmmu_page_size_big]))
		return -ENOMEM;

	nvhost_dbg_info("init space for pmu va_limit=0x%llx num_pdes=%d",
		   vm->va_limit, vm->pdes.num_pdes);

	/* allocate the page table directory */
	err = alloc_gmmu_pages(vm, 0, &vm->pdes.ref,
			       &vm->pdes.sgt, &vm->pdes.size);
	if (err)
		goto clean_up;

	err = map_gmmu_pages(vm->pdes.ref, vm->pdes.sgt, &vm->pdes.kv,
			     vm->pdes.size);
	if (err) {
		free_gmmu_pages(vm, vm->pdes.ref, vm->pdes.sgt, 0,
					vm->pdes.size);
		goto clean_up;
	}
	nvhost_dbg_info("pmu pdes phys @ 0x%llx",
			(u64)gk20a_mm_iova_addr(vm->pdes.sgt->sgl));
	/* we could release vm->pdes.kv but it's only one page... */

	pde_addr = gk20a_mm_iova_addr(vm->pdes.sgt->sgl);
	pde_addr_lo = u64_lo32(pde_addr >> 12);
	pde_addr_hi = u64_hi32(pde_addr);

	nvhost_dbg_info("pde pa=0x%llx pde_addr_lo=0x%x pde_addr_hi=0x%x",
			(u64)pde_addr, pde_addr_lo, pde_addr_hi);

	/* allocate instance mem for pmu */
	inst_block->size = GK20A_PMU_INST_SIZE;
	inst_block->cpuva = dma_alloc_coherent(d, inst_block->size,
				&inst_block->iova, GFP_KERNEL);
	if (!inst_block->cpuva) {
		nvhost_err(d, "%s: memory allocation failed\n", __func__);
		err = -ENOMEM;
		goto clean_up;
	}

	inst_block->cpu_pa = gk20a_get_phys_from_iova(d, inst_block->iova);
	if (!inst_block->cpu_pa) {
		nvhost_err(d, "%s: failed to get phys address\n", __func__);
		err = -ENOMEM;
		goto clean_up;
	}

	inst_pa = inst_block->cpu_pa;
	inst_ptr = inst_block->cpuva;

	nvhost_dbg_info("pmu inst block physical addr: 0x%llx", (u64)inst_pa);

	memset(inst_ptr, 0, GK20A_PMU_INST_SIZE);

	mem_wr32(inst_ptr, ram_in_page_dir_base_lo_w(),
		ram_in_page_dir_base_target_vid_mem_f() |
		ram_in_page_dir_base_vol_true_f() |
		ram_in_page_dir_base_lo_f(pde_addr_lo));

	mem_wr32(inst_ptr, ram_in_page_dir_base_hi_w(),
		ram_in_page_dir_base_hi_f(pde_addr_hi));

	mem_wr32(inst_ptr, ram_in_adr_limit_lo_w(),
		 u64_lo32(vm->va_limit) | 0xFFF);

	mem_wr32(inst_ptr, ram_in_adr_limit_hi_w(),
		ram_in_adr_limit_hi_f(u64_hi32(vm->va_limit)));

	nvhost_allocator_init(&vm->vma[gmmu_page_size_small], "gk20a_pmu",
			      (vm->va_start >> 12), /* start */
			      (vm->va_limit - vm->va_start) >> 12, /*length*/
			      1); /* align */
	/* initialize just in case we try to use it anyway */
	nvhost_allocator_init(&vm->vma[gmmu_page_size_big], "gk20a_pmu-unused",
			      0x0badc0de, /* start */
			      1, /* length */
			      1); /* align */


	vm->mapped_buffers = RB_ROOT;

	mutex_init(&vm->update_gmmu_lock);
	kref_init(&vm->ref);
	INIT_LIST_HEAD(&vm->reserved_va_list);

	return 0;

clean_up:
	/* free, etc */
	if (inst_block->cpuva)
		dma_free_coherent(d, inst_block->size,
			inst_block->cpuva, inst_block->iova);
	inst_block->cpuva = NULL;
	inst_block->iova = 0;
	return err;
}

/* Flushes the compression bit cache as well as "data".
 * Note: the name here is a bit of a misnomer.  ELPG uses this
 * internally... but ELPG doesn't have to be on to do it manually.
 */
static void gk20a_mm_g_elpg_flush_locked(struct gk20a *g)
{
	u32 data;
	s32 retry = 100;

	nvhost_dbg_fn("");

	/* Make sure all previous writes are committed to the L2. There's no
	   guarantee that writes are to DRAM. This will be a sysmembar internal
	   to the L2. */
	gk20a_writel(g, ltc_ltss_g_elpg_r(),
		     ltc_ltss_g_elpg_flush_pending_f());
	do {
		data = gk20a_readl(g, ltc_ltss_g_elpg_r());

		if (ltc_ltss_g_elpg_flush_v(data) ==
		    ltc_ltss_g_elpg_flush_pending_v()) {
			nvhost_dbg_info("g_elpg_flush 0x%x", data);
			retry--;
			usleep_range(20, 40);
		} else
			break;
	} while (retry >= 0);

	if (retry < 0)
		nvhost_warn(dev_from_gk20a(g),
			    "g_elpg_flush too many retries");

}

void gk20a_mm_fb_flush(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	u32 data;
	s32 retry = 100;

	nvhost_dbg_fn("");

	mutex_lock(&mm->l2_op_lock);

	gk20a_mm_g_elpg_flush_locked(g);

	/* Make sure all previous writes are committed to the L2. There's no
	   guarantee that writes are to DRAM. This will be a sysmembar internal
	   to the L2. */
	gk20a_writel(g, flush_fb_flush_r(),
		flush_fb_flush_pending_busy_f());

	do {
		data = gk20a_readl(g, flush_fb_flush_r());

		if (flush_fb_flush_outstanding_v(data) ==
			flush_fb_flush_outstanding_true_v() ||
		    flush_fb_flush_pending_v(data) ==
			flush_fb_flush_pending_busy_v()) {
				nvhost_dbg_info("fb_flush 0x%x", data);
				retry--;
				usleep_range(20, 40);
		} else
			break;
	} while (retry >= 0);

	if (retry < 0)
		nvhost_warn(dev_from_gk20a(g),
			"fb_flush too many retries");

	mutex_unlock(&mm->l2_op_lock);
}

static void gk20a_mm_l2_invalidate_locked(struct gk20a *g)
{
	u32 data;
	s32 retry = 200;

	/* Invalidate any clean lines from the L2 so subsequent reads go to
	   DRAM. Dirty lines are not affected by this operation. */
	gk20a_writel(g, flush_l2_system_invalidate_r(),
		flush_l2_system_invalidate_pending_busy_f());

	do {
		data = gk20a_readl(g, flush_l2_system_invalidate_r());

		if (flush_l2_system_invalidate_outstanding_v(data) ==
			flush_l2_system_invalidate_outstanding_true_v() ||
		    flush_l2_system_invalidate_pending_v(data) ==
			flush_l2_system_invalidate_pending_busy_v()) {
				nvhost_dbg_info("l2_system_invalidate 0x%x",
						data);
				retry--;
				usleep_range(20, 40);
		} else
			break;
	} while (retry >= 0);

	if (retry < 0)
		nvhost_warn(dev_from_gk20a(g),
			"l2_system_invalidate too many retries");
}

void gk20a_mm_l2_invalidate(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	mutex_lock(&mm->l2_op_lock);
	gk20a_mm_l2_invalidate_locked(g);
	mutex_unlock(&mm->l2_op_lock);
}

void gk20a_mm_l2_flush(struct gk20a *g, bool invalidate)
{
	struct mm_gk20a *mm = &g->mm;
	u32 data;
	s32 retry = 200;

	nvhost_dbg_fn("");

	mutex_lock(&mm->l2_op_lock);

	/* Flush all dirty lines from the L2 to DRAM. Lines are left in the L2
	   as clean, so subsequent reads might hit in the L2. */
	gk20a_writel(g, flush_l2_flush_dirty_r(),
		flush_l2_flush_dirty_pending_busy_f());

	do {
		data = gk20a_readl(g, flush_l2_flush_dirty_r());

		if (flush_l2_flush_dirty_outstanding_v(data) ==
			flush_l2_flush_dirty_outstanding_true_v() ||
		    flush_l2_flush_dirty_pending_v(data) ==
			flush_l2_flush_dirty_pending_busy_v()) {
				nvhost_dbg_info("l2_flush_dirty 0x%x", data);
				retry--;
				usleep_range(20, 40);
		} else
			break;
	} while (retry >= 0);

	if (retry < 0)
		nvhost_warn(dev_from_gk20a(g),
			"l2_flush_dirty too many retries");

	if (invalidate)
		gk20a_mm_l2_invalidate_locked(g);

	mutex_unlock(&mm->l2_op_lock);
}


int gk20a_vm_find_buffer(struct vm_gk20a *vm, u64 gpu_va,
			 struct mem_mgr **mgr, struct mem_handle **r,
			 u64 *offset)
{
	struct mapped_buffer_node *mapped_buffer;

	nvhost_dbg_fn("gpu_va=0x%llx", gpu_va);

	mutex_lock(&vm->update_gmmu_lock);

	mapped_buffer = find_mapped_buffer_range_locked(&vm->mapped_buffers,
							gpu_va);
	if (!mapped_buffer) {
		mutex_unlock(&vm->update_gmmu_lock);
		return -EINVAL;
	}

	*mgr = mapped_buffer->memmgr;
	*r = mapped_buffer->handle_ref;
	*offset = gpu_va - mapped_buffer->addr;

	mutex_unlock(&vm->update_gmmu_lock);

	return 0;
}

void gk20a_mm_tlb_invalidate(struct vm_gk20a *vm)
{
	struct mm_gk20a *mm = vm->mm;
	struct gk20a *g = gk20a_from_vm(vm);
	u32 addr_lo = u64_lo32(gk20a_mm_iova_addr(vm->pdes.sgt->sgl) >> 12);
	u32 data;
	s32 retry = 200;

	nvhost_dbg_fn("");

	/* pagetables are considered sw states which are preserved after
	   prepare_poweroff. When gk20a deinit releases those pagetables,
	   common code in vm unmap path calls tlb invalidate that touches
	   hw. Use the power_on flag to skip tlb invalidation when gpu
	   power is turned off */

	if (!g->power_on)
		return;

	/* No need to invalidate if tlb is clean */
	mutex_lock(&vm->update_gmmu_lock);
	if (!vm->tlb_dirty) {
		mutex_unlock(&vm->update_gmmu_lock);
		return;
	}
	vm->tlb_dirty = false;
	mutex_unlock(&vm->update_gmmu_lock);

	mutex_lock(&mm->tlb_lock);
	do {
		data = gk20a_readl(g, fb_mmu_ctrl_r());
		if (fb_mmu_ctrl_pri_fifo_space_v(data) != 0)
			break;
		usleep_range(20, 40);
		retry--;
	} while (retry >= 0);

	if (retry < 0)
		nvhost_warn(dev_from_gk20a(g),
			"wait mmu fifo space too many retries");

	gk20a_writel(g, fb_mmu_invalidate_pdb_r(),
		fb_mmu_invalidate_pdb_addr_f(addr_lo) |
		fb_mmu_invalidate_pdb_aperture_vid_mem_f());

	/* this is a sledgehammer, it would seem */
	gk20a_writel(g, fb_mmu_invalidate_r(),
		fb_mmu_invalidate_all_pdb_true_f() |
		fb_mmu_invalidate_all_va_true_f() |
		fb_mmu_invalidate_trigger_true_f());

	do {
		data = gk20a_readl(g, fb_mmu_ctrl_r());
		if (fb_mmu_ctrl_pri_fifo_empty_v(data) !=
			fb_mmu_ctrl_pri_fifo_empty_false_f())
			break;
		retry--;
		usleep_range(20, 40);
	} while (retry >= 0);

	if (retry < 0)
		nvhost_warn(dev_from_gk20a(g),
			"mmu invalidate too many retries");

	mutex_unlock(&mm->tlb_lock);
}

#if 0 /* VM DEBUG */

/* print pdes/ptes for a gpu virtual address range under a vm */
void gk20a_mm_dump_vm(struct vm_gk20a *vm,
		u64 va_begin, u64 va_end, char *label)
{
	struct mem_mgr *client = mem_mgr_from_vm(vm);
	struct mm_gk20a *mm = vm->mm;
	struct page_table_gk20a *pte_s;
	u64 pde_va, pte_va;
	u32 pde_i, pde_lo, pde_hi;
	u32 pte_i, pte_lo, pte_hi;
	u32 pte_space_page_cur, pte_space_offset_cur;
	u32 pte_space_page_offset;
	u32 num_ptes, page_size;
	void *pde, *pte;
	phys_addr_t pte_addr;
	int err;

	pde_range_from_vaddr_range(vm, va_begin, va_end,
			&pde_lo, &pde_hi);

	nvhost_err(dev_from_vm(vm),
		"%s page table entries for gpu va 0x%016llx -> 0x%016llx\n",
		label, va_begin, va_end);

	for (pde_i = pde_lo; pde_i <= pde_hi; pde_i++) {
		pde = pde_from_index(vm, pde_i);
		pde_va = pde_i * mm->pde_stride;
		nvhost_err(dev_from_vm(vm),
			"\t[0x%016llx -> 0x%016llx] pde @ 0x%08x: 0x%08x, 0x%08x\n",
			pde_va, pde_va + mm->pde_stride - 1,
			gk20a_mm_iova_addr(vm->pdes.sgt->sgl)
				+ pde_i * gmmu_pde__size_v(),
			mem_rd32(pde, 0), mem_rd32(pde, 1));

		pte_s = vm->pdes.ptes[pte_s->pgsz_idx] + pde_i;

		num_ptes = mm->page_table_sizing[pte_s->pgsz_idx].num_ptes;
		page_size = mm->pde_stride / num_ptes;
		pte_lo = 0;
		pte_hi = num_ptes - 1;

		pte_space_page_offset_from_index(pte_lo,
						&pte_space_page_cur,
						&pte_space_offset_cur);

		err = map_gmmu_pages(pte_s->ref, pte_s->sgt, &pte);
		pte_s->sgt = nvhost_memmgr_sg_table(client, pte_s->ref);
		if (WARN_ON(IS_ERR(pte_s->sgt)))
			return;
		pte_addr = gk20a_mm_iova_addr(pte_s->sgt->sgl);

		for (pte_i = pte_lo; pte_i <= pte_hi; pte_i++) {

			pte_va = pde_va + pte_i * page_size;

			if (pte_va < va_begin)
				continue;
			if (pte_va > va_end)
				break;

			pte_space_page_offset = pte_i;

			nvhost_err(dev_from_vm(vm),
				"\t\t[0x%016llx -> 0x%016llx] pte @ 0x%08x : 0x%08x, 0x%08x\n",
				pte_va, pte_va + page_size - 1,
				pte_addr + pte_i * gmmu_pte__size_v(),
				mem_rd32(pte + pte_space_page_offset * 8, 0),
				mem_rd32(pte + pte_space_page_offset * 8, 1));
		}

		unmap_gmmu_pages(pte_s->ref, pte_s->sgt, pte);
	}
}
#endif /* VM DEBUG */

int gk20a_mm_suspend(struct gk20a *g)
{
	nvhost_dbg_fn("");

	gk20a_gr_flush_comptags(g, true);
	gk20a_mm_fb_flush(g);
	gk20a_mm_l2_flush(g, true);

	nvhost_dbg_fn("done");
	return 0;
}

void gk20a_mm_ltc_isr(struct gk20a *g)
{
	u32 intr;

	intr = gk20a_readl(g, ltc_ltc0_ltss_intr_r());
	nvhost_err(dev_from_gk20a(g), "ltc: %08x\n", intr);
	gk20a_writel(g, ltc_ltc0_ltss_intr_r(), intr);
}

bool gk20a_mm_mmu_debug_mode_enabled(struct gk20a *g)
{
	u32 debug_ctrl = gk20a_readl(g, fb_mmu_debug_ctrl_r());
	return fb_mmu_debug_ctrl_debug_v(debug_ctrl) ==
		fb_mmu_debug_ctrl_debug_enabled_v();
}

