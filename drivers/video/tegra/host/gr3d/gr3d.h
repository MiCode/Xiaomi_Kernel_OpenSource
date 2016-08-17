/*
 * drivers/video/tegra/host/gr3d/gr3d.h
 *
 * Tegra Graphics Host 3D
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_GR3D_GR3D_H
#define __NVHOST_GR3D_GR3D_H

#include "host1x/host1x_hwctx.h"
#include <linux/types.h>

/* Registers of 3D unit */

#define AR3D_PSEQ_QUAD_ID 0x545
#define AR3D_DW_MEMORY_OUTPUT_ADDRESS 0x904
#define AR3D_DW_MEMORY_OUTPUT_DATA 0x905
#define AR3D_FDC_CONTROL_0 0xa00
#define AR3D_FDC_CONTROL_0_RESET_VAL 0xe00
#define AR3D_FDC_CONTROL_0_INVALIDATE 1
#define AR3D_GSHIM_WRITE_MASK 0xb00
#define AR3D_GSHIM_READ_SELECT 0xb01
#define AR3D_GLOBAL_MEMORY_OUTPUT_READS 0xe40
#define AR3D_PIPEALIAS_DW_MEMORY_OUTPUT_DATA 0xc10
#define AR3D_PIPEALIAS_DW_MEMORY_OUTPUT_INCR 0xc20

struct nvhost_hwctx;
struct nvhost_channel;
struct kref;

/* Functions used commonly by all 3D context switch modules */
void nvhost_3dctx_restore_begin(struct host1x_hwctx_handler *h, u32 *ptr);
void nvhost_3dctx_restore_direct(u32 *ptr, u32 start_reg, u32 count);
void nvhost_3dctx_restore_indirect(u32 *ptr, u32 offset_reg,
		u32 offset,	u32 data_reg, u32 count);
void nvhost_3dctx_restore_end(struct host1x_hwctx_handler *h, u32 *ptr);
struct host1x_hwctx *nvhost_3dctx_alloc_common(
		struct host1x_hwctx_handler *p,
		struct nvhost_channel *ch, bool map_restore);
void nvhost_3dctx_get(struct nvhost_hwctx *ctx);
void nvhost_3dctx_free(struct kref *ref);
void nvhost_3dctx_put(struct nvhost_hwctx *ctx);
int nvhost_gr3d_prepare_power_off(struct platform_device *dev);

#endif
