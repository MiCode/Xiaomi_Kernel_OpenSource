/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_MEM_MGR_H_
#define _CAM_MEM_MGR_H_

#include <linux/mutex.h>
#include <linux/dma-buf.h>
#include <media/cam_req_mgr.h>
#include "cam_mem_mgr_api.h"

/*Enum for possible SMMU operations */
enum cam_smmu_mapping_client {
	CAM_SMMU_MAPPING_USER,
	CAM_SMMU_MAPPING_KERNEL,
};

/**
 * struct cam_mem_buf_queue
 *
 * @dma_buf:     pointer to the allocated dma_buf in the table
 * @q_lock:      mutex lock for buffer
 * @hdls:        list of mapped handles
 * @num_hdl:     number of handles
 * @fd:          file descriptor of buffer
 * @buf_handle:  unique handle for buffer
 * @align:       alignment for allocation
 * @len:         size of buffer
 * @flags:       attributes of buffer
 * @vaddr:       IOVA of buffer
 * @kmdvaddr:    Kernel virtual address
 * @active:      state of the buffer
 * @is_imported: Flag indicating if buffer is imported from an FD in user space
 * @is_internal: Flag indicating kernel allocated buffer
 * @timestamp:   Timestamp at which this entry in tbl was made
 */
struct cam_mem_buf_queue {
	struct dma_buf *dma_buf;
	struct mutex q_lock;
	int32_t hdls[CAM_MEM_MMU_MAX_HANDLE];
	int32_t num_hdl;
	int32_t fd;
	int32_t buf_handle;
	int32_t align;
	size_t len;
	uint32_t flags;
	uint64_t vaddr;
	uintptr_t kmdvaddr;
	bool active;
	bool is_imported;
	bool is_internal;
	struct timespec64 timestamp;
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

#endif /* _CAM_MEM_MGR_H_ */
