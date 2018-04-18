/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/msm_ion.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>

#include "cam_req_mgr_util.h"
#include "cam_mem_mgr.h"
#include "cam_smmu_api.h"
#include "cam_debug_util.h"

static struct cam_mem_table tbl;

static int cam_mem_util_map_cpu_va(struct ion_handle *hdl,
	uint64_t *vaddr,
	size_t *len)
{
	*vaddr = (uintptr_t)ion_map_kernel(tbl.client, hdl);
	if (IS_ERR_OR_NULL((void *)*vaddr)) {
		CAM_ERR(CAM_CRM, "kernel map fail");
		return -ENOSPC;
	}

	if (ion_handle_get_size(tbl.client, hdl, len)) {
		CAM_ERR(CAM_CRM, "kernel get len failed");
		ion_unmap_kernel(tbl.client, hdl);
		return -ENOSPC;
	}

	return 0;
}

static int cam_mem_util_get_dma_dir(uint32_t flags)
{
	int rc = -EINVAL;

	if (flags & CAM_MEM_FLAG_HW_READ_ONLY)
		rc = DMA_TO_DEVICE;
	else if (flags & CAM_MEM_FLAG_HW_WRITE_ONLY)
		rc = DMA_FROM_DEVICE;
	else if (flags & CAM_MEM_FLAG_HW_READ_WRITE)
		rc = DMA_BIDIRECTIONAL;
	else if (flags & CAM_MEM_FLAG_PROTECTED_MODE)
		rc = DMA_BIDIRECTIONAL;

	return rc;
}

static int cam_mem_util_client_create(void)
{
	int rc = 0;

	tbl.client = msm_ion_client_create("camera_global_pool");
	if (IS_ERR_OR_NULL(tbl.client)) {
		CAM_ERR(CAM_CRM, "fail to create client");
		rc = -EINVAL;
	}

	return rc;
}

static void cam_mem_util_client_destroy(void)
{
	ion_client_destroy(tbl.client);
	tbl.client = NULL;
}

int cam_mem_mgr_init(void)
{
	int rc;
	int i;
	int bitmap_size;

	memset(tbl.bufq, 0, sizeof(tbl.bufq));

	rc = cam_mem_util_client_create();
	if (rc < 0) {
		CAM_ERR(CAM_CRM, "fail to create ion client");
		goto client_fail;
	}

	bitmap_size = BITS_TO_LONGS(CAM_MEM_BUFQ_MAX) * sizeof(long);
	tbl.bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!tbl.bitmap) {
		rc = -ENOMEM;
		goto bitmap_fail;
	}
	tbl.bits = bitmap_size * BITS_PER_BYTE;
	bitmap_zero(tbl.bitmap, tbl.bits);
	/* We need to reserve slot 0 because 0 is invalid */
	set_bit(0, tbl.bitmap);

	for (i = 1; i < CAM_MEM_BUFQ_MAX; i++) {
		tbl.bufq[i].fd = -1;
		tbl.bufq[i].buf_handle = -1;
	}
	mutex_init(&tbl.m_lock);
	return rc;

bitmap_fail:
	cam_mem_util_client_destroy();
client_fail:
	return rc;
}

static int32_t cam_mem_get_slot(void)
{
	int32_t idx;

	mutex_lock(&tbl.m_lock);
	idx = find_first_zero_bit(tbl.bitmap, tbl.bits);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		mutex_unlock(&tbl.m_lock);
		return -ENOMEM;
	}

	set_bit(idx, tbl.bitmap);
	tbl.bufq[idx].active = true;
	mutex_init(&tbl.bufq[idx].q_lock);
	mutex_unlock(&tbl.m_lock);

	return idx;
}

static void cam_mem_put_slot(int32_t idx)
{
	mutex_lock(&tbl.m_lock);
	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].active = false;
	mutex_unlock(&tbl.bufq[idx].q_lock);
	mutex_destroy(&tbl.bufq[idx].q_lock);
	clear_bit(idx, tbl.bitmap);
	mutex_unlock(&tbl.m_lock);
}

int cam_mem_get_io_buf(int32_t buf_handle, int32_t mmu_handle,
	uint64_t *iova_ptr, size_t *len_ptr)
{
	int rc = 0, idx;

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0)
		return -EINVAL;

	if (!tbl.bufq[idx].active)
		return -EINVAL;

	mutex_lock(&tbl.bufq[idx].q_lock);
	if (buf_handle != tbl.bufq[idx].buf_handle) {
		rc = -EINVAL;
		goto handle_mismatch;
	}

	if (CAM_MEM_MGR_IS_SECURE_HDL(buf_handle))
		rc = cam_smmu_get_stage2_iova(mmu_handle,
			tbl.bufq[idx].fd,
			iova_ptr,
			len_ptr);
	else
		rc = cam_smmu_get_iova(mmu_handle,
			tbl.bufq[idx].fd,
			iova_ptr,
			len_ptr);
	if (rc < 0)
		CAM_ERR(CAM_CRM, "fail to get buf hdl :%d", buf_handle);

