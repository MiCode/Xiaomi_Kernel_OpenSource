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
#include "media/msm_vidc.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_resources.h"

struct smem_client {
	int mem_type;
	void *clnt;
	struct msm_vidc_platform_resources *res;
	enum session_type session_type;
	bool tme_encode_mode;
};

static int msm_ion_get_device_address(struct smem_client *smem_client,
		struct ion_handle *hndl, unsigned long align,
		ion_phys_addr_t *iova, unsigned long *buffer_size,
		unsigned long flags, enum hal_buffer buffer_type,
		struct dma_mapping_info *mapping_info)
{
	int rc = 0;
	struct ion_client *clnt = NULL;
	struct dma_buf *buf = NULL;
	struct dma_buf_attachment *attach;
	struct sg_table *table = NULL;
	struct context_bank_info *cb = NULL;

	if (!iova || !buffer_size || !hndl || !smem_client || !mapping_info) {
		dprintk(VIDC_ERR, "Invalid params: %pK, %pK, %pK, %pK\n",
				smem_client, hndl, iova, buffer_size);
		return -EINVAL;
	}

	clnt = smem_client->clnt;
	if (!clnt) {
		dprintk(VIDC_ERR, "Invalid client\n");
		return -EINVAL;
	}

	if (is_iommu_present(smem_client->res)) {
		cb = msm_smem_get_context_bank(smem_client, flags & SMEM_SECURE,
				buffer_type);
		if (!cb) {
			dprintk(VIDC_ERR,
				"%s: Failed to get context bank device\n",
				 __func__);
			rc = -EIO;
			goto mem_map_failed;
		}

		/* Convert an Ion handle to a dma buf */
		buf = ion_share_dma_buf(clnt, hndl);
		if (IS_ERR_OR_NULL(buf)) {
			rc = PTR_ERR(buf) ?: -ENOMEM;
			dprintk(VIDC_ERR, "Share ION buf to DMA failed\n");
			goto mem_map_failed;
		}

		/* Check if the dmabuf size matches expected size */
		if (buf->size < *buffer_size) {
			rc = -EINVAL;
			dprintk(VIDC_ERR,
				"Size mismatch! Dmabuf size: %zu Expected Size: %lu",
				buf->size, *buffer_size);
			msm_vidc_res_handle_fatal_hw_error(smem_client->res,
					true);
			goto mem_buf_size_mismatch;
		}
		/* Prepare a dma buf for dma on the given device */
		attach = dma_buf_attach(buf, cb->dev);
		if (IS_ERR_OR_NULL(attach)) {
			rc = PTR_ERR(attach) ?: -ENOMEM;
			dprintk(VIDC_ERR, "Failed to attach dmabuf\n");
			goto mem_buf_attach_failed;
		}

		/* Get the scatterlist for the given attachment */
		table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
		if (IS_ERR_OR_NULL(table)) {
			rc = PTR_ERR(table) ?: -ENOMEM;
			dprintk(VIDC_ERR, "Failed to map table\n");
			goto mem_map_table_failed;
		}

		/* debug trace's need to be updated later */
		trace_msm_smem_buffer_iommu_op_start("MAP", 0, 0,
			align, *iova, *buffer_size);

		/* Map a scatterlist into SMMU */
		if (smem_client->res->sys_cache_present) {
			/* with sys cache attribute & delayed unmap */
			rc = msm_dma_map_sg_attrs(cb->dev, table->sgl,
				table->nents, DMA_BIDIRECTIONAL,
				buf, DMA_ATTR_IOMMU_USE_UPSTREAM_HINT);
		} else {
			/* with delayed unmap */
			rc = msm_dma_map_sg_lazy(cb->dev, table->sgl,
				table->nents, DMA_BIDIRECTIONAL, buf);
		}

		if (rc != table->nents) {
			dprintk(VIDC_ERR,
				"Mapping failed with rc(%d), expected rc(%d)\n",
				rc, table->nents);
			rc = -ENOMEM;
			goto mem_map_sg_failed;
		}
		if (table->sgl) {
			*iova = table->sgl->dma_address;
			*buffer_size = table->sgl->dma_length;
		} else {
			dprintk(VIDC_ERR, "sgl is NULL\n");
			rc = -ENOMEM;
			goto mem_map_sg_failed;
		}

		mapping_info->dev = cb->dev;
		mapping_info->mapping = cb->mapping;
		mapping_info->table = table;
		mapping_info->attach = attach;
		mapping_info->buf = buf;

		trace_msm_smem_buffer_iommu_op_end("MAP", 0, 0,
			align, *iova, *buffer_size);
	} else {
		dprintk(VIDC_DBG, "Using physical memory address\n");
		rc = ion_phys(clnt, hndl, iova, (size_t *)buffer_size);
		if (rc) {
			dprintk(VIDC_ERR, "ion memory map failed - %d\n", rc);
			goto mem_map_failed;
		}
	}

