/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_MEM_MGR_H_
#define _CAM_MEM_MGR_H_

#include <linux/mutex.h>
#include <linux/dma-buf.h>
#if IS_REACHABLE(CONFIG_DMABUF_HEAPS)
#include <linux/dma-heap.h>
#endif
#include <media/cam_req_mgr.h>
#include "cam_mem_mgr_api.h"

/* Enum for possible mem mgr states */
enum cam_mem_mgr_state {
	CAM_MEM_MGR_UNINITIALIZED,
	CAM_MEM_MGR_INITIALIZED,
};

/*Enum for possible SMMU operations */
enum cam_smmu_mapping_client {
	CAM_SMMU_MAPPING_USER,
	CAM_SMMU_MAPPING_KERNEL,
};

#ifdef CONFIG_CAM_PRESIL
struct cam_presil_dmabuf_params {
	int32_t fd_for_umd_daemon;
	uint32_t refcount;
};
#endif

/**
 * struct cam_mem_buf_queue
 *
 * @dma_buf:        pointer to the allocated dma_buf in the table
 * @q_lock:         mutex lock for buffer
 * @hdls:           list of mapped handles
 * @num_hdl:        number of handles
 * @fd:             file descriptor of buffer
 * @i_ino:          inode number of this dmabuf. Uniquely identifies a buffer
 * @buf_handle:     unique handle for buffer
 * @align:          alignment for allocation
 * @len:            size of buffer
 * @flags:          attributes of buffer
 * @vaddr:          IOVA of buffer
 * @kmdvaddr:       Kernel virtual address
 * @active:         state of the buffer
 * @is_imported:    Flag indicating if buffer is imported from an FD in user space
 * @is_internal:    Flag indicating kernel allocated buffer
 * @timestamp:      Timestamp at which this entry in tbl was made
 * @presil_params:  Parameters specific to presil environment
 */
struct cam_mem_buf_queue {
	struct dma_buf *dma_buf;
	struct mutex q_lock;
	int32_t hdls[CAM_MEM_MMU_MAX_HANDLE];
	int32_t num_hdl;
	int32_t fd;
	unsigned long i_ino;
	int32_t buf_handle;
	int32_t align;
	size_t len;
	uint32_t flags;
	dma_addr_t vaddr;
	uintptr_t kmdvaddr;
	bool active;
	bool is_imported;
	bool is_internal;
	struct timespec64 timestamp;

#ifdef CONFIG_CAM_PRESIL
	struct cam_presil_dmabuf_params presil_params;
#endif
};

/**
 * struct cam_mem_table
 *
 * @m_lock: mutex lock for table
 * @bitmap: bitmap of the mem mgr utility
 * @bits: max bits of the utility
 * @bufq: array of buffers
 * @dentry: Debugfs entry
 * @alloc_profile_enable: Whether to enable alloc profiling
 * @dbg_buf_idx: debug buffer index to get usecases info
 * @force_cache_allocs: Force all internal buffer allocations with cache
 * @need_shared_buffer_padding: Whether padding is needed for shared buffer
 *                              allocations.
 * @system_heap: Handle to system heap
 * @system_uncached_heap: Handle to system uncached heap
 * @camera_heap: Handle to camera heap
 * @camera_uncached_heap: Handle to camera uncached heap
 * @secure_display_heap: Handle to secure display heap
 */
struct cam_mem_table {
	struct mutex m_lock;
	void *bitmap;
	size_t bits;
	struct cam_mem_buf_queue bufq[CAM_MEM_BUFQ_MAX];
	struct dentry *dentry;
	bool alloc_profile_enable;
	size_t dbg_buf_idx;
	bool force_cache_allocs;
	bool need_shared_buffer_padding;
#if IS_REACHABLE(CONFIG_DMABUF_HEAPS)
	struct dma_heap *system_heap;
	struct dma_heap *system_uncached_heap;
	struct dma_heap *camera_heap;
	struct dma_heap *camera_uncached_heap;
	struct dma_heap *secure_display_heap;
#endif

};

/**
 * struct cam_mem_table_mini_dump
 *
 * @bufq: array of buffers
 * @dbg_buf_idx: debug buffer index to get usecases info
 * @alloc_profile_enable: Whether to enable alloc profiling
 * @dbg_buf_idx: debug buffer index to get usecases info
 * @force_cache_allocs: Force all internal buffer allocations with cache
 * @need_shared_buffer_padding: Whether padding is needed for shared buffer
 *                              allocations.
 */
struct cam_mem_table_mini_dump {
	struct cam_mem_buf_queue bufq[CAM_MEM_BUFQ_MAX];
	size_t dbg_buf_idx;
	bool   alloc_profile_enable;
	bool   force_cache_allocs;
	bool   need_shared_buffer_padding;
};

/**
 * @brief: Allocates and maps buffer
 *
 * @cmd:   Allocation information
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_mem_mgr_alloc_and_map(struct cam_mem_mgr_alloc_cmd *cmd);

/**
 * @brief: Releases a buffer reference
 *
 * @cmd:   Buffer release information
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_mem_mgr_release(struct cam_mem_mgr_release_cmd *cmd);

/**
 * @brief Maps a buffer
 *
 * @cmd: Buffer mapping information
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_mem_mgr_map(struct cam_mem_mgr_map_cmd *cmd);

/**
 * @brief: Perform cache ops on the buffer
 *
 * @cmd:   Cache ops information
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_mem_mgr_cache_ops(struct cam_mem_cache_ops_cmd *cmd);

/**
 * @brief: Initializes the memory manager
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_mem_mgr_init(void);

/**
 * @brief:  Tears down the memory manager
 *
 * @return None
 */
void cam_mem_mgr_deinit(void);

/**
 * @brief: Copy buffer content to presil mem for all buffers of
 *       iommu handle
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_mem_mgr_send_all_buffers_to_presil(int32_t iommu_hdl);

/**
 * @brief: Copy buffer content of single buffer to presil
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_mem_mgr_send_buffer_to_presil(int32_t iommu_hdl, int32_t buf_handle);

/**
 * @brief: Copy back buffer content of single buffer from
 *       presil
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_mem_mgr_retrieve_buffer_from_presil(int32_t buf_handle,
	uint32_t buf_size, uint32_t offset, int32_t iommu_hdl);
#endif /* _CAM_MEM_MGR_H_ */
