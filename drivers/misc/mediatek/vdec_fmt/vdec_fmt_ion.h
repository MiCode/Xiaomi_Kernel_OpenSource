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

#ifndef __VDEC_FMT_ION_H__
#define __VDEC_FMT_ION_H__

#ifdef CONFIG_MTK_IOMMU_V2
#include "mach/mt_iommu.h"
#include <soc/mediatek/smi.h>
#endif
#include <ion_priv.h>

#define FMT_FD_RESERVE         3
struct ionmap {
	int fd;
	u64 iova;
};

void fmt_ion_create(const char *name);
void fmt_ion_destroy(void);
int fmt_ion_get_iova(struct ion_handle *handle,
	u64 *iova, int port);
struct ion_handle *fmt_ion_import_handle(int fd);
void fmt_ion_free_handle(struct ion_handle *handle);
void fmt_ion_cache_flush(struct ion_handle *handle);
u64 fmt_translate_fd(u64 fd, u32 offset, struct ionmap map[]);

#endif /*__VDEC_FMT_ION_H__*/
