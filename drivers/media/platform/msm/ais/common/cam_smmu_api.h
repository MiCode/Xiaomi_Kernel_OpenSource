/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_SMMU_API_H_
#define _CAM_SMMU_API_H_

#include <linux/dma-direction.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <asm/dma-iommu.h>
#include <linux/dma-direction.h>
#include <linux/dma-attrs.h>
#include <linux/of_platform.h>
#include <linux/iommu.h>
#include <linux/random.h>
#include <linux/spinlock_types.h>
#include <linux/mutex.h>

/*
 * Enum for possible CAM SMMU operations
 */
enum cam_smmu_ops_param {
	CAM_SMMU_ATTACH,
	CAM_SMMU_DETACH,
	CAM_SMMU_VOTE,
	CAM_SMMU_DEVOTE,
	CAM_SMMU_OPS_INVALID
};

enum cam_smmu_map_dir {
	CAM_SMMU_MAP_READ,
	CAM_SMMU_MAP_WRITE,
	CAM_SMMU_MAP_RW,
	CAM_SMMU_MAP_INVALID
};

int cam_smmu_query_vaddr_in_range(int handle,
	unsigned long fault_addr, unsigned long *start_addr,
	unsigned long *end_addr, int *fd);

/**
 * @param identifier: Unique identifier to be used by clients which they
 *                    should get from device tree. CAM SMMU driver will
 *                    not enforce how this string is obtained and will
 *                    only validate this against the list of permitted
 *                    identifiers
 * @param handle_ptr: Based on the indentifier, CAM SMMU drivier will
 *		      fill the handle pointed by handle_ptr
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_get_handle(char *identifier, int *handle_ptr);

/**
 * @param handle: Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 * @param op    : Operation to be performed. Can be either CAM_SMMU_ATTACH
 *                or CAM_SMMU_DETACH
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_ops(int handle, enum cam_smmu_ops_param op);

/**
 * @param handle: Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 * @param ion_fd: ION handle identifying the memory buffer.
 * @phys_addr   : Pointer to physical address where mapped address will be
 *                returned.
 * @dir         : Mapping direction: which will traslate toDMA_BIDIRECTIONAL,
 *                DMA_TO_DEVICE or DMA_FROM_DEVICE
 * @len         : Length of buffer mapped returned by CAM SMMU driver.
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_get_phy_addr(int handle,
				int ion_fd, enum cam_smmu_map_dir dir,
				dma_addr_t *dma_addr, size_t *len_ptr);

/**
 * @param handle: Handle to identify the CAMSMMU client (VFE, CPP, FD etc.)
 * @param ion_fd: ION handle identifying the memory buffer.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_put_phy_addr(int handle, int ion_fd);

/**
 * @brief	   : Allocates a scratch buffer
 *
 * This function allocates a scratch virtual buffer of length virt_len in the
 * device virtual address space mapped to phys_len physically contiguous bytes
 * in that device's SMMU.
 *
 * virt_len and phys_len are expected to be aligned to PAGE_SIZE and with each
 * other, otherwise -EINVAL is returned.
 *
 * -EINVAL will be returned if virt_len is less than phys_len.
 *
 * Passing a too large phys_len might also cause failure if that much size is
 * not available for allocation in a physically contiguous way.
 *
 * @param handle   : Handle to identify the CAMSMMU client (VFE, CPP, FD etc.)
 * @param dir      : Direction of mapping which will translate to IOMMU_READ
 *			IOMMU_WRITE or a bit mask of both.
 * @param paddr_ptr: Device virtual address that the client device will be
 *		able to read from/write to
 * @param virt_len : Virtual length of the scratch buffer
 * @param phys_len : Physical length of the scratch buffer
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int cam_smmu_get_phy_addr_scratch(int handle,
				  enum cam_smmu_map_dir dir,
				  dma_addr_t *paddr_ptr,
				  size_t virt_len,
				  size_t phys_len);

/**
 * @brief	   : Frees a scratch buffer
 *
 * This function frees a scratch buffer and releases the corresponding SMMU
 * mappings.
 *
 * @param handle   : Handle to identify the CAMSMMU client (VFE, CPP, FD etc.)
 *			IOMMU_WRITE or a bit mask of both.
 * @param paddr_ptr: Device virtual address of client's scratch buffer that
 *			will be freed.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int cam_smmu_put_phy_addr_scratch(int handle,
				  dma_addr_t paddr);

/**
 * @param handle: Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_destroy_handle(int handle);

/**
 * @return numger of client. Zero in case of error.
 */
int cam_smmu_get_num_of_clients(void);

/**
 * @param handle: Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 * @return Index of SMMU client. Nagative in case of error.
 */
int cam_smmu_find_index_by_handle(int hdl);

/**
 * @param handle: Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 * @param client_page_fault_handler: It is triggered in IOMMU page fault
 * @param token: It is input param when trigger page fault handler
 */
void cam_smmu_reg_client_page_fault_handler(int handle,
		void (*client_page_fault_handler)(struct iommu_domain *,
		struct device *, unsigned long,
		int, void*), void *token);

#endif /* _CAM_SMMU_API_H_ */
