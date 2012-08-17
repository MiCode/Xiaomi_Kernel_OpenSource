/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

struct smem_client {
	int mem_type;
	void *clnt;
};

static int get_device_address(struct ion_client *clnt,
		struct ion_handle *hndl, int domain_num, int partition_num,
		unsigned long align, unsigned long *iova,
		unsigned long *buffer_size,	unsigned long flags)
{
	int rc;

	if (!iova || !buffer_size || !hndl || !clnt) {
		pr_err("Invalid params: %p, %p, %p, %p\n",
				clnt, hndl, iova, buffer_size);
		return -EINVAL;
	}
	if (align < 4096)
		align = 4096;
	flags |= UNCACHED;
	pr_debug("\n In %s  domain: %d, Partition: %d\n",
		__func__, domain_num, partition_num);
	rc = ion_map_iommu(clnt, hndl, domain_num, partition_num, align,
			0, iova, buffer_size, flags, 0);
	if (rc)
		pr_err("ion_map_iommu failed(%d).domain: %d,partition: %d\n",
				rc, domain_num, partition_num);

	return rc;
}

static void put_device_address(struct ion_client *clnt,
		struct ion_handle *hndl, int domain_num, int partition_num)
{
	ion_unmap_iommu(clnt, hndl, domain_num, partition_num);
}

static int ion_user_to_kernel(struct smem_client *client,
			int fd, u32 offset, int domain, int partition,
			struct msm_smem *mem)
{
	struct ion_handle *hndl;
	unsigned long ionflag;
	unsigned long iova = 0;
	unsigned long buffer_size = 0;
	int rc = 0;
	hndl = ion_import_dma_buf(client->clnt, fd);
	if (IS_ERR_OR_NULL(hndl)) {
		pr_err("Failed to get handle: %p, %d, %d, %p\n",
				client, fd, offset, hndl);
		rc = -ENOMEM;
		goto fail_import_fd;
	}
	rc = ion_handle_get_flags(client->clnt, hndl, &ionflag);
	if (rc) {
		pr_err("Failed to get ion flags: %d", rc);
		goto fail_map;
	}
	mem->kvaddr = ion_map_kernel(client->clnt, hndl, ionflag);
	if (!mem->kvaddr) {
		pr_err("Failed to map shared mem in kernel\n");
		rc = -EIO;
		goto fail_map;
	}
	mem->domain = domain;
	mem->partition_num = partition;
	rc = get_device_address(client->clnt, hndl, mem->domain,
		mem->partition_num, 4096, &iova, &buffer_size, UNCACHED);
	if (rc) {
		pr_err("Failed to get device address: %d\n", rc);
		goto fail_device_address;
	}

	mem->kvaddr += offset;
	mem->mem_type = client->mem_type;
	mem->smem_priv = hndl;
	mem->device_addr = iova + offset;
	mem->size = buffer_size;
	pr_debug("Buffer device address: 0x%lx, size: %d\n",
		mem->device_addr, mem->size);
	return rc;
fail_device_address:
	ion_unmap_kernel(client->clnt, hndl);
fail_map:
	ion_free(client->clnt, hndl);
fail_import_fd:
	return rc;
}