	return 0;
mem_map_sg_failed:
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
mem_map_table_failed:
	dma_buf_detach(buf, attach);
mem_buf_size_mismatch:
mem_buf_attach_failed:
	dma_buf_put(buf);
mem_map_failed:
	return rc;
}

static int msm_ion_put_device_address(struct smem_client *smem_client,
	struct ion_handle *hndl, u32 flags,
	struct dma_mapping_info *mapping_info,
	enum hal_buffer buffer_type)
{
	int rc = 0;

	if (!hndl || !smem_client || !mapping_info) {
		dprintk(VIDC_WARN, "Invalid params: %pK, %pK\n",
				smem_client, hndl);
		return -EINVAL;
	}

	if (!mapping_info->dev || !mapping_info->table ||
		!mapping_info->buf || !mapping_info->attach) {
		dprintk(VIDC_WARN, "Invalid params:\n");
		return -EINVAL;
	}

	if (is_iommu_present(smem_client->res)) {
		trace_msm_smem_buffer_iommu_op_start("UNMAP", 0, 0, 0, 0, 0);
		msm_dma_unmap_sg(mapping_info->dev, mapping_info->table->sgl,
			mapping_info->table->nents, DMA_BIDIRECTIONAL,
			mapping_info->buf);
		dma_buf_unmap_attachment(mapping_info->attach,
			mapping_info->table, DMA_BIDIRECTIONAL);
		dma_buf_detach(mapping_info->buf, mapping_info->attach);
		dma_buf_put(mapping_info->buf);
		trace_msm_smem_buffer_iommu_op_end("UNMAP", 0, 0, 0, 0, 0);

		mapping_info->dev = NULL;
		mapping_info->mapping = NULL;
		mapping_info->table = NULL;
		mapping_info->attach = NULL;
		mapping_info->buf = NULL;
	}

	return rc;
}

static void *msm_ion_get_dma_buf(int fd)
{
	struct dma_buf *dma_buf;

	dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		dprintk(VIDC_ERR, "Failed to get dma_buf for %d, error %ld\n",
				fd, PTR_ERR(dma_buf));
		dma_buf = NULL;
	}

	return dma_buf;
}

void *msm_smem_get_dma_buf(int fd)
{
	return (void *)msm_ion_get_dma_buf(fd);
}

static void msm_ion_put_dma_buf(struct dma_buf *dma_buf)
{
	if (!dma_buf) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK\n",
				__func__, dma_buf);
		return;
	}

	dma_buf_put(dma_buf);
}

void msm_smem_put_dma_buf(void *dma_buf)
{
	return msm_ion_put_dma_buf((struct dma_buf *)dma_buf);
}

static struct ion_handle *msm_ion_get_handle(void *ion_client,
		struct dma_buf *dma_buf)
{
	struct ion_handle *handle;

	if (!ion_client || !dma_buf) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK %pK\n",
				__func__, ion_client, dma_buf);
		return NULL;
	}

	handle = ion_import_dma_buf(ion_client, dma_buf);
	if (IS_ERR_OR_NULL(handle)) {
		dprintk(VIDC_ERR, "Failed to get ion_handle: %pK, %pK, %ld\n",
				ion_client, dma_buf, PTR_ERR(handle));
		handle = NULL;
	}

	return handle;
}

void *msm_smem_get_handle(struct smem_client *client, void *dma_buf)
{
	if (!client)
		return NULL;

	return (void *)msm_ion_get_handle(client->clnt,
			(struct dma_buf *)dma_buf);
}

static void msm_ion_put_handle(struct ion_client *ion_client,
		struct ion_handle *ion_handle)
{
	if (!ion_client || !ion_handle) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK %pK\n",
				__func__, ion_client, ion_handle);
		return;
	}

	ion_free(ion_client, ion_handle);
}

