#ifndef __IO_PGTABLE_H
#define __IO_PGTABLE_H

#include <linux/scatterlist.h>
#include <soc/qcom/msm_tz_smmu.h>

/*
 * Public API for use by IOMMU drivers
 */
enum io_pgtable_fmt {
	ARM_32_LPAE_S1,
	ARM_32_LPAE_S2,
	ARM_64_LPAE_S1,
	ARM_64_LPAE_S2,
	ARM_MSM_SECURE,
	ARM_V8L_FAST,
	IO_PGTABLE_NUM_FMTS,
};

/**
 * struct iommu_gather_ops - IOMMU callbacks for TLB and page table management.
 *
 * @tlb_flush_all: Synchronously invalidate the entire TLB context.
 * @tlb_add_flush: Queue up a TLB invalidation for a virtual address range.
 * @tlb_sync:      Ensure any queued TLB invalidation has taken effect, and
 *                 any corresponding page table updates are visible to the
 *                 IOMMU.
 * @alloc_pages_exact: Allocate page table memory (optional, defaults to
 *                     alloc_pages_exact)
 * @free_pages_exact:  Free page table memory (optional, defaults to
 *                     free_pages_exact)
 *
 * Note that these can all be called in atomic context and must therefore
 * not block.
 */
struct iommu_gather_ops {
	void (*tlb_flush_all)(void *cookie);
	void (*tlb_add_flush)(unsigned long iova, size_t size, bool leaf,
			      void *cookie);
	void (*tlb_sync)(void *cookie);
	void *(*alloc_pages_exact)(void *cookie, size_t size, gfp_t gfp_mask);
	void (*free_pages_exact)(void *cookie, void *virt, size_t size);
};

/**
 * struct io_pgtable_cfg - Configuration data for a set of page tables.
 *
 * @quirks:        A bitmap of hardware quirks that require some special
 *                 action by the low-level page table allocator.
 * @pgsize_bitmap: A bitmap of page sizes supported by this set of page
 *                 tables.
 * @ias:           Input address (iova) size, in bits.
 * @oas:           Output address (paddr) size, in bits.
 * @tlb:           TLB management callbacks for this set of tables.
 * @iommu_dev:     The device representing the DMA configuration for the
 *                 page table walker.
 */
struct io_pgtable_cfg {
	/*
	 * IO_PGTABLE_QUIRK_PAGE_TABLE_COHERENT: Set the page table as
	 * coherent.
	 */
	#define IO_PGTABLE_QUIRK_ARM_NS	(1 << 0)	/* Set NS bit in PTEs */
	#define IO_PGTABLE_QUIRK_PAGE_TABLE_COHERENT (1 << 1)
	int				quirks;
	unsigned long			pgsize_bitmap;
	unsigned int			ias;
	unsigned int			oas;
	const struct iommu_gather_ops	*tlb;
	struct device			*iommu_dev;

	/* Low-level data specific to the table format */
	union {
		struct {
			u64	ttbr[2];
			u64	tcr;
			u64	mair[2];
		} arm_lpae_s1_cfg;

		struct {
			u64	vttbr;
			u64	vtcr;
		} arm_lpae_s2_cfg;

		struct {
			enum tz_smmu_device_id sec_id;
			int cbndx;
		} arm_msm_secure_cfg;

		struct {
			u64	ttbr[2];
			u64	tcr;
			u64	mair[2];
			void	*pmds;
		} av8l_fast_cfg;
	};
};

/**
 * struct io_pgtable_ops - Page table manipulation API for IOMMU drivers.
 *
 * @map:          Map a physically contiguous memory region.
 * @map_sg:	  Map a scatterlist.  Returns the number of bytes mapped,
 *		  or 0 on failure.  The size parameter contains the size
 *		  of the partial mapping in case of failure.
 * @unmap:        Unmap a physically contiguous memory region.
 * @iova_to_phys: Translate iova to physical address.
 *
 * These functions map directly onto the iommu_ops member functions with
 * the same names.
 */
