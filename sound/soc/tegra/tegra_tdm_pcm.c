/*
 * tegra_tdm_pcm.c - Tegra TDM PCM driver
 *
 * Author: Nitin Pai <npai@nvidia.com>
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * Based on code copyright/by:
 *
 * Copyright (c) 2009-2010, NVIDIA CORPORATION.  All rights reserved.
 * Scott Peterson <speterson@nvidia.com>
 * Stephen Warren <swarren@nvidia.com>
 * Vijay Mali <vmali@nvidia.com>
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

#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tegra_pcm.h"

#define DRV_NAME "tegra-tdm-pcm-audio"

static const struct snd_pcm_hardware tegra_tdm_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min		= 8,
	.channels_max		= 16,
	.period_bytes_min	= 8 * 1024,
	.period_bytes_max	= 16 * 1024,
	.periods_min		= 4,
	.periods_max		= 4,
	.buffer_bytes_max	= 16 * 4 * 1024,
	.fifo_size		= 4,
};

static int tegra_tdm_pcm_open(struct snd_pcm_substream *substream)
{
	return tegra_pcm_allocate(substream,
					TEGRA_DMA_MODE_CONTINUOUS_DOUBLE,
					&tegra_tdm_pcm_hardware);

}

static int tegra_tdm_pcm_close(struct snd_pcm_substream *substream)
{
	return tegra_pcm_close(substream);
}

static int tegra_tdm_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	return tegra_pcm_hw_params(substream, params);
}

static int tegra_tdm_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return tegra_pcm_hw_free(substream);
}

static int tegra_tdm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return tegra_pcm_trigger(substream, cmd);
}

static int tegra_tdm_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	return tegra_pcm_mmap(substream, vma);
}

static struct snd_pcm_ops tegra_tdm_pcm_ops = {
	.open		= tegra_tdm_pcm_open,
	.close		= tegra_tdm_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= tegra_tdm_pcm_hw_params,
	.hw_free	= tegra_tdm_pcm_hw_free,
	.trigger	= tegra_tdm_pcm_trigger,
	.pointer	= tegra_pcm_pointer,
	.mmap		= tegra_tdm_pcm_mmap,
};

static int tegra_tdm_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	return tegra_pcm_dma_allocate(rtd ,
				tegra_tdm_pcm_hardware.buffer_bytes_max);
}

static void tegra_tdm_pcm_free(struct snd_pcm *pcm)
{
	return tegra_pcm_free(pcm);
}

struct snd_soc_platform_driver tegra_tdm_pcm_platform = {
	.ops		= &tegra_tdm_pcm_ops,
	.pcm_new	= tegra_tdm_pcm_new,
	.pcm_free	= tegra_tdm_pcm_free,
};

static int __devinit tegra_tdm_pcm_platform_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &tegra_tdm_pcm_platform);
}

static int __devexit tegra_tdm_pcm_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver tegra_tdm_pcm_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tegra_tdm_pcm_platform_probe,
	.remove = __devexit_p(tegra_tdm_pcm_platform_remove),
};

static int __init snd_tegra_tdm_pcm_init(void)
{
	return platform_driver_register(&tegra_tdm_pcm_driver);
}
module_init(snd_tegra_tdm_pcm_init);

static void __exit snd_tegra_tdm_pcm_exit(void)
{
	platform_driver_unregister(&tegra_tdm_pcm_driver);
}
module_exit(snd_tegra_tdm_pcm_exit);

MODULE_AUTHOR("Nitin Pai <npai@nvidia.com>");
MODULE_DESCRIPTION("Tegra PCM ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
