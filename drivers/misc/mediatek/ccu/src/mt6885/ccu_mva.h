/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
#include <dt-bindings/memory/mt6885-larb-port.h>
#else
#include "m4u.h"
#endif

struct CcuMemInfo {
	int shareFd;
	char *va;
	unsigned int align_mva;
	unsigned int mva;
	unsigned int size;
	unsigned int occupiedSize;
	bool cached;
	bool ion_log;
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
