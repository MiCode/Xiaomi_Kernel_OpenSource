/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef KGSL_REGMAP_H
#define KGSL_REGMAP_H

struct kgsl_regmap;
struct kgsl_regmap_region;

/**
 * @ksgl_regmap_ops - Helper functions to access registers in a regmap region
 */
struct kgsl_regmap_ops {
	/**
	 * @preaccess: called before accesses to the register. This is used by
	 * adreno to call kgsl_pre_hwaccess()
	 */
	void (*preaccess)(struct kgsl_regmap_region *region);
};

/**
 * struct kgsl_regmap_region - Defines a region of registers in a kgsl_regmap
 */
struct kgsl_regmap_region {
	/** @virt: Kernel address for the re-mapped region */
	void __iomem *virt;
	/** @offset: Dword offset of the region from the regmap base */
	u32 offset;
	/** @size: Size of the region in dwords */
	u32 size;
	/** @ops: Helper functions to access registers in the region */
	const struct kgsl_regmap_ops *ops;
	/** @priv: Private data to send to the ops */
	void *priv;
};

/**
 * struct kgsl_regmap - Define a set of individual regions that are all indexed
 * from a commmon base. This is used to access GPU and GMU registers in
 * separate io-remmaped regions from a single set of function calls.
 */
struct kgsl_regmap {
	/**
	 * @base: Resource pointer for the "base" region (the region that all
	 * other regions are indexed from)
	 */
	struct resource *base;
	/** @region: Array of regions for this regmap */
	struct kgsl_regmap_region region[3];
	/** @count: Number of active regions in @region */
	int count;
};

/**
 * struct kgsl_regmap_list
 */
struct kgsl_regmap_list {
	/** offset: Dword offset of the register to write */
	u32 offset;
	/** val: Value to write */
	u32 val;
};

/**
 * kgsl_regmap_init - Initialize a regmap
 * @pdev: Pointer to the platform device that owns @name
 * @regmap: Pointer to the regmap to initialize
 * @name: Name of the resource to map
 * @ops: Pointer to the regmap ops for this region
 * @priv: Private data to pass to the regmap ops
 *
 * Initialize a regmap and set the resource @name as the base region in the
 * regmap. All other regions will be indexed from the start of this region.
 * This will nominally be the start of the GPU register region.
 *
 * Return: 0 on success or negative error on failure.
 */
int kgsl_regmap_init(struct platform_device *pdev, struct kgsl_regmap *regmap,
		const char *name, const struct kgsl_regmap_ops *ops,
		void *priv);

/**
 * kgsl_regmap_add_region - Add a region to an existing regmap
 * @regmap: The regmap to add the region to
 * @pdev: Pointer to the platform device that owns @name
 * @name: Name of the resource to map
 * @ops: Pointer to the regmap ops for this region
 * @priv: Private data to pass to the regmap ops
 *
 * Add a new region to the regmap. It will be indexed against the base
 * address already defined when the regmap was initialized. For example,
 * if the base GPU address is at physical address 0x3d000000 and the new
 * region is at physical address 0x3d010000 this region will be added at
 * (0x3d010000 - 0x3d000000) or dword offset 0x4000.
 *
 * Return: 0 on success or negative error on failure.
 */
int kgsl_regmap_add_region(struct kgsl_regmap *regmap, struct platform_device *pdev,
		const char *name, const struct kgsl_regmap_ops *ops, void *priv);

/**
 * kgsl_regmap_read - Read a register from the regmap
 * @regmap: The regmap to read from
 * @offset: The dword offset to read
 *
 * Read the register at the specified offset indexed against the base address in
 * the regmap. An offset that falls out of mapped regions will WARN and return
 * 0.
 *
 * Return: The value of the register at @offset
 */
u32 kgsl_regmap_read(struct kgsl_regmap *regmap, u32 offset);

/**
 * kgsl_regmap_write - Write a register to the regmap
 * @regmap: The regmap to write to
 * @data: The value to write to @offset
 * @offset: The dword offset to write
 *
 * Write @data to the register at the specified offset indexed against the base
 * address in he regmap. An offset that falls out of mapped regions will WARN
 * and skip the write.
 */
void kgsl_regmap_write(struct kgsl_regmap *regmap, u32 value, u32 offset);

/**
 * kgsl_regmap_multi_write - Write a list of registers
 * @regmap: The regmap to write to
 * @list: A pointer to an array of &strut kgsl_regmap_list items
 * @count: NUmber of items in @list
 *
 * Write all the registers in @list to the regmap.
 */

void kgsl_regmap_multi_write(struct kgsl_regmap *regmap,
	const struct kgsl_regmap_list *list, int count);