handle_mismatch:
	mutex_unlock(&tbl.bufq[idx].q_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_get_io_buf);

int cam_mem_get_cpu_buf(int32_t buf_handle, uint64_t *vaddr_ptr, size_t *len)
{
	int rc = 0;
	int idx;
	struct ion_handle *ion_hdl = NULL;
	uint64_t kvaddr = 0;
	size_t klen = 0;

	if (!buf_handle || !vaddr_ptr || !len)
		return -EINVAL;

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0)
		return -EINVAL;

	if (!tbl.bufq[idx].active)
		return -EPERM;

	mutex_lock(&tbl.bufq[idx].q_lock);
	if (buf_handle != tbl.bufq[idx].buf_handle) {
		rc = -EINVAL;
		goto exit_func;
	}

	ion_hdl = tbl.bufq[idx].i_hdl;
	if (!ion_hdl) {
		CAM_ERR(CAM_CRM, "Invalid ION handle");
		rc = -EINVAL;
		goto exit_func;
	}

	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS) {
		if (!tbl.bufq[idx].kmdvaddr) {
			rc = cam_mem_util_map_cpu_va(ion_hdl,
				&kvaddr, &klen);
			if (rc)
				goto exit_func;
			tbl.bufq[idx].kmdvaddr = kvaddr;
		}
	} else {
		rc = -EINVAL;
		goto exit_func;
	}

	*vaddr_ptr = tbl.bufq[idx].kmdvaddr;
	*len = tbl.bufq[idx].len;

exit_func:
	mutex_unlock(&tbl.bufq[idx].q_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_get_cpu_buf);

int cam_mem_mgr_cache_ops(struct cam_mem_cache_ops_cmd *cmd)
{
	int rc = 0, idx;
	uint32_t ion_cache_ops;
	unsigned long ion_flag = 0;

	if (!cmd)
		return -EINVAL;

	idx = CAM_MEM_MGR_GET_HDL_IDX(cmd->buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0)
		return -EINVAL;

	mutex_lock(&tbl.bufq[idx].q_lock);

	if (!tbl.bufq[idx].active) {
		rc = -EINVAL;
		goto fail;
	}

	if (cmd->buf_handle != tbl.bufq[idx].buf_handle) {
		rc = -EINVAL;
		goto fail;
	}

	rc = ion_handle_get_flags(tbl.client, tbl.bufq[idx].i_hdl,
		&ion_flag);
	if (rc) {
		CAM_ERR(CAM_CRM, "cache get flags failed %d", rc);
		goto fail;
	}

	if (ION_IS_CACHED(ion_flag)) {
		switch (cmd->mem_cache_ops) {
		case CAM_MEM_CLEAN_CACHE:
			ion_cache_ops = ION_IOC_CLEAN_CACHES;
			break;
		case CAM_MEM_INV_CACHE:
			ion_cache_ops = ION_IOC_INV_CACHES;
			break;
		case CAM_MEM_CLEAN_INV_CACHE:
			ion_cache_ops = ION_IOC_CLEAN_INV_CACHES;
			break;
		default:
			CAM_ERR(CAM_CRM,
				"invalid cache ops :%d", cmd->mem_cache_ops);
			rc = -EINVAL;
			goto fail;
		}

		rc = msm_ion_do_cache_op(tbl.client,
				tbl.bufq[idx].i_hdl,
				(void *)tbl.bufq[idx].vaddr,
				tbl.bufq[idx].len,
				ion_cache_ops);
		if (rc)
			CAM_ERR(CAM_CRM, "cache operation failed %d", rc);
	}
fail:
	mutex_unlock(&tbl.bufq[idx].q_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_cache_ops);

static int cam_mem_util_get_dma_buf(size_t len,
	size_t align,
	unsigned int heap_id_mask,
	unsigned int flags,
	struct ion_handle **hdl,
	struct dma_buf **buf)
{
	int rc = 0;

	if (!hdl || !buf) {
		CAM_ERR(CAM_CRM, "Invalid params");
		return -EINVAL;
	}

	*hdl = ion_alloc(tbl.client, len, align, heap_id_mask, flags);
	if (IS_ERR_OR_NULL(*hdl))
		return -ENOMEM;

	*buf = ion_share_dma_buf(tbl.client, *hdl);
	if (IS_ERR_OR_NULL(*buf)) {
		CAM_ERR(CAM_CRM, "get dma buf fail");
		rc = -EINVAL;
		goto get_buf_fail;
	}

	return rc;

get_buf_fail:
	ion_free(tbl.client, *hdl);
	return rc;

}

static int cam_mem_util_get_dma_buf_fd(size_t len,
	size_t align,
	unsigned int heap_id_mask,
	unsigned int flags,
	struct ion_handle **hdl,
	int *fd)
{
	int rc = 0;

	if (!hdl || !fd) {
		CAM_ERR(CAM_CRM, "Invalid params");
		return -EINVAL;
	}

	*hdl = ion_alloc(tbl.client, len, align, heap_id_mask, flags);
	if (IS_ERR_OR_NULL(*hdl))
		return -ENOMEM;

	*fd = ion_share_dma_buf_fd(tbl.client, *hdl);
	if (*fd < 0) {
		CAM_ERR(CAM_CRM, "get fd fail");
		rc = -EINVAL;
		goto get_fd_fail;
	}

	return rc;

get_fd_fail:
	ion_free(tbl.client, *hdl);
	return rc;
}

static int cam_mem_util_ion_alloc(struct cam_mem_mgr_alloc_cmd *cmd,
	struct ion_handle **hdl,
	int *fd)
{
	uint32_t heap_id;
	uint32_t ion_flag = 0;
	int rc;

	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		heap_id = ION_HEAP(ION_SECURE_DISPLAY_HEAP_ID);
		ion_flag |= ION_FLAG_SECURE | ION_FLAG_CP_CAMERA;
	} else {
		heap_id = ION_HEAP(ION_SYSTEM_HEAP_ID) |
			ION_HEAP(ION_CAMERA_HEAP_ID);
	}

	if (cmd->flags & CAM_MEM_FLAG_CACHE)
		ion_flag |= ION_FLAG_CACHED;
	else
		ion_flag &= ~ION_FLAG_CACHED;

	rc = cam_mem_util_get_dma_buf_fd(cmd->len,
		cmd->align,
		heap_id,
		ion_flag,
		hdl,
		fd);

	return rc;
}


