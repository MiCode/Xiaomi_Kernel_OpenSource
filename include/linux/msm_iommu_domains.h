/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_MSM_IOMMU_DOMAINS_H
#define _LINUX_MSM_IOMMU_DOMAINS_H

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/msm_ion.h>

#define MSM_IOMMU_DOMAIN_SECURE	0x1

struct mem_pool {
	struct mutex pool_mutex;
	unsigned long *bitmap;
	unsigned long nr_pages;
	phys_addr_t paddr;
	unsigned long size;
	unsigned long free;
	unsigned int id;
};

enum {
	VIDEO_DOMAIN,
	CAMERA_DOMAIN,
	DISPLAY_READ_DOMAIN,
	DISPLAY_WRITE_DOMAIN,
	ROTATOR_SRC_DOMAIN,
	ROTATOR_DST_DOMAIN,
	MAX_DOMAINS
};

enum {
	VIDEO_FIRMWARE_POOL,
	VIDEO_MAIN_POOL,
	GEN_POOL,
};

struct msm_iommu_domain_name {
	char *name;
	int domain;
};

struct msm_iommu_domain {
	/* iommu domain to map in */
	struct iommu_domain *domain;
	/* total number of allocations from this domain */
	atomic_t allocation_cnt;
	/* number of iova pools */
	int npools;
	/*
	 * array of gen_pools for allocating iovas.
	 * behavior is undefined if these overlap
	 */
	struct mem_pool *iova_pools;
};

struct iommu_domains_pdata {
	struct msm_iommu_domain *domains;
	int ndomains;
	struct msm_iommu_domain_name *domain_names;
	int nnames;
	unsigned int domain_alloc_flags;
};


struct msm_iova_partition {
	unsigned long start;
	unsigned long size;
};

struct msm_iova_layout {
	struct msm_iova_partition *partitions;
	int npartitions;
	const char *client_name;
	unsigned int domain_flags;
	unsigned int is_secure;
};

#if defined(CONFIG_MSM_IOMMU)
/**
 * ion_map_iommu - map the given handle into an iommu
 *
 * @client - client who allocated the handle
 * @handle - handle to map
 * @domain_num - domain number to map to
 * @partition_num - partition number to allocate iova from
 * @align - alignment for the iova
 * @iova_length - length of iova to map. If the iova length is
 *		greater than the handle length, the remaining
 *		address space will be mapped to a dummy buffer.
 * @iova - pointer to store the iova address
 * @buffer_size - pointer to store the size of the buffer
 * @flags - flags for options to map
 * @iommu_flags - flags specific to the iommu.
 *
 * Maps the handle into the iova space specified via domain number. Iova
 * will be allocated from the partition specified via partition_num.
 * Returns 0 on success, negative value on error.
 */
int ion_map_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num, unsigned long align,
			unsigned long iova_length, ion_phys_addr_t *iova,
			unsigned long *buffer_size,
			unsigned long flags, unsigned long iommu_flags);

/**
 * ion_unmap_iommu - unmap the handle from an iommu
 *
 * @client - client who allocated the handle
 * @handle - handle to unmap
 * @domain_num - domain to unmap from
 * @partition_num - partition to unmap from
 *
 * Decrement the reference count on the iommu mapping. If the count is
 * 0, the mapping will be removed from the iommu.
 */
void ion_unmap_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num);

extern void msm_iommu_set_client_name(struct iommu_domain *domain,
				      char const *name);
extern struct iommu_domain *msm_get_iommu_domain(int domain_num);
extern int msm_find_domain_no(const struct iommu_domain *domain);
extern struct iommu_domain *msm_iommu_domain_find(const char *name);
extern int msm_iommu_domain_no_find(const char *name);


extern int msm_allocate_iova_address(unsigned int iommu_domain,
					unsigned int partition_no,
					unsigned long size,
					unsigned long align,
					unsigned long *iova);

extern void msm_free_iova_address(unsigned long iova,
			unsigned int iommu_domain,
			unsigned int partition_no,
			unsigned long size);

extern int msm_use_iommu(void);

extern int msm_iommu_map_extra(struct iommu_domain *domain,
						unsigned long start_iova,
						phys_addr_t phys_addr,
						unsigned long size,
						unsigned long page_size,
						int cached);

