/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <mach/iommu_domains.h>
#include "msm_smem.h"
#include "msm_vidc_debug.h"

struct smem_client {
	int mem_type;
	void *clnt;
	struct msm_vidc_platform_resources *res;
};

static u32 get_tz_usage(struct smem_client *client, enum hal_buffer buffer_type)
{
	int i;
	struct buffer_usage_set *buffer_usage_set;
	struct buffer_usage_table *buffer_usage_tbl;

	buffer_usage_set = &client->res->buffer_usage_set;
	if (!buffer_usage_set) {
		dprintk(VIDC_DBG, "no buffer usage set present!\n");
		return 0;
	}

	for (i = 0; i < buffer_usage_set->count; i++) {
		buffer_usage_tbl = &buffer_usage_set->buffer_usage_tbl[i];
		if (buffer_usage_tbl->buffer_type & buffer_type)
			return buffer_usage_tbl->tz_usage;
	}
	dprintk(VIDC_DBG, "No tz usage found for buffer type: %x\n",
			buffer_type);
	return 0;
}

static int get_device_address(struct smem_client *smem_client,
		struct ion_handle *hndl, unsigned long align,
		unsigned long *iova, unsigned long *buffer_size,
		u32 flags, enum hal_buffer buffer_type)
{
	int rc = 0;
	int domain, partition;
	struct ion_client *clnt = NULL;

	if (!iova || !buffer_size || !hndl || !smem_client) {
		dprintk(VIDC_ERR, "Invalid params: %p, %p, %p, %p\n",
				smem_client, hndl, iova, buffer_size);
		return -EINVAL;
	}

	clnt = smem_client->clnt;
	if (!clnt) {
		dprintk(VIDC_ERR, "Invalid client");
		return -EINVAL;
	}

	rc = msm_smem_get_domain_partition(smem_client, flags, buffer_type,
			&domain, &partition);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get domain and partition: %d", rc);
		goto mem_domain_get_failed;
	}

	if (flags & SMEM_SECURE) {
		rc = msm_ion_secure_buffer(clnt, hndl,
			get_tz_usage(smem_client, buffer_type), 0);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to secure memory\n");
			goto mem_domain_get_failed;
		}
	}
	if (is_iommu_present(smem_client->res)) {
		dprintk(VIDC_DBG,
				"Calling ion_map_iommu - domain: %d, partition: %d",
				domain, partition);
		rc = ion_map_iommu(clnt, hndl, domain, partition, align,
				0, iova, buffer_size, 0, 0);
	} else {
		dprintk(VIDC_DBG, "Using physical memory address");
		rc = ion_phys(clnt, hndl, iova, (size_t *)buffer_size);
	}
	if (rc) {
		dprintk(VIDC_ERR, "ion memory map failed - %d", rc);
		goto mem_map_failed;
	}

	return 0;
mem_map_failed:
	if (flags & SMEM_SECURE)
		msm_ion_unsecure_buffer(clnt, hndl);
mem_domain_get_failed:
	return rc;
}

static void put_device_address(struct smem_client *smem_client,
	struct ion_handle *hndl, int domain_num, int partition_num, u32 flags)
{
	struct ion_client *clnt = NULL;

	if (!hndl || !smem_client) {
		dprintk(VIDC_WARN, "Invalid params: %p, %p\n",
				smem_client, hndl);
		return;
	}

	clnt = smem_client->clnt;
	if (!clnt) {
		dprintk(VIDC_WARN, "Invalid client");
		return;
	}
	if (is_iommu_present(smem_client->res)) {
		dprintk(VIDC_DBG,
				"Calling ion_unmap_iommu - domain: %d, parition: %d",
				domain_num, partition_num);
		ion_unmap_iommu(clnt, hndl, domain_num, partition_num);
	}
	if (flags & SMEM_SECURE) {
		if (msm_ion_unsecure_buffer(clnt, hndl))
			dprintk(VIDC_ERR, "Failed to unsecure memory\n");
	}
}

