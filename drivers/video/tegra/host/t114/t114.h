/*
 * drivers/video/tegra/host/t114/t114.h
 *
 * Tegra Graphics Chip support for T114
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef _NVHOST_T114_H_
#define _NVHOST_T114_H_

#include "chip_support.h"

struct nvhost_master;

/* number of host channels */
#define NV_HOST1X_CHANNELS_T114 9

#ifdef TEGRA_11X_OR_HIGHER_CONFIG
int nvhost_init_t114_support(struct nvhost_master *host,
		struct nvhost_chip_support *);
#else
static inline int nvhost_init_t114_support(struct nvhost_master *host,
					   struct nvhost_chip_support *op)
{
	return -ENODEV;
}
#endif

extern struct nvhost_device_data t11_host1x_info;
extern struct nvhost_device_data t11_gr3d_info;
extern struct nvhost_device_data t11_gr2d_info;
extern struct nvhost_device_data t11_isp_info;
extern struct nvhost_device_data t11_vi_info;
extern struct nvhost_device_data t11_msenc_info;
extern struct nvhost_device_data t11_tsec_info;

#endif
