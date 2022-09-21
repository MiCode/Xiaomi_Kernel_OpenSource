/*
 * Copyright (c) 2012, 2014, 2017, 2021 The Linux Foundation. All rights reserved.
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _UAPI_LINUX_MSM_AUDIO_H
#define _UAPI_LINUX_MSM_AUDIO_H

#include <linux/types.h>
#include <linux/ioctl.h>

/* PCM Audio */

#define AUDIO_IOCTL_MAGIC 'a'

#define AUDIO_START        _IOW(AUDIO_IOCTL_MAGIC, 0, unsigned int)
#define AUDIO_STOP         _IOW(AUDIO_IOCTL_MAGIC, 1, unsigned int)
#define AUDIO_FLUSH        _IOW(AUDIO_IOCTL_MAGIC, 2, unsigned int)
#define AUDIO_GET_CONFIG   _IOR(AUDIO_IOCTL_MAGIC, 3, \
		struct msm_audio_config)
#define AUDIO_SET_CONFIG   _IOW(AUDIO_IOCTL_MAGIC, 4, \
		struct msm_audio_config)
#define AUDIO_GET_STATS    _IOR(AUDIO_IOCTL_MAGIC, 5, \
		struct msm_audio_stats)
#define AUDIO_ENABLE_AUDPP _IOW(AUDIO_IOCTL_MAGIC, 6, unsigned int)
#define AUDIO_SET_ADRC     _IOW(AUDIO_IOCTL_MAGIC, 7, unsigned int)
#define AUDIO_SET_EQ       _IOW(AUDIO_IOCTL_MAGIC, 8, unsigned int)
#define AUDIO_SET_RX_IIR   _IOW(AUDIO_IOCTL_MAGIC, 9, unsigned int)
#define AUDIO_SET_VOLUME   _IOW(AUDIO_IOCTL_MAGIC, 10, unsigned int)
#define AUDIO_PAUSE        _IOW(AUDIO_IOCTL_MAGIC, 11, unsigned int)
#define AUDIO_PLAY_DTMF    _IOW(AUDIO_IOCTL_MAGIC, 12, unsigned int)
#define AUDIO_GET_EVENT    _IOR(AUDIO_IOCTL_MAGIC, 13, \
		struct msm_audio_event)
#define AUDIO_ABORT_GET_EVENT _IOW(AUDIO_IOCTL_MAGIC, 14, unsigned int)
#define AUDIO_REGISTER_PMEM _IOW(AUDIO_IOCTL_MAGIC, 15, unsigned int)
#define AUDIO_DEREGISTER_PMEM _IOW(AUDIO_IOCTL_MAGIC, 16, unsigned int)
#define AUDIO_ASYNC_WRITE _IOW(AUDIO_IOCTL_MAGIC, 17, \
		struct msm_audio_aio_buf)
#define AUDIO_ASYNC_READ _IOW(AUDIO_IOCTL_MAGIC, 18, \
		struct msm_audio_aio_buf)
#define AUDIO_SET_INCALL _IOW(AUDIO_IOCTL_MAGIC, 19, struct msm_voicerec_mode)
#define AUDIO_GET_NUM_SND_DEVICE _IOR(AUDIO_IOCTL_MAGIC, 20, unsigned int)
#define AUDIO_GET_SND_DEVICES _IOWR(AUDIO_IOCTL_MAGIC, 21, \
				struct msm_snd_device_list)
#define AUDIO_ENABLE_SND_DEVICE _IOW(AUDIO_IOCTL_MAGIC, 22, unsigned int)
#define AUDIO_DISABLE_SND_DEVICE _IOW(AUDIO_IOCTL_MAGIC, 23, unsigned int)
#define AUDIO_ROUTE_STREAM _IOW(AUDIO_IOCTL_MAGIC, 24, \
				struct msm_audio_route_config)
