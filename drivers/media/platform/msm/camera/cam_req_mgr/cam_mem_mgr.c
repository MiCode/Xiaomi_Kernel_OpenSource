/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/ion_kernel.h>
#include <linux/dma-buf.h>

#include "cam_req_mgr_util.h"
#include "cam_mem_mgr.h"
#include "cam_smmu_api.h"
#include "cam_debug_util.h"

static struct cam_mem_table tbl;
static atomic_t cam_mem_mgr_state = ATOMIC_INIT(CAM_MEM_MGR_UNINITIALIZED);

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

static int cam_mem_util_map_cpu_va(struct dma_buf *dmabuf,
	uintptr_t *vaddr,
	size_t *len)
{
	int i, j, rc;
	void *addr;

	/*
	 * dma_buf_begin_cpu_access() and dma_buf_end_cpu_access()
	 * need to be called in pair to avoid stability issue.
	 */
	rc = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	if (rc) {
		CAM_ERR(CAM_MEM, "dma begin access failed rc=%d", rc);
		return rc;
	}

	/*
	 * Code could be simplified if ION support of dma_buf_vmap is
	 * available. This workaround takes the avandaage that ion_alloc
	 * returns a virtually contiguous memory region, so we just need
	 * to _kmap each individual page and then only use the virtual
	 * address returned from the first call to _kmap.
	 */
	for (i = 0; i < PAGE_ALIGN(dmabuf->size) / PAGE_SIZE; i++) {
		addr = dma_buf_kmap(dmabuf, i);
		if (IS_ERR_OR_NULL(addr)) {
			CAM_ERR(CAM_MEM, "kernel map fail");
			for (j = 0; j < i; j++)
				dma_buf_kunmap(dmabuf,
					j,
					(void *)(*vaddr + (j * PAGE_SIZE)));
			*vaddr = 0;
			*len = 0;
			rc = -ENOSPC;
			goto fail;
		}
		if (i == 0)
			*vaddr = (uint64_t)addr;
	}

	*len = dmabuf->size;

	return 0;

fail:
	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	return rc;
}
static int cam_mem_util_unmap_cpu_va(struct dma_buf *dmabuf,
	uint64_t vaddr)
{
	int i, rc = 0, page_num;

	if (!dmabuf || !vaddr) {
		CAM_ERR(CAM_MEM, "Invalid input args %pK %llX", dmabuf, vaddr);
		return -EINVAL;
	}

	page_num = PAGE_ALIGN(dmabuf->size) / PAGE_SIZE;

	for (i = 0; i < page_num; i++) {
		dma_buf_kunmap(dmabuf, i,
			(void *)(vaddr + (i * PAGE_SIZE)));
	}

	/*
	 * dma_buf_begin_cpu_access() and
	 * dma_buf_end_cpu_access() need to be called in pair
	 * to avoid stability issue.
	 */
	rc = dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	if (rc) {
		CAM_ERR(CAM_MEM, "Failed in end cpu access, dmabuf=%pK",
			dmabuf);
		return rc;
	}

	return rc;
}

int cam_mem_mgr_init(void)
{
	int i;
	int bitmap_size;

	memset(tbl.bufq, 0, sizeof(tbl.bufq));

	bitmap_size = BITS_TO_LONGS(CAM_MEM_BUFQ_MAX) * sizeof(long);
	tbl.bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!tbl.bitmap)
		return -ENOMEM;

	tbl.bits = bitmap_size * BITS_PER_BYTE;
	bitmap_zero(tbl.bitmap, tbl.bits);
	/* We need to reserve slot 0 because 0 is invalid */
	set_bit(0, tbl.bitmap);

	for (i = 1; i < CAM_MEM_BUFQ_MAX; i++) {
		tbl.bufq[i].fd = -1;
		tbl.bufq[i].buf_handle = -1;
	}
	mutex_init(&tbl.m_lock);

	atomic_set(&cam_mem_mgr_state, CAM_MEM_MGR_INITIALIZED);

	return 0;
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

	*len_ptr = 0;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

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
	if (rc) {
		CAM_ERR(CAM_MEM,
			"fail to map buf_hdl:0x%x, mmu_hdl: 0x%x for fd:%d",
			buf_handle, mmu_handle, tbl.bufq[idx].fd);
		goto handle_mismatch;
	}

	CAM_DBG(CAM_MEM,
		"handle:0x%x fd:%d iova_ptr:%pK len_ptr:%llu",
		mmu_handle, tbl.bufq[idx].fd, iova_ptr, *len_ptr);
