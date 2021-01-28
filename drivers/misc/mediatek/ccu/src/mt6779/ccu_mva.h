/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef _CCU_MVA_H_
#define _CCU_MVA_H_

#include "mtk_ion.h"
#include "ion_drv.h"
#include <linux/iommu.h>
#ifdef CONFIG_MTK_IOMMU
#include "mtk_iommu.h"
#include <dt-bindings/memory/mt6763-larb-port.h>
#else
#include "m4u.h"
#endif

int ccu_ion_init(void);
int ccu_ion_uninit(void);
int ccu_allocate_mva(uint32_t *mva, void *va, int buffer_size);
int ccu_deallocate_mva(uint32_t mva);
struct ion_handle *ccu_ion_import_handle(int fd);
void ccu_ion_free_import_handle(struct ion_handle *handle);

#endif
