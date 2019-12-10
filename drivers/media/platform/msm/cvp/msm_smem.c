// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <asm/dma-iommu.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/iommu.h>
#include <linux/msm_dma_iommu_mapping.h>
#include <linux/msm_ion.h>
#include <linux/ion_kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include "msm_cvp_core.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_resources.h"


static int msm_dma_get_device_address(struct dma_buf *dbuf, u32 align,
	dma_addr_t *iova, u32 *buffer_size, u32 flags, unsigned long ion_flags,
	u32 session_type, struct msm_cvp_platform_resources *res,
	struct cvp_dma_mapping_info *mapping_info)
{
	int rc = 0;
	struct dma_buf_attachment *attach;
	struct sg_table *table = NULL;
	struct context_bank_info *cb = NULL;

	if (!dbuf || !iova || !buffer_size || !mapping_info) {
		dprintk(CVP_ERR, "Invalid params: %pK, %pK, %pK, %pK\n",
			dbuf, iova, buffer_size, mapping_info);
		return -EINVAL;
	}

	if (is_iommu_present(res)) {
		cb = msm_cvp_smem_get_context_bank(
				session_type, (flags & SMEM_SECURE),
				res, ion_flags);
		if (!cb) {
			dprintk(CVP_ERR,
				"%s: Failed to get context bank device\n",
				 __func__);
			rc = -EIO;
			goto mem_map_failed;
		}

		/* Check if the dmabuf size matches expected size */
		if (dbuf->size < *buffer_size) {
			rc = -EINVAL;
			dprintk(CVP_ERR,
				"Size mismatch: Dmabuf size: %zu Expected Size: %lu",
				dbuf->size, *buffer_size);
			msm_cvp_res_handle_fatal_hw_error(res,
					true);
			goto mem_buf_size_mismatch;
		}

		/* Prepare a dma buf for dma on the given device */
		attach = dma_buf_attach(dbuf, cb->dev);
		if (IS_ERR_OR_NULL(attach)) {
			rc = PTR_ERR(attach) ?: -ENOMEM;
			dprintk(CVP_ERR, "Failed to attach dmabuf\n");
			goto mem_buf_attach_failed;
		}

		/*
		 * Get the scatterlist for the given attachment
		 * Mapping of sg is taken care by map attachment
		 */
		attach->dma_map_attrs = DMA_ATTR_DELAYED_UNMAP;
		/*
		 * We do not need dma_map function to perform cache operations
		 * on the whole buffer size and hence pass skip sync flag.
		 * We do the required cache operations separately for the
		 * required buffer size
		 */
		attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
		if (res->sys_cache_present)
			attach->dma_map_attrs |=
				DMA_ATTR_IOMMU_USE_UPSTREAM_HINT;

		table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
		if (IS_ERR_OR_NULL(table)) {
			rc = PTR_ERR(table) ?: -ENOMEM;
			dprintk(CVP_ERR, "Failed to map table\n");
			goto mem_map_table_failed;
		}

		/* debug trace's need to be updated later */
		trace_msm_cvp_smem_buffer_iommu_op_start("MAP", 0, 0,
			align, *iova, *buffer_size);

		if (table->sgl) {
			*iova = table->sgl->dma_address;
			*buffer_size = table->sgl->dma_length;
		} else {
			dprintk(CVP_ERR, "sgl is NULL\n");
			rc = -ENOMEM;
			goto mem_map_sg_failed;
		}

		mapping_info->dev = cb->dev;
		mapping_info->domain = cb->domain;
		mapping_info->table = table;
		mapping_info->attach = attach;
		mapping_info->buf = dbuf;
		mapping_info->cb_info = (void *)cb;

		trace_msm_cvp_smem_buffer_iommu_op_end("MAP", 0, 0,
			align, *iova, *buffer_size);
	} else {
		dprintk(CVP_DBG, "iommu not present, use phys mem addr\n");
	}

	return 0;
mem_map_sg_failed:
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
mem_map_table_failed:
	dma_buf_detach(dbuf, attach);
mem_buf_size_mismatch:
mem_buf_attach_failed:
mem_map_failed:
	return rc;
}

static int msm_dma_put_device_address(u32 flags,
	struct cvp_dma_mapping_info *mapping_info)
{
	int rc = 0;

	if (!mapping_info) {
		dprintk(CVP_WARN, "Invalid mapping_info\n");
		return -EINVAL;
	}

	if (!mapping_info->dev || !mapping_info->table ||
		!mapping_info->buf || !mapping_info->attach ||
		!mapping_info->cb_info) {
		dprintk(CVP_WARN, "Invalid params\n");
		return -EINVAL;
	}

