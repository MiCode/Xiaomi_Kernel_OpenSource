/*
 * drivers/video/tegra/host/gk20a/mm_gk20a.h
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
#ifndef __MM_GK20A_H__
#define __MM_GK20A_H__

#include <linux/scatterlist.h>
#include <linux/iommu.h>
#include <asm/dma-iommu.h>
#include "../nvhost_allocator.h"

/* This "address bit" in the gmmu ptes (and other gk20a accesses)
 * signals the address as presented should be translated by the SMMU.
 * Without this bit present gk20a accesses are *not* translated.
 */
/* Hack, get this from manuals somehow... */
#define NV_MC_SMMU_VADDR_TRANSLATION_BIT     34
#define NV_MC_SMMU_VADDR_TRANSLATE(x) (x | \
				(1ULL << NV_MC_SMMU_VADDR_TRANSLATION_BIT))

/* For now keep the size relatively small-ish compared to the full
 * 40b va.  32GB for now. It consists of two 16GB spaces. */
#define NV_GMMU_VA_RANGE	35ULL
#define NV_GMMU_VA_IS_UPPER(x)	((x) >= ((u64)0x1 << (NV_GMMU_VA_RANGE-1)))

struct mem_desc {
	struct mem_handle *ref;
	struct sg_table *sgt;
	u32 size;
};

struct mem_desc_sub {
	u32 offset;
	u32 size;
};

struct gpfifo_desc {
	size_t size;
	u32 entry_num;

	u32 get;
	u32 put;

	bool wrap;

	u64 iova;
	struct gpfifo *cpu_va;
	u64 gpu_va;
};

struct mmu_desc {
	void *cpuva;
	u64 iova;
	size_t size;
};

struct inst_desc {
	u64 iova;
	void *cpuva;
	phys_addr_t cpu_pa;
	size_t size;
};

struct surface_mem_desc {
	u64 iova;
	void *cpuva;
	struct sg_table *sgt;
	size_t size;
};

struct userd_desc {
	struct sg_table *sgt;
	u64 iova;
	void *cpuva;
	size_t size;
	u64 gpu_va;
};

struct runlist_mem_desc {
	u64 iova;
	void *cpuva;
	size_t size;
};

struct patch_desc {
	struct page **pages;
	u64 iova;
	size_t size;
	void *cpu_va;
	u64 gpu_va;
	u32 data_count;
};

struct pmu_mem_desc {
	void *cpuva;
	u64 iova;
	u64 pmu_va;
	size_t size;
};

struct priv_cmd_queue_mem_desc {
	u64 base_iova;
	u32 *base_cpuva;
	size_t size;
};

struct zcull_ctx_desc {
	struct mem_desc mem;
	u64 gpu_va;
	u32 ctx_attr;
	u32 ctx_sw_mode;
};

struct pm_ctx_desc {
	struct mem_desc mem;
	u64 gpu_va;
	u32 ctx_attr;
	u32 ctx_sw_mode;
};

struct gr_ctx_desc {
	struct page **pages;
	u64 iova;
	size_t size;
	u64 gpu_va;
};

struct compbit_store_desc {
	struct mem_desc mem;
	u64 base_pa;
	u32 alignment;
};

struct page_table_gk20a {
	/* backing for */
	/* Either a *page or a *mem_handle */
	void *ref;
	/* track mapping cnt on this page table */
	u32 ref_cnt;
	struct sg_table *sgt;
	size_t size;
};

enum gmmu_pgsz_gk20a {
	gmmu_page_size_small = 0,
	gmmu_page_size_big   = 1,
	gmmu_nr_page_sizes   = 2
};


struct page_directory_gk20a {
	/* backing for */
	u32 num_pdes;
	void *kv;
	/* Either a *page or a *mem_handle */
	void *ref;
	struct sg_table *sgt;
	size_t size;
	struct page_table_gk20a *ptes[gmmu_nr_page_sizes];
};

struct mapped_buffer_node {
	struct vm_gk20a *vm;
	struct rb_node node;
	struct list_head unmap_list;
	struct list_head va_buffers_list;
	struct vm_reserved_va_node *va_node;
	u64 addr;
	u64 size;
	struct mem_mgr *memmgr;
	struct mem_handle *handle_ref;
	struct sg_table *sgt;
	struct kref ref;
	u32 user_mapped;
	bool own_mem_ref;
	u32 pgsz_idx;
	u32 ctag_offset;
	u32 ctag_lines;
	u32 flags;
	bool va_allocated;
};

struct vm_reserved_va_node {
	struct list_head reserved_va_list;
	struct list_head va_buffers_list;
	u32 pgsz_idx;
	u64 vaddr_start;
	u64 size;
	bool sparse;
};

struct vm_gk20a {
	struct mm_gk20a *mm;
	struct nvhost_as_share *as_share; /* as_share this represents */

	u64 va_start;
	u64 va_limit;

	int num_user_mapped_buffers;

	bool big_pages;   /* enable large page support */
	bool enable_ctag;
	bool tlb_dirty;
	bool mapped;

	struct kref ref;

	struct mutex update_gmmu_lock;

	struct page_directory_gk20a pdes;

	struct nvhost_allocator vma[gmmu_nr_page_sizes];
	struct rb_root mapped_buffers;

	struct list_head reserved_va_list;

	dma_addr_t zero_page_iova;
	void *zero_page_cpuva;
	struct sg_table *zero_page_sgt;
};

