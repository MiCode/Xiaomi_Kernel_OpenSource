/*
 *  sst_platform_pvt.h - Intel MID Platform driver header file
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

#ifndef __SST_PLATFORM_PVT_H__
#define __SST_PLATFORM_PVT_H__

/* TODO rmv this global */
extern struct sst_device *sst_dsp;

#define SST_MONO		1
#define SST_STEREO		2

#define SST_MIN_RATE		8000
#define SST_MAX_RATE		48000
#define SST_MIN_CHANNEL		1
#define SST_MAX_CHANNEL		2

#define SST_MAX_BUFFER		96000 /*500ms@48K,16bit,2ch - CLV*/
#define SST_MIN_PERIOD_BYTES	1536  /*24ms@16K,16bit,2ch - For VoIP on Mrfld*/
#define SST_MAX_PERIOD_BYTES	48000 /*250ms@48K,16bit,2ch - CLV*/

#define SST_MIN_PERIODS		2
#define SST_MAX_PERIODS		50
#define SST_FIFO_SIZE		0
#define SST_CODEC_TYPE_PCM	1

#define SST_HEADSET_DAI		"Headset-cpu-dai"
#define SST_SPEAKER_DAI		"Speaker-cpu-dai"
#define SST_VOICE_DAI		"Voice-cpu-dai"
#define SST_VIRTUAL_DAI		"Virtual-cpu-dai"
#define SST_LOOPBACK_DAI	"Loopback-cpu-dai"
#define SST_POWER_DAI		"Power-cpu-dai"
#define SST_COMPRESS_DAI	"Compress-cpu-dai"
#define SST_PROBE_DAI		"Probe-cpu-dai"
#define SST_VOIP_DAI		"Voip-cpu-dai"
#define SST_DEEPBUFFER_DAI	"Deepbuffer-cpu-dai"
#define SST_LOWLATENCY_DAI	"Lowlatency-cpu-dai"

struct sst_device;

enum sst_drv_status {
	SST_PLATFORM_UNINIT,
	SST_PLATFORM_INIT,
	SST_PLATFORM_RUNNING,
	SST_PLATFORM_PAUSED,
	SST_PLATFORM_DROPPED,
};

enum ssp_port {
	SST_SSP_PORT0 = 0,
	SST_SSP_PORT1,
	SST_SSP_PORT2,
	SST_SSP_PORT3,
};

#define SST_PIPE_CONTROL	0x0
#define SST_COMPRESS_VOL	0x01

int sst_platform_clv_init(struct snd_soc_platform *platform);
int sst_dsp_init(struct snd_soc_platform *platform);
int sst_dsp_init_v2_dpcm(struct snd_soc_platform *platform);
int sst_dsp_init_v2_dpcm_dfw(struct snd_soc_platform *platform);
int sst_send_pipe_gains(struct snd_soc_dai *dai, int stream, int mute);
void send_ssp_cmd(struct snd_soc_platform *platform, const char *id, bool enable);
void sst_handle_vb_timer(struct snd_soc_platform *platform, bool enable);

unsigned int sst_soc_read(struct snd_soc_platform *platform, unsigned int reg);
int sst_soc_write(struct snd_soc_platform *platform, unsigned int reg, unsigned int val);
unsigned int sst_reg_read(struct sst_data *sst, unsigned int reg,
			  unsigned int shift, unsigned int max);
unsigned int sst_reg_write(struct sst_data *sst, unsigned int reg,
			   unsigned int shift, unsigned int max, unsigned int val);

int sst_algo_int_ctl_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo);
void sst_set_stream_status(struct sst_runtime_stream *stream, int state);
int sst_fill_stream_params(void *substream, const struct sst_data *ctx,
			   struct snd_sst_params *str_params, bool is_compress);
int sst_dpcm_probe_send(struct snd_soc_platform *platform, u16 probe_pipe,
			int substream, int direction, bool on);
int sst_byte_control_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol);
int sst_byte_control_set(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol);

struct sst_algo_int_control_v2 {
	struct soc_mixer_control mc;
	u16 module_id; /* module identifieer */
	u16 pipe_id; /* location info: pipe_id + instance_id */
	u16 instance_id;
	unsigned int value; /* Value received is stored here */
};

struct sst_lowlatency_deepbuff {
	/* Thresholds for low latency & deep buffer */
	unsigned long	*low_latency;
	unsigned long	*deep_buffer;
	unsigned long	period_time;
};

struct sst_pcm_format {
	unsigned int sample_bits;
	unsigned int rate_min;
	unsigned int rate_max;
	unsigned int channels_min;
	unsigned int channels_max;
};


struct sst_vtsv_result {
	u8 data[VTSV_MAX_TOTAL_RESULT_ARRAY_SIZE];
};

struct sst_data {
	struct platform_device *pdev;
	struct sst_platform_data *pdata;
	unsigned int lpe_mixer_input_ihf;
	unsigned int lpe_mixer_input_hs;
	u32 *widget;
	char *byte_stream;
	struct mutex lock;
	/* Pipe_id for probe_stream to be saved in stream map */
	u8 pipe_id;
	bool vtsv_enroll;
	struct sst_lowlatency_deepbuff ll_db;
	struct sst_vtsv_result vtsv_result;
};
#endif
