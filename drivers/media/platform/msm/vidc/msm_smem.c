/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include <asm/dma-iommu.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/iommu.h>
#include <linux/msm_dma_iommu_mapping.h>
#include <linux/msm_ion.h>
#include <linux/slab.h>
#include <linux/types.h>
#include "msm_vidc.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_resources.h"

struct smem_client {
	int mem_type;
	void *clnt;
	struct msm_vidc_platform_resources *res;
	enum session_type session_type;
};

#define ion_phys_addr_t dma_addr_t

struct ion_handle {
	struct kref ref;
	unsigned int user_ref_count;
	struct ion_client *client;
	struct ion_buffer *buffer;
	struct rb_node node;
	unsigned int kmap_cnt;
	int id;
};

static inline int msm_ion_get_device_address(struct smem_client *smem_client,
		struct ion_handle *hndl, unsigned long align,
		ion_phys_addr_t *iova, unsigned long *buffer_size,
		unsigned long flags, enum hal_buffer buffer_type,
		struct dma_mapping_info *mapping_info)
{
	return -ENODEV;
}

static inline int msm_ion_put_device_address(struct smem_client *smem_client,
	struct ion_handle *hndl, u32 flags,
	struct dma_mapping_info *mapping_info,
	enum hal_buffer buffer_type)
{
	return -ENODEV;
}

static inline void *msm_ion_get_dma_buf(int fd)
{
	return ERR_PTR(-ENODEV);
}

void *msm_smem_get_dma_buf(int fd)
{
	return ERR_PTR(-ENODEV);
}

static inline void msm_ion_put_dma_buf(struct dma_buf *dma_buf)
{
}

void msm_smem_put_dma_buf(void *dma_buf)
{
}

static inline struct ion_handle *msm_ion_get_handle(void *ion_client,
		struct dma_buf *dma_buf)
{
	return ERR_PTR(-ENODEV);
}

void *msm_smem_get_handle(struct smem_client *client, void *dma_buf)
{
	return ERR_PTR(-ENODEV);
}

static inline void msm_ion_put_handle(struct ion_client *ion_client,
		struct ion_handle *ion_handle)
{
}

void msm_smem_put_handle(struct smem_client *client, void *handle)
{
}

static inline  int msm_ion_map_dma_buf(struct msm_vidc_inst *inst,
		struct msm_smem *smem)
{
	return -ENODEV;
}

int msm_smem_map_dma_buf(struct msm_vidc_inst *inst, struct msm_smem *smem)
{
	return -ENODEV;
}

static inline int msm_ion_unmap_dma_buf(struct msm_vidc_inst *inst,
		struct msm_smem *smem)
{
	return -ENODEV;
}

int msm_smem_unmap_dma_buf(struct msm_vidc_inst *inst, struct msm_smem *smem)
{
	return -ENODEV;
}

static inline int get_secure_flag_for_buffer_type(
		struct smem_client *client, enum hal_buffer buffer_type)
{
	return -ENODEV;
}

static inline int alloc_ion_mem(struct smem_client *client, size_t size,
			u32 align, u32 flags, enum hal_buffer buffer_type,
			struct msm_smem *mem, int map_kernel)
{
	return -ENODEV;
}

static inline int free_ion_mem(struct smem_client *client, struct msm_smem *mem)
{
	return -ENODEV;
}

static inline void *ion_new_client(void)
{
	return ERR_PTR(-ENODEV);
};

static inline void ion_delete_client(struct smem_client *client)
{
}

static inline int msm_ion_cache_operations(void *ion_client, void *ion_handle,
		unsigned long offset, unsigned long size,
		enum smem_cache_ops cache_op)
{
	return -ENODEV;
}

int msm_smem_cache_operations(struct smem_client *client,
		void *handle, unsigned long offset, unsigned long size,
		enum smem_cache_ops cache_op)
{
	return -ENODEV;
}

void *msm_smem_new_client(enum smem_type mtype,
		void *platform_resources, enum session_type stype)
{
	return ERR_PTR(-ENODEV);
}

int msm_smem_alloc(struct smem_client *client, size_t size,
		u32 align, u32 flags, enum hal_buffer buffer_type,
		int map_kernel, struct msm_smem *smem)
{
	return -ENODEV;
}

int msm_smem_free(void *clt, struct msm_smem *smem)
{
	return -ENODEV;
};

void msm_smem_delete_client(void *clt)
{
}

struct context_bank_info *msm_smem_get_context_bank(void *clt,
				bool is_secure, enum hal_buffer buffer_type)
{
	return ERR_PTR(-ENODEV);
}