handle_mismatch:
	mutex_unlock(&tbl.bufq[idx].q_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_get_io_buf);

int cam_mem_get_cpu_buf(int32_t buf_handle, uintptr_t *vaddr_ptr, size_t *len)
{
	int rc = 0;
	int idx;
	struct dma_buf *dmabuf = NULL;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!buf_handle || !vaddr_ptr || !len)
		return -EINVAL;

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0)
		return -EINVAL;

	mutex_lock(&tbl.bufq[idx].q_lock);
	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_MEM, "idx: %d not active", idx);
		rc = -EPERM;
		goto end;
	}

	if (buf_handle != tbl.bufq[idx].buf_handle) {
		CAM_ERR(CAM_MEM, "idx: %d Invalid buf handle %d",
				idx, buf_handle);
		rc = -EINVAL;
		goto end;
	}

	if (!(tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS)) {
		CAM_ERR(CAM_MEM, "idx: %d Invalid flag 0x%x",
					idx, tbl.bufq[idx].flags);
		rc = -EINVAL;
		goto end;
	}

	if (tbl.bufq[idx].kmdvaddr) {
		dmabuf = tbl.bufq[idx].dma_buf;
		if (!dmabuf) {
			CAM_ERR(CAM_MEM, "Invalid DMA buffer pointer");
			rc = -EINVAL;
			goto end;
		}
		rc = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
		if (rc) {
			CAM_ERR(CAM_MEM, "dma begin access failed rc=%d", rc);
			goto end;
		}
	} else {
		CAM_ERR(CAM_MEM, "Invalid kmdvaddr");
		rc = -EINVAL;
		goto end;
	}

	*vaddr_ptr = tbl.bufq[idx].kmdvaddr;
	*len = tbl.bufq[idx].len;

end:
	mutex_unlock(&tbl.bufq[idx].q_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_get_cpu_buf);

int cam_mem_put_cpu_buf(int32_t buf_handle)
{
	int rc = 0;
	int idx;
	struct dma_buf *dmabuf = NULL;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!buf_handle)
		return -EINVAL;

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0)
		return -EINVAL;

	mutex_lock(&tbl.bufq[idx].q_lock);
	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_MEM, "idx: %d not active", idx);
		rc = -EPERM;
		goto end;
	}

	if (buf_handle != tbl.bufq[idx].buf_handle) {
		CAM_ERR(CAM_MEM, "idx: %d Invalid buf handle %d",
				idx, buf_handle);
		rc = -EINVAL;
		goto end;
	}

	dmabuf = tbl.bufq[idx].dma_buf;
	if (!dmabuf) {
		CAM_ERR(CAM_CRM, "Invalid DMA buffer pointer");
		rc = -EINVAL;
		goto end;
	}

	if ((tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS) &&
		(tbl.bufq[idx].kmdvaddr)) {
		rc = dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
		if (rc)
			CAM_ERR(CAM_MEM, "dma begin access failed rc=%d", rc);
	} else {
		CAM_ERR(CAM_MEM, "Invalid buf flag");
		rc = -EINVAL;
	}
end:
	mutex_unlock(&tbl.bufq[idx].q_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_put_cpu_buf);