struct gk20a;
struct channel_gk20a;

int gk20a_init_mm_support(struct gk20a *g);
int gk20a_init_mm_setup_sw(struct gk20a *g);
int gk20a_init_bar1_vm(struct mm_gk20a *mm);
int gk20a_init_pmu_vm(struct mm_gk20a *mm);

void gk20a_mm_fb_flush(struct gk20a *g);
void gk20a_mm_l2_flush(struct gk20a *g, bool invalidate);
void gk20a_mm_l2_invalidate(struct gk20a *g);

struct mm_gk20a {
	struct gk20a *g;

	u32 big_page_size;
	u32 pde_stride;
	u32 pde_stride_shift;

	struct {
		u32 order;
		u32 num_ptes;
	} page_table_sizing[gmmu_nr_page_sizes];


	struct {
		u64 size;
	} channel;

	struct {
		u32 aperture_size;
		struct vm_gk20a vm;
		struct inst_desc inst_block;
	} bar1;

	struct {
		u32 aperture_size;
		struct vm_gk20a vm;
		struct inst_desc inst_block;
	} pmu;

	struct mutex tlb_lock;
	struct mutex l2_op_lock;

	void (*remove_support)(struct mm_gk20a *mm);
	bool sw_ready;
#ifdef CONFIG_DEBUG_FS
	u32 ltc_enabled;
	u32 ltc_enabled_debug;
#endif
};

int gk20a_mm_init(struct mm_gk20a *mm);

#define gk20a_from_mm(mm) ((mm)->g)
#define gk20a_from_vm(vm) ((vm)->mm->g)

#define mem_mgr_from_mm(mm) (gk20a_from_mm(mm)->host->memmgr)
#define mem_mgr_from_vm(vm) (gk20a_from_vm(vm)->host->memmgr)
#define dev_from_vm(vm) dev_from_gk20a(vm->mm->g)

#define DEFAULT_ALLOC_FLAGS (mem_mgr_flag_uncacheable)
#define DEFAULT_ALLOC_ALIGNMENT (4*1024)

static inline int bar1_aperture_size_mb_gk20a(void)
{
	return 128; /*TBD read this from fuses?*/
}
/* max address bits */
static inline int max_physaddr_bits_gk20a(void)
{
	return 40;/*"old" sys physaddr, meaningful? */
}
static inline int max_vid_physaddr_bits_gk20a(void)
{
	/* "vid phys" is asid/smmu phys?,
	 * i.e. is this the real sys physaddr? */
	return 37;
}
static inline int max_vaddr_bits_gk20a(void)
{
	return 40; /* chopped for area? */
}

#if 0 /*related to addr bits above, concern below TBD on which is accurate */
#define bar1_instance_block_shift_gk20a() (max_physaddr_bits_gk20a() -\
					   bus_bar1_block_ptr_s())
#else
#define bar1_instance_block_shift_gk20a() bus_bar1_block_ptr_shift_v()
#endif

void gk20a_mm_dump_vm(struct vm_gk20a *vm,
		u64 va_begin, u64 va_end, char *label);

int gk20a_mm_suspend(struct gk20a *g);

phys_addr_t gk20a_get_phys_from_iova(struct device *d,
				u64 dma_addr);

int gk20a_get_sgtable(struct device *d, struct sg_table **sgt,
			void *cpuva, u64 iova,
			size_t size);

int gk20a_get_sgtable_from_pages(struct device *d, struct sg_table **sgt,
			struct page **pages, u64 iova,
			size_t size);

void gk20a_free_sgtable(struct sg_table **sgt);

u64 gk20a_mm_iova_addr(struct scatterlist *sgl);

void gk20a_mm_ltc_isr(struct gk20a *g);

bool gk20a_mm_mmu_debug_mode_enabled(struct gk20a *g);

u64 gk20a_gmmu_map(struct vm_gk20a *vm,
		struct sg_table **sgt,
		u64 size,
		u32 flags,
		int rw_flag);

void gk20a_gmmu_unmap(struct vm_gk20a *vm,
		u64 vaddr,
		u64 size,
		int rw_flag);

u64 gk20a_vm_map(struct vm_gk20a *vm,
		 struct mem_mgr *memmgr,
		 struct mem_handle *r,
		 u64 offset_align,
		 u32 flags /*NVHOST_MAP_BUFFER_FLAGS_*/,
		 u32 kind,
		 struct sg_table **sgt,
		 bool user_mapped,
		 int rw_flag);

/* unmap handle from kernel */
void gk20a_vm_unmap(struct vm_gk20a *vm, u64 offset);

/* get reference to all currently mapped buffers */
int gk20a_vm_get_buffers(struct vm_gk20a *vm,
			 struct mapped_buffer_node ***mapped_buffers,
			 int *num_buffers);

/* put references on the given buffers */
void gk20a_vm_put_buffers(struct vm_gk20a *vm,
			  struct mapped_buffer_node **mapped_buffers,
			  int num_buffers);

/* invalidate tlbs for the vm area */
void gk20a_mm_tlb_invalidate(struct vm_gk20a *vm);

/* find buffer corresponding to va */
int gk20a_vm_find_buffer(struct vm_gk20a *vm, u64 gpu_va,
			 struct mem_mgr **memmgr, struct mem_handle **r,
			 u64 *offset);

void gk20a_vm_get(struct vm_gk20a *vm);
void gk20a_vm_put(struct vm_gk20a *vm);

#endif /*_MM_GK20A_H_ */