	trace_msm_cvp_smem_buffer_iommu_op_start("UNMAP", 0, 0, 0, 0, 0);
	dma_buf_unmap_attachment(mapping_info->attach,
		mapping_info->table, DMA_BIDIRECTIONAL);
	dma_buf_detach(mapping_info->buf, mapping_info->attach);
	trace_msm_cvp_smem_buffer_iommu_op_end("UNMAP", 0, 0, 0, 0, 0);

	mapping_info->dev = NULL;
	mapping_info->domain = NULL;
	mapping_info->table = NULL;
	mapping_info->attach = NULL;
	mapping_info->buf = NULL;
	mapping_info->cb_info = NULL;


	return rc;
}

struct dma_buf *msm_cvp_smem_get_dma_buf(int fd)
{
	struct dma_buf *dma_buf;

	dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		dprintk(CVP_ERR, "Failed to get dma_buf for %d, error %ld\n",
				fd, PTR_ERR(dma_buf));
		dma_buf = NULL;
	}

	return dma_buf;
}

void msm_cvp_smem_put_dma_buf(void *dma_buf)
{
	if (!dma_buf) {
		dprintk(CVP_ERR, "%s: NULL dma_buf\n", __func__);
		return;
	}

	dma_buf_put((struct dma_buf *)dma_buf);
}

int msm_cvp_smem_map_dma_buf(struct msm_cvp_inst *inst,
				struct msm_cvp_smem *smem)
{
	int rc = 0;

	dma_addr_t iova = 0;
	u32 temp = 0;
	u32 buffer_size = 0;
	u32 align = SZ_4K;
	struct dma_buf *dbuf;
	unsigned long ion_flags = 0;

	if (!inst || !smem) {
		dprintk(CVP_ERR, "%s: Invalid params: %pK %pK\n",
				__func__, inst, smem);
		return -EINVAL;
	}

	if (smem->refcount) {
		smem->refcount++;
		return rc;
	}

	if (smem->fd >= 0) {
		dbuf = msm_cvp_smem_get_dma_buf(smem->fd);
		if (!dbuf) {
			rc = -EINVAL;
			dprintk(CVP_ERR, "%s: Invalid fd=%d", __func__,
				smem->fd);
			return rc;
		}
		smem->dma_buf = dbuf;
	} else {
		dbuf = smem->dma_buf;
		get_dma_buf(dbuf);
	}

	rc = dma_buf_get_flags(dbuf, &ion_flags);
	if (rc) {
		dprintk(CVP_ERR, "Failed to get dma buf flags: %d\n", rc);
		goto exit;
	}
	if (ion_flags & ION_FLAG_CACHED)
		smem->flags |= SMEM_CACHED;

	if (ion_flags & ION_FLAG_SECURE)
		smem->flags |= SMEM_SECURE;

	buffer_size = smem->size;
	if (smem->offset > dbuf->size - 1 ||
		smem->offset + buffer_size > dbuf->size) {
		dprintk(CVP_WARN, "%s: invalid offset %d or size %d\n",
			__func__, smem->offset, buffer_size);
		rc = -EINVAL;
		goto exit;
	}

	/* Ignore the buffer_type from user space. Only use ion flags */
	rc = msm_dma_get_device_address(dbuf, align, &iova, &buffer_size,
			smem->flags, ion_flags, inst->session_type,
			&(inst->core->resources), &smem->mapping_info);
	if (rc) {
		dprintk(CVP_ERR, "Failed to get device address: %d\n", rc);
		goto exit;
	}
	temp = (u32)iova;
	if ((dma_addr_t)temp != iova) {
		dprintk(CVP_ERR, "iova(%pa) truncated to %#x", &iova, temp);
		rc = -EINVAL;
		goto exit;
	}

	smem->device_addr = (u32)iova;
	smem->refcount++;

	return rc;
exit:
	dma_buf_put(dbuf);
	smem->device_addr = 0x0;
	smem->dma_buf = NULL;
	return rc;
}

int msm_cvp_smem_unmap_dma_buf(struct msm_cvp_inst *inst,
				struct msm_cvp_smem *smem)
{
	int rc = 0;

	if (!inst || !smem) {
		dprintk(CVP_ERR, "%s: Invalid params: %pK %pK\n",
				__func__, inst, smem);
		rc = -EINVAL;
		goto exit;
	}

	if (smem->refcount) {
		smem->refcount--;
	} else {
		dprintk(CVP_WARN,
			"unmap called while refcount is zero already\n");
		return -EINVAL;
	}