int cam_mem_mgr_cache_ops(struct cam_mem_cache_ops_cmd *cmd)
{
	int rc = 0, idx;
	uint32_t cache_dir;
	unsigned long dmabuf_flag = 0;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!cmd)
		return -EINVAL;

	idx = CAM_MEM_MGR_GET_HDL_IDX(cmd->buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0)
		return -EINVAL;

	mutex_lock(&tbl.bufq[idx].q_lock);

	if (!tbl.bufq[idx].active) {
		rc = -EINVAL;
		goto end;
	}

	if (cmd->buf_handle != tbl.bufq[idx].buf_handle) {
		rc = -EINVAL;
		goto end;
	}

	rc = dma_buf_get_flags(tbl.bufq[idx].dma_buf, &dmabuf_flag);
	if (rc) {
		CAM_ERR(CAM_MEM, "cache get flags failed %d", rc);
		goto end;
	}

	if (dmabuf_flag & ION_FLAG_CACHED) {
		switch (cmd->mem_cache_ops) {
		case CAM_MEM_CLEAN_CACHE:
			cache_dir = DMA_TO_DEVICE;
			break;
		case CAM_MEM_INV_CACHE:
			cache_dir = DMA_FROM_DEVICE;
			break;
		case CAM_MEM_CLEAN_INV_CACHE:
			cache_dir = DMA_BIDIRECTIONAL;
			break;
		default:
			CAM_ERR(CAM_MEM,
				"invalid cache ops :%d", cmd->mem_cache_ops);
			rc = -EINVAL;
			goto end;
		}
	} else {
		CAM_DBG(CAM_MEM, "BUF is not cached");
		goto end;
	}

	rc = dma_buf_begin_cpu_access(tbl.bufq[idx].dma_buf,
		(cmd->mem_cache_ops == CAM_MEM_CLEAN_INV_CACHE) ?
		DMA_BIDIRECTIONAL : DMA_TO_DEVICE);
	if (rc) {
		CAM_ERR(CAM_MEM, "dma begin access failed rc=%d", rc);
		goto end;
	}

	rc = dma_buf_end_cpu_access(tbl.bufq[idx].dma_buf,
		cache_dir);
	if (rc) {
		CAM_ERR(CAM_MEM, "dma end access failed rc=%d", rc);
		goto end;
	}

end:
	mutex_unlock(&tbl.bufq[idx].q_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_cache_ops);

static int cam_mem_util_get_dma_buf(size_t len,
	unsigned int heap_id_mask,
	unsigned int flags,
	struct dma_buf **buf)
{
	int rc = 0;

	if (!buf) {
		CAM_ERR(CAM_MEM, "Invalid params");
		return -EINVAL;
	}

	*buf = ion_alloc(len, heap_id_mask, flags);
	if (IS_ERR_OR_NULL(*buf))
		return -ENOMEM;

	return rc;
}

static int cam_mem_util_get_dma_buf_fd(size_t len,
	size_t align,
	unsigned int heap_id_mask,
	unsigned int flags,
	struct dma_buf **buf,
	int *fd)
{
	struct dma_buf *dmabuf = NULL;
	int rc = 0;

	if (!buf || !fd) {
		CAM_ERR(CAM_MEM, "Invalid params, buf=%pK, fd=%pK", buf, fd);
		return -EINVAL;
	}

	*buf = ion_alloc(len, heap_id_mask, flags);
	if (IS_ERR_OR_NULL(*buf))
		return -ENOMEM;

	*fd = dma_buf_fd(*buf, O_CLOEXEC);
	if (*fd < 0) {
		CAM_ERR(CAM_MEM, "get fd fail, *fd=%d", *fd);
		rc = -EINVAL;
		goto get_fd_fail;
	}

	/*
	 * increment the ref count so that ref count becomes 2 here
	 * when we close fd, refcount becomes 1 and when we do
	 * dmap_put_buf, ref count becomes 0 and memory will be freed.
	 */
	dmabuf = dma_buf_get(*fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		CAM_ERR(CAM_MEM, "dma_buf_get failed, *fd=%d", *fd);
		rc = -EINVAL;
	}

	return rc;

get_fd_fail:
	dma_buf_put(*buf);
	return rc;
}

static int cam_mem_util_ion_alloc(struct cam_mem_mgr_alloc_cmd *cmd,
	struct dma_buf **dmabuf,
	int *fd)
{
	uint32_t heap_id;
	uint32_t ion_flag = 0;
	int rc;

	if ((cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE) &&
		(cmd->flags & CAM_MEM_FLAG_CDSP_OUTPUT)) {
		heap_id = ION_HEAP(ION_SECURE_DISPLAY_HEAP_ID);
		ion_flag |=
			ION_FLAG_SECURE | ION_FLAG_CP_CAMERA | ION_FLAG_CP_CDSP;
	} else if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE) {
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
		dmabuf,
		fd);

	return rc;
}


