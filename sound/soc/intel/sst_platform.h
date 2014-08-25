/*
 *  sst_platform.h - Intel MID Platform driver header file
 *
 *  Copyright (C) 2010 Intel Corp
 *  Author: Vinod Koul <vinod.koul@intel.com>
 *  Author: Harsha Priya <priya.harsha@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *
 */

#ifndef __SST_PLATFORM_H__
#define __SST_PLATFORM_H__

#include <sound/soc.h>

#define SST_MAX_BIN_BYTES 1024

struct sst_data;

enum sst_audio_device_type {
	SND_SST_DEVICE_HEADSET = 1,
	SND_SST_DEVICE_IHF,
	SND_SST_DEVICE_VIBRA,
	SND_SST_DEVICE_HAPTIC,
	SND_SST_DEVICE_CAPTURE,
	SND_SST_DEVICE_COMPRESS,
};

enum snd_sst_input_stream {
	SST_INPUT_STREAM_NONE = 0x0,
	SST_INPUT_STREAM_PCM = 0x6,
	SST_INPUT_STREAM_COMPRESS = 0x8,
	SST_INPUT_STREAM_MIXED = 0xE,
};

enum sst_stream_ops {
	STREAM_OPS_PLAYBACK = 0,        /* Decode */
	STREAM_OPS_CAPTURE,             /* Encode */
	STREAM_OPS_COMPRESSED_PATH,     /* Offload playback/capture */

};
enum snd_sst_stream_type {
	SST_STREAM_DEVICE_HS = 32,
	SST_STREAM_DEVICE_IHF = 33,
	SST_STREAM_DEVICE_MIC0 = 34,
	SST_STREAM_DEVICE_MIC1 = 35,
};

enum sst_controls {
	SST_SND_ALLOC =			0x1000,
	SST_SND_PAUSE =			0x1001,
	SST_SND_RESUME =		0x1002,
	SST_SND_DROP =			0x1003,
	SST_SND_FREE =			0x1004,
	SST_SND_BUFFER_POINTER =	0x1005,
	SST_SND_STREAM_INIT =		0x1006,
	SST_SND_START	 =		0x1007,
	SST_SET_RUNTIME_PARAMS =	0x1008,
	SST_SET_ALGO_PARAMS =		0x1009,
	SST_SET_BYTE_STREAM =		0x100A,
	SST_GET_BYTE_STREAM =		0x100B,
	SST_SET_SSP_CONFIG =		0x100C,
	SST_SET_PROBE_BYTE_STREAM =     0x100D,
	SST_GET_PROBE_BYTE_STREAM =	0x100E,
	SST_SET_VTSV_INFO =		0x100F,
	SST_SET_MONITOR_LPE =           0x1010,
};

struct pcm_stream_info {
	int str_id;
	void *mad_substream;
	void (*period_elapsed) (void *mad_substream);
	unsigned long long buffer_ptr;
	unsigned long long pcm_delay;
	int sfreq;
};

struct sst_compress_cb {
	void *param;
	void (*compr_cb)(void *param);
	void *drain_cb_param;
	void (*drain_notify)(void *param);

};

struct snd_sst_params;

struct compress_sst_ops {
	const char *name;
	int (*open) (struct snd_sst_params *str_params,
			struct sst_compress_cb *cb);
	int (*control) (unsigned int cmd, unsigned int str_id);
	int (*tstamp) (unsigned int str_id, struct snd_compr_tstamp *tstamp);
	int (*ack) (unsigned int str_id, unsigned long bytes);
	int (*close) (unsigned int str_id);
	int (*get_caps) (struct snd_compr_caps *caps);
	int (*get_codec_caps) (struct snd_compr_codec_caps *codec);
	int (*set_metadata) (unsigned int str_id, struct snd_compr_metadata *metadata);

};

enum lpe_param_types_mixer {
	SST_ALGO_PARAM_MIXER_STREAM_CFG = 0x801,
};

struct mad_ops_wq {
	int stream_id;
	enum sst_controls control_op;
	struct work_struct wq;
};

struct sst_ops {
	int (*open) (struct snd_sst_params *str_param);
	int (*device_control) (int cmd, void *arg);
	int (*set_generic_params) (enum sst_controls cmd, void *arg);
	int (*close) (unsigned int str_id);
	int (*power) (bool state);
};

struct sst_runtime_stream {
	int     stream_status;
	unsigned int id;
	size_t bytes_written;
	struct pcm_stream_info stream_info;
	struct sst_ops *ops;
	struct compress_sst_ops *compr_ops;
	spinlock_t	status_lock;
};

struct sst_device {
	char *name;
	struct device *dev;
	struct sst_ops *ops;
	struct platform_device *pdev;
	struct compress_sst_ops *compr_ops;
};

int sst_register_dsp(struct sst_device *sst);
int sst_unregister_dsp(struct sst_device *sst);
#endif
