/*
 * drivers/video/tegra/host/t148/t148.h
 *
 * Support for T148 Architecture Chips
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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
#ifndef _NVHOST_T148_H_
#define _NVHOST_T148_H_

#include "chip_support.h"

#ifdef TEGRA_14X_OR_HIGHER_CONFIG
int nvhost_init_t148_support(struct nvhost_master *host,
		struct nvhost_chip_support *);
#else
static inline int nvhost_init_t148_support(struct nvhost_master *host,
					   struct nvhost_chip_support *op)
{
	return -ENODEV;
}
#endif

extern struct nvhost_device_data t14_host1x_info;
extern struct nvhost_device_data t14_gr3d_info;
extern struct nvhost_device_data t14_gr2d_info;
extern struct nvhost_device_data t14_isp_info;
extern struct nvhost_device_data t14_vi_info;
extern struct nvhost_device_data t14_msenc_info;
extern struct nvhost_device_data t14_tsec_info;

#endif
