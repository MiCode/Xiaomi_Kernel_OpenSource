/* Copyright (c) 2002,2007-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __KGSL_MMU_H
#define __KGSL_MMU_H

#define KGSL_MMU_ALIGN_SHIFT    13
#define KGSL_MMU_ALIGN_MASK     (~((1 << KGSL_MMU_ALIGN_SHIFT) - 1))

/* Identifier for the global page table */
/* Per process page tables will probably pass in the thread group
   as an identifier */

#define KGSL_MMU_GLOBAL_PT 0

struct kgsl_device;

#define GSL_PT_SUPER_PTE 8
#define GSL_PT_PAGE_WV		0x00000001
#define GSL_PT_PAGE_RV		0x00000002
#define GSL_PT_PAGE_DIRTY	0x00000004

/* MMU registers - the register locations for all cores are the
   same.  The method for getting to those locations differs between
   2D and 3D, but the 2D and 3D register functions do that magic
   for us */

#define MH_MMU_CONFIG                0x0040
#define MH_MMU_VA_RANGE              0x0041
#define MH_MMU_PT_BASE               0x0042
#define MH_MMU_PAGE_FAULT            0x0043
#define MH_MMU_TRAN_ERROR            0x0044
#define MH_MMU_INVALIDATE            0x0045
#define MH_MMU_MPU_BASE              0x0046
#define MH_MMU_MPU_END               0x0047

#define MH_INTERRUPT_MASK            0x0A42
#define MH_INTERRUPT_STATUS          0x0A43
#define MH_INTERRUPT_CLEAR           0x0A44
#define MH_AXI_ERROR                 0x0A45
#define MH_ARBITER_CONFIG            0x0A40
#define MH_DEBUG_CTRL                0x0A4E
#define MH_DEBUG_DATA                0x0A4F
#define MH_AXI_HALT_CONTROL          0x0A50
#define MH_CLNT_INTF_CTRL_CONFIG1    0x0A54
#define MH_CLNT_INTF_CTRL_CONFIG2    0x0A55

/* MH_MMU_CONFIG bit definitions */

#define MH_MMU_CONFIG__RB_W_CLNT_BEHAVIOR__SHIFT           0x00000004
#define MH_MMU_CONFIG__CP_W_CLNT_BEHAVIOR__SHIFT           0x00000006
#define MH_MMU_CONFIG__CP_R0_CLNT_BEHAVIOR__SHIFT          0x00000008
#define MH_MMU_CONFIG__CP_R1_CLNT_BEHAVIOR__SHIFT          0x0000000a
#define MH_MMU_CONFIG__CP_R2_CLNT_BEHAVIOR__SHIFT          0x0000000c
#define MH_MMU_CONFIG__CP_R3_CLNT_BEHAVIOR__SHIFT          0x0000000e
#define MH_MMU_CONFIG__CP_R4_CLNT_BEHAVIOR__SHIFT          0x00000010
#define MH_MMU_CONFIG__VGT_R0_CLNT_BEHAVIOR__SHIFT         0x00000012
#define MH_MMU_CONFIG__VGT_R1_CLNT_BEHAVIOR__SHIFT         0x00000014
#define MH_MMU_CONFIG__TC_R_CLNT_BEHAVIOR__SHIFT           0x00000016
#define MH_MMU_CONFIG__PA_W_CLNT_BEHAVIOR__SHIFT           0x00000018

/* MMU Flags */
#define KGSL_MMUFLAGS_TLBFLUSH         0x10000000
#define KGSL_MMUFLAGS_PTUPDATE         0x20000000

#define MH_INTERRUPT_MASK__AXI_READ_ERROR                  0x00000001L
#define MH_INTERRUPT_MASK__AXI_WRITE_ERROR                 0x00000002L
#define MH_INTERRUPT_MASK__MMU_PAGE_FAULT                  0x00000004L

#ifdef CONFIG_MSM_KGSL_MMU
#define KGSL_MMU_INT_MASK \
	(MH_INTERRUPT_MASK__AXI_READ_ERROR | \
	 MH_INTERRUPT_MASK__AXI_WRITE_ERROR | \
	 MH_INTERRUPT_MASK__MMU_PAGE_FAULT)
#else
#define KGSL_MMU_INT_MASK \
	(MH_INTERRUPT_MASK__AXI_READ_ERROR | \
	 MH_INTERRUPT_MASK__AXI_WRITE_ERROR)
#endif