static int cam_mem_util_check_flags(struct cam_mem_mgr_alloc_cmd *cmd)
{
	if (!cmd->flags) {
		CAM_ERR(CAM_CRM, "Invalid flags");
		return -EINVAL;
	}

	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_CRM, "Num of mmu hdl exceeded maximum(%d)",
			CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE &&
		cmd->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		CAM_ERR(CAM_CRM, "Kernel mapping in secure mode not allowed");
		return -EINVAL;
	}

	return 0;
}

static int cam_mem_util_check_map_flags(struct cam_mem_mgr_map_cmd *cmd)
{
	if (!cmd->flags) {
		CAM_ERR(CAM_CRM, "Invalid flags");
		return -EINVAL;
	}

	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_CRM, "Num of mmu hdl exceeded maximum(%d)",
			CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE &&
		cmd->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		CAM_ERR(CAM_CRM, "Kernel mapping in secure mode not allowed");
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		CAM_ERR(CAM_CRM,
			"Shared memory buffers are not allowed to be mapped");
		return -EINVAL;
	}

	return 0;
}

static int cam_mem_util_map_hw_va(uint32_t flags,
	int32_t *mmu_hdls,
	int32_t num_hdls,
	int fd,
	dma_addr_t *hw_vaddr,
	size_t *len,
	enum cam_smmu_region_id region)
{
	int i;
	int rc = -1;
	int dir = cam_mem_util_get_dma_dir(flags);

	if (dir < 0) {
		CAM_ERR(CAM_CRM, "fail to map DMA direction");
		return dir;
	}

	if (flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		for (i = 0; i < num_hdls; i++) {
			rc = cam_smmu_map_stage2_iova(mmu_hdls[i],
				fd,
				dir,
				tbl.client,
				(ion_phys_addr_t *)hw_vaddr,
				len);

			if (rc < 0) {
				CAM_ERR(CAM_CRM,
					"Failed to securely map to smmu");
				goto multi_map_fail;
			}
		}
	} else {
		for (i = 0; i < num_hdls; i++) {
			rc = cam_smmu_map_user_iova(mmu_hdls[i],
				fd,
				dir,
				(dma_addr_t *)hw_vaddr,
				len,
				region);

			if (rc < 0) {
				CAM_ERR(CAM_CRM, "Failed to map to smmu");
				goto multi_map_fail;
			}
		}
	}

	return rc;
multi_map_fail:
	if (flags & CAM_MEM_FLAG_PROTECTED_MODE)
		for (--i; i > 0; i--)
			cam_smmu_unmap_stage2_iova(mmu_hdls[i], fd);
	else
		for (--i; i > 0; i--)
			cam_smmu_unmap_user_iova(mmu_hdls[i],
				fd,
				CAM_SMMU_REGION_IO);
	return rc;

}