void msm_smem_put_handle(struct smem_client *client, void *handle)
{
	if (!client) {
		dprintk(VIDC_ERR, "%s: Invalid params %pK %pK\n",
				__func__, client, handle);
		return;
	}
	return msm_ion_put_handle(client->clnt, (struct ion_handle *)handle);
}

static int msm_ion_map_dma_buf(struct msm_vidc_inst *inst,
		struct msm_smem *smem)
{
	int rc = 0;
	ion_phys_addr_t iova = 0;
	u32 temp = 0;
	unsigned long buffer_size = 0;
	unsigned long align = SZ_4K;
	unsigned long ion_flags = 0;
	struct ion_client *ion_client;
	struct ion_handle *ion_handle;
	struct dma_buf *dma_buf;

	if (!inst || !inst->mem_client || !inst->mem_client->clnt) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK %pK\n",
				__func__, inst, smem);
		return -EINVAL;
	}

	ion_client = inst->mem_client->clnt;
	dma_buf = msm_ion_get_dma_buf(smem->fd);
	if (!dma_buf)
		return -EINVAL;
	ion_handle = msm_ion_get_handle(ion_client, dma_buf);
	if (!ion_handle)
		return -EINVAL;

	smem->dma_buf = dma_buf;
	smem->handle = ion_handle;
	rc = ion_handle_get_flags(ion_client, ion_handle, &ion_flags);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get ion flags: %d\n", rc);
		goto exit;
	}

	if (ion_flags & ION_FLAG_CACHED)
		smem->flags |= SMEM_CACHED;

	if (ion_flags & ION_FLAG_SECURE)
		smem->flags |= SMEM_SECURE;

	buffer_size = smem->size;
	rc = msm_ion_get_device_address(inst->mem_client, ion_handle,
			align, &iova, &buffer_size, smem->flags,
			smem->buffer_type, &smem->mapping_info);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get device address: %d\n", rc);
		goto exit;
	}
	temp = (u32)iova;
	if ((ion_phys_addr_t)temp != iova) {
		dprintk(VIDC_ERR, "iova(%pa) truncated to %#x", &iova, temp);
		rc = -EINVAL;
		goto exit;
	}

	smem->device_addr = (u32)iova + smem->offset;

exit:
	return rc;
}

int msm_smem_map_dma_buf(struct msm_vidc_inst *inst, struct msm_smem *smem)
{
	int rc = 0;

	if (!inst || !smem) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK %pK\n",
				__func__, inst, smem);
		return -EINVAL;
	}

	if (smem->refcount) {
		smem->refcount++;
		return rc;
	}

	switch (inst->mem_client->mem_type) {
	case SMEM_ION:
		rc = msm_ion_map_dma_buf(inst, smem);
		break;
	default:
		dprintk(VIDC_ERR, "%s: Unknown mem_type %d\n",
			__func__, inst->mem_client->mem_type);
		rc = -EINVAL;
		break;
	}
	if (!rc)
		smem->refcount++;

	return rc;
}

static int msm_ion_unmap_dma_buf(struct msm_vidc_inst *inst,
		struct msm_smem *smem)
{
	int rc = 0;

	if (!inst || !inst->mem_client || !smem) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK %pK\n",
				__func__, inst, smem);
		return -EINVAL;
	}

	rc = msm_ion_put_device_address(inst->mem_client, smem->handle,
			smem->flags, &smem->mapping_info, smem->buffer_type);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to put device address: %d\n", rc);
		goto exit;
	}

	msm_ion_put_handle(inst->mem_client->clnt, smem->handle);
	msm_ion_put_dma_buf(smem->dma_buf);

	smem->device_addr = 0x0;
	smem->handle = NULL;
	smem->dma_buf = NULL;

exit:
	return rc;
}