static int cam_mem_util_check_alloc_flags(struct cam_mem_mgr_alloc_cmd *cmd)
{
	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_MEM, "Num of mmu hdl exceeded maximum(%d)",
			CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE &&
		cmd->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		CAM_ERR(CAM_MEM, "Kernel mapping in secure mode not allowed");
		return -EINVAL;
	}

	return 0;
}

static int cam_mem_util_check_map_flags(struct cam_mem_mgr_map_cmd *cmd)
{
	if (!cmd->flags) {
		CAM_ERR(CAM_MEM, "Invalid flags");
		return -EINVAL;
	}

	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_MEM, "Num of mmu hdl %d exceeded maximum(%d)",
			cmd->num_hdl, CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE &&
		cmd->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		CAM_ERR(CAM_MEM,
			"Kernel mapping in secure mode not allowed, flags=0x%x",
			cmd->flags);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		CAM_ERR(CAM_MEM,
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
		CAM_ERR(CAM_MEM, "fail to map DMA direction, dir=%d", dir);
		return dir;
	}

	CAM_DBG(CAM_MEM,
		"map_hw_va : fd = %d,  flags = 0x%x, dir=%d, num_hdls=%d",
		fd, flags, dir, num_hdls);

	if (flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		for (i = 0; i < num_hdls; i++) {
			rc = cam_smmu_map_stage2_iova(mmu_hdls[i],
				fd,
				dir,
				hw_vaddr,
				len);

			if (rc < 0) {
				CAM_ERR(CAM_MEM,
					"Failed to securely map to smmu, i=%d, fd=%d, dir=%d, mmu_hdl=%d, rc=%d",
					i, fd, dir, mmu_hdls[i], rc);
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
				CAM_ERR(CAM_MEM,
					"Failed to map to smmu, i=%d, fd=%d, dir=%d, mmu_hdl=%d, region=%d, rc=%d",
					i, fd, dir, mmu_hdls[i], region, rc);
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
	struct dma_buf *dmabuf = NULL;
	int fd = -1;
	dma_addr_t hw_vaddr = 0;
	size_t len;
	uintptr_t kvaddr = 0;
	size_t klen;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_MEM, " Invalid argument");
		return -EINVAL;
	}
	len = cmd->len;

	rc = cam_mem_util_check_alloc_flags(cmd);
	if (rc) {
		CAM_ERR(CAM_MEM, "Invalid flags: flags = 0x%X, rc=%d",
			cmd->flags, rc);
		return rc;
	}

	rc = cam_mem_util_ion_alloc(cmd,
		&dmabuf,
		&fd);
	if (rc) {
		CAM_ERR(CAM_MEM,
			"Ion Alloc failed, len=%llu, align=%llu, flags=0x%x, num_hdl=%d",
			cmd->len, cmd->align, cmd->flags, cmd->num_hdl);
		return rc;
	}

	idx = cam_mem_get_slot();
	if (idx < 0) {
		CAM_ERR(CAM_MEM, "Failed in getting mem slot, idx=%d", idx);
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

		if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)
			region = CAM_SMMU_REGION_SECHEAP;

		rc = cam_mem_util_map_hw_va(cmd->flags,
			cmd->mmu_hdls,
			cmd->num_hdl,
			fd,
			&hw_vaddr,
			&len,
			region);

		if (rc) {
			CAM_ERR(CAM_MEM,
				"Failed in map_hw_va, flags=0x%x, fd=%d, region=%d, num_hdl=%d, rc=%d",
				cmd->flags, fd, region, cmd->num_hdl, rc);
			goto map_hw_fail;
		}
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].fd = fd;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].flags = cmd->flags;
	tbl.bufq[idx].buf_handle = GET_MEM_HANDLE(idx, fd);
	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)
		CAM_MEM_MGR_SET_SECURE_HDL(tbl.bufq[idx].buf_handle, true);

	if (cmd->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		rc = cam_mem_util_map_cpu_va(dmabuf, &kvaddr, &klen);
		if (rc) {
			CAM_ERR(CAM_MEM, "dmabuf: %pK mapping failed: %d",
				dmabuf, rc);
			goto map_kernel_fail;
		}
	}

	tbl.bufq[idx].kmdvaddr = kvaddr;
	tbl.bufq[idx].vaddr = hw_vaddr;
	tbl.bufq[idx].dma_buf = dmabuf;
	tbl.bufq[idx].len = cmd->len;
	tbl.bufq[idx].num_hdl = cmd->num_hdl;
	memcpy(tbl.bufq[idx].hdls, cmd->mmu_hdls,
		sizeof(int32_t) * cmd->num_hdl);
	tbl.bufq[idx].is_imported = false;
	mutex_unlock(&tbl.bufq[idx].q_lock);

	cmd->out.buf_handle = tbl.bufq[idx].buf_handle;
	cmd->out.fd = tbl.bufq[idx].fd;
	cmd->out.vaddr = 0;

	CAM_DBG(CAM_MEM,
		"fd=%d, flags=0x%x, num_hdl=%d, idx=%d, buf handle=%x, len=%zu",
		cmd->out.fd, cmd->flags, cmd->num_hdl, idx, cmd->out.buf_handle,
		tbl.bufq[idx].len);

	return rc;