int cam_mem_mgr_alloc_and_map(struct cam_mem_mgr_alloc_cmd *cmd)
{
	int rc;
	int32_t idx;
	struct ion_handle *ion_hdl;
	int ion_fd;
	dma_addr_t hw_vaddr = 0;
	size_t len;

	if (!cmd) {
		CAM_ERR(CAM_CRM, " Invalid argument");
		return -EINVAL;
	}
	len = cmd->len;

	rc = cam_mem_util_check_flags(cmd);
	if (rc) {
		CAM_ERR(CAM_CRM, "Invalid flags: flags = %X", cmd->flags);
		return rc;
	}

	rc = cam_mem_util_ion_alloc(cmd,
		&ion_hdl,
		&ion_fd);
	if (rc) {
		CAM_ERR(CAM_CRM, "Ion allocation failed");
		return rc;
	}

	idx = cam_mem_get_slot();
	if (idx < 0) {
		rc = -ENOMEM;
		goto slot_fail;
	}

	if ((cmd->flags & CAM_MEM_FLAG_HW_READ_WRITE) ||
		(cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) ||
		(cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)) {

		enum cam_smmu_region_id region;

		if (cmd->flags & CAM_MEM_FLAG_HW_READ_WRITE)
			region = CAM_SMMU_REGION_IO;


		if (cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS)
			region = CAM_SMMU_REGION_SHARED;

		rc = cam_mem_util_map_hw_va(cmd->flags,
			cmd->mmu_hdls,
			cmd->num_hdl,
			ion_fd,
			&hw_vaddr,
			&len,
			region);
		if (rc)
			goto map_hw_fail;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].fd = ion_fd;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].flags = cmd->flags;
	tbl.bufq[idx].buf_handle = GET_MEM_HANDLE(idx, ion_fd);
	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)
		CAM_MEM_MGR_SET_SECURE_HDL(tbl.bufq[idx].buf_handle, true);
	tbl.bufq[idx].kmdvaddr = 0;

	if (cmd->num_hdl > 0)
		tbl.bufq[idx].vaddr = hw_vaddr;
	else
		tbl.bufq[idx].vaddr = 0;

	tbl.bufq[idx].i_hdl = ion_hdl;
	tbl.bufq[idx].len = cmd->len;
	tbl.bufq[idx].num_hdl = cmd->num_hdl;
	memcpy(tbl.bufq[idx].hdls, cmd->mmu_hdls,
		sizeof(int32_t) * cmd->num_hdl);
	tbl.bufq[idx].is_imported = false;
	mutex_unlock(&tbl.bufq[idx].q_lock);

	cmd->out.buf_handle = tbl.bufq[idx].buf_handle;
	cmd->out.fd = tbl.bufq[idx].fd;
	cmd->out.vaddr = 0;

	CAM_DBG(CAM_CRM, "buf handle: %x, fd: %d, len: %zu",
		cmd->out.buf_handle, cmd->out.fd,
		tbl.bufq[idx].len);

	return rc;

map_hw_fail:
	cam_mem_put_slot(idx);
slot_fail:
	ion_free(tbl.client, ion_hdl);
	return rc;
}

int cam_mem_mgr_map(struct cam_mem_mgr_map_cmd *cmd)
{
	int32_t idx;
	int rc;
	struct ion_handle *ion_hdl;
	dma_addr_t hw_vaddr = 0;
	size_t len = 0;

	if (!cmd || (cmd->fd < 0)) {
		CAM_ERR(CAM_CRM, "Invalid argument");
		return -EINVAL;
	}

	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE)
		return -EINVAL;

	rc = cam_mem_util_check_map_flags(cmd);
	if (rc) {
		CAM_ERR(CAM_CRM, "Invalid flags: flags = %X", cmd->flags);
		return rc;
	}

	ion_hdl = ion_import_dma_buf_fd(tbl.client, cmd->fd);
	if (IS_ERR_OR_NULL((void *)(ion_hdl))) {
		CAM_ERR(CAM_CRM, "Failed to import ion fd");
		return -EINVAL;
	}

	if ((cmd->flags & CAM_MEM_FLAG_HW_READ_WRITE) ||
		(cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)) {
		rc = cam_mem_util_map_hw_va(cmd->flags,
			cmd->mmu_hdls,
			cmd->num_hdl,
			cmd->fd,
			&hw_vaddr,
			&len,
			CAM_SMMU_REGION_IO);
		if (rc)
			goto map_fail;
	}

	idx = cam_mem_get_slot();
	if (idx < 0) {
		rc = -ENOMEM;
		goto map_fail;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].fd = cmd->fd;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].flags = cmd->flags;
	tbl.bufq[idx].buf_handle = GET_MEM_HANDLE(idx, cmd->fd);
	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)
		CAM_MEM_MGR_SET_SECURE_HDL(tbl.bufq[idx].buf_handle, true);
	tbl.bufq[idx].kmdvaddr = 0;

	if (cmd->num_hdl > 0)
		tbl.bufq[idx].vaddr = hw_vaddr;
	else
		tbl.bufq[idx].vaddr = 0;

	tbl.bufq[idx].i_hdl = ion_hdl;
	tbl.bufq[idx].len = len;
	tbl.bufq[idx].num_hdl = cmd->num_hdl;
	memcpy(tbl.bufq[idx].hdls, cmd->mmu_hdls,
		sizeof(int32_t) * cmd->num_hdl);
	tbl.bufq[idx].is_imported = true;
	mutex_unlock(&tbl.bufq[idx].q_lock);

	cmd->out.buf_handle = tbl.bufq[idx].buf_handle;
	cmd->out.vaddr = 0;

	return rc;