static int ion_user_to_kernel(struct smem_client *client, int fd, u32 offset,
		struct msm_smem *mem, enum hal_buffer buffer_type)
{
	struct ion_handle *hndl;
	unsigned long iova = 0;
	unsigned long buffer_size = 0;
	unsigned long ionflags = 0;
	int rc = 0;
	int align = SZ_4K;

	hndl = ion_import_dma_buf(client->clnt, fd);
	if (IS_ERR_OR_NULL(hndl)) {
		dprintk(VIDC_ERR, "Failed to get handle: %p, %d, %d, %p\n",
				client, fd, offset, hndl);
		rc = -ENOMEM;
		goto fail_import_fd;
	}
	mem->kvaddr = NULL;
	rc = ion_handle_get_flags(client->clnt, hndl, &ionflags);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get ion flags: %d\n", rc);
		goto fail_device_address;
	}

	mem->flags = ionflags;
	mem->buffer_type = buffer_type;
	if (mem->flags & SMEM_SECURE)
		align = ALIGN(align, SZ_1M);

	rc = get_device_address(client, hndl, align, &iova, &buffer_size,
					mem->flags, buffer_type);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get device address: %d\n", rc);
		goto fail_device_address;
	}

	mem->mem_type = client->mem_type;
	mem->smem_priv = hndl;
	mem->device_addr = iova;
	mem->size = buffer_size;
	dprintk(VIDC_DBG,
		"%s: ion_handle = 0x%p, fd = %d, device_addr = 0x%x, size = %d, kvaddr = 0x%p, buffer_type = %d\n",
		__func__, mem->smem_priv, fd, (u32)mem->device_addr,
		mem->size, mem->kvaddr, mem->buffer_type);
	return rc;
fail_device_address:
	ion_free(client->clnt, hndl);
fail_import_fd:
	return rc;
}

static int alloc_ion_mem(struct smem_client *client, size_t size, u32 align,
	u32 flags, enum hal_buffer buffer_type, struct msm_smem *mem,
	int map_kernel)
{
	struct ion_handle *hndl;
	unsigned long iova = 0;
	unsigned long buffer_size = 0;
	unsigned long heap_mask = 0;
	int rc = 0;

	align = ALIGN(align, SZ_4K);
	size = ALIGN(size, SZ_4K);

	if (flags & SMEM_SECURE) {
		size = ALIGN(size, SZ_1M);
		align = ALIGN(align, SZ_1M);
	}

	if (is_iommu_present(client->res)) {
		heap_mask = ION_HEAP(ION_IOMMU_HEAP_ID);
	} else {
		dprintk(VIDC_DBG,
			"allocate shared memory from adsp heap size %d align %d\n",
			size, align);
		heap_mask = ION_HEAP(ION_ADSP_HEAP_ID);
	}

	if (flags & SMEM_SECURE)
		heap_mask = ION_HEAP(ION_CP_MM_HEAP_ID);

	hndl = ion_alloc(client->clnt, size, align, heap_mask, flags);
	if (IS_ERR_OR_NULL(hndl)) {
		dprintk(VIDC_ERR,
		"Failed to allocate shared memory = %p, %d, %d, 0x%x\n",
		client, size, align, flags);
		rc = -ENOMEM;
		goto fail_shared_mem_alloc;
	}
	mem->mem_type = client->mem_type;
	mem->smem_priv = hndl;
	mem->flags = flags;
	mem->buffer_type = buffer_type;
	if (map_kernel) {
		mem->kvaddr = ion_map_kernel(client->clnt, hndl);
		if (!mem->kvaddr) {
			dprintk(VIDC_ERR,
				"Failed to map shared mem in kernel\n");
			rc = -EIO;
			goto fail_map;
		}
	} else
		mem->kvaddr = NULL;

	rc = get_device_address(client, hndl, align, &iova, &buffer_size,
				flags, buffer_type);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get device address: %d\n",
			rc);
		goto fail_device_address;
	}
	mem->device_addr = iova;
	mem->size = size;
	dprintk(VIDC_DBG,
		"%s: ion_handle = 0x%p, device_addr = 0x%x, size = %d, kvaddr = 0x%p, buffer_type = %d\n",
		__func__, mem->smem_priv, (u32)mem->device_addr,
		mem->size, mem->kvaddr, mem->buffer_type);
	return rc;