map_kernel_fail:
	mutex_unlock(&tbl.bufq[idx].q_lock);
map_hw_fail:
	cam_mem_put_slot(idx);
slot_fail:
	dma_buf_put(dmabuf);
	return rc;
}

int cam_mem_mgr_map(struct cam_mem_mgr_map_cmd *cmd)
{
	int32_t idx;
	int rc;
	struct dma_buf *dmabuf;
	dma_addr_t hw_vaddr = 0;
	size_t len = 0;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!cmd || (cmd->fd < 0)) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_MEM, "Num of mmu hdl %d exceeded maximum(%d)",
			cmd->num_hdl, CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	rc = cam_mem_util_check_map_flags(cmd);
	if (rc) {
		CAM_ERR(CAM_MEM, "Invalid flags: flags = %X", cmd->flags);
		return rc;
	}

	dmabuf = dma_buf_get(cmd->fd);
	if (IS_ERR_OR_NULL((void *)(dmabuf))) {
		CAM_ERR(CAM_MEM, "Failed to import dma_buf fd");
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
		if (rc) {
			CAM_ERR(CAM_MEM,
				"Failed in map_hw_va, flags=0x%x, fd=%d, region=%d, num_hdl=%d, rc=%d",
				cmd->flags, cmd->fd, CAM_SMMU_REGION_IO,
				cmd->num_hdl, rc);
			goto map_fail;
		}
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

	tbl.bufq[idx].dma_buf = dmabuf;
	tbl.bufq[idx].len = len;
	tbl.bufq[idx].num_hdl = cmd->num_hdl;
	memcpy(tbl.bufq[idx].hdls, cmd->mmu_hdls,
		sizeof(int32_t) * cmd->num_hdl);
	tbl.bufq[idx].is_imported = true;
	mutex_unlock(&tbl.bufq[idx].q_lock);

	cmd->out.buf_handle = tbl.bufq[idx].buf_handle;
	cmd->out.vaddr = 0;

	CAM_DBG(CAM_MEM,
		"fd=%d, flags=0x%x, num_hdl=%d, idx=%d, buf handle=%x, len=%zu",
		cmd->fd, cmd->flags, cmd->num_hdl, idx, cmd->out.buf_handle,
		tbl.bufq[idx].len);

	return rc;

map_fail:
	dma_buf_put(dmabuf);
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
	int rc = 0;

	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index");
		return -EINVAL;
	}

	flags = tbl.bufq[idx].flags;
	mmu_hdls = tbl.bufq[idx].hdls;
	num_hdls = tbl.bufq[idx].num_hdl;
	fd = tbl.bufq[idx].fd;

	CAM_DBG(CAM_MEM,
		"unmap_hw_va : idx=%d, fd=%x, flags=0x%x, num_hdls=%d, client=%d",
		idx, fd, flags, num_hdls, client);

	if (flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		for (i = 0; i < num_hdls; i++) {
			rc = cam_smmu_unmap_stage2_iova(mmu_hdls[i], fd);
			if (rc < 0) {
				CAM_ERR(CAM_MEM,
					"Failed in secure unmap, i=%d, fd=%d, mmu_hdl=%d, rc=%d",
					i, fd, mmu_hdls[i], rc);
				goto unmap_end;
			}
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
				CAM_ERR(CAM_MEM,
					"invalid caller for unmapping : %d",
					client);
				rc = -EINVAL;
			}
			if (rc < 0) {
				CAM_ERR(CAM_MEM,
					"Failed in unmap, i=%d, fd=%d, mmu_hdl=%d, region=%d, rc=%d",
					i, fd, mmu_hdls[i], region, rc);
				goto unmap_end;
			}
		}
	}

	return rc;