map_fail:
	ion_free(tbl.client, ion_hdl);
	return rc;
}

static int cam_mem_util_unmap_hw_va(int32_t idx,
	enum cam_smmu_region_id region,
	enum cam_smmu_mapping_client client)
{
	int i;
	uint32_t flags;
	int32_t *mmu_hdls;
	int num_hdls;
	int fd;
	int rc = -EINVAL;

	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_CRM, "Incorrect index");
		return rc;
	}

	flags = tbl.bufq[idx].flags;
	mmu_hdls = tbl.bufq[idx].hdls;
	num_hdls = tbl.bufq[idx].num_hdl;
	fd = tbl.bufq[idx].fd;

	if (flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		for (i = 0; i < num_hdls; i++) {
			rc = cam_smmu_unmap_stage2_iova(mmu_hdls[i], fd);
			if (rc < 0)
				goto unmap_end;
		}
	} else {
		for (i = 0; i < num_hdls; i++) {
			if (client == CAM_SMMU_MAPPING_USER) {
			rc = cam_smmu_unmap_user_iova(mmu_hdls[i],
				fd, region);
			} else if (client == CAM_SMMU_MAPPING_KERNEL) {
				rc = cam_smmu_unmap_kernel_iova(mmu_hdls[i],
					tbl.bufq[idx].dma_buf, region);
			} else {
				CAM_ERR(CAM_CRM,
					"invalid caller for unmapping : %d",
					client);
				rc = -EINVAL;
			}
			if (rc < 0)
				goto unmap_end;
		}
	}

	return rc;

unmap_end:
	CAM_ERR(CAM_CRM, "unmapping failed");
	return rc;
}

static void cam_mem_mgr_unmap_active_buf(int idx)
{
	enum cam_smmu_region_id region = CAM_SMMU_REGION_SHARED;

	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_SHARED_ACCESS)
		region = CAM_SMMU_REGION_SHARED;
	else if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_READ_WRITE)
		region = CAM_SMMU_REGION_IO;

	cam_mem_util_unmap_hw_va(idx, region, CAM_SMMU_MAPPING_USER);
}

static int cam_mem_mgr_cleanup_table(void)
{
	int i;

	mutex_lock(&tbl.m_lock);
	for (i = 1; i < CAM_MEM_BUFQ_MAX; i++) {
		if (!tbl.bufq[i].active) {
			CAM_DBG(CAM_CRM,
				"Buffer inactive at idx=%d, continuing", i);
			continue;
		} else {
			CAM_DBG(CAM_CRM,
			"Active buffer at idx=%d, possible leak needs unmapping",
			i);
			cam_mem_mgr_unmap_active_buf(i);
		}

		mutex_lock(&tbl.bufq[i].q_lock);
		if (tbl.bufq[i].i_hdl) {
			ion_free(tbl.client, tbl.bufq[i].i_hdl);
			tbl.bufq[i].i_hdl = NULL;
		}
		tbl.bufq[i].fd = -1;
		tbl.bufq[i].flags = 0;
		tbl.bufq[i].buf_handle = -1;
		tbl.bufq[i].vaddr = 0;
		tbl.bufq[i].len = 0;
		memset(tbl.bufq[i].hdls, 0,
			sizeof(int32_t) * tbl.bufq[i].num_hdl);
		tbl.bufq[i].num_hdl = 0;
		tbl.bufq[i].i_hdl = NULL;
		tbl.bufq[i].active = false;
		mutex_unlock(&tbl.bufq[i].q_lock);
		mutex_destroy(&tbl.bufq[i].q_lock);
	}
	bitmap_zero(tbl.bitmap, tbl.bits);
	/* We need to reserve slot 0 because 0 is invalid */
	set_bit(0, tbl.bitmap);
	mutex_unlock(&tbl.m_lock);

	return 0;
}

void cam_mem_mgr_deinit(void)
{
	cam_mem_mgr_cleanup_table();
	mutex_lock(&tbl.m_lock);
	bitmap_zero(tbl.bitmap, tbl.bits);
	kfree(tbl.bitmap);
	tbl.bitmap = NULL;
	cam_mem_util_client_destroy();
	mutex_unlock(&tbl.m_lock);
	mutex_destroy(&tbl.m_lock);
}

