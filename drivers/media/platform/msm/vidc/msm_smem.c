/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include "msm_smem.h"

struct smem_client {
	int mem_type;
	void *clnt;
};

static int ion_user_to_kernel(struct smem_client *client,
			int fd, u32 offset, struct msm_smem *mem)
{
	struct ion_handle *hndl;
	unsigned long ionflag;
	size_t len;
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
	rc = ion_phys(client->clnt, hndl, &mem->paddr, &len);
	if (rc) {
		pr_err("Failed to get physical address\n");
		goto fail_map;
	}
	mem->kvaddr = ion_map_kernel(client->clnt, hndl, ionflag);
	if (!mem->kvaddr) {
		pr_err("Failed to map shared mem in kernel\n");
		rc = -EIO;
		goto fail_map;
	}

	mem->kvaddr += offset;
	mem->paddr += offset;
	mem->mem_type = client->mem_type;
	mem->smem_priv = hndl;
	mem->device_addr = mem->paddr;
	mem->size = len;
	return rc;
fail_map:
	ion_free(client->clnt, hndl);
fail_import_fd:
	return rc;
}

static int alloc_ion_mem(struct smem_client *client, size_t size,
		u32 align, u32 flags, struct msm_smem *mem)
{
	struct ion_handle *hndl;
	size_t len;
	int rc = 0;
	if (size == 0)
		goto skip_mem_alloc;
	flags = flags | ION_HEAP(ION_CP_MM_HEAP_ID);
	hndl = ion_alloc(client->clnt, size, align, flags);
	if (IS_ERR_OR_NULL(hndl)) {
		pr_err("Failed to allocate shared memory = %p, %d, %d, 0x%x\n",
				client, size, align, flags);
		rc = -ENOMEM;
		goto fail_shared_mem_alloc;
	}
	mem->mem_type = client->mem_type;
	mem->smem_priv = hndl;
	if (ion_phys(client->clnt, hndl, &mem->paddr, &len)) {
		pr_err("Failed to get physical address\n");
		rc = -EIO;
		goto fail_map;
	}
	mem->device_addr = mem->paddr;
	mem->size = size;
	mem->kvaddr = ion_map_kernel(client->clnt, hndl, 0);
	if (!mem->kvaddr) {
		pr_err("Failed to map shared mem in kernel\n");
		rc = -EIO;
		goto fail_map;
	}
	return rc;
fail_map:
	ion_free(client->clnt, hndl);
fail_shared_mem_alloc:
skip_mem_alloc:
	return rc;
}

static void free_ion_mem(struct smem_client *client, struct msm_smem *mem)
{
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

struct msm_smem *msm_smem_user_to_kernel(void *clt, int fd, u32 offset)
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
		rc = ion_user_to_kernel(clt, fd, offset, mem);
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

struct msm_smem *msm_smem_alloc(void *clt, size_t size, u32 align, u32 flags)
{
	struct smem_client *client;
	int rc = 0;
	struct msm_smem *mem;

	client = clt;
	if (!client) {
		pr_err("Invalid  client passed\n");
		return NULL;
	}
	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		pr_err("Failed to allocate shared mem\n");
		return NULL;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		rc = alloc_ion_mem(client, size, align, flags, mem);
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