	if (smem->refcount)
		goto exit;

	rc = msm_dma_put_device_address(smem->flags, &smem->mapping_info);
	if (rc) {
		dprintk(CVP_ERR, "Failed to put device address: %d\n", rc);
		goto exit;
	}

	msm_cvp_smem_put_dma_buf(smem->dma_buf);

	smem->device_addr = 0x0;
	smem->dma_buf = NULL;

exit:
	return rc;
}

static int alloc_dma_mem(size_t size, u32 align, u32 flags, int map_kernel,
	struct msm_cvp_platform_resources *res, u32 session_type,
	struct msm_cvp_smem *mem)
{
	dma_addr_t iova = 0;
	u32 buffer_size = 0;
	unsigned long heap_mask = 0;
	int rc = 0;
	int ion_flags = 0;
	struct dma_buf *dbuf = NULL;

	if (!res) {
		dprintk(CVP_ERR, "%s: NULL res\n", __func__);
		return -EINVAL;
	}

	align = ALIGN(align, SZ_4K);
	size = ALIGN(size, SZ_4K);

	if (is_iommu_present(res)) {
		if (flags & SMEM_ADSP) {
			dprintk(CVP_DBG, "Allocating from ADSP heap\n");
			heap_mask = ION_HEAP(ION_ADSP_HEAP_ID);
		} else {
			heap_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
		}
	} else {
		dprintk(CVP_DBG,
		"allocate shared memory from adsp heap size %zx align %d\n",
		size, align);
		heap_mask = ION_HEAP(ION_ADSP_HEAP_ID);
	}

	if (flags & SMEM_CACHED)
		ion_flags |= ION_FLAG_CACHED;

	if (flags & SMEM_NON_PIXEL)
		ion_flags |= ION_FLAG_CP_NON_PIXEL;

	if (flags & SMEM_SECURE) {
		ion_flags |= ION_FLAG_SECURE;
		heap_mask = ION_HEAP(ION_SECURE_HEAP_ID);
	}

	trace_msm_cvp_smem_buffer_dma_op_start("ALLOC", (u32)ion_flags,
		heap_mask, size, align, flags, map_kernel);
	dbuf = ion_alloc(size, heap_mask, ion_flags);
	if (IS_ERR_OR_NULL(dbuf)) {
		dprintk(CVP_ERR,
		"Failed to allocate shared memory = %x bytes, %llx, %x\n",
		size, heap_mask, ion_flags);
		rc = -ENOMEM;
		goto fail_shared_mem_alloc;
	}
	trace_msm_cvp_smem_buffer_dma_op_end("ALLOC", (u32)ion_flags,
		heap_mask, size, align, flags, map_kernel);

	mem->flags = flags;
	mem->buffer_type = ion_flags;
	mem->offset = 0;
	mem->size = size;
	mem->dma_buf = dbuf;
	mem->kvaddr = NULL;

	rc = msm_dma_get_device_address(dbuf, align, &iova, &buffer_size, flags,
			ion_flags, session_type, res, &mem->mapping_info);
	if (rc) {
		dprintk(CVP_ERR, "Failed to get device address: %d\n",
			rc);
		goto fail_device_address;
	}
	mem->device_addr = (u32)iova;
	if ((dma_addr_t)mem->device_addr != iova) {
		dprintk(CVP_ERR, "iova(%pa) truncated to %#x",
			&iova, mem->device_addr);
		goto fail_device_address;
	}

	if (map_kernel) {
		dma_buf_begin_cpu_access(dbuf, DMA_BIDIRECTIONAL);
		mem->kvaddr = dma_buf_vmap(dbuf);
		if (!mem->kvaddr) {
			dprintk(CVP_ERR,
				"Failed to map shared mem in kernel\n");
			rc = -EIO;
			goto fail_map;
		}
	}

	dprintk(CVP_DBG,
		"%s: dma_buf = %pK, device_addr = %x, size = %d, kvaddr = %pK, buffer_type = %#x, flags = %#lx\n",
		__func__, mem->dma_buf, mem->device_addr, mem->size,
		mem->kvaddr, mem->buffer_type, mem->flags);
	return rc;

fail_map:
	if (map_kernel)
		dma_buf_end_cpu_access(dbuf, DMA_BIDIRECTIONAL);
fail_device_address:
	dma_buf_put(dbuf);
fail_shared_mem_alloc:
	return rc;
}