static int cam_mem_util_unmap(int32_t idx,
	enum cam_smmu_mapping_client client)
{
	int rc = 0;
	enum cam_smmu_region_id region = CAM_SMMU_REGION_SHARED;

	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_CRM, "Incorrect index");
		return -EINVAL;
	}

	CAM_DBG(CAM_CRM, "Flags = %X idx %d", tbl.bufq[idx].flags, idx);

	mutex_lock(&tbl.m_lock);
	if ((!tbl.bufq[idx].active) &&
		(tbl.bufq[idx].vaddr) == 0) {
		CAM_WARN(CAM_CRM, "Buffer at idx=%d is already unmapped,",
			idx);
		mutex_unlock(&tbl.m_lock);
		return 0;
	}


	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS)
		if (tbl.bufq[idx].i_hdl && tbl.bufq[idx].kmdvaddr)
			ion_unmap_kernel(tbl.client, tbl.bufq[idx].i_hdl);

	/* SHARED flag gets precedence, all other flags after it */
	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		region = CAM_SMMU_REGION_SHARED;
	} else {
		if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_READ_WRITE)
			region = CAM_SMMU_REGION_IO;
	}

	if ((tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_READ_WRITE) ||
		(tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) ||
		(tbl.bufq[idx].flags & CAM_MEM_FLAG_PROTECTED_MODE))
		rc = cam_mem_util_unmap_hw_va(idx, region, client);


	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].flags = 0;
	tbl.bufq[idx].buf_handle = -1;
	tbl.bufq[idx].vaddr = 0;
	memset(tbl.bufq[idx].hdls, 0,
		sizeof(int32_t) * CAM_MEM_MMU_MAX_HANDLE);

	CAM_DBG(CAM_CRM,
		"Ion handle at idx = %d freeing = %pK, fd = %d, imported %d dma_buf %pK",
		idx, tbl.bufq[idx].i_hdl, tbl.bufq[idx].fd,
		tbl.bufq[idx].is_imported,
		tbl.bufq[idx].dma_buf);

	if (tbl.bufq[idx].i_hdl) {
		ion_free(tbl.client, tbl.bufq[idx].i_hdl);
		tbl.bufq[idx].i_hdl = NULL;
	}

	tbl.bufq[idx].fd = -1;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].is_imported = false;
	tbl.bufq[idx].len = 0;
	tbl.bufq[idx].num_hdl = 0;
	tbl.bufq[idx].active = false;
	mutex_unlock(&tbl.bufq[idx].q_lock);
	mutex_destroy(&tbl.bufq[idx].q_lock);
	clear_bit(idx, tbl.bitmap);
	mutex_unlock(&tbl.m_lock);

	return rc;
}

int cam_mem_mgr_release(struct cam_mem_mgr_release_cmd *cmd)
{
	int idx;
	int rc;

	if (!cmd) {
		CAM_ERR(CAM_CRM, "Invalid argument");
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(cmd->buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_CRM, "Incorrect index extracted from mem handle");
		return -EINVAL;
	}

	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_CRM, "Released buffer state should be active");
		return -EINVAL;
	}

	if (tbl.bufq[idx].buf_handle != cmd->buf_handle) {
		CAM_ERR(CAM_CRM,
			"Released buf handle not matching within table");
		return -EINVAL;
	}

	CAM_DBG(CAM_CRM, "Releasing hdl = %u", cmd->buf_handle);
	rc = cam_mem_util_unmap(idx, CAM_SMMU_MAPPING_USER);

	return rc;
}

int cam_mem_mgr_request_mem(struct cam_mem_mgr_request_desc *inp,
	struct cam_mem_mgr_memory_desc *out)
{
	struct ion_handle *hdl;
	struct dma_buf *buf = NULL;
	int ion_fd = -1;
	int rc = 0;
	uint32_t heap_id;
	int32_t ion_flag = 0;
	uint64_t kvaddr;
	dma_addr_t iova = 0;
	size_t request_len = 0;
	uint32_t mem_handle;
	int32_t idx;
	int32_t smmu_hdl = 0;
	int32_t num_hdl = 0;

	enum cam_smmu_region_id region = CAM_SMMU_REGION_SHARED;

	if (!inp || !out) {
		CAM_ERR(CAM_CRM, "Invalid params");
		return -EINVAL;
	}

	if (!(inp->flags & CAM_MEM_FLAG_HW_READ_WRITE ||
		inp->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS ||
		inp->flags & CAM_MEM_FLAG_CACHE)) {
		CAM_ERR(CAM_CRM, "Invalid flags for request mem");
		return -EINVAL;
	}

