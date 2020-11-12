/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2002,2007-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_SHAREDMEM_H
#define __KGSL_SHAREDMEM_H

#include <linux/bitfield.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "kgsl.h"
#include "kgsl_mmu.h"

struct kgsl_device;
struct kgsl_process_private;

#define KGSL_CACHE_OP_INV       0x01
#define KGSL_CACHE_OP_FLUSH     0x02
#define KGSL_CACHE_OP_CLEAN     0x03

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc);

int kgsl_sharedmem_readl(const struct kgsl_memdesc *memdesc,
			uint32_t *dst,
			uint64_t offsetbytes);

/**
 * kgsl_sharedmem_writel - write a 32 bit value to a shared memory object
 * @memdesc: Pointer to a GPU memory object
 * @offsetbytes: Offset inside of @memdesc to write to
 * @src: Value to write
 *
 * Write @src to @offsetbytes from the start of @memdesc
 */
void kgsl_sharedmem_writel(const struct kgsl_memdesc *memdesc,
			uint64_t offsetbytes,
			uint32_t src);

int kgsl_sharedmem_readq(const struct kgsl_memdesc *memdesc,
			uint64_t *dst,
			uint64_t offsetbytes);

/**
 * kgsl_sharedmem_writeq - write a 64 bit value to a shared memory object
 * @memdesc: Pointer to a GPU memory object
 * @offsetbytes: Offset inside of @memdesc to write to
 * @src: Value to write
 *
 * Write @src to @offsetbytes from the start of @memdesc
 */
void kgsl_sharedmem_writeq(const struct kgsl_memdesc *memdesc,
			uint64_t offsetbytes,
			uint64_t src);

int kgsl_cache_range_op(struct kgsl_memdesc *memdesc,
			uint64_t offset, uint64_t size,
			unsigned int op);

void kgsl_memdesc_init(struct kgsl_device *device,
			struct kgsl_memdesc *memdesc, uint64_t flags);

void kgsl_process_init_sysfs(struct kgsl_device *device,
		struct kgsl_process_private *private);

int kgsl_sharedmem_init_sysfs(void);

void kgsl_get_memory_usage(char *str, size_t len, uint64_t memflags);

void kgsl_free_secure_page(struct page *page);

struct page *kgsl_alloc_secure_page(void);

/**
 * kgsl_allocate_user - Allocate user visible GPU memory
 * @device: A GPU device handle
 * @memdesc: Memory descriptor for the object
 * @size: Size of the allocation in bytes
 * @flags: Control flags for the allocation
 * @priv: Internal flags for the allocation
 *
 * Allocate GPU memory on behalf of the user.
 * Return: 0 on success or negative on failure.
 */
int kgsl_allocate_user(struct kgsl_device *device, struct kgsl_memdesc *memdesc,
		u64 size, u64 flags, u32 priv);

/**
 * kgsl_allocate_kernel - Allocate kernel visible GPU memory
 * @device: A GPU device handle
 * @memdesc: Memory descriptor for the object
 * @size: Size of the allocation in bytes
 * @flags: Control flags for the allocation
 * @priv: Internal flags for the allocation
 *
 * Allocate GPU memory on for use by the kernel. Kernel objects are
 * automatically mapped into the kernel address space (except for secure).
 * Return: 0 on success or negative on failure.
 */
int kgsl_allocate_kernel(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u64 size, u64 flags, u32 priv);

/**
 * kgsl_allocate_global - Allocate a global GPU memory object
 * @device: A GPU device handle
 * @size: Size of the allocation in bytes
 * @padding: Amount of extra adding to add to the VA allocation
 * @flags: Control flags for the allocation
 * @priv: Internal flags for the allocation
 * @name: Name of the allocation (for the debugfs file)
 *
 * Allocate a global GPU object for use by all processes. The buffer is
 * automatically mapped into the kernel address space and added to the list of
 * global buffers that get mapped into each newly created pagetable.
 * Return: The memory descriptor on success or a ERR_PTR encoded error on
 * failure.
 */
struct kgsl_memdesc *kgsl_allocate_global(struct kgsl_device *device,
		u64 size, u32 padding, u64 flags, u32 priv, const char *name);

/**
 * kgsl_allocate_global_fixed - Allocate a global GPU memory object from a fixed
 * region defined in the device tree
 * @device: A GPU device handle
 * @size: Size of the allocation in bytes
 * @flags: Control flags for the allocation
 * @priv: Internal flags for the allocation
 *
 * Allocate a global GPU object for use by all processes. The buffer is
 * added to the list of global buffers that get mapped into each newly created
 * pagetable.
 *
 * Return: The memory descriptor on success or a ERR_PTR encoded error on
 * failure.
 */
struct kgsl_memdesc *kgsl_allocate_global_fixed(struct kgsl_device *device,
		const char *resource, const char *name);

/**
 * kgsl_free_globals - Free all global objects
 * @device: A GPU device handle
 *
 * Free all the global buffer objects. Should only be called during shutdown
 * after the pagetables have been freed
 */
