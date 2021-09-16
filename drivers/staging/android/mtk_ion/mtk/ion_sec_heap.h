/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ION_SEC_HEAP_H__
#define __ION_SEC_HEAP_H__

#include <linux/dma-buf.h>
#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
#include "trusted_mem_api.h"
#endif
#include "ion_drv.h"

struct ion_sec_buffer_info {
	struct mutex lock;/*mutex lock on secure buffer*/
	int module_id;
	unsigned int security;
	unsigned int coherent;
	void *VA;
	unsigned int MVA;
	unsigned int FIXED_MVA;
	unsigned long iova_start;
	unsigned long iova_end;
	ion_phys_addr_t priv_phys;
	struct ion_mm_buf_debug_info dbg_info;
	pid_t pid;
};

#ifdef CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM
enum TRUSTED_MEM_REQ_TYPE ion_get_trust_mem_type(struct dma_buf *dmabuf);

/*
 * return: trustmem type.
 *      -1: nomal buffer
 *     >=0: valid tmem_type
 *
 * handle: input ion handle
 * sec: used for return.
 *      0: protected buffer;
 *      1: secure buffer;
 *     <0: error buffer;
 * iommu_sec_id: used for return
 *      <0: error buffer;
 *     >=0: valid sec_id
 * sec_hdl: used for return
 *       0: normal buffer, no secure handle
 *  others: secure handle
 */
enum TRUSTED_MEM_REQ_TYPE
ion_hdl2sec_type(struct ion_handle *handle,
		 int *sec, int *iommu_sec_id,
		 ion_phys_addr_t *sec_hdl);

/*
 * return: trustmem type.
 *      -1: nomal buffer
 *     >=0: valid tmem_type
 *
 * fd: input ion buffer fd
 *
 * sec: used for return.
 *      0: protected buffer;
 *      1: secure buffer;
 *     <0: error buffer;
 *
 * iommu_sec_id: used for return
 *      <0: error buffer;
 *     >=0: valid sec_id
 *
 * sec_hdl: used for return
 *       0: normal buffer, no secure handle
 *  others: secure handle
 */
enum TRUSTED_MEM_REQ_TYPE
ion_fd2sec_type(int fd, int *sec, int *iommu_sec_id,
		ion_phys_addr_t *sec_hdl);

#endif
#endif
