/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
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
