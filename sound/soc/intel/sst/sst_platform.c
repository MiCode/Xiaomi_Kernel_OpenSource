/*
 * sst_platform.c: SST platform  data initilization file
 *
 * Copyright (C) 2012 Intel Corporation
 * Author: Jeeja KP <jeeja.kp@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sfi.h>
#include <linux/platform_device.h>
#include <asm/platform_sst_audio.h>
#include <asm/intel-mid.h>
#include <asm/platform_byt_audio.h>
#include <asm/platform_cht_audio.h>
#include <sound/asound.h>
#include "sst.h"

static struct sst_platform_data sst_platform_pdata;
static struct sst_dev_stream_map dpcm_strm_map_byt[] = {
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* Reserved, not in use */
	{BYT_DPCM_AUDIO, 0, SNDRV_PCM_STREAM_PLAYBACK, PIPE_MEDIA1_IN,
					SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
	{BYT_DPCM_VOIP,  0, SNDRV_PCM_STREAM_PLAYBACK, PIPE_VOIP_IN,
					SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
	{BYT_DPCM_AUDIO, 0, SNDRV_PCM_STREAM_CAPTURE, PIPE_PCM1_OUT,
					SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
	{BYT_DPCM_VOIP,  0, SNDRV_PCM_STREAM_CAPTURE, PIPE_VOIP_OUT,
					SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
};

static struct sst_dev_stream_map dpcm_strm_map_cht[] = {
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* Reserved, not in use */
	{CHT_DPCM_AUDIO, 0, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD,
					SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
	{CHT_DPCM_VOIP,  0, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD,
					SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
	{CHT_DPCM_AUDIO, 0, SNDRV_PCM_STREAM_CAPTURE, PIPE_RSVD,
					SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
	{CHT_DPCM_VOIP,  0, SNDRV_PCM_STREAM_CAPTURE, PIPE_RSVD,
					SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
};

static const int sst_ssp_mux_shift[SST_NUM_SSPS] = {
	[SST_SSP0] = -1,	/* no register shift, i.e. single mux value */
	[SST_SSP1] = SST_BT_FM_MUX_SHIFT,
	[SST_SSP2] = -1,
};

static const int sst_ssp_domain_shift[SST_NUM_SSPS][SST_MAX_SSP_MUX] = {
	[SST_SSP0][0] = -1,	/* no domain shift, i.e. single domain */
	[SST_SSP1] = {
		[SST_SSP_FM_MUX] = -1,
		[SST_SSP_BT_MUX] = SST_BT_MODE_SHIFT,
	},
	[SST_SSP2][0] = -1,
};

#define SST_SSP_CODEC_MUX		0
#define SST_SSP_CODEC_DOMAIN		0
#define SST_SSP_MODEM_MUX		0
#define SST_SSP_MODEM_DOMAIN		0
#define SST_SSP_FM_MUX			0
#define SST_SSP_FM_DOMAIN		0
#define SST_SSP_BT_MUX			1
#define SST_SSP_BT_NB_DOMAIN		0
#define SST_SSP_BT_WB_DOMAIN		1

/**
 * sst_ssp_config - contains SSP configuration for different UCs
 *
 * The 3-D array contains SSP configuration for different SSPs for different
 * domains (e.g. NB, WB), as well as muxed SSPs.
 *
 * The first dimension has SSP number
 * The second dimension has SSP Muxing (e.g. BT/FM muxed on same SSP)
 * The third dimension has SSP domains (e.g. NB/WB for BT)
 */
static const struct sst_ssp_config
sst_ssp_configs_mrfld[SST_NUM_SSPS][SST_MAX_SSP_MUX][SST_MAX_SSP_DOMAINS] = {
	[SST_SSP0] = {
		[SST_SSP_MODEM_MUX] = {
			[SST_SSP_MODEM_DOMAIN] = {
				.ssp_id = SSP_MODEM,
				.bits_per_slot = 16,
				.slots = 1,
				.ssp_mode = SSP_MODE_MASTER,
				.pcm_mode = SSP_PCM_MODE_NETWORK,
				.data_polarity = 1,
				.duplex = SSP_DUPLEX,
				.ssp_protocol = SSP_MODE_PCM,
				.fs_width = 1,
				.fs_frequency = SSP_FS_48_KHZ,
				.active_slot_map = 0x1,
				.start_delay = 1,
			},
		},
	},
	[SST_SSP1] = {
		[SST_SSP_FM_MUX] = {
			[SST_SSP_FM_DOMAIN] = {
				.ssp_id = SSP_FM,
				.bits_per_slot = 16,
				.slots = 2,
				.ssp_mode = SSP_MODE_MASTER,
				.pcm_mode = SSP_PCM_MODE_NORMAL,
				.data_polarity = 1,
				.duplex = SSP_DUPLEX,
				.ssp_protocol = SSP_MODE_I2S,
				.fs_width = 32,
				.fs_frequency = SSP_FS_48_KHZ,
				.active_slot_map = 0x3,
				.start_delay = 0,
			},
		},
		[SST_SSP_BT_MUX] = {
			[SST_SSP_BT_NB_DOMAIN] = {
				.ssp_id = SSP_BT,
				.bits_per_slot = 16,
				.slots = 1,
				.ssp_mode = SSP_MODE_MASTER,
				.pcm_mode = SSP_PCM_MODE_NORMAL,
				.data_polarity = 1,
				.duplex = SSP_DUPLEX,
				.ssp_protocol = SSP_MODE_PCM,
				.fs_width = 1,
				.fs_frequency = SSP_FS_8_KHZ,
				.active_slot_map = 0x1,
				.start_delay = 1,
			},
			[SST_SSP_BT_WB_DOMAIN] = {
				.ssp_id = SSP_BT,
				.bits_per_slot = 16,
				.slots = 1,
				.ssp_mode = SSP_MODE_MASTER,
				.pcm_mode = SSP_PCM_MODE_NORMAL,
				.data_polarity = 1,
				.duplex = SSP_DUPLEX,
				.ssp_protocol = SSP_MODE_PCM,
				.fs_width = 1,
				.fs_frequency = SSP_FS_16_KHZ,
				.active_slot_map = 0x1,
				.start_delay = 1,
			},
		},
	},
	[SST_SSP2] = {
		[SST_SSP_CODEC_MUX] = {
			[SST_SSP_CODEC_DOMAIN] = {
				.ssp_id = SSP_CODEC,
				.bits_per_slot = 24,
				.slots = 4,
				.ssp_mode = SSP_MODE_MASTER,
				.pcm_mode = SSP_PCM_MODE_NETWORK,
				.data_polarity = 1,
				.duplex = SSP_DUPLEX,
				.ssp_protocol = SSP_MODE_PCM,
				.fs_width = 1,
				.fs_frequency = SSP_FS_48_KHZ,
				.active_slot_map = 0xF,
				.start_delay = 0,
			},
		},
	},
};

static const struct sst_ssp_config
sst_ssp_configs_cht_cr[SST_NUM_SSPS][SST_MAX_SSP_MUX][SST_MAX_SSP_DOMAINS] = {
	[SST_SSP0] = {
		[SST_SSP_MODEM_MUX] = {
			[SST_SSP_MODEM_DOMAIN] = {
				.ssp_id = SSP_MODEM,
				.bits_per_slot = 16,
				.slots = 1,
				.ssp_mode = SSP_MODE_MASTER,
				.pcm_mode = SSP_PCM_MODE_NETWORK,
				.data_polarity = 1,
				.duplex = SSP_DUPLEX,
				.ssp_protocol = SSP_MODE_PCM,
				.fs_width = 1,
				.fs_frequency = SSP_FS_48_KHZ,
				.active_slot_map = 0x1,
				.start_delay = 1,
			},
		},
	},
	[SST_SSP1] = {
		[SST_SSP_FM_MUX] = {
			[SST_SSP_FM_DOMAIN] = {
				.ssp_id = SSP_FM,
				.bits_per_slot = 16,
				.slots = 2,
				.ssp_mode = SSP_MODE_MASTER,
				.pcm_mode = SSP_PCM_MODE_NORMAL,
				.data_polarity = 1,
				.duplex = SSP_DUPLEX,
				.ssp_protocol = SSP_MODE_I2S,
				.fs_width = 32,
				.fs_frequency = SSP_FS_48_KHZ,
				.active_slot_map = 0x3,
				.start_delay = 0,
			},
		},
		[SST_SSP_BT_MUX] = {
			[SST_SSP_BT_NB_DOMAIN] = {
				.ssp_id = SSP_BT,
				.bits_per_slot = 16,
				.slots = 1,
				.ssp_mode = SSP_MODE_MASTER,
				.pcm_mode = SSP_PCM_MODE_NORMAL,
				.data_polarity = 1,
				.duplex = SSP_DUPLEX,
				.ssp_protocol = SSP_MODE_PCM,
				.fs_width = 1,
				.fs_frequency = SSP_FS_8_KHZ,
				.active_slot_map = 0x1,
				.start_delay = 0,
			},
			[SST_SSP_BT_WB_DOMAIN] = {
				.ssp_id = SSP_BT,
				.bits_per_slot = 16,
				.slots = 1,
				.ssp_mode = SSP_MODE_MASTER,
				.pcm_mode = SSP_PCM_MODE_NORMAL,
				.data_polarity = 1,
				.duplex = SSP_DUPLEX,
				.ssp_protocol = SSP_MODE_PCM,
				.fs_width = 1,
				.fs_frequency = SSP_FS_16_KHZ,
				.active_slot_map = 0x1,
				.start_delay = 1,
			},
		},
	},
	[SST_SSP2] = {
		[SST_SSP_CODEC_MUX] = {
			[SST_SSP_CODEC_DOMAIN] = {
				.ssp_id = SSP_CODEC,
				.bits_per_slot = 24,
				.slots = 2,
				.ssp_mode = SSP_MODE_MASTER,
				.pcm_mode = SSP_PCM_MODE_NORMAL,
				.data_polarity = 0,
				.duplex = SSP_DUPLEX,
				.ssp_protocol = SSP_MODE_I2S,
				.fs_width = 24,
				.fs_frequency = SSP_FS_48_KHZ,
				.active_slot_map = 0x3,
				.start_delay = 1,
			},
		},
	},
};


static void set_cht_platform_config(void)
{
	sst_platform_pdata.pdev_strm_map = dpcm_strm_map_cht;
	sst_platform_pdata.strm_map_size = ARRAY_SIZE(dpcm_strm_map_cht);
	sst_platform_pdata.dfw_enable = 1;
	memcpy(sst_platform_pdata.ssp_config, sst_ssp_configs_mrfld, sizeof(sst_ssp_configs_mrfld));
	memcpy(sst_platform_pdata.mux_shift, sst_ssp_mux_shift, sizeof(sst_ssp_mux_shift));
	memcpy(sst_platform_pdata.domain_shift, sst_ssp_domain_shift, sizeof(sst_ssp_domain_shift));
	pr_info("audio:%s\n", __func__);
}

static void set_cht_cr_platform_config(void)
{
	sst_platform_pdata.pdev_strm_map = dpcm_strm_map_byt;
	sst_platform_pdata.strm_map_size = ARRAY_SIZE(dpcm_strm_map_byt);
	sst_platform_pdata.dfw_enable = 0;
	memcpy(sst_platform_pdata.ssp_config, sst_ssp_configs_cht_cr, sizeof(sst_ssp_configs_cht_cr));
	memcpy(sst_platform_pdata.mux_shift, sst_ssp_mux_shift, sizeof(sst_ssp_mux_shift));
	memcpy(sst_platform_pdata.domain_shift, sst_ssp_domain_shift, sizeof(sst_ssp_domain_shift));
	pr_info("audio:%s\n", __func__);
}

static int  populate_platform_data(int dev_id)
{
	if (dev_id == SST_CHT_PCI_ID)
		set_cht_platform_config();
	else if (dev_id == SST_BYT_PCI_ID)
		set_cht_cr_platform_config();
	else {
		pr_err("%s: invalid device_id %d\n", __func__, dev_id);
		return  -EINVAL;
	}
	return 0;
}

static int add_sst_platform_device(int dev_id)
{
	struct platform_device *pdev = NULL;
	int ret;

	ret = populate_platform_data(dev_id);
	if (ret) {
		pr_err("failed to populate sst platform data\n");
		return  -EINVAL;
	}

	pdev = platform_device_alloc("sst-platform", -1);
	if (!pdev) {
		pr_err("failed to allocate audio platform device\n");
		return -EINVAL;
	}

	ret = platform_device_add_data(pdev, &sst_platform_pdata,
					sizeof(sst_platform_pdata));
	if (ret) {
		pr_err("failed to add sst platform data\n");
		platform_device_put(pdev);
		return  -EINVAL;
	}
	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add audio platform device\n");
		platform_device_put(pdev);
		return  -EINVAL;
	}
	return ret;
}


int sst_audio_platform_init(int dev_id)
{
	int ret;

	pr_info("Enter: %s\n", __func__);

	ret = add_sst_platform_device(dev_id);
	if (ret < 0)
		pr_err("%s failed to add sst-platform device\n", __func__);

	return ret;
}