int msm_smem_unmap_dma_buf(struct msm_vidc_inst *inst, struct msm_smem *smem)
{
	int rc = 0;

	if (!inst || !smem) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK %pK\n",
				__func__, inst, smem);
		return -EINVAL;
	}

	if (smem->refcount) {
		smem->refcount--;
	} else {
		dprintk(VIDC_WARN,
			"unmap called while refcount is zero already\n");
		return -EINVAL;
	}

	if (smem->refcount)
		return rc;

	switch (inst->mem_client->mem_type) {
	case SMEM_ION:
		rc = msm_ion_unmap_dma_buf(inst, smem);
		break;
	default:
		dprintk(VIDC_ERR, "%s: Unknown mem_type %d\n",
			__func__, inst->mem_client->mem_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int get_secure_flag_for_buffer_type(
		struct smem_client *client, enum hal_buffer buffer_type)
{

	if (!client) {
		dprintk(VIDC_ERR, "%s - invalid params\n", __func__);
		return -EINVAL;
	}

	switch (buffer_type) {
	case HAL_BUFFER_INPUT:
		if (client->session_type == MSM_VIDC_ENCODER)
			return ION_FLAG_CP_PIXEL;
		else
			return ION_FLAG_CP_BITSTREAM;
	case HAL_BUFFER_OUTPUT:
	case HAL_BUFFER_OUTPUT2:
		if (client->session_type == MSM_VIDC_ENCODER)
			return ION_FLAG_CP_BITSTREAM;
		else
			return ION_FLAG_CP_PIXEL;
	case HAL_BUFFER_INTERNAL_SCRATCH:
		return ION_FLAG_CP_BITSTREAM;
	case HAL_BUFFER_INTERNAL_SCRATCH_1:
		return ION_FLAG_CP_NON_PIXEL;
	case HAL_BUFFER_INTERNAL_SCRATCH_2:
		return ION_FLAG_CP_PIXEL;
	case HAL_BUFFER_INTERNAL_PERSIST:
		return ION_FLAG_CP_BITSTREAM;
	case HAL_BUFFER_INTERNAL_PERSIST_1:
		return ION_FLAG_CP_NON_PIXEL;
	default:
		WARN(1, "No matching secure flag for buffer type : %x\n",
				buffer_type);
		return -EINVAL;
	}
}

static int alloc_ion_mem(struct smem_client *client, size_t size, u32 align,
	u32 flags, enum hal_buffer buffer_type, struct msm_smem *mem,
	int map_kernel)
{
	struct ion_handle *hndl;
	ion_phys_addr_t iova = 0;
	unsigned long buffer_size = 0;
	unsigned long heap_mask = 0;
	int rc = 0;
	int ion_flags = 0;

	if (!client || !mem) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK %pK\n",
				__func__, client, mem);
		return -EINVAL;
	}

	align = ALIGN(align, SZ_4K);
	size = ALIGN(size, SZ_4K);

	if (is_iommu_present(client->res)) {
		heap_mask = ION_HEAP(ION_IOMMU_HEAP_ID);
	} else {
		dprintk(VIDC_DBG,
			"allocate shared memory from adsp heap size %zx align %d\n",
			size, align);
		heap_mask = ION_HEAP(ION_ADSP_HEAP_ID);
	}

	if (flags & SMEM_CACHED)
		ion_flags |= ION_FLAG_CACHED;

	if (flags & SMEM_SECURE) {
		int secure_flag =
			get_secure_flag_for_buffer_type(client, buffer_type);
		if (secure_flag < 0) {
			rc = secure_flag;
			goto fail_shared_mem_alloc;
		}

		ion_flags |= ION_FLAG_SECURE | secure_flag;
		heap_mask = ION_HEAP(ION_SECURE_HEAP_ID);

		if (client->res->slave_side_cp) {
			heap_mask = ION_HEAP(ION_CP_MM_HEAP_ID);
			size = ALIGN(size, SZ_1M);
			align = ALIGN(size, SZ_1M);
		}
	}

	trace_msm_smem_buffer_ion_op_start("ALLOC", (u32)buffer_type,
		heap_mask, size, align, flags, map_kernel);
	hndl = ion_alloc(client->clnt, size, align, heap_mask, ion_flags);
	if (IS_ERR_OR_NULL(hndl)) {
		dprintk(VIDC_ERR,
		"Failed to allocate shared memory = %pK, %zx, %d, %#x\n",
		client, size, align, flags);
		rc = -ENOMEM;
		goto fail_shared_mem_alloc;
	}
	trace_msm_smem_buffer_ion_op_end("ALLOC", (u32)buffer_type,
		heap_mask, size, align, flags, map_kernel);

	mem->handle = hndl;
	mem->flags = flags;
	mem->buffer_type = buffer_type;
	mem->offset = 0;
	mem->size = size;

	if (map_kernel) {
		mem->kvaddr = ion_map_kernel(client->clnt, hndl);
		if (IS_ERR_OR_NULL(mem->kvaddr)) {
			dprintk(VIDC_ERR,
				"Failed to map shared mem in kernel\n");
			rc = -EIO;
			goto fail_map;
		}
	} else {
		mem->kvaddr = NULL;
	}

	rc = msm_ion_get_device_address(client, hndl, align, &iova,
			&buffer_size, flags, buffer_type, &mem->mapping_info);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get device address: %d\n",
			rc);
		goto fail_device_address;
	}
	mem->device_addr = (u32)iova;
	if ((ion_phys_addr_t)mem->device_addr != iova) {
		dprintk(VIDC_ERR, "iova(%pa) truncated to %#x",
			&iova, mem->device_addr);
		goto fail_device_address;
	}
	dprintk(VIDC_DBG,
		"%s: ion_handle = %pK, device_addr = %x, size = %d, kvaddr = %pK, buffer_type = %#x, flags = %#lx\n",
		__func__, mem->handle, mem->device_addr, mem->size,
		mem->kvaddr, mem->buffer_type, mem->flags);
	return rc;
