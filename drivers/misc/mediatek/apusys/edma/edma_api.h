/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: JB Tsai <jb.tsai@mediatek.com>
 */

#ifndef __EDMA_API_H__
#define __EDMA_API_H__

#include "edma_ioctl.h"

int edma_set_external_with_normal(u8 *buf, u32 size,
					struct edma_normal *edma_normal);
int edma_set_external_with_fill(u8 *buf, u32 size,
					struct edma_fill *edma_fill);
int edma_set_external_with_numerical(u8 *buf, u32 size,
					struct edma_numerical *edma_numerical);
int edma_set_external_with_format(u8 *buf, u32 size,
					struct edma_format *edma_format);
int edma_set_external_with_compress(u8 *buf, u32 size,
					struct edma_compress *edma_compress);
int edma_set_external_with_decompress(u8 *buf, u32 size,
				struct edma_decompress *edma_decompress);
int edma_set_external_with_raw(u8 *buf, u32 size,
					struct edma_raw *edma_raw);

#endif /* __EDMA_API_H__ */