fail_device_address:
	ion_unmap_kernel(client->clnt, hndl);
fail_map:
	ion_free(client->clnt, hndl);
fail_shared_mem_alloc:
	return rc;
}

static void free_ion_mem(struct smem_client *client, struct msm_smem *mem)
{
	int domain, partition, rc;

	dprintk(VIDC_DBG,
		"%s: ion_handle = 0x%p, device_addr = 0x%x, size = %d, kvaddr = 0x%p, buffer_type = %d\n",
		__func__, mem->smem_priv, (u32)mem->device_addr,
		mem->size, mem->kvaddr, mem->buffer_type);
	rc = msm_smem_get_domain_partition((void *)client, mem->flags,
			mem->buffer_type, &domain, &partition);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get domain, partition: %d", rc);
		return;
	}

	if (mem->device_addr)
		put_device_address(client,
			mem->smem_priv, domain, partition, mem->flags);
	if (mem->kvaddr)
		ion_unmap_kernel(client->clnt, mem->smem_priv);
	if (mem->smem_priv)
		ion_free(client->clnt, mem->smem_priv);
}

static void *ion_new_client(void)
{
	struct ion_client *client = NULL;
	client = msm_ion_client_create(-1, "video_client");
	if (!client)
		dprintk(VIDC_ERR, "Failed to create smem client\n");
	return client;
};

static void ion_delete_client(struct smem_client *client)
{
	ion_client_destroy(client->clnt);
}

struct msm_smem *msm_smem_user_to_kernel(void *clt, int fd, u32 offset,
		enum hal_buffer buffer_type)
{
	struct smem_client *client = clt;
	int rc = 0;
	struct msm_smem *mem;
	if (fd < 0) {
		dprintk(VIDC_ERR, "Invalid fd: %d\n", fd);
		return NULL;
	}
	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		dprintk(VIDC_ERR, "Failed to allocte shared mem\n");
		return NULL;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		rc = ion_user_to_kernel(clt, fd, offset, mem, buffer_type);
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		rc = -EINVAL;
		break;
	}
	if (rc) {
		dprintk(VIDC_ERR, "Failed to allocate shared memory\n");
		kfree(mem);
		mem = NULL;
	}
	return mem;
}

static int ion_cache_operations(struct smem_client *client,
	struct msm_smem *mem, enum smem_cache_ops cache_op)
{
	unsigned long ionflag = 0;
	int rc = 0;
	int msm_cache_ops = 0;
	if (!mem || !client) {
		dprintk(VIDC_ERR, "Invalid params: %p, %p\n",
			mem, client);
		return -EINVAL;
	}
	rc = ion_handle_get_flags(client->clnt,	mem->smem_priv,
		&ionflag);
	if (rc) {
		dprintk(VIDC_ERR,
			"ion_handle_get_flags failed: %d\n", rc);
		goto cache_op_failed;
	}
	if (ION_IS_CACHED(ionflag)) {
		switch (cache_op) {
		case SMEM_CACHE_CLEAN:
			msm_cache_ops = ION_IOC_CLEAN_CACHES;
			break;
		case SMEM_CACHE_INVALIDATE:
			msm_cache_ops = ION_IOC_INV_CACHES;
			break;
		case SMEM_CACHE_CLEAN_INVALIDATE:
			msm_cache_ops = ION_IOC_CLEAN_INV_CACHES;
			break;
		default:
			dprintk(VIDC_ERR, "cache operation not supported\n");
			rc = -EINVAL;
			goto cache_op_failed;
		}
		rc = msm_ion_do_cache_op(client->clnt,
				(struct ion_handle *)mem->smem_priv,
				0, (unsigned long)mem->size,
				msm_cache_ops);
		if (rc) {
			dprintk(VIDC_ERR,
					"cache operation failed %d\n", rc);
			goto cache_op_failed;
		}
	}
cache_op_failed:
	return rc;
}

