/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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
