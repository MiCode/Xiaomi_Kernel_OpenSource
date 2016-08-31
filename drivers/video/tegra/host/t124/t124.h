/*
 * drivers/video/tegra/host/t124/t124.h
 *
 * Tegra Graphics Chip support for T124
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
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
#ifndef _NVHOST_T124_H_
#define _NVHOST_T124_H_

#include "chip_support.h"

/* HACK.  Get this from auto-generated hardware def'n instead... */
#define T124_NVHOST_NUMCHANNELS 12
#define NVHOST_CHANNEL_BASE 0

struct nvhost_chip_support;

#ifdef TEGRA_12X_OR_HIGHER_CONFIG
int nvhost_init_t124_support(struct nvhost_master *,
		struct nvhost_chip_support *);
#else
static inline int nvhost_init_t124_support(struct nvhost_master *host,
					   struct nvhost_chip_support *op)
{
	return -ENODEV;
}
#endif
int nvhost_init_t124_channel_support(struct nvhost_master *,
		struct nvhost_chip_support *);
int nvhost_init_t124_cdma_support(struct nvhost_chip_support *);
int nvhost_init_t124_debug_support(struct nvhost_chip_support *);
int nvhost_init_t124_syncpt_support(struct nvhost_master *,
		struct nvhost_chip_support *);
int nvhost_init_t124_intr_support(struct nvhost_chip_support *);
int nvhost_init_t124_cpuaccess_support(struct nvhost_master *,
		struct nvhost_chip_support *);
int nvhost_init_t124_as_support(struct nvhost_chip_support *);

/* these sort of stick out, per module support */
int t124_nvhost_hwctx_handler_init(struct nvhost_channel *ch);

struct gk20a;

struct t124 {
	struct nvhost_master *host;
	struct gk20a *gk20a;
};

extern struct nvhost_device_data t124_host1x_info;
extern struct nvhost_device_data t124_isp_info;
extern struct nvhost_device_data t124_ispb_info;
extern struct nvhost_device_data t124_vi_info;
extern struct nvhost_device_data t124_vib_info;
extern struct nvhost_device_data t124_msenc_info;
extern struct nvhost_device_data t124_tsec_info;
extern struct nvhost_device_data t124_vic_info;

#endif /* _NVHOST_T124_H_ */