	if (inp->flags & CAM_MEM_FLAG_CACHE)
		ion_flag |= ION_FLAG_CACHED;
	else
		ion_flag &= ~ION_FLAG_CACHED;

	heap_id = ION_HEAP(ION_SYSTEM_HEAP_ID) |
		ION_HEAP(ION_CAMERA_HEAP_ID);

	rc = cam_mem_util_get_dma_buf(inp->size,
		inp->align,
		heap_id,
		ion_flag,
		&hdl,
		&buf);

	if (rc) {
		CAM_ERR(CAM_CRM, "ION alloc failed for shared buffer");
		goto ion_fail;
	} else {
		CAM_DBG(CAM_CRM, "Got dma_buf = %pK, hdl = %pK", buf, hdl);
	}

	rc = cam_mem_util_map_cpu_va(hdl, &kvaddr, &request_len);
	if (rc) {
		CAM_ERR(CAM_CRM, "Failed to get kernel vaddr");
		goto map_fail;
	}

	if (!inp->smmu_hdl) {
		CAM_ERR(CAM_CRM, "Invalid SMMU handle");
		rc = -EINVAL;
		goto smmu_fail;
	}

	/* SHARED flag gets precedence, all other flags after it */
	if (inp->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		region = CAM_SMMU_REGION_SHARED;
	} else {
		if (inp->flags & CAM_MEM_FLAG_HW_READ_WRITE)
			region = CAM_SMMU_REGION_IO;
	}

	rc = cam_smmu_map_kernel_iova(inp->smmu_hdl,
		buf,
		CAM_SMMU_MAP_RW,
		&iova,
		&request_len,
		region);

	if (rc < 0) {
		CAM_ERR(CAM_CRM, "SMMU mapping failed");
		goto smmu_fail;
	}

	smmu_hdl = inp->smmu_hdl;
	num_hdl = 1;

	idx = cam_mem_get_slot();
	if (idx < 0) {
		rc = -ENOMEM;
		goto slot_fail;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	mem_handle = GET_MEM_HANDLE(idx, ion_fd);
	tbl.bufq[idx].dma_buf = buf;
	tbl.bufq[idx].fd = -1;
	tbl.bufq[idx].flags = inp->flags;
	tbl.bufq[idx].buf_handle = mem_handle;
	tbl.bufq[idx].kmdvaddr = kvaddr;

	tbl.bufq[idx].vaddr = iova;

	tbl.bufq[idx].i_hdl = hdl;
	tbl.bufq[idx].len = inp->size;
	tbl.bufq[idx].num_hdl = num_hdl;
	memcpy(tbl.bufq[idx].hdls, &smmu_hdl,
		sizeof(int32_t));
	tbl.bufq[idx].is_imported = false;
	mutex_unlock(&tbl.bufq[idx].q_lock);

	out->kva = kvaddr;
	out->iova = (uint32_t)iova;
	out->smmu_hdl = smmu_hdl;
	out->mem_handle = mem_handle;
	out->len = inp->size;
	out->region = region;

	return rc;
slot_fail:
	cam_smmu_unmap_kernel_iova(inp->smmu_hdl,
	buf, region);
smmu_fail:
	ion_unmap_kernel(tbl.client, hdl);
map_fail:
	ion_free(tbl.client, hdl);
ion_fail:
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_request_mem);

int cam_mem_mgr_release_mem(struct cam_mem_mgr_memory_desc *inp)
{
	int32_t idx;
	int rc;

	if (!inp) {
		CAM_ERR(CAM_CRM, "Invalid argument");
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(inp->mem_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_CRM, "Incorrect index extracted from mem handle");
		return -EINVAL;
	}

	if (!tbl.bufq[idx].active) {
		if (tbl.bufq[idx].vaddr == 0) {
			CAM_ERR(CAM_CRM, "buffer is released already");
			return 0;
		}
		CAM_ERR(CAM_CRM, "Released buffer state should be active");
		return -EINVAL;
	}

	if (tbl.bufq[idx].buf_handle != inp->mem_handle) {
		CAM_ERR(CAM_CRM,
			"Released buf handle not matching within table");
		return -EINVAL;
	}

	CAM_DBG(CAM_CRM, "Releasing hdl = %X", inp->mem_handle);
	rc = cam_mem_util_unmap(idx, CAM_SMMU_MAPPING_KERNEL);

	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_release_mem);

int cam_mem_mgr_reserve_memory_region(struct cam_mem_mgr_request_desc *inp,
	enum cam_smmu_region_id region,
	struct cam_mem_mgr_memory_desc *out)
{
	struct ion_handle *hdl;
	struct dma_buf *buf = NULL;
	int rc = 0;
	int ion_fd = -1;
	uint32_t heap_id;
	dma_addr_t iova = 0;
	size_t request_len = 0;
	uint32_t mem_handle;
	int32_t idx;
	int32_t smmu_hdl = 0;
	int32_t num_hdl = 0;

