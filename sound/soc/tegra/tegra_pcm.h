/*
 * tegra_pcm.h - Definitions for Tegra PCM driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (C) 2010-2012 - NVIDIA, Inc.
 *
 * Based on code copyright/by:
 *
 * Copyright (c) 2009-2010, NVIDIA Corporation.
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

#include <mach/dma.h>
#include <linux/nvmap.h>

#define MAX_DMA_REQ_COUNT 2

#define TEGRA30_USE_SMMU 0

struct tegra_pcm_dma_params {
	unsigned long addr;
	unsigned long wrap;
	unsigned long width;
	unsigned long req_sel;
};

struct tegra_runtime_data {
	struct snd_pcm_substream *substream;
	spinlock_t lock;
	int running;
	int dma_pos;
	int dma_pos_end;
	int period_index;
	int dma_req_idx;
	struct tegra_dma_req dma_req[MAX_DMA_REQ_COUNT];
	struct tegra_dma_channel *dma_chan;
	int dma_req_count;
	int disable_intr;
	unsigned int avp_dma_addr;
};

#if TEGRA30_USE_SMMU
struct tegra_smmu_data {
	struct nvmap_client *pcm_nvmap_client;
	struct nvmap_handle_ref *pcm_nvmap_handle;
};
#endif

int tegra_pcm_trigger(struct snd_pcm_substream *substream, int cmd);
int tegra_pcm_allocate(struct snd_pcm_substream *substream,
					int dma_mode,
					const struct snd_pcm_hardware *pcm_hardware);
int tegra_pcm_close(struct snd_pcm_substream *substream);
int tegra_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params);
int tegra_pcm_trigger(struct snd_pcm_substream *substream, int cmd);
int tegra_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma);
int tegra_pcm_dma_allocate(struct snd_soc_pcm_runtime *rtd, size_t size);
void tegra_pcm_free(struct snd_pcm *pcm);
snd_pcm_uframes_t tegra_pcm_pointer(struct snd_pcm_substream *substream);
int tegra_pcm_hw_free(struct snd_pcm_substream *substream);

#endif