fail_device_address:
	if (mem->kvaddr)
		ion_unmap_kernel(client->clnt, hndl);
fail_map:
	ion_free(client->clnt, hndl);
fail_shared_mem_alloc:
	return rc;
}

static int free_ion_mem(struct smem_client *client, struct msm_smem *mem)
{
	int rc = 0;

	if (!client || !mem) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK %pK\n",
				__func__, client, mem);
		return -EINVAL;
	}

	dprintk(VIDC_DBG,
		"%s: ion_handle = %pK, device_addr = %x, size = %d, kvaddr = %pK, buffer_type = %#x\n",
		__func__, mem->handle, mem->device_addr, mem->size,
		mem->kvaddr, mem->buffer_type);

	if (mem->device_addr) {
		msm_ion_put_device_address(client, mem->handle, mem->flags,
			&mem->mapping_info, mem->buffer_type);
		mem->device_addr = 0x0;
	}

	if (mem->kvaddr) {
		ion_unmap_kernel(client->clnt, mem->handle);
		mem->kvaddr = NULL;
	}

	if (mem->handle) {
		trace_msm_smem_buffer_ion_op_start("FREE",
				(u32)mem->buffer_type, -1, mem->size, -1,
				mem->flags, -1);
		ion_free(client->clnt, mem->handle);
		mem->handle = NULL;
		trace_msm_smem_buffer_ion_op_end("FREE", (u32)mem->buffer_type,
			-1, mem->size, -1, mem->flags, -1);
	}

	return rc;
}

static void *ion_new_client(void)
{
	struct ion_client *client = NULL;

	client = msm_ion_client_create("video_client");
	if (!client)
		dprintk(VIDC_ERR, "Failed to create smem client\n");

	dprintk(VIDC_DBG, "%s: client %pK\n", __func__, client);

	return client;
};

static void ion_delete_client(struct smem_client *client)
{
	if (!client) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK\n",
				__func__, client);
		return;
	}

	dprintk(VIDC_DBG, "%s: client %pK\n", __func__, client->clnt);
	ion_client_destroy(client->clnt);
	client->clnt = NULL;
}

static int msm_ion_cache_operations(void *ion_client, void *ion_handle,
		unsigned long offset, unsigned long size,
		enum smem_cache_ops cache_op)
{
	int rc = 0;
	unsigned long flags = 0;
	int msm_cache_ops = 0;

	if (!ion_client || !ion_handle) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK %pK\n",
			__func__, ion_client, ion_handle);
		return -EINVAL;
	}

	rc = ion_handle_get_flags(ion_client, ion_handle, &flags);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: ion_handle_get_flags failed: %d, ion client %pK, ion handle %pK\n",
			__func__, rc, ion_client, ion_handle);
		goto exit;
	}

	if (!ION_IS_CACHED(flags))
		goto exit;

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
		dprintk(VIDC_ERR, "%s: cache (%d) operation not supported\n",
			__func__, cache_op);
		rc = -EINVAL;
		goto exit;
	}

	rc = msm_ion_do_cache_offset_op(ion_client, ion_handle, NULL,
			offset, size, msm_cache_ops);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: cache operation failed %d, ion client %pK, ion handle %pK, offset %lu, size %lu, msm_cache_ops %u\n",
			__func__, rc, ion_client, ion_handle, offset,
			size, msm_cache_ops);
		goto exit;
	}