struct io_pgtable_ops {
	int (*map)(struct io_pgtable_ops *ops, unsigned long iova,
		   phys_addr_t paddr, size_t size, int prot);
	int (*map_sg)(struct io_pgtable_ops *ops, unsigned long iova,
			 struct scatterlist *sg, unsigned int nents,
			 int prot, size_t *size);
	size_t (*unmap)(struct io_pgtable_ops *ops, unsigned long iova,
			size_t size);
	phys_addr_t (*iova_to_phys)(struct io_pgtable_ops *ops,
				    unsigned long iova);
	bool (*is_iova_coherent)(struct io_pgtable_ops *ops,
				unsigned long iova);

};

/**
 * alloc_io_pgtable_ops() - Allocate a page table allocator for use by an IOMMU.
 *
 * @fmt:    The page table format.
 * @cfg:    The page table configuration. This will be modified to represent
 *          the configuration actually provided by the allocator (e.g. the
 *          pgsize_bitmap may be restricted).
 * @cookie: An opaque token provided by the IOMMU driver and passed back to
 *          the callback routines in cfg->tlb.
 */
struct io_pgtable_ops *alloc_io_pgtable_ops(enum io_pgtable_fmt fmt,
					    struct io_pgtable_cfg *cfg,
					    void *cookie);

/**
 * free_io_pgtable_ops() - Free an io_pgtable_ops structure. The caller
 *                         *must* ensure that the page table is no longer
 *                         live, but the TLB can be dirty.
 *
 * @ops: The ops returned from alloc_io_pgtable_ops.
 */
void free_io_pgtable_ops(struct io_pgtable_ops *ops);


/*
 * Internal structures for page table allocator implementations.
 */

/**
 * struct io_pgtable - Internal structure describing a set of page tables.
 *
 * @fmt:    The page table format.
 * @cookie: An opaque token provided by the IOMMU driver and passed back to
 *          any callback routines.
 * @cfg:    A copy of the page table configuration.
 * @ops:    The page table operations in use for this set of page tables.
 */
struct io_pgtable {
	enum io_pgtable_fmt	fmt;
	void			*cookie;
	struct io_pgtable_cfg	cfg;
	struct io_pgtable_ops	ops;
};

/**
 * struct io_pgtable_init_fns - Alloc/free a set of page tables for a
 *                              particular format.
 *
 * @alloc: Allocate a set of page tables described by cfg.
 * @free:  Free the page tables associated with iop.
 */
struct io_pgtable_init_fns {
	struct io_pgtable *(*alloc)(struct io_pgtable_cfg *cfg, void *cookie);
	void (*free)(struct io_pgtable *iop);
};

/**
 * io_pgtable_alloc_pages_exact - allocate an exact number physically-contiguous pages.
 * @size: the number of bytes to allocate
 * @gfp_mask: GFP flags for the allocation
 *
 * Like alloc_pages_exact(), but with some additional accounting for debug
 * purposes.
 */
void *io_pgtable_alloc_pages_exact(struct io_pgtable_cfg *cfg, void *cookie,
				   size_t size, gfp_t gfp_mask);

/**
 * io_pgtable_free_pages_exact - release memory allocated via io_pgtable_alloc_pages_exact()
 * @virt: the value returned by alloc_pages_exact.
 * @size: size of allocation, same value as passed to alloc_pages_exact().
 *
 * Like free_pages_exact(), but with some additional accounting for debug
 * purposes.
 */
void io_pgtable_free_pages_exact(struct io_pgtable_cfg *cfg, void *cookie,
				 void *virt, size_t size);

extern struct io_pgtable_init_fns io_pgtable_arm_32_lpae_s1_init_fns;
extern struct io_pgtable_init_fns io_pgtable_arm_32_lpae_s2_init_fns;
extern struct io_pgtable_init_fns io_pgtable_arm_64_lpae_s1_init_fns;
extern struct io_pgtable_init_fns io_pgtable_arm_64_lpae_s2_init_fns;

#endif /* __IO_PGTABLE_H */