enum kgsl_mmutype {
	KGSL_MMU_TYPE_GPU = 0,
	KGSL_MMU_TYPE_IOMMU,
	KGSL_MMU_TYPE_NONE
};

struct kgsl_pagetable {
	spinlock_t lock;
	struct kref refcount;
	unsigned int   max_entries;
	struct gen_pool *pool;
	struct list_head list;
	unsigned int name;
	struct kobject *kobj;

	struct {
		unsigned int entries;
		unsigned int mapped;
		unsigned int max_mapped;
		unsigned int max_entries;
	} stats;
	const struct kgsl_mmu_pt_ops *pt_ops;
	void *priv;
};

struct kgsl_mmu_ops {
	int (*mmu_init) (struct kgsl_device *device);
	int (*mmu_close) (struct kgsl_device *device);
	int (*mmu_start) (struct kgsl_device *device);
	int (*mmu_stop) (struct kgsl_device *device);
	void (*mmu_setstate) (struct kgsl_device *device,
		struct kgsl_pagetable *pagetable);
	void (*mmu_device_setstate) (struct kgsl_device *device,
					uint32_t flags);
	void (*mmu_pagefault) (struct kgsl_device *device);
	unsigned int (*mmu_get_current_ptbase)
			(struct kgsl_device *device);
};

struct kgsl_mmu_pt_ops {
	int (*mmu_map) (void *mmu_pt,
			struct kgsl_memdesc *memdesc,
			unsigned int protflags);
	int (*mmu_unmap) (void *mmu_pt,
			struct kgsl_memdesc *memdesc);
	void *(*mmu_create_pagetable) (void);
	void (*mmu_destroy_pagetable) (void *pt);
	int (*mmu_pt_equal) (struct kgsl_pagetable *pt,
			unsigned int pt_base);
	unsigned int (*mmu_pt_get_flags) (struct kgsl_pagetable *pt,
				enum kgsl_deviceid id);
};

struct kgsl_mmu {
	unsigned int     refcnt;
	uint32_t      flags;
	struct kgsl_device     *device;
	unsigned int     config;
	struct kgsl_memdesc    setstate_memory;
	/* current page table object being used by device mmu */
	struct kgsl_pagetable  *defaultpagetable;
	struct kgsl_pagetable  *hwpagetable;
	const struct kgsl_mmu_ops *mmu_ops;
	void *priv;
};

#include "kgsl_gpummu.h"

extern struct kgsl_mmu_ops iommu_ops;
extern struct kgsl_mmu_pt_ops iommu_pt_ops;

struct kgsl_pagetable *kgsl_mmu_getpagetable(unsigned long name);
void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable);
void kgsl_mh_start(struct kgsl_device *device);
void kgsl_mh_intrcallback(struct kgsl_device *device);
int kgsl_mmu_init(struct kgsl_device *device);
int kgsl_mmu_start(struct kgsl_device *device);
int kgsl_mmu_stop(struct kgsl_device *device);
int kgsl_mmu_close(struct kgsl_device *device);
int kgsl_mmu_map(struct kgsl_pagetable *pagetable,
		 struct kgsl_memdesc *memdesc,
		 unsigned int protflags);
int kgsl_mmu_map_global(struct kgsl_pagetable *pagetable,
			struct kgsl_memdesc *memdesc, unsigned int protflags);
int kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
		    struct kgsl_memdesc *memdesc);
unsigned int kgsl_virtaddr_to_physaddr(void *virtaddr);
void kgsl_setstate(struct kgsl_device *device, uint32_t flags);
void kgsl_mmu_device_setstate(struct kgsl_device *device, uint32_t flags);
void kgsl_mmu_setstate(struct kgsl_device *device,
			struct kgsl_pagetable *pt);
int kgsl_mmu_get_ptname_from_ptbase(unsigned int pt_base);
int kgsl_mmu_pt_get_flags(struct kgsl_pagetable *pt,
			enum kgsl_deviceid id);

void kgsl_mmu_ptpool_destroy(void *ptpool);
void *kgsl_mmu_ptpool_init(int ptsize, int entries);
int kgsl_mmu_enabled(void);
int kgsl_mmu_pt_equal(struct kgsl_pagetable *pt,
			unsigned int pt_base);
void kgsl_mmu_set_mmutype(char *mmutype);
unsigned int kgsl_mmu_get_current_ptbase(struct kgsl_device *device);
enum kgsl_mmutype kgsl_mmu_get_mmutype(void);
#endif /* __KGSL_MMU_H */
