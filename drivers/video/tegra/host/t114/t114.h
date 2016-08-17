/*
 * drivers/video/tegra/host/t114/t114.h
 *
 * Tegra Graphics Chip support for T114
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation. All rights reserved.
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

struct nvhost_master;

/* number of host channels */
#define NV_HOST1X_CHANNELS_T114 9

/*  T114 specicic sync point assignments */
#define NVSYNCPT_MSENC			     (23)
#define NVSYNCPT_TSEC			     (28)

#define NVWAITBASE_MSENC  (4)
#define NVWAITBASE_TSEC   (5)

int nvhost_init_t114_channel_support(struct nvhost_master *,
		struct nvhost_chip_support *);
int nvhost_init_t114_support(struct nvhost_master *host,
		struct nvhost_chip_support *);

extern struct nvhost_device_data t11_host1x_info;
extern struct nvhost_device_data t11_gr3d_info;
extern struct nvhost_device_data t11_gr2d_info;
extern struct nvhost_device_data t11_isp_info;
extern struct nvhost_device_data t11_vi_info;
extern struct nvhost_device_data t11_msenc_info;
extern struct nvhost_device_data t11_tsec_info;

#endif