void kgsl_free_globals(struct kgsl_device *device);

/*
 * kgsl_memdesc_get_align - Get alignment flags from a memdesc
 * @memdesc - the memdesc
 *
 * Returns the alignment requested, as power of 2 exponent.
 */
static inline int
kgsl_memdesc_get_align(const struct kgsl_memdesc *memdesc)
{
	return FIELD_GET(KGSL_MEMALIGN_MASK, memdesc->flags);
}

/*
 * kgsl_memdesc_get_cachemode - Get cache mode of a memdesc
 * @memdesc: the memdesc
 *
 * Returns a KGSL_CACHEMODE* value.
 */
static inline int
kgsl_memdesc_get_cachemode(const struct kgsl_memdesc *memdesc)
{
	return FIELD_GET(KGSL_CACHEMODE_MASK, memdesc->flags);
}

static inline unsigned int
kgsl_memdesc_get_memtype(const struct kgsl_memdesc *memdesc)
{
	return FIELD_GET(KGSL_MEMTYPE_MASK, memdesc->flags);
}
/*
 * kgsl_memdesc_set_align - Set alignment flags of a memdesc
 * @memdesc - the memdesc
 * @align - alignment requested, as a power of 2 exponent.
 */
static inline int
kgsl_memdesc_set_align(struct kgsl_memdesc *memdesc, unsigned int align)
{
	if (align > 32)
		align = 32;

	memdesc->flags &= ~(uint64_t)KGSL_MEMALIGN_MASK;
	memdesc->flags |= FIELD_PREP(KGSL_MEMALIGN_MASK, align);
	return 0;
}

/**
 * kgsl_memdesc_usermem_type - return buffer type
 * @memdesc - the memdesc
 *
 * Returns a KGSL_MEM_ENTRY_* value for this buffer, which
 * identifies if was allocated by us, or imported from
 * another allocator.
 */
static inline unsigned int
kgsl_memdesc_usermem_type(const struct kgsl_memdesc *memdesc)
{
	return FIELD_GET(KGSL_MEMFLAGS_USERMEM_MASK, memdesc->flags);
}

/**
 * kgsl_memdesc_sg_dma - Turn a dma_addr (from CMA) into a sg table
 * @memdesc: Pointer to a memory descriptor
 * @addr: Physical address from the dma_alloc function
 * @size: Size of the chunk
 *
 * Create a sg table for the contiguous chunk specified by addr and size.
 *
 * Return: 0 on success or negative on failure.
 */
int kgsl_memdesc_sg_dma(struct kgsl_memdesc *memdesc,
		phys_addr_t addr, u64 size);

/*
 * kgsl_memdesc_is_global - is this a globally mapped buffer?
 * @memdesc: the memdesc
 *
 * Return: True if this is a global mapping
 */
static inline bool kgsl_memdesc_is_global(const struct kgsl_memdesc *memdesc)
{
	return memdesc && (memdesc->priv & KGSL_MEMDESC_GLOBAL);
}

/*
 * kgsl_memdesc_is_secured - is this a secure buffer?
 * @memdesc: the memdesc
 *
 * Returns true if this is a secure mapping, false otherwise
 */
static inline bool kgsl_memdesc_is_secured(const struct kgsl_memdesc *memdesc)
{
	return memdesc && (memdesc->priv & KGSL_MEMDESC_SECURE);
}

/*
 * kgsl_memdesc_use_cpu_map - use the same virtual mapping on CPU and GPU?
 * @memdesc: the memdesc
 *
 * Return: true if the memdesc is using SVM mapping
 */
static inline bool
kgsl_memdesc_use_cpu_map(const struct kgsl_memdesc *memdesc)
{
	return memdesc && (memdesc->flags & KGSL_MEMFLAGS_USE_CPU_MAP);
}

/*
 * kgsl_memdesc_footprint - get the size of the mmap region
 * @memdesc - the memdesc
 *
 * The entire memdesc must be mapped. Additionally if the
 * CPU mapping is going to be mirrored, there must be room
 * for the guard page to be mapped so that the address spaces
 * match up.
 */
static inline uint64_t
kgsl_memdesc_footprint(const struct kgsl_memdesc *memdesc)
{
	if (!(memdesc->priv & KGSL_MEMDESC_GUARD_PAGE))
		return memdesc->size;

	return PAGE_ALIGN(memdesc->size + PAGE_SIZE);
}

/**
 * kgsl_cachemode_is_cached - Return true if the passed flags indicate a cached
 * buffer
 * @flags: A bitmask of KGSL_MEMDESC_ flags
 *
 * Return: true if the flags indicate a cached buffer
 */
static inline bool kgsl_cachemode_is_cached(u64 flags)
{
	u64 mode = FIELD_GET(KGSL_CACHEMODE_MASK, flags);

	return (mode != KGSL_CACHEMODE_UNCACHED &&
		mode != KGSL_CACHEMODE_WRITECOMBINE);
}
#endif /* __KGSL_SHAREDMEM_H */