#define AUDIO_GET_PCM_CONFIG _IOR(AUDIO_IOCTL_MAGIC, 30, unsigned int)
#define AUDIO_SET_PCM_CONFIG _IOW(AUDIO_IOCTL_MAGIC, 31, unsigned int)
#define AUDIO_SWITCH_DEVICE  _IOW(AUDIO_IOCTL_MAGIC, 32, unsigned int)
#define AUDIO_SET_MUTE       _IOW(AUDIO_IOCTL_MAGIC, 33, unsigned int)
#define AUDIO_UPDATE_ACDB    _IOW(AUDIO_IOCTL_MAGIC, 34, unsigned int)
#define AUDIO_START_VOICE    _IOW(AUDIO_IOCTL_MAGIC, 35, unsigned int)
#define AUDIO_STOP_VOICE     _IOW(AUDIO_IOCTL_MAGIC, 36, unsigned int)
#define AUDIO_REINIT_ACDB    _IOW(AUDIO_IOCTL_MAGIC, 39, unsigned int)
#define AUDIO_OUTPORT_FLUSH  _IOW(AUDIO_IOCTL_MAGIC, 40, unsigned short)
#define AUDIO_SET_ERR_THRESHOLD_VALUE _IOW(AUDIO_IOCTL_MAGIC, 41, \
					unsigned short)
#define AUDIO_GET_BITSTREAM_ERROR_INFO _IOR(AUDIO_IOCTL_MAGIC, 42, \
			       struct msm_audio_bitstream_error_info)

#define AUDIO_SET_SRS_TRUMEDIA_PARAM _IOW(AUDIO_IOCTL_MAGIC, 43, unsigned int)

/* Qualcomm Technologies, Inc. extensions */
#define AUDIO_SET_STREAM_CONFIG   _IOW(AUDIO_IOCTL_MAGIC, 80, \
				struct msm_audio_stream_config)
#define AUDIO_GET_STREAM_CONFIG   _IOR(AUDIO_IOCTL_MAGIC, 81, \
				struct msm_audio_stream_config)
#define AUDIO_GET_SESSION_ID _IOR(AUDIO_IOCTL_MAGIC, 82, unsigned short)
#define AUDIO_GET_STREAM_INFO   _IOR(AUDIO_IOCTL_MAGIC, 83, \
			       struct msm_audio_bitstream_info)
#define AUDIO_SET_PAN       _IOW(AUDIO_IOCTL_MAGIC, 84, unsigned int)
#define AUDIO_SET_QCONCERT_PLUS       _IOW(AUDIO_IOCTL_MAGIC, 85, unsigned int)
#define AUDIO_SET_MBADRC       _IOW(AUDIO_IOCTL_MAGIC, 86, unsigned int)
#define AUDIO_SET_VOLUME_PATH   _IOW(AUDIO_IOCTL_MAGIC, 87, \
				     struct msm_vol_info)
#define AUDIO_SET_MAX_VOL_ALL _IOW(AUDIO_IOCTL_MAGIC, 88, unsigned int)
#define AUDIO_ENABLE_AUDPRE  _IOW(AUDIO_IOCTL_MAGIC, 89, unsigned int)
#define AUDIO_SET_AGC        _IOW(AUDIO_IOCTL_MAGIC, 90, unsigned int)
#define AUDIO_SET_NS         _IOW(AUDIO_IOCTL_MAGIC, 91, unsigned int)
#define AUDIO_SET_TX_IIR     _IOW(AUDIO_IOCTL_MAGIC, 92, unsigned int)
#define AUDIO_GET_BUF_CFG    _IOW(AUDIO_IOCTL_MAGIC, 93, \
					struct msm_audio_buf_cfg)
#define AUDIO_SET_BUF_CFG    _IOW(AUDIO_IOCTL_MAGIC, 94, \
					struct msm_audio_buf_cfg)
#define AUDIO_SET_ACDB_BLK _IOW(AUDIO_IOCTL_MAGIC, 95,  \
					struct msm_acdb_cmd_device)
#define AUDIO_GET_ACDB_BLK _IOW(AUDIO_IOCTL_MAGIC, 96,  \
					struct msm_acdb_cmd_device)
#define IOCTL_MAP_PHYS_ADDR _IOW(AUDIO_IOCTL_MAGIC, 97, int)
#define IOCTL_UNMAP_PHYS_ADDR _IOW(AUDIO_IOCTL_MAGIC, 98, int)
#define AUDIO_SET_EFFECTS_CONFIG   _IOW(AUDIO_IOCTL_MAGIC, 99, \
				struct msm_hwacc_effects_config)
