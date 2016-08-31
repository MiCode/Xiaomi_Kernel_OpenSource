/*
 * tegra_pcm.h - Definitions for Tegra PCM driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Based on code copyright/by:
 *
 * Copyright (c) 2009-2010, NVIDIA CORPORATION.  All rights reserved.
 * Scott Peterson <speterson@nvidia.com>
 *
 * Copyright (C) 2010 Google, Inc.
 * Iliyan Malchev <malchev@google.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __TEGRA_PCM_H__
#define __TEGRA_PCM_H__

#include <linux/nvmap.h>

#define MAX_DMA_REQ_COUNT 2

#define TEGRA30_USE_SMMU 0

struct tegra_pcm_dma_params {
	unsigned long addr;
	unsigned long wrap;
	unsigned long width;
	unsigned long req_sel;
};

#if TEGRA30_USE_SMMU
struct tegra_smmu_data {
	struct nvmap_client *pcm_nvmap_client;
	struct nvmap_handle_ref *pcm_nvmap_handle;
};
#endif

struct tegra_runtime_data {
	int running;
	int disable_intr;
	dma_addr_t avp_dma_addr;
};

int tegra_pcm_platform_register(struct device *dev);
void tegra_pcm_platform_unregister(struct device *dev);

#endif