	if (!inp || !out) {
		CAM_ERR(CAM_CRM, "Invalid param(s)");
		return -EINVAL;
	}

	if (!inp->smmu_hdl) {
		CAM_ERR(CAM_CRM, "Invalid SMMU handle");
		return -EINVAL;
	}

	if (region != CAM_SMMU_REGION_SECHEAP) {
		CAM_ERR(CAM_CRM, "Only secondary heap supported");
		return -EINVAL;
	}

	heap_id = ION_HEAP(ION_SYSTEM_HEAP_ID) |
		ION_HEAP(ION_CAMERA_HEAP_ID);
	rc = cam_mem_util_get_dma_buf(inp->size,
		inp->align,
		heap_id,
		0,
		&hdl,
		&buf);

	if (rc) {
		CAM_ERR(CAM_CRM, "ION alloc failed for sec heap buffer");
		goto ion_fail;
	} else {
		CAM_DBG(CAM_CRM, "Got dma_buf = %pK, hdl = %pK", buf, hdl);
	}

	rc = cam_smmu_reserve_sec_heap(inp->smmu_hdl,
		buf,
		&iova,
		&request_len);

	if (rc) {
		CAM_ERR(CAM_CRM, "Reserving secondary heap failed");
		goto smmu_fail;
	}

	smmu_hdl = inp->smmu_hdl;
	num_hdl = 1;

	idx = cam_mem_get_slot();
	if (idx < 0) {
		rc = -ENOMEM;
		goto slot_fail;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	mem_handle = GET_MEM_HANDLE(idx, ion_fd);
	tbl.bufq[idx].fd = -1;
	tbl.bufq[idx].dma_buf = buf;
	tbl.bufq[idx].flags = inp->flags;
	tbl.bufq[idx].buf_handle = mem_handle;
	tbl.bufq[idx].kmdvaddr = 0;

	tbl.bufq[idx].vaddr = iova;

	tbl.bufq[idx].i_hdl = hdl;
	tbl.bufq[idx].len = request_len;
	tbl.bufq[idx].num_hdl = num_hdl;
	memcpy(tbl.bufq[idx].hdls, &smmu_hdl,
		sizeof(int32_t));
	tbl.bufq[idx].is_imported = false;
	mutex_unlock(&tbl.bufq[idx].q_lock);

	out->kva = 0;
	out->iova = (uint32_t)iova;
	out->smmu_hdl = smmu_hdl;
	out->mem_handle = mem_handle;
	out->len = request_len;
	out->region = region;

	return rc;

slot_fail:
	cam_smmu_release_sec_heap(smmu_hdl);
smmu_fail:
	ion_free(tbl.client, hdl);
ion_fail:
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_reserve_memory_region);

int cam_mem_mgr_free_memory_region(struct cam_mem_mgr_memory_desc *inp)
{
	int32_t idx;
	int rc;
	int32_t smmu_hdl;

	if (!inp) {
		CAM_ERR(CAM_CRM, "Invalid argument");
		return -EINVAL;
	}

	if (inp->region != CAM_SMMU_REGION_SECHEAP) {
		CAM_ERR(CAM_CRM, "Only secondary heap supported");
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(inp->mem_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_CRM, "Incorrect index extracted from mem handle");
		return -EINVAL;
	}

	if (!tbl.bufq[idx].active) {
		if (tbl.bufq[idx].vaddr == 0) {
			CAM_ERR(CAM_CRM, "buffer is released already");
			return 0;
		}
		CAM_ERR(CAM_CRM, "Released buffer state should be active");
		return -EINVAL;
	}

	if (tbl.bufq[idx].buf_handle != inp->mem_handle) {
		CAM_ERR(CAM_CRM,
			"Released buf handle not matching within table");
		return -EINVAL;
	}

	if (tbl.bufq[idx].num_hdl != 1) {
		CAM_ERR(CAM_CRM,
			"Sec heap region should have only one smmu hdl");
		return -ENODEV;
	}

	memcpy(&smmu_hdl, tbl.bufq[idx].hdls,
		sizeof(int32_t));
	if (inp->smmu_hdl != smmu_hdl) {
		CAM_ERR(CAM_CRM,
			"Passed SMMU handle doesn't match with internal hdl");
		return -ENODEV;
	}

	rc = cam_smmu_release_sec_heap(inp->smmu_hdl);
	if (rc) {
		CAM_ERR(CAM_CRM,
			"Sec heap region release failed");
		return -ENODEV;
	}

	CAM_DBG(CAM_CRM, "Releasing hdl = %X", inp->mem_handle);
	rc = cam_mem_util_unmap(idx, CAM_SMMU_MAPPING_KERNEL);
	if (rc)
		CAM_ERR(CAM_CRM, "unmapping secondary heap failed");

	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_free_memory_region);