#define AUDIO_EFFECTS_SET_BUF_LEN _IOW(AUDIO_IOCTL_MAGIC, 100, \
				struct msm_hwacc_buf_cfg)
#define AUDIO_EFFECTS_GET_BUF_AVAIL _IOW(AUDIO_IOCTL_MAGIC, 101, \
				struct msm_hwacc_buf_avail)
#define AUDIO_EFFECTS_WRITE _IOW(AUDIO_IOCTL_MAGIC, 102, void *)
#define AUDIO_EFFECTS_READ _IOWR(AUDIO_IOCTL_MAGIC, 103, void *)
#define AUDIO_EFFECTS_SET_PP_PARAMS _IOW(AUDIO_IOCTL_MAGIC, 104, void *)

#define AUDIO_PM_AWAKE      _IOW(AUDIO_IOCTL_MAGIC, 105, unsigned int)
#define AUDIO_PM_RELAX      _IOW(AUDIO_IOCTL_MAGIC, 106, unsigned int)
#define AUDIO_REGISTER_ION   _IOW(AUDIO_IOCTL_MAGIC, 107, struct msm_audio_ion_info)
#define AUDIO_DEREGISTER_ION _IOW(AUDIO_IOCTL_MAGIC, 108, struct msm_audio_ion_info)
#define	AUDIO_MAX_COMMON_IOCTL_NUM	109


#define HANDSET_MIC			0x01
#define HANDSET_SPKR			0x02
#define HEADSET_MIC			0x03
#define HEADSET_SPKR_MONO		0x04
#define HEADSET_SPKR_STEREO		0x05
#define SPKR_PHONE_MIC			0x06
#define SPKR_PHONE_MONO			0x07
#define SPKR_PHONE_STEREO		0x08
#define BT_SCO_MIC			0x09
#define BT_SCO_SPKR			0x0A
#define BT_A2DP_SPKR			0x0B
#define TTY_HEADSET_MIC			0x0C
#define TTY_HEADSET_SPKR		0x0D

/* Default devices are not supported in a */
/* device switching context. Only supported */
/* for stream devices. */
/* DO NOT USE */
#define DEFAULT_TX			0x0E
#define DEFAULT_RX			0x0F

#define BT_A2DP_TX			0x10

#define HEADSET_MONO_PLUS_SPKR_MONO_RX         0x11
#define HEADSET_MONO_PLUS_SPKR_STEREO_RX       0x12
#define HEADSET_STEREO_PLUS_SPKR_MONO_RX       0x13
#define HEADSET_STEREO_PLUS_SPKR_STEREO_RX     0x14

#define I2S_RX				0x20
#define I2S_TX				0x21

#define ADRC_ENABLE		0x0001
#define EQUALIZER_ENABLE	0x0002
#define IIR_ENABLE		0x0004
#define QCONCERT_PLUS_ENABLE	0x0008
#define MBADRC_ENABLE		0x0010
#define SRS_ENABLE		0x0020
#define SRS_DISABLE	0x0040

#define AGC_ENABLE		0x0001
#define NS_ENABLE		0x0002
#define TX_IIR_ENABLE		0x0004
#define FLUENCE_ENABLE		0x0008

#define VOC_REC_UPLINK		0x00
#define VOC_REC_DOWNLINK	0x01
#define VOC_REC_BOTH		0x02

struct msm_audio_config {
	__u32 buffer_size;
	__u32 buffer_count;
	__u32 channel_count;
	__u32 sample_rate;
	__u32 type;
	__u32 meta_field;
	__u32 bits;
	__u32 unused[3];
};

struct msm_audio_stream_config {
	__u32 buffer_size;
	__u32 buffer_count;
};

struct msm_audio_buf_cfg {
	__u32 meta_info_enable;
	__u32 frames_per_buf;
};

struct msm_audio_stats {
	__u32 byte_count;
	__u32 sample_count;
	__u32 unused[2];
};

struct msm_audio_ion_info {
	int fd;
	void *vaddr;
};

struct msm_audio_pmem_info {
	int fd;
	void *vaddr;
};

struct msm_audio_aio_buf {
	void *buf_addr;
	__u32 buf_len;
	__u32 data_len;
	void *private_data;
	unsigned short mfield_sz; /*only useful for data has meta field */
};

/* Audio routing */