/**
 * kgsl_regmap_rmw - read-modify-write a register in the regmap
 * @regmap: The regmap to write to
 * @offset: The dword offset to write
 * @mask: Mask the register contents against this mask
 * @or: OR these bits into the register before writing it back again
 *
 * Read the register at @offset, mask it against @mask, OR the bits in @or and
 * write it back to @offset. @offset will be indexed against the base
 * address in the regmap. An offset that falls out of mapped regions will WARN
 * and skip the operation.
 */
void kgsl_regmap_rmw(struct kgsl_regmap *regmap, u32 offset, u32 mask,
		u32 or);

/**
 * kgsl_regmap_bulk_write - Write an array of values to a I/O region
 * @regmap: The regmap to write to
 * @offset: The dword offset to start writing to
 * @data: The data to write
 * @dwords: Number of dwords to write
 *
 * Bulk write @data to the I/O region starting at @offset for @dwords.
 * The write operation must fit fully inside a single region (no crossing the
 * boundaries). @offset will be indexed against the base
 * address in he regmap. An offset that falls out of mapped regions will WARN
 * and skip the operation.
 */
void kgsl_regmap_bulk_write(struct kgsl_regmap *regmap, u32 offset,
		const void *data, int dwords);

/**
 * kgsl_regmap_bulk_read - Read an array of values to a I/O region
 * @regmap: The regmap to read from
 * @offset: The dword offset to start reading from
 * @data: The data pointer to read into
 * @dwords: Number of dwords to read
 *
 * Bulk read into @data the I/O region starting at @offset for @dwords.
 * The read operation must fit fully inside a single region (no crossing the
 * boundaries). @offset will be indexed against the base
 * address in the regmap. An offset that falls out of mapped regions will WARN
 * and skip the operation.
 */
void kgsl_regmap_bulk_read(struct kgsl_regmap *regmap, u32 offset,
		const void *data, int dwords);

/**
 * kgsl_regmap_virt - Return the kernel address for a offset
 * @regmap: The regmap to write to
 * @offset: The dword offset to map to a kernel address
 *
 * Return: The kernel address for @offset or NULL if out of range.
 */
void __iomem *kgsl_regmap_virt(struct kgsl_regmap *regmap, u32 offset);

/**
 * kgsl_regmap_read_indexed - Read a indexed pair of registers
 * @regmap: The regmap to read from
 * @addr: The offset of the address register for the index pair
 * @data: The offset of the data register for the index pair
 * @dest: An array to put the values
 * @count: Number of dwords to read from @data
 *
 * This function configures the address register once and then
 * reads from the data register in a loop.
 */
void kgsl_regmap_read_indexed(struct kgsl_regmap *regmap, u32 addr,
		u32 data, u32 *dest, int count);

/**
 * kgsl_regmap_read_indexed_interleaved - Dump an indexed pair of registers
 * @regmap: The regmap to read from
 * @addr: The offset of the address register for the index pair
 * @data: The offset of the data register for the index pair
 * @dest: An array to put the values
 * @start: Starting value to be programmed in the address register
 * @count: Number of dwords to read from @data
 *
 * This function is slightly different than kgsl_regmap_read_indexed()
 * in that it takes as argument a start value that is to be programmed
 * in the address register and secondly, the address register is to be
 * configured before every read of the data register.
 */
void kgsl_regmap_read_indexed_interleaved(struct kgsl_regmap *regmap, u32 addr,
		u32 data, u32 *dest, u32 start, int count);

/**
 * kgsl_regmap_get_region - Return the region for the given offset
 * @regmap: The regmap to query
 * @offset: The offset to query
 *
 * Return: The &struct kgsl_regmap_region that owns the offset or NULL
 */
struct kgsl_regmap_region *kgsl_regmap_get_region(struct kgsl_regmap *regmap,
		u32 offset);

/**
 * kgsl_regmap_poll_read - A helper function for kgsl_regmap_read_poll_timeout
 * @region: Pointer to a &struct kgsl_regmap_region
 * @offset: Offset to read
 * @val: Pointer for the result
 *
 * This is a special helper function to be called only from
 * kgsl_regmap_read_poll_timeout.
 *
 * Return: 0 on success or -ENODEV if the region is NULL.
 */
int kgsl_regmap_poll_read(struct kgsl_regmap_region *region, u32 offset,
		u32 *val);

#define kgsl_regmap_read_poll_timeout(regmap, offset, val, cond,		\
		sleep_us, timeout_us)						\
({										\
	int __ret, __tmp;							\
	struct kgsl_regmap_region *region =					\
		kgsl_regmap_get_region(regmap, offset);				\
										\
	if (region && region->ops && region->ops->preaccess)			\
		region->ops->preaccess(region);					\
	__tmp = read_poll_timeout(kgsl_regmap_poll_read, __ret, __ret || (cond),\
			sleep_us, timeout_us, false, region, offset, &(val));	\
	__ret ?: __tmp;								\
})

#endif
