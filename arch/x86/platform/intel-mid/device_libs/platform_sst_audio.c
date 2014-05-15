/*
 * platform_sst_libs.c: SST platform  data initilization file
 *
 * Copyright (C) 2012 Intel Corporation
 * Author: Jeeja KP <jeeja.kp@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sfi.h>
#include <linux/platform_device.h>
#include <asm/platform_sst_audio.h>
#include <asm/intel-mid.h>
#include <asm/intel_sst_mrfld.h>
#include <asm/platform_byt_audio.h>
#include <sound/asound.h>

static struct sst_platform_data sst_platform_pdata;
static struct sst_dev_stream_map byt_bl_strm_map[] = {
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* Reserved, not in use */
	{BYT_AUD_AIF1, 0, SNDRV_PCM_STREAM_PLAYBACK, SST_PCM_OUT0, SST_TASK_ID_NONE, SST_DEV_MAP_IN_USE},
	{BYT_AUD_AIF1, 1, SNDRV_PCM_STREAM_PLAYBACK, SST_PCM_OUT1, SST_TASK_ID_NONE, SST_DEV_MAP_IN_USE},
	{BYT_AUD_COMPR_DEV, 0, SNDRV_PCM_STREAM_PLAYBACK, SST_COMPRESSED_OUT,
					SST_TASK_ID_NONE, SST_DEV_MAP_IN_USE},
	{BYT_AUD_AIF1, 0, SNDRV_PCM_STREAM_CAPTURE, SST_CAPTURE_IN, SST_TASK_ID_NONE, SST_DEV_MAP_IN_USE},
};
static struct sst_dev_stream_map byt_cr_strm_map[] = {
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* Reserved, not in use */
	{BYT_CR_AUD_AIF1, 0, SNDRV_PCM_STREAM_PLAYBACK, SST_PCM_OUT0, SST_TASK_ID_NONE, SST_DEV_MAP_IN_USE},
	{BYT_CR_AUD_AIF1, 1, SNDRV_PCM_STREAM_PLAYBACK, SST_PCM_OUT1, SST_TASK_ID_NONE, SST_DEV_MAP_IN_USE},
	{BYT_CR_AUD_COMPR_DEV, 0, SNDRV_PCM_STREAM_PLAYBACK, SST_COMPRESSED_OUT,
					SST_TASK_ID_NONE, SST_DEV_MAP_IN_USE},
	{BYT_CR_AUD_AIF1, 0, SNDRV_PCM_STREAM_CAPTURE, SST_CAPTURE_IN, SST_TASK_ID_NONE, SST_DEV_MAP_IN_USE},
};