#define SND_IOCTL_MAGIC 's'

#define SND_MUTE_UNMUTED 0
#define SND_MUTE_MUTED   1

struct msm_mute_info {
	__u32 mute;
	__u32 path;
};

struct msm_vol_info {
	__u32 vol;
	__u32 path;
};

struct msm_voicerec_mode {
	__u32 rec_mode;
};

struct msm_snd_device_config {
	__u32 device;
	__u32 ear_mute;
	__u32 mic_mute;
};

#define SND_SET_DEVICE _IOW(SND_IOCTL_MAGIC, 2, struct msm_device_config *)

enum cad_device_path_type {
	CAD_DEVICE_PATH_RX,	/*For Decoding session*/
	CAD_DEVICE_PATH_TX,	/* For Encoding session*/
	CAD_DEVICE_PATH_RX_TX, /* For Voice call */
	CAD_DEVICE_PATH_LB,	/* For loopback (FM Analog)*/
	CAD_DEVICE_PATH_MAX
};

struct cad_devices_type {
	__u32 rx_device;
	__u32 tx_device;
	enum cad_device_path_type pathtype;
};

struct msm_cad_device_config {
	struct cad_devices_type device;
	__u32 ear_mute;
	__u32 mic_mute;
};

#define CAD_SET_DEVICE _IOW(SND_IOCTL_MAGIC, 2, struct msm_cad_device_config *)

#define SND_METHOD_VOICE 0
#define SND_METHOD_MIDI 4

struct msm_snd_volume_config {
	__u32 device;
	__u32 method;
	__u32 volume;
};

#define SND_SET_VOLUME _IOW(SND_IOCTL_MAGIC, 3, struct msm_snd_volume_config *)

struct msm_cad_volume_config {
	struct cad_devices_type device;
	__u32 method;
	__u32 volume;
};

#define CAD_SET_VOLUME _IOW(SND_IOCTL_MAGIC, 3, struct msm_cad_volume_config *)

/* Returns the number of SND endpoints supported. */

#define SND_GET_NUM_ENDPOINTS _IOR(SND_IOCTL_MAGIC, 4, unsigned int *)

struct msm_snd_endpoint {
	int id; /* input and output */
	char name[64]; /* output only */
};

/* Takes an index between 0 and one less than the number returned by
 * SND_GET_NUM_ENDPOINTS, and returns the SND index and name of a
 * SND endpoint.  On input, the .id field contains the number of the
 * endpoint, and on exit it contains the SND index, while .name contains
 * the description of the endpoint.
 */

#define SND_GET_ENDPOINT _IOWR(SND_IOCTL_MAGIC, 5, struct msm_snd_endpoint *)


#define SND_AVC_CTL _IOW(SND_IOCTL_MAGIC, 6, unsigned int *)
#define SND_AGC_CTL _IOW(SND_IOCTL_MAGIC, 7, unsigned int *)

/*return the number of CAD endpoints supported. */

#define CAD_GET_NUM_ENDPOINTS _IOR(SND_IOCTL_MAGIC, 4, unsigned int *)

struct msm_cad_endpoint {
	int id; /* input and output */
	char name[64]; /* output only */
};

/* Takes an index between 0 and one less than the number returned by
 * SND_GET_NUM_ENDPOINTS, and returns the CAD index and name of a
 * CAD endpoint.  On input, the .id field contains the number of the
 * endpoint, and on exit it contains the SND index, while .name contains
 * the description of the endpoint.
 */

#define CAD_GET_ENDPOINT _IOWR(SND_IOCTL_MAGIC, 5, struct msm_cad_endpoint *)

struct msm_audio_pcm_config {
	__u32 pcm_feedback;	/* 0 - disable > 0 - enable */
	__u32 buffer_count;	/* Number of buffers to allocate */
	__u32 buffer_size;	/* Size of buffer for capturing of
				 * PCM samples
				 */
};

#define AUDIO_EVENT_SUSPEND 0
#define AUDIO_EVENT_RESUME 1
#define AUDIO_EVENT_WRITE_DONE 2
#define AUDIO_EVENT_READ_DONE   3
#define AUDIO_EVENT_STREAM_INFO 4
#define AUDIO_EVENT_BITSTREAM_ERROR_INFO 5