unmap_end:
	CAM_ERR(CAM_MEM, "unmapping failed");
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
			CAM_DBG(CAM_MEM,
				"Buffer inactive at idx=%d, continuing", i);
			continue;
		} else {
			CAM_DBG(CAM_MEM,
			"Active buffer at idx=%d, possible leak needs unmapping",
			i);
			cam_mem_mgr_unmap_active_buf(i);
		}

		mutex_lock(&tbl.bufq[i].q_lock);
		if (tbl.bufq[i].dma_buf) {
			dma_buf_put(tbl.bufq[i].dma_buf);
			tbl.bufq[i].dma_buf = NULL;
		}
		tbl.bufq[i].fd = -1;
		tbl.bufq[i].flags = 0;
		tbl.bufq[i].buf_handle = -1;
		tbl.bufq[i].vaddr = 0;
		tbl.bufq[i].len = 0;
		memset(tbl.bufq[i].hdls, 0,
			sizeof(int32_t) * tbl.bufq[i].num_hdl);
		tbl.bufq[i].num_hdl = 0;
		tbl.bufq[i].dma_buf = NULL;
		tbl.bufq[i].active = false;
		tbl.bufq[i].kmdvaddr = 0;
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
	atomic_set(&cam_mem_mgr_state, CAM_MEM_MGR_UNINITIALIZED);
	cam_mem_mgr_cleanup_table();
	mutex_lock(&tbl.m_lock);
	bitmap_zero(tbl.bitmap, tbl.bits);
	kfree(tbl.bitmap);
	tbl.bitmap = NULL;
	mutex_unlock(&tbl.m_lock);
	mutex_destroy(&tbl.m_lock);
}

static int cam_mem_util_unmap(int32_t idx,
	enum cam_smmu_mapping_client client)
{
	int rc = 0;
	enum cam_smmu_region_id region = CAM_SMMU_REGION_SHARED;

	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index");
		return -EINVAL;
	}

	CAM_DBG(CAM_MEM, "Flags = %X idx %d", tbl.bufq[idx].flags, idx);

	mutex_lock(&tbl.m_lock);
	if ((!tbl.bufq[idx].active) &&
		(tbl.bufq[idx].vaddr) == 0) {
		CAM_WARN(CAM_MEM, "Buffer at idx=%d is already unmapped,",
			idx);
		mutex_unlock(&tbl.m_lock);
		return 0;
	}

	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS) {
		if (tbl.bufq[idx].dma_buf && tbl.bufq[idx].kmdvaddr) {
			rc = cam_mem_util_unmap_cpu_va(tbl.bufq[idx].dma_buf,
				tbl.bufq[idx].kmdvaddr);
			if (rc)
				CAM_ERR(CAM_MEM,
					"Failed, dmabuf=%pK, kmdvaddr=%llxK",
					tbl.bufq[idx].dma_buf,
					tbl.bufq[idx].kmdvaddr);
		}
	}

	/* SHARED flag gets precedence, all other flags after it */
	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		region = CAM_SMMU_REGION_SHARED;
	} else {
		if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_READ_WRITE)
			region = CAM_SMMU_REGION_IO;
	}

	if ((tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_READ_WRITE) ||
		(tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) ||
		(tbl.bufq[idx].flags & CAM_MEM_FLAG_PROTECTED_MODE)) {
		if (cam_mem_util_unmap_hw_va(idx, region, client))
			CAM_ERR(CAM_MEM, "Failed, dmabuf=%pK",
				tbl.bufq[idx].dma_buf);
		if (client == CAM_SMMU_MAPPING_KERNEL)
			tbl.bufq[idx].dma_buf = NULL;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].flags = 0;
	tbl.bufq[idx].buf_handle = -1;
	tbl.bufq[idx].vaddr = 0;
	memset(tbl.bufq[idx].hdls, 0,
		sizeof(int32_t) * CAM_MEM_MMU_MAX_HANDLE);

	CAM_DBG(CAM_MEM,
		"Ion buf at idx = %d freeing fd = %d, imported %d, dma_buf %pK",
		idx, tbl.bufq[idx].fd,
		tbl.bufq[idx].is_imported,
		tbl.bufq[idx].dma_buf);

	if (tbl.bufq[idx].dma_buf)
		dma_buf_put(tbl.bufq[idx].dma_buf);

	tbl.bufq[idx].fd = -1;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].is_imported = false;
	tbl.bufq[idx].len = 0;
	tbl.bufq[idx].num_hdl = 0;
	tbl.bufq[idx].active = false;
	tbl.bufq[idx].kmdvaddr = 0;
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

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(cmd->buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index %d extracted from mem handle",
			idx);
		return -EINVAL;
	}

	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_MEM, "Released buffer state should be active");
		return -EINVAL;
	}

	if (tbl.bufq[idx].buf_handle != cmd->buf_handle) {
		CAM_ERR(CAM_MEM,
			"Released buf handle %d not matching within table %d, idx=%d",
			cmd->buf_handle, tbl.bufq[idx].buf_handle, idx);
		return -EINVAL;
	}

	CAM_DBG(CAM_MEM, "Releasing hdl = %x, idx = %d", cmd->buf_handle, idx);
	rc = cam_mem_util_unmap(idx, CAM_SMMU_MAPPING_USER);

	return rc;
}

