/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SMMU_API_H_
#define _CAM_SMMU_API_H_

#include <linux/dma-direction.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/of_platform.h>
#include <linux/iommu.h>
#include <linux/random.h>
#include <linux/spinlock_types.h>
#include <linux/mutex.h>
#include <linux/msm_ion.h>

/*Enum for possible CAM SMMU operations */
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

enum cam_smmu_region_id {
	CAM_SMMU_REGION_FIRMWARE,
	CAM_SMMU_REGION_SHARED,
	CAM_SMMU_REGION_SCRATCH,
	CAM_SMMU_REGION_IO,
	CAM_SMMU_REGION_SECHEAP,
	CAM_SMMU_REGION_QDSS,
	CAM_SMMU_REGION_FWUNCACHED,
};

/**
 * @brief          : cam_smmu_pf_info
 *
 * @param domain   : Iommu domain received in iommu page fault handler
 * @param dev      : Device received in iommu page fault handler
 * @param iova     : IOVA where page fault occurred
 * @param flags    : Flags received in iommu page fault handler
 * @param token    : Userdata given during callback registration
 * @param buf_info : Closest mapped buffer info
 * @bid            : bus id
 * @pid            : unique id for hw group of ports
 * @mid            : port id of hw
 */

struct cam_smmu_pf_info {
	struct iommu_domain *domain;
	struct device        *dev;
	unsigned long        iova;
	int                  flags;
	void                *token;
	uint32_t             buf_info;
	uint32_t             bid;
	uint32_t             pid;
	uint32_t             mid;
};

/**
 * @brief            : Structure to store region information
 *
 * @param iova_start         : Start address of region
 * @param iova_len           : length of region
 * @param discard_iova_start : iova addr start from where should not be used
 * @param discard_iova_len   : length of discard iova region
 */
struct cam_smmu_region_info {
	dma_addr_t iova_start;
	size_t iova_len;
	dma_addr_t discard_iova_start;
	size_t discard_iova_len;
};

/**
 * @brief           : Gets an smmu handle
 *
 * @param identifier: Unique identifier to be used by clients which they
 *                    should get from device tree. CAM SMMU driver will
 *                    not enforce how this string is obtained and will
 *                    only validate this against the list of permitted
 *                    identifiers
 * @param handle_ptr: Based on the indentifier, CAM SMMU drivier will
 *                    fill the handle pointed by handle_ptr
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_get_handle(char *identifier, int *handle_ptr);

/**
 * @brief       : Performs IOMMU operations
 *
 * @param handle: Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 * @param op    : Operation to be performed. Can be either CAM_SMMU_ATTACH
 *                or CAM_SMMU_DETACH
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_ops(int handle, enum cam_smmu_ops_param op);

/**
 * @brief       : Maps user space IOVA for calling driver
 *
 * @param handle: Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 * @param ion_fd: ION handle identifying the memory buffer.
 * @param dmabuf: DMA buf handle identifying the memory buffer.
 * @param dis_delayed_unmap: Whether to disable Delayed Unmap feature
 *                           for this mapping
 * @dir         : Mapping direction: which will traslate toDMA_BIDIRECTIONAL,
 *                DMA_TO_DEVICE or DMA_FROM_DEVICE
 * @dma_addr    : Pointer to physical address where mapped address will be
 *                returned if region_id is CAM_SMMU_REGION_IO. If region_id is
 *                CAM_SMMU_REGION_SHARED, dma_addr is used as an input parameter
 *                which specifies the cpu virtual address to map.
 * @len_ptr     : Length of buffer mapped returned by CAM SMMU driver.
 * @region_id   : Memory region identifier
 * @is_internal: Specifies if this buffer is kernel allocated.
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_map_user_iova(int handle, int ion_fd, struct dma_buf *dmabuf,
	bool dis_delayed_unmap, enum cam_smmu_map_dir dir, dma_addr_t *dma_addr, size_t *len_ptr,
	enum cam_smmu_region_id region_id, bool is_internal);

/**
 * @brief        : Maps kernel space IOVA for calling driver
 *
 * @param handle : Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 * @param buf    : dma_buf allocated for kernel usage in mem_mgr
 * @dir          : Mapping direction: which will traslate toDMA_BIDIRECTIONAL,
 *                 DMA_TO_DEVICE or DMA_FROM_DEVICE
 * @dma_addr     : Pointer to physical address where mapped address will be
 *                 returned if region_id is CAM_SMMU_REGION_IO. If region_id is
 *                 CAM_SMMU_REGION_SHARED, dma_addr is used as an input
 *                 parameter which specifies the cpu virtual address to map.
 * @len_ptr      : Length of buffer mapped returned by CAM SMMU driver.
 * @region_id    : Memory region identifier
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_map_kernel_iova(int handle,
	struct dma_buf *buf, enum cam_smmu_map_dir dir,
	dma_addr_t *dma_addr, size_t *len_ptr,
	enum cam_smmu_region_id region_id);

/**
 * @brief       : Unmaps user space IOVA for calling driver
 *
 * @param handle: Handle to identify the CAMSMMU client (VFE, CPP, FD etc.)
 * @param ion_fd: ION handle identifying the memory buffer.
 * @param dma_buf: DMA Buf handle identifying the memory buffer.
 * @param region_id: Region id from which to unmap buffer.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_unmap_user_iova(int handle,
	int ion_fd, struct dma_buf *dma_buf, enum cam_smmu_region_id region_id);

/**
 * @brief       : Unmaps kernel IOVA for calling driver
 *
 * @param handle: Handle to identify the CAMSMMU client (VFE, CPP, FD etc.)
 * @param buf   : dma_buf allocated for the kernel
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_unmap_kernel_iova(int handle,
	struct dma_buf *buf, enum cam_smmu_region_id region_id);

/**
 * @brief          : Allocates a scratch buffer
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
 *                   IOMMU_WRITE or a bit mask of both.
 * @param paddr_ptr: Device virtual address that the client device will be
 *                   able to read from/write to
 * @param virt_len : Virtual length of the scratch buffer
 * @param phys_len : Physical length of the scratch buffer
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int cam_smmu_get_scratch_iova(int handle,
	enum cam_smmu_map_dir dir,
	dma_addr_t *paddr_ptr,
	size_t virt_len,
	size_t phys_len);

/**
 * @brief          : Frees a scratch buffer
 *
 * This function frees a scratch buffer and releases the corresponding SMMU
 * mappings.
 *
 * @param handle   : Handle to identify the CAMSMMU client (IFE, ICP, etc.)
 * @param paddr    : Device virtual address of client's scratch buffer that
 *                   will be freed.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */

