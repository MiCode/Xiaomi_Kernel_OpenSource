/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MDP_M4U_H__
#define __MDP_M4U_H__

#ifdef CONFIG_MTK_IOMMU_V2
#include "mach/mt_iommu.h"
#include <soc/mediatek/smi.h>
#endif
#include <ion_priv.h>

void mdp_ion_create(const char *name);
void mdp_ion_destroy(void);
int mdp_ion_get_mva(struct ion_handle *handle,
	unsigned long *mva, unsigned long fixed_mva, int port);
struct ion_handle *mdp_ion_import_handle(int fd);
void mdp_ion_free_handle(struct ion_handle *handle);
void mdp_ion_cache_flush(struct ion_handle *handle);

#endif
