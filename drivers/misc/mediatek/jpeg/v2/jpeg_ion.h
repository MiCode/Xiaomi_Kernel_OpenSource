/*
 * Copyright (C) 2021 MediaTek Inc.
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

#ifndef __JPEG_ION_H__
#define __JPEG_ION_H__

#ifdef CONFIG_MTK_IOMMU_V2
#include "mach/mt_iommu.h"
#include <soc/mediatek/smi.h>
#endif
#include <ion_priv.h>


void jpg_ion_create(const char *name);
void jpg_ion_destroy(void);
int jpg_ion_get_iova(struct ion_handle *handle,
	u64 *iova, int port);
struct ion_handle *jpg_ion_import_handle(int fd);
struct ion_handle *jpg_ion_alloc_handle(size_t size, size_t align, unsigned int flags);
int jpg_ion_share_handle(struct ion_handle *handle);
void *jpg_ion_map_handle(struct ion_handle *handle);
void jpg_ion_unmap_handle(struct ion_handle *handle);
void jpg_ion_free_handle(struct ion_handle *handle);
void jpg_ion_cache_flush(struct ion_handle *handle);
u64 jpg_translate_fd(u64 fd, u32 offset, u32 port);

#endif /*__JPEG_ION_H__*/