int cam_smmu_put_scratch_iova(int handle,
	dma_addr_t paddr);

/**
 *@brief        : Destroys an smmu handle
 *
 * @param handle: Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_destroy_handle(int handle);

/**
 * @brief       : Returns if context bank identified by handle has a shared region
 *
 * @param handle: Handle to identify the context bank
 * @return      : True if context banks supports shared region, false otherwise
 * @note        : Currently, only ICP context banks support shared regions.
 */
bool cam_smmu_supports_shared_region(int handle);

/**
 * @brief       : Registers smmu fault handler for client
 *
 * @param handle: Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 * @param handler_cb: It is triggered in IOMMU page fault
 * @param token: It is input param when trigger page fault handler
 */
void cam_smmu_set_client_page_fault_handler(int handle,
	void (*handler_cb)(struct cam_smmu_pf_info  *pf_info), void *token);

/**
 * @brief       : Unregisters smmu fault handler for client
 *
 * @param handle: Handle to identify the CAM SMMU client (VFE, CPP, FD etc.)
 * @param token: It is input param when trigger page fault handler
 */
void cam_smmu_unset_client_page_fault_handler(int handle, void *token);

/**
 * @brief Maps memory from an ION fd into IOVA space
 *
 * @param handle: SMMU handle identifying the context bank to map to
 * @param ion_fd: ION fd of memory to map to
 * @param dma_buf: DMA buf of memory to map to
 * @param paddr_ptr: Pointer IOVA address that will be returned
 * @param len_ptr: Length of memory mapped
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_get_iova(int handle, int ion_fd, struct dma_buf *dma_buf,
	dma_addr_t *paddr_ptr, size_t *len_ptr);

/**
 * @brief Maps memory from an ION fd into IOVA space
 *
 * @param handle: SMMU handle identifying the secure context bank to map to
 * @param ion_fd: ION fd of memory to map to
 * @param dma_buf: DMA Buf of memory to map to
 * @param paddr_ptr: Pointer IOVA address that will be returned
 * @param len_ptr: Length of memory mapped
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_get_stage2_iova(int handle, int ion_fd, struct dma_buf *dma_buf,
	dma_addr_t *paddr_ptr, size_t *len_ptr);

/**
 * @brief Unmaps memory from context bank
 *
 * @param handle: SMMU handle identifying the context bank
 * @param ion_fd: ION fd of memory to unmap
 * @param dma_buf: DMA Buf of memory to unmap
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_put_iova(int handle, int ion_fd, struct dma_buf *dma_buf);

/**
 * @brief Maps secure memory for SMMU handle
 *
 * @param handle: SMMU handle identifying secure context bank
 * @param ion_fd: ION fd to map securely
 * @param dmabuf: DMA buf to map securely
 * @param dir: DMA Direction for the mapping
 * @param dma_addr: Returned IOVA address after mapping
 * @param len_ptr: Length of memory mapped
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_map_stage2_iova(int handle, int ion_fd, struct dma_buf *dmabuf,
	enum cam_smmu_map_dir dir, dma_addr_t *dma_addr, size_t *len_ptr);

/**
 * @brief Unmaps secure memopry for SMMU handle
 *
 * @param handle: SMMU handle identifying secure context bank
 * @param ion_fd: ION fd to unmap
 * @param dma_buf: DMA Buf to unmap
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_unmap_stage2_iova(int handle, int ion_fd, struct dma_buf *dma_buf);

/**
 * @brief Allocates firmware for context bank
 *
 * @param smmu_hdl: SMMU handle identifying context bank
 * @param iova: IOVA address of allocated firmware
 * @param kvaddr: CPU mapped address of allocated firmware
 * @param len: Length of allocated firmware memory
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_alloc_firmware(int32_t smmu_hdl,
	dma_addr_t *iova,
	uintptr_t *kvaddr,
	size_t *len);

/**
 * @brief Deallocates firmware memory for context bank
 *
 * @param smmu_hdl: SMMU handle identifying the context bank
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_dealloc_firmware(int32_t smmu_hdl);

/**
 * @brief Gets region information specified by smmu handle and region id
 *
 * @param smmu_hdl: SMMU handle identifying the context bank
 * @param region_id: Region id for which information is desired
 * @param region_info: Struct populated with region information
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_get_region_info(int32_t smmu_hdl,
	enum cam_smmu_region_id region_id,
	struct cam_smmu_region_info *region_info);

/**
 * @brief Reserves a region with buffer
 *
 * @param region: Region id
 * @param smmu_hdl: SMMU handle identifying the context bank
 * @param iova: IOVA of secondary heap after reservation has completed
 * @param buf: Allocated dma_buf for secondary heap
 * @param request_len: Length of secondary heap after reservation has completed
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_reserve_buf_region(enum cam_smmu_region_id region,
	int32_t smmu_hdl, struct dma_buf *buf,
	dma_addr_t *iova, size_t *request_len);

/**
 * @brief Releases buffer in reserved region
 *
 * @param region: Region id
 * @param smmu_hdl: SMMU handle identifying the context bank
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_release_buf_region(enum cam_smmu_region_id region,
	int32_t smmu_hdl);

/**
 * @brief Allocates qdss for context bank
 *
 * @param smmu_hdl: SMMU handle identifying context bank
 * @param iova: IOVA address of allocated qdss
 * @param len: Length of allocated qdss memory
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_alloc_qdss(int32_t smmu_hdl,
	dma_addr_t *iova,
	size_t *len);

/**
 * @brief Deallocates qdss memory for context bank
 *
 * @param smmu_hdl: SMMU handle identifying the context bank
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_dealloc_qdss(int32_t smmu_hdl);

/**
 * @brief Get start addr & len of I/O region for a given cb
 *
 * @param smmu_hdl: SMMU handle identifying the context bank
 * @param iova: IOVA address of allocated I/O region
 * @param len: Length of allocated I/O memory
 * @param discard_iova_start: Start address of io space to discard
 * @param discard_iova_len: Length of io space to discard
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_smmu_get_io_region_info(int32_t smmu_hdl,
	dma_addr_t *iova, size_t *len,
	dma_addr_t *discard_iova_start, size_t *discard_iova_len);

/**
 * @brief : API to reset the call context bank page fault count
 *          This should be done on the starting of new camera open
 * @return void.
 */
void cam_smmu_reset_cb_page_fault_cnt(void);

/**
 * @brief : API to register SMMU hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_smmu_init_module(void);

/**
 * @brief : API to remove SMMU Hw from platform framework.
 */
void cam_smmu_exit_module(void);

/**
 * @brief : API to determine whether to force all allocations to CACHED
 */
int cam_smmu_need_force_alloc_cached(bool *force_alloc_cached);

/**
 * @brief : API to determine whether padding is needed for shared buffers
 */
bool cam_smmu_need_shared_buffer_padding(void);

/**
 * @brief : API to determine whether certain HW is 36-bit memory addressable
 */
bool cam_smmu_is_expanded_memory(void);

#endif /* _CAM_SMMU_API_H_ */
