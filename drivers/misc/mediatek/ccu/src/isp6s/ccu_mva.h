/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CCU_MVA_H_
#define _CCU_MVA_H_

#include <linux/iommu.h>
#if defined(CONFIG_MTK_IOMMU_V2)
#include "mtk_iommu.h"
#include "mach/pseudo_m4u.h"
#elif defined(CONFIG_MTK_IOMMU)
#include "mtk_iommu.h"
#include <dt-bindings/memory/mt6873-larb-port.h>
#else

#endif
struct CcuMemInfo {
	int shareFd;
	char *va;
	unsigned int align_mva;
	unsigned int mva;
	unsigned int size;
	unsigned int occupiedSize;
	bool cached;
};
struct CcuMemHandle {
	struct ion_handle *ionHandleKd;
	struct CcuMemInfo meminfo;
	dma_addr_t  mva;
};

int ccu_allocate_mem(struct ccu_device_s *dev, struct CcuMemHandle *memHandle,
			 int size, bool cached);
int ccu_deallocate_mem(struct ccu_device_s *dev, struct CcuMemHandle *memHandle);
struct CcuMemInfo *ccu_get_binary_memory(void);
#endif