#define AUDIO_CODEC_TYPE_MP3 0
#define AUDIO_CODEC_TYPE_AAC 1

struct msm_audio_bitstream_info {
	__u32 codec_type;
	__u32 chan_info;
	__u32 sample_rate;
	__u32 bit_stream_info;
	__u32 bit_rate;
	__u32 unused[3];
};

struct msm_audio_bitstream_error_info {
	__u32 dec_id;
	__u32 err_msg_indicator;
	__u32 err_type;
};

union msm_audio_event_payload {
	struct msm_audio_aio_buf aio_buf;
	struct msm_audio_bitstream_info stream_info;
	struct msm_audio_bitstream_error_info error_info;
	int reserved;
};

struct msm_audio_event {
	int event_type;
	int timeout_ms;
	union msm_audio_event_payload event_payload;
};

#define MSM_SNDDEV_CAP_RX 0x1
#define MSM_SNDDEV_CAP_TX 0x2
#define MSM_SNDDEV_CAP_VOICE 0x4

struct msm_snd_device_info {
	__u32 dev_id;
	__u32 dev_cap; /* bitmask describe capability of device */
	char dev_name[64];
};

struct msm_snd_device_list {
	__u32  num_dev; /* Indicate number of device info to be retrieved */
	struct msm_snd_device_info *list;
};

struct msm_dtmf_config {
	__u16 path;
	__u16 dtmf_hi;
	__u16 dtmf_low;
	__u16 duration;
	__u16 tx_gain;
	__u16 rx_gain;
	__u16 mixing;
};

#define AUDIO_ROUTE_STREAM_VOICE_RX 0
#define AUDIO_ROUTE_STREAM_VOICE_TX 1
#define AUDIO_ROUTE_STREAM_PLAYBACK 2
#define AUDIO_ROUTE_STREAM_REC      3

struct msm_audio_route_config {
	__u32 stream_type;
	__u32 stream_id;
	__u32 dev_id;
};

#define AUDIO_MAX_EQ_BANDS 12

struct msm_audio_eq_band {
	__u16     band_idx; /* The band index, 0 .. 11 */
	__u32     filter_type; /* Filter band type */
	__u32     center_freq_hz; /* Filter band center frequency */
	__u32     filter_gain; /* Filter band initial gain (dB) */
			/* Range is +12 dB to -12 dB with 1dB increments. */
	__u32     q_factor;
} __attribute__ ((packed));

struct msm_audio_eq_stream_config {
	__u32	enable; /* Number of consequtive bands specified */
	__u32	num_bands;
	struct msm_audio_eq_band	eq_bands[AUDIO_MAX_EQ_BANDS];
} __attribute__ ((packed));

struct msm_acdb_cmd_device {
	__u32     command_id;
	__u32     device_id;
	__u32     network_id;
	__u32     sample_rate_id;      /* Actual sample rate value */
	__u32     interface_id;        /* See interface id's above */
	__u32     algorithm_block_id;  /* See enumerations above */
	__u32     total_bytes;         /* Length in bytes used by buffer */
	__u32     *phys_buf;           /* Physical Address of data */
};

struct msm_hwacc_data_config {
	__u32 buf_size;
	__u32 num_buf;
	__u32 num_channels;
	__u8 channel_map[8];
	__u32 sample_rate;
	__u32 bits_per_sample;
};

struct msm_hwacc_buf_cfg {
	__u32 input_len;
	__u32 output_len;
};

struct msm_hwacc_buf_avail {
	__u32 input_num_avail;
	__u32 output_num_avail;
};

struct msm_hwacc_effects_config {
	struct msm_hwacc_data_config input;
	struct msm_hwacc_data_config output;
	struct msm_hwacc_buf_cfg buf_cfg;
	__u32 meta_mode_enabled;
	__u32 overwrite_topology;
	__s32 topology;
};

#define ADSP_STREAM_PP_EVENT				0
#define ADSP_STREAM_ENCDEC_EVENT			1
#define ADSP_STREAM_IEC_61937_FMT_UPDATE_EVENT		2
#define ADSP_STREAM_EVENT_MAX				3

struct msm_adsp_event_data {
	__u32 event_type;
	__u32 payload_len;
	__u8 payload[0];
};

#endif