static struct sst_dev_stream_map mrfld_strm_map[] = {
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* Reserved, not in use */
	{MERR_SALTBAY_AUDIO, 0, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_AUDIO, 1, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_AUDIO, 2, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_COMPR, 0, SNDRV_PCM_STREAM_PLAYBACK, PIPE_MEDIA0_IN, SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
	{MERR_SALTBAY_VOIP, 0, SNDRV_PCM_STREAM_PLAYBACK, PIPE_VOIP_IN, SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
	{MERR_SALTBAY_AUDIO, 0, SNDRV_PCM_STREAM_CAPTURE, PIPE_PCM1_OUT, SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
	{MERR_SALTBAY_VOIP, 0, SNDRV_PCM_STREAM_CAPTURE, PIPE_VOIP_OUT, SST_TASK_ID_MEDIA, SST_DEV_MAP_IN_USE},
	{MERR_SALTBAY_PROBE, 0, SNDRV_PCM_STREAM_CAPTURE, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 1, SNDRV_PCM_STREAM_CAPTURE, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 2, SNDRV_PCM_STREAM_CAPTURE, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 3, SNDRV_PCM_STREAM_CAPTURE, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 4, SNDRV_PCM_STREAM_CAPTURE, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 5, SNDRV_PCM_STREAM_CAPTURE, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 6, SNDRV_PCM_STREAM_CAPTURE, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 7, SNDRV_PCM_STREAM_CAPTURE, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 0, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 1, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 2, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 3, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 4, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 5, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 6, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_PROBE, 7, SNDRV_PCM_STREAM_PLAYBACK, PIPE_RSVD, SST_TASK_ID_MEDIA, SST_DEV_MAP_FREE},
	{MERR_SALTBAY_AWARE, 0, SNDRV_PCM_STREAM_CAPTURE, PIPE_AWARE_OUT, SST_TASK_ID_AWARE, SST_DEV_MAP_IN_USE},
	{MERR_SALTBAY_VAD, 0, SNDRV_PCM_STREAM_CAPTURE, PIPE_VAD_OUT, SST_TASK_ID_AWARE, SST_DEV_MAP_IN_USE},
};

#define EQ_EFFECT_ALGO_ID 0x99
static struct sst_dev_effects_map mrfld_effs_map[] = {
	{
	  {0xc1, 0x47, 0xa2, 0xf7, 0x7b, 0x1a, 0xe0, 0x11, 0x0d, 0xbb, 0x2a, 0x30, 0xdf, 0xd7, 0x20, 0x45},/* uuid */
	   EQ_EFFECT_ALGO_ID,										   /* algo id */
	  {0x00, 0x43, 0xed, 0x0b, 0xd6, 0xdd, 0xdb, 0x11, 0x34, 0x8f, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b, /* descriptor */
	   0xc1, 0x47, 0xa2, 0xf7, 0x7b, 0x1a, 0xe0, 0x11, 0x0d, 0xbb, 0x2a, 0x30, 0xdf, 0xd7, 0x20, 0x45,
	   0x12, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x45, 0x71, 0x75, 0x61,
	   0x6c, 0x69, 0x7a, 0x65, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x6e, 0x74, 0x65,
	   0x6c, 0x20, 0x43, 0x6f, 0x72, 0x70, 0x6f, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	  },
	}
};

static struct sst_dev_effects_resource_map mrfld_effs_res_map[] = {
	{
	 {0xc1, 0x47, 0xa2, 0xf7, 0x7b, 0x1a, 0xe0, 0x11, 0x0d, 0xbb, 0x2a, 0x30, 0xdf, 0xd7, 0x20, 0x45}, /* uuid */
	  0x50, /* Flags */
	  0x00, /* Cpu load */
	  0x01, /* Memory Usage */
	 }
};

static const int sst_ssp_mux_shift[SST_NUM_SSPS] = {
	[SST_SSP0] = -1,			/* no register shift, i.e. single mux value */
	[SST_SSP1] = SST_BT_FM_MUX_SHIFT,
	[SST_SSP2] = -1,
};

static const int sst_ssp_domain_shift[SST_NUM_SSPS][SST_MAX_SSP_MUX] = {
	[SST_SSP0][0] = -1,			/* no domain shift, i.e. single domain */
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

static void set_byt_cr_platform_config(void)
{
	sst_platform_pdata.pdev_strm_map = byt_cr_strm_map;
	sst_platform_pdata.strm_map_size =  ARRAY_SIZE(byt_cr_strm_map);
	pr_debug("audio:byt_cr:strm_map_size %d\n", sst_platform_pdata.strm_map_size);
}

static void set_byt_platform_config(void)
{
	sst_platform_pdata.pdev_strm_map = byt_bl_strm_map;
	sst_platform_pdata.strm_map_size =  ARRAY_SIZE(byt_bl_strm_map);
}

static void set_cht_platform_config(void)
{
	sst_platform_pdata.pdev_strm_map = mrfld_strm_map;
	sst_platform_pdata.strm_map_size = ARRAY_SIZE(mrfld_strm_map);
	memcpy(sst_platform_pdata.ssp_config, sst_ssp_configs_mrfld, sizeof(sst_ssp_configs_mrfld));
	memcpy(sst_platform_pdata.mux_shift, sst_ssp_mux_shift, sizeof(sst_ssp_mux_shift));
	memcpy(sst_platform_pdata.domain_shift, sst_ssp_domain_shift, sizeof(sst_ssp_domain_shift));
}

static void set_cht_cr_platform_config(void)
{
	sst_platform_pdata.pdev_strm_map = mrfld_strm_map;
	sst_platform_pdata.strm_map_size = ARRAY_SIZE(mrfld_strm_map);
	memcpy(sst_platform_pdata.ssp_config, sst_ssp_configs_cht_cr, sizeof(sst_ssp_configs_cht_cr));
	memcpy(sst_platform_pdata.mux_shift, sst_ssp_mux_shift, sizeof(sst_ssp_mux_shift));
	memcpy(sst_platform_pdata.domain_shift, sst_ssp_domain_shift, sizeof(sst_ssp_domain_shift));
}

static void set_mrfld_platform_config(void)
{
	sst_platform_pdata.pdev_strm_map = mrfld_strm_map;
	sst_platform_pdata.strm_map_size = ARRAY_SIZE(mrfld_strm_map);
	sst_platform_pdata.pdev_effs.effs_map = mrfld_effs_map;
	sst_platform_pdata.pdev_effs.effs_res_map = mrfld_effs_res_map;
	sst_platform_pdata.pdev_effs.effs_num_map = ARRAY_SIZE(mrfld_effs_map);
	memcpy(sst_platform_pdata.ssp_config, sst_ssp_configs_mrfld, sizeof(sst_ssp_configs_mrfld));
	memcpy(sst_platform_pdata.mux_shift, sst_ssp_mux_shift, sizeof(sst_ssp_mux_shift));
	memcpy(sst_platform_pdata.domain_shift, sst_ssp_domain_shift, sizeof(sst_ssp_domain_shift));
}

static void  populate_platform_data(void)
{
	set_cht_cr_platform_config();
}

int add_sst_platform_device(void)
{
	struct platform_device *pdev = NULL;
	int ret;
	populate_platform_data();

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