static int free_dma_mem(struct msm_cvp_smem *mem)
{
	dprintk(CVP_DBG,
		"%s: dma_buf = %pK, device_addr = %x, size = %d, kvaddr = %pK, buffer_type = %#x\n",
		__func__, mem->dma_buf, mem->device_addr, mem->size,
		mem->kvaddr, mem->buffer_type);

	if (mem->device_addr) {
		msm_dma_put_device_address(mem->flags, &mem->mapping_info);
		mem->device_addr = 0x0;
	}

	if (mem->kvaddr) {
		dma_buf_vunmap(mem->dma_buf, mem->kvaddr);
		mem->kvaddr = NULL;
		dma_buf_end_cpu_access(mem->dma_buf, DMA_BIDIRECTIONAL);
	}

	if (mem->dma_buf) {
		trace_msm_cvp_smem_buffer_dma_op_start("FREE",
				(u32)mem->buffer_type, -1, mem->size, -1,
				mem->flags, -1);
		dma_buf_put(mem->dma_buf);
		mem->dma_buf = NULL;
		trace_msm_cvp_smem_buffer_dma_op_end(
			"FREE", (u32)mem->buffer_type,
			-1, mem->size, -1, mem->flags, -1);
	}

	return 0;
}

int msm_cvp_smem_alloc(size_t size, u32 align, u32 flags, int map_kernel,
	void *res, u32 session_type, struct msm_cvp_smem *smem)
{
	int rc = 0;

	if (!smem || !size) {
		dprintk(CVP_ERR, "%s: NULL smem or %d size\n",
			__func__, (u32)size);
		return -EINVAL;
	}

	rc = alloc_dma_mem(size, align, flags, map_kernel,
				(struct msm_cvp_platform_resources *)res,
				session_type, smem);

	return rc;
}

int msm_cvp_smem_free(struct msm_cvp_smem *smem)
{
	int rc = 0;

	if (!smem) {
		dprintk(CVP_ERR, "NULL smem passed\n");
		return -EINVAL;
	}
	rc = free_dma_mem(smem);

	return rc;
};

int msm_cvp_smem_cache_operations(struct dma_buf *dbuf,
	enum smem_cache_ops cache_op, unsigned long offset, unsigned long size)
{
	int rc = 0;
	unsigned long flags = 0;

	if (!dbuf) {
		dprintk(CVP_ERR, "%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	/* Return if buffer doesn't support caching */
	rc = dma_buf_get_flags(dbuf, &flags);
	if (rc) {
		dprintk(CVP_ERR, "%s: dma_buf_get_flags failed, err %d\n",
			__func__, rc);
		return rc;
	} else if (!(flags & ION_FLAG_CACHED)) {
		return rc;
	}

	switch (cache_op) {
	case SMEM_CACHE_CLEAN:
	case SMEM_CACHE_CLEAN_INVALIDATE:
		rc = dma_buf_begin_cpu_access_partial(dbuf, DMA_BIDIRECTIONAL,
				offset, size);
		if (rc)
			break;
		rc = dma_buf_end_cpu_access_partial(dbuf, DMA_BIDIRECTIONAL,
				offset, size);
		break;
	case SMEM_CACHE_INVALIDATE:
		rc = dma_buf_begin_cpu_access_partial(dbuf, DMA_TO_DEVICE,
				offset, size);
		if (rc)
			break;
		rc = dma_buf_end_cpu_access_partial(dbuf, DMA_FROM_DEVICE,
				offset, size);
		break;
	default:
		dprintk(CVP_ERR, "%s: cache (%d) operation not supported\n",
			__func__, cache_op);
		rc = -EINVAL;
		break;
	}

	return rc;
}

struct context_bank_info *msm_cvp_smem_get_context_bank(u32 session_type,
	bool is_secure, struct msm_cvp_platform_resources *res,
	unsigned long ion_flags)
{
	struct context_bank_info *cb = NULL, *match = NULL;
	char *search_str;
	char *non_secure_cb = "cvp_hlos";
	char *secure_nonpixel_cb = "cvp_sec_nonpixel";
	char *secure_pixel_cb = "cvp_sec_pixel";

	if (ion_flags & ION_FLAG_CP_PIXEL)
		search_str = secure_pixel_cb;
	else if (ion_flags & ION_FLAG_CP_NON_PIXEL)
		search_str = secure_nonpixel_cb;
	else
		search_str = non_secure_cb;

	list_for_each_entry(cb, &res->context_banks, list) {
		if (cb->is_secure == is_secure &&
			!strcmp(search_str, cb->name)) {
			match = cb;
			break;
		}
	}

	if (!match)
		dprintk(CVP_ERR,
			"%s: cb not found for buffer_type %x, is_secure %d\n",
			__func__, ion_flags, is_secure);

	return match;
}