int cam_mem_mgr_request_mem(struct cam_mem_mgr_request_desc *inp,
	struct cam_mem_mgr_memory_desc *out)
{
	struct dma_buf *buf = NULL;
	int ion_fd = -1;
	int rc = 0;
	uint32_t heap_id;
	int32_t ion_flag = 0;
	uintptr_t kvaddr;
	dma_addr_t iova = 0;
	size_t request_len = 0;
	uint32_t mem_handle;
	int32_t idx;
	int32_t smmu_hdl = 0;
	int32_t num_hdl = 0;

	enum cam_smmu_region_id region = CAM_SMMU_REGION_SHARED;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!inp || !out) {
		CAM_ERR(CAM_MEM, "Invalid params");
		return -EINVAL;
	}

	if (!(inp->flags & CAM_MEM_FLAG_HW_READ_WRITE ||
		inp->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS ||
		inp->flags & CAM_MEM_FLAG_CACHE)) {
		CAM_ERR(CAM_MEM, "Invalid flags for request mem");
		return -EINVAL;
	}

	if (inp->flags & CAM_MEM_FLAG_CACHE)
		ion_flag |= ION_FLAG_CACHED;
	else
		ion_flag &= ~ION_FLAG_CACHED;

	heap_id = ION_HEAP(ION_SYSTEM_HEAP_ID) |
		ION_HEAP(ION_CAMERA_HEAP_ID);

	rc = cam_mem_util_get_dma_buf(inp->size,
		heap_id,
		ion_flag,
		&buf);

	if (rc) {
		CAM_ERR(CAM_MEM, "ION alloc failed for shared buffer");
		goto ion_fail;
	} else {
		CAM_DBG(CAM_MEM, "Got dma_buf = %pK", buf);
	}

	/*
	 * we are mapping kva always here,
	 * update flags so that we do unmap properly
	 */
	inp->flags |= CAM_MEM_FLAG_KMD_ACCESS;
	rc = cam_mem_util_map_cpu_va(buf, &kvaddr, &request_len);
	if (rc) {
		CAM_ERR(CAM_MEM, "Failed to get kernel vaddr");
		goto map_fail;
	}

	if (!inp->smmu_hdl) {
		CAM_ERR(CAM_MEM, "Invalid SMMU handle");
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
		CAM_ERR(CAM_MEM, "SMMU mapping failed");
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
	cam_mem_util_unmap_cpu_va(buf, kvaddr);
map_fail:
	dma_buf_put(buf);
ion_fail:
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_request_mem);