static int alloc_ion_mem(struct smem_client *client, size_t size,
		u32 align, u32 flags, int domain, int partition,
		struct msm_smem *mem)
{
	struct ion_handle *hndl;
	unsigned long iova = 0;
	unsigned long buffer_size = 0;
	int rc = 0;
	flags = flags | ION_HEAP(ION_CP_MM_HEAP_ID);
	if (align < 4096)
		align = 4096;
	size = (size + 4095) & (~4095);
	pr_debug("\n in %s domain: %d, Partition: %d\n",
		__func__, domain, partition);
	hndl = ion_alloc(client->clnt, size, align, flags);
	if (IS_ERR_OR_NULL(hndl)) {
		pr_err("Failed to allocate shared memory = %p, %d, %d, 0x%x\n",
				client, size, align, flags);
		rc = -ENOMEM;
		goto fail_shared_mem_alloc;
	}
	mem->mem_type = client->mem_type;
	mem->smem_priv = hndl;
	mem->domain = domain;
	mem->partition_num = partition;
	mem->kvaddr = ion_map_kernel(client->clnt, hndl, 0);
	if (!mem->kvaddr) {
		pr_err("Failed to map shared mem in kernel\n");
		rc = -EIO;
		goto fail_map;
	}
	rc = get_device_address(client->clnt, hndl, mem->domain,
		mem->partition_num, align, &iova, &buffer_size, UNCACHED);
	if (rc) {
		pr_err("Failed to get device address: %d\n", rc);
		goto fail_device_address;
	}
	mem->device_addr = iova;
	pr_debug("device_address = 0x%lx, kvaddr = 0x%p\n",
		mem->device_addr, mem->kvaddr);
	mem->size = size;
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
	put_device_address(client->clnt,
		mem->smem_priv, mem->domain, mem->partition_num);
	ion_unmap_kernel(client->clnt, mem->smem_priv);
	ion_free(client->clnt, mem->smem_priv);
}

static void *ion_new_client(void)
{
	struct ion_client *client = NULL;
	client = msm_ion_client_create(-1, "video_client");
	if (!client)
		pr_err("Failed to create smem client\n");
	return client;
};

static void ion_delete_client(struct smem_client *client)
{
	ion_client_destroy(client->clnt);
}

struct msm_smem *msm_smem_user_to_kernel(void *clt, int fd, u32 offset,
	int domain, int partition)
{
	struct smem_client *client = clt;
	int rc = 0;
	struct msm_smem *mem;
	if (fd < 0) {
		pr_err("Invalid fd: %d\n", fd);
		return NULL;
	}
	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		pr_err("Failed to allocte shared mem\n");
		return NULL;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		rc = ion_user_to_kernel(clt, fd, offset,
			domain, partition, mem);
		break;
	default:
		pr_err("Mem type not supported\n");
		rc = -EINVAL;
		break;
	}
	if (rc) {
		pr_err("Failed to allocate shared memory\n");
		kfree(mem);
		mem = NULL;
	}
	return mem;
}

void *msm_smem_new_client(enum smem_type mtype)
{
	struct smem_client *client = NULL;
	void *clnt = NULL;
	switch (mtype) {
	case SMEM_ION:
		clnt = ion_new_client();
		break;
	default:
		pr_err("Mem type not supported\n");
		break;
	}
	if (clnt) {
		client = kzalloc(sizeof(*client), GFP_KERNEL);
		if (client) {
			client->mem_type = mtype;
			client->clnt = clnt;
		}
	} else {
		pr_err("Failed to create new client: mtype = %d\n", mtype);
	}
	return client;
};

struct msm_smem *msm_smem_alloc(void *clt, size_t size, u32 align, u32 flags,
		int domain, int partition)
{
	struct smem_client *client;
	int rc = 0;
	struct msm_smem *mem;
	client = clt;
	if (!client) {
		pr_err("Invalid  client passed\n");
		return NULL;
	}
	if (!size) {
		pr_err("No need to allocate memory of size: %d\n", size);
		return NULL;
	}
	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		pr_err("Failed to allocate shared mem\n");
		return NULL;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		rc = alloc_ion_mem(client, size, align, flags,
			domain, partition, mem);
		break;
	default:
		pr_err("Mem type not supported\n");
		rc = -EINVAL;
		break;
	}
	if (rc) {
		pr_err("Failed to allocate shared memory\n");
		kfree(mem);
		mem = NULL;
	}
	return mem;
}

void msm_smem_free(void *clt, struct msm_smem *mem)
{
	struct smem_client *client = clt;
	if (!client || !mem) {
		pr_err("Invalid  client/handle passed\n");
		return;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		free_ion_mem(client, mem);
		break;
	default:
		pr_err("Mem type not supported\n");
		break;
	}
	kfree(mem);
};

void msm_smem_delete_client(void *clt)
{
	struct smem_client *client = clt;
	if (!client) {
		pr_err("Invalid  client passed\n");
		return;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		ion_delete_client(client);
		break;
	default:
		pr_err("Mem type not supported\n");
		break;
	}
	kfree(client);
}