extern void msm_iommu_unmap_extra(struct iommu_domain *domain,
						unsigned long start_iova,
						unsigned long size,
						unsigned long page_size);

extern int msm_iommu_map_contig_buffer(phys_addr_t phys,
				unsigned int domain_no,
				unsigned int partition_no,
				unsigned long size,
				unsigned long align,
				unsigned long cached,
				dma_addr_t *iova_val);


extern void msm_iommu_unmap_contig_buffer(dma_addr_t iova,
					unsigned int domain_no,
					unsigned int partition_no,
					unsigned long size);

extern int msm_register_domain(struct msm_iova_layout *layout);
extern int msm_unregister_domain(struct iommu_domain *domain);

int msm_map_dma_buf(struct dma_buf *dma_buf, struct sg_table *table,
			int domain_num, int partition_num, unsigned long align,
			unsigned long iova_length, ion_phys_addr_t *iova,
			unsigned long *buffer_size,
			unsigned long flags, unsigned long iommu_flags);

void msm_unmap_dma_buf(struct sg_table *table, int domain_num,
			int partition_num);
#else
static inline int ion_map_iommu(struct ion_client *client,
			struct ion_handle *handle, int domain_num,
			int partition_num, unsigned long align,
			unsigned long iova_length, ion_phys_addr_t *iova,
			unsigned long *buffer_size,
			unsigned long flags,
			unsigned long iommu_flags)
{
	return -ENODEV;
}

static inline void ion_unmap_iommu(struct ion_client *client,
			struct ion_handle *handle, int domain_num,
			int partition_num)
{
}

static inline void msm_iommu_set_client_name(struct iommu_domain *domain,
					     char const *name)
{
}

static inline struct iommu_domain
	*msm_get_iommu_domain(int subsys_id) { return NULL; }


static inline int msm_find_domain_no(const struct iommu_domain *domain)
{
	return -EINVAL;
}

static inline int msm_allocate_iova_address(unsigned int iommu_domain,
					unsigned int partition_no,
					unsigned long size,
					unsigned long align,
					unsigned long *iova) { return -ENOMEM; }

static inline void msm_free_iova_address(unsigned long iova,
			unsigned int iommu_domain,
			unsigned int partition_no,
			unsigned long size) { }

static inline int msm_use_iommu(void)
{
	return 0;
}

static inline int msm_iommu_map_extra(struct iommu_domain *domain,
						unsigned long start_iova,
						phys_addr_t phys_addr,
						unsigned long size,
						unsigned long page_size,
						int cached)
{
	return -ENODEV;
}

static inline void msm_iommu_unmap_extra(struct iommu_domain *domain,
						unsigned long start_iova,
						unsigned long size,
						unsigned long page_size)
{
}

static inline int msm_iommu_map_contig_buffer(phys_addr_t phys,
				unsigned int domain_no,
				unsigned int partition_no,
				unsigned long size,
				unsigned long align,
				unsigned long cached,
				dma_addr_t *iova_val)
{
	*iova_val = phys;
	return 0;
}

static inline void msm_iommu_unmap_contig_buffer(dma_addr_t iova,
					unsigned int domain_no,
					unsigned int partition_no,
					unsigned long size)
{
}

static inline int msm_register_domain(struct msm_iova_layout *layout)
{
	return -ENODEV;
}

static inline int msm_unregister_domain(struct iommu_domain *domain)
{
	return -ENODEV;
}

static inline struct iommu_domain *msm_iommu_domain_find(const char *name)
{
	return NULL;
}

static inline int msm_iommu_domain_no_find(const char *name)
{
	return -ENODEV;
}

static inline int msm_map_dma_buf(struct dma_buf *dma_buf,
			struct sg_table *table,
			int domain_num, int partition_num, unsigned long align,
			unsigned long iova_length, ion_phys_addr_t *iova,
			unsigned long *buffer_size,
			unsigned long flags, unsigned long iommu_flags)
{
	return -ENODEV;
}

static inline void msm_unmap_dma_buf(struct sg_table *table, int domain_num,
			int partition_num)
{
}

#endif

#endif
