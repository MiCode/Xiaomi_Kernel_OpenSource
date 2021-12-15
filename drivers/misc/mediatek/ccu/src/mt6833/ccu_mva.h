/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _CCU_MVA_H_
#define _CCU_MVA_H_

#include "mtk_ion.h"
#include "ion_drv.h"
#include <linux/iommu.h>
#if defined(CONFIG_MTK_IOMMU_V2)
#include "mtk_iommu.h"
#include "mach/pseudo_m4u.h"
#elif defined(CONFIG_MTK_IOMMU)
#include "mtk_iommu.h"
#include <dt-bindings/memory/mt6833-larb-port.h>
#else
#include "m4u.h"
#endif

struct CcuMemInfo {
	int shareFd;
	unsigned int align_mva;
	unsigned int mva;
	unsigned int size;
	unsigned int occupiedSize;
	unsigned int cached;
	bool ion_log;
	char *va;
};

struct CcuMemHandle {
	struct ion_handle *ionHandleKd;
	struct CcuMemInfo meminfo;
};

int ccu_ion_init(void);
int ccu_ion_uninit(void);
int ccu_allocate_mva(uint32_t *mva, void *va,
	struct ion_handle **handle, int buffer_size);
int ccu_deallocate_mva(struct ion_handle **handle);
struct ion_handle *ccu_ion_import_handle(int fd);
void ccu_ion_free_import_handle(struct ion_handle *handle);
int ccu_config_m4u_port(void);
int ccu_allocate_mem(struct CcuMemHandle *memHandle, int size, bool cached);
int ccu_deallocate_mem(struct CcuMemHandle *memHandle);

struct CcuMemInfo *ccu_get_binary_memory(void);

#endif