exit:
	return rc;
}

int msm_smem_cache_operations(struct smem_client *client,
		void *handle, unsigned long offset, unsigned long size,
		enum smem_cache_ops cache_op)
{
	int rc = 0;

	if (!client || !handle) {
		dprintk(VIDC_ERR, "%s: Invalid params: %pK %pK\n",
			__func__, client, handle);
		return -EINVAL;
	}

	switch (client->mem_type) {
	case SMEM_ION:
		rc = msm_ion_cache_operations(client->clnt, handle,
				offset, size, cache_op);
		if (rc)
			dprintk(VIDC_ERR,
			"%s: Failed cache operations: %d\n", __func__, rc);
		break;
	default:
		dprintk(VIDC_ERR, "%s: Mem type (%d) not supported\n",
			__func__, client->mem_type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

void *msm_smem_new_client(enum smem_type mtype,
		void *platform_resources, enum session_type stype)
{
	struct smem_client *client = NULL;
	void *clnt = NULL;
	struct msm_vidc_platform_resources *res = platform_resources;

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
			client->session_type = stype;
		}
	} else {
		dprintk(VIDC_ERR, "Failed to create new client: mtype = %d\n",
			mtype);
	}
	return client;
}

void msm_smem_set_tme_encode_mode(struct smem_client *client, bool enable)
{
	if (!client)
		return;
	client->tme_encode_mode = enable;
}

int msm_smem_alloc(struct smem_client *client, size_t size,
		u32 align, u32 flags, enum hal_buffer buffer_type,
		int map_kernel, struct msm_smem *smem)
{
	int rc = 0;

	if (!client || !smem || !size) {
		dprintk(VIDC_ERR, "%s: Invalid params %pK %pK %d\n",
				__func__, client, smem, (u32)size);
		return -EINVAL;
	}

	switch (client->mem_type) {
	case SMEM_ION:
		rc = alloc_ion_mem(client, size, align, flags, buffer_type,
					smem, map_kernel);
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		rc = -EINVAL;
		break;
	}
	if (rc) {
		dprintk(VIDC_ERR, "Failed to allocate memory\n");
	}

	return rc;
}

int msm_smem_free(void *clt, struct msm_smem *smem)
{
	int rc = 0;
	struct smem_client *client = clt;

	if (!client || !smem) {
		dprintk(VIDC_ERR, "Invalid  client/handle passed\n");
		return -EINVAL;
	}
	switch (client->mem_type) {
	case SMEM_ION:
		rc = free_ion_mem(client, smem);
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		rc = -EINVAL;
		break;
	}
	if (rc)
		dprintk(VIDC_ERR, "Failed to free memory\n");

	return rc;
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

struct context_bank_info *msm_smem_get_context_bank(void *clt,
			bool is_secure, enum hal_buffer buffer_type)
{
	struct smem_client *client = clt;
	struct context_bank_info *cb = NULL, *match = NULL;

	if (!clt) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return NULL;
	}

	/*
	 * HAL_BUFFER_INPUT is directly mapped to bitstream CB in DT
	 * as the buffer type structure was initially designed
	 * just for decoder. For Encoder, input should be mapped to
	 * pixel CB. So swap the buffer types just in this local scope.
	 */
	if (is_secure && client->session_type == MSM_VIDC_ENCODER) {
		if (buffer_type == HAL_BUFFER_INPUT)
			buffer_type = HAL_BUFFER_OUTPUT;
		else if (buffer_type == HAL_BUFFER_OUTPUT &&
			!client->tme_encode_mode)
			buffer_type = HAL_BUFFER_INPUT;
	}

	list_for_each_entry(cb, &client->res->context_banks, list) {
		if (cb->is_secure == is_secure &&
				cb->buffer_type & buffer_type) {
			match = cb;
			break;
		}
	}
	if (!match)
		dprintk(VIDC_ERR,
			"%s: cb not found for buffer_type %x, is_secure %d\n",
			__func__, buffer_type, is_secure);

	return match;
}