int cam_mem_mgr_release_mem(struct cam_mem_mgr_memory_desc *inp)
{
	int32_t idx;
	int rc;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!inp) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(inp->mem_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index extracted from mem handle");
		return -EINVAL;
	}

	if (!tbl.bufq[idx].active) {
		if (tbl.bufq[idx].vaddr == 0) {
			CAM_ERR(CAM_MEM, "buffer is released already");
			return 0;
		}
		CAM_ERR(CAM_MEM, "Released buffer state should be active");
		return -EINVAL;
	}

	if (tbl.bufq[idx].buf_handle != inp->mem_handle) {
		CAM_ERR(CAM_MEM,
			"Released buf handle not matching within table");
		return -EINVAL;
	}

	CAM_DBG(CAM_MEM, "Releasing hdl = %X", inp->mem_handle);
	rc = cam_mem_util_unmap(idx, CAM_SMMU_MAPPING_KERNEL);

	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_release_mem);

int cam_mem_mgr_reserve_memory_region(struct cam_mem_mgr_request_desc *inp,
	enum cam_smmu_region_id region,
	struct cam_mem_mgr_memory_desc *out)
{
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

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!inp || !out) {
		CAM_ERR(CAM_MEM, "Invalid param(s)");
		return -EINVAL;
	}

	if (!inp->smmu_hdl) {
		CAM_ERR(CAM_MEM, "Invalid SMMU handle");
		return -EINVAL;
	}

	if (region != CAM_SMMU_REGION_SECHEAP) {
		CAM_ERR(CAM_MEM, "Only secondary heap supported");
		return -EINVAL;
	}

	heap_id = ION_HEAP(ION_SYSTEM_HEAP_ID) |
		ION_HEAP(ION_CAMERA_HEAP_ID);
	rc = cam_mem_util_get_dma_buf(inp->size,
		heap_id,
		0,
		&buf);

	if (rc) {
		CAM_ERR(CAM_MEM, "ION alloc failed for sec heap buffer");
		goto ion_fail;
	} else {
		CAM_DBG(CAM_MEM, "Got dma_buf = %pK", buf);
	}

	rc = cam_smmu_reserve_sec_heap(inp->smmu_hdl,
		buf,
		&iova,
		&request_len);

	if (rc) {
		CAM_ERR(CAM_MEM, "Reserving secondary heap failed");
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
	dma_buf_put(buf);
ion_fail:
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_reserve_memory_region);

int cam_mem_mgr_free_memory_region(struct cam_mem_mgr_memory_desc *inp)
{
	int32_t idx;
	int rc;
	int32_t smmu_hdl;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!inp) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	if (inp->region != CAM_SMMU_REGION_SECHEAP) {
		CAM_ERR(CAM_MEM, "Only secondary heap supported");
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(inp->mem_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index extracted from mem handle");
		return -EINVAL;
	}

	if (!tbl.bufq[idx].active) {
		if (tbl.bufq[idx].vaddr == 0) {
			CAM_ERR(CAM_MEM, "buffer is released already");
			return 0;
		}
		CAM_ERR(CAM_MEM, "Released buffer state should be active");
		return -EINVAL;
	}

	if (tbl.bufq[idx].buf_handle != inp->mem_handle) {
		CAM_ERR(CAM_MEM,
			"Released buf handle not matching within table");
		return -EINVAL;
	}

	if (tbl.bufq[idx].num_hdl != 1) {
		CAM_ERR(CAM_MEM,
			"Sec heap region should have only one smmu hdl");
		return -ENODEV;
	}

	memcpy(&smmu_hdl, tbl.bufq[idx].hdls,
		sizeof(int32_t));
	if (inp->smmu_hdl != smmu_hdl) {
		CAM_ERR(CAM_MEM,
			"Passed SMMU handle doesn't match with internal hdl");
		return -ENODEV;
	}

	rc = cam_smmu_release_sec_heap(inp->smmu_hdl);
	if (rc) {
		CAM_ERR(CAM_MEM,
			"Sec heap region release failed");
		return -ENODEV;
	}

	CAM_DBG(CAM_MEM, "Releasing hdl = %X", inp->mem_handle);
	rc = cam_mem_util_unmap(idx, CAM_SMMU_MAPPING_KERNEL);
	if (rc)
		CAM_ERR(CAM_MEM, "unmapping secondary heap failed");

	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_free_memory_region);
