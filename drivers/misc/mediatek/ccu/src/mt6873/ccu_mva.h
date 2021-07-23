/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu.h"
#include <dt-bindings/memory/mt6873-larb-port.h>
#else
#include "m4u.h"
#endif

int ccu_ion_init(void);
int ccu_ion_uninit(void);
int ccu_allocate_mva(uint32_t *mva, void *va,
	struct ion_handle **handle, int buffer_size);
int ccu_deallocate_mva(struct ion_handle **handle);
struct ion_handle *ccu_ion_import_handle(int fd);
void ccu_ion_free_import_handle(struct ion_handle *handle);

#endif