int msm_smem_cache_operations(void *clt, struct msm_smem *mem,
		enum smem_cache_ops cache_op)
{
	struct smem_client *client = clt;
	int rc = 0;
	if (!client) {
		dprintk(VIDC_ERR, "Invalid params: %p\n",
			client);
		return -EINVAL;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		rc = ion_cache_operations(client, mem, cache_op);
		if (rc)
			dprintk(VIDC_ERR,
			"Failed cache operations: %d\n", rc);
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		break;
	}
	return rc;
}

void *msm_smem_new_client(enum smem_type mtype,
		struct msm_vidc_platform_resources *res)
{
	struct smem_client *client = NULL;
	void *clnt = NULL;
	switch (mtype) {
	case SMEM_ION:
		clnt = ion_new_client();
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		break;
	}
	if (clnt) {
		client = kzalloc(sizeof(*client), GFP_KERNEL);
		if (client) {
			client->mem_type = mtype;
			client->clnt = clnt;
			client->res = res;
		}
	} else {
		dprintk(VIDC_ERR, "Failed to create new client: mtype = %d\n",
			mtype);
	}
	return client;
};

struct msm_smem *msm_smem_alloc(void *clt, size_t size, u32 align, u32 flags,
		enum hal_buffer buffer_type, int map_kernel)
{
	struct smem_client *client;
	int rc = 0;
	struct msm_smem *mem;
	client = clt;
	if (!client) {
		dprintk(VIDC_ERR, "Invalid  client passed\n");
		return NULL;
	}
	if (!size) {
		dprintk(VIDC_ERR, "No need to allocate memory of size: %d\n",
			size);
		return NULL;
	}
	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		dprintk(VIDC_ERR, "Failed to allocate shared mem\n");
		return NULL;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		rc = alloc_ion_mem(client, size, align, flags, buffer_type,
					mem, map_kernel);
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		rc = -EINVAL;
		break;
	}
	if (rc) {
		dprintk(VIDC_ERR, "Failed to allocate shared memory\n");
		kfree(mem);
		mem = NULL;
	}
	return mem;
}

void msm_smem_free(void *clt, struct msm_smem *mem)
{
	struct smem_client *client = clt;
	if (!client || !mem) {
		dprintk(VIDC_ERR, "Invalid  client/handle passed\n");
		return;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		free_ion_mem(client, mem);
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		break;
	}
	kfree(mem);
};

void msm_smem_delete_client(void *clt)
{
	struct smem_client *client = clt;
	if (!client) {
		dprintk(VIDC_ERR, "Invalid  client passed\n");
		return;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		ion_delete_client(client);
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		break;
	}
	kfree(client);
}

int msm_smem_get_domain_partition(void *clt, u32 flags, enum hal_buffer
		buffer_type, int *domain_num, int *partition_num)
{
	struct smem_client *client = clt;
	struct iommu_set *iommu_group_set = &client->res->iommu_group_set;
	int i;
	int j;
	bool is_secure = (flags & SMEM_SECURE);
	struct iommu_info *iommu_map;
	if (!domain_num || !partition_num) {
		dprintk(VIDC_DBG, "passed null to get domain partition!");
		return -EINVAL;
	}

	*domain_num = -1;
	*partition_num = -1;
	if (!iommu_group_set) {
		dprintk(VIDC_DBG, "no iommu group set present!\n");
		return -ENOENT;
	}

	for (i = 0; i < iommu_group_set->count; i++) {
		iommu_map = &iommu_group_set->iommu_maps[i];
		if (iommu_map->is_secure == is_secure) {
			for (j = 0; j < iommu_map->npartitions; j++) {
				if (iommu_map->buffer_type[j] & buffer_type) {
					*domain_num = iommu_map->domain;
					*partition_num = j;
					break;
				}
			}
		}
	}
	dprintk(VIDC_DBG, "domain: %d, partition: %d found!\n",
			*domain_num, *partition_num);
	return 0;
}
