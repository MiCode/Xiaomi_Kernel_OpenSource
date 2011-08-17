/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DAL_AUDIO_H__
#define __DAL_AUDIO_H__

#include "dal_audio_format.h"

#define AUDIO_DAL_DEVICE 0x02000028
#define AUDIO_DAL_PORT "DAL_AQ_AUD"

enum {
	AUDIO_OP_CONTROL = DAL_OP_FIRST_DEVICE_API,
	AUDIO_OP_DATA,
	AUDIO_OP_INIT,
};	

/* ---- common audio structures ---- */

/* This flag, if set, indicates that the beginning of the data in the*/
/* buffer is a synchronization point or key frame, meaning no data */
/* before it in the stream is required in order to render the stream */
/* from this point onward. */
#define ADSP_AUDIO_BUFFER_FLAG_SYNC_POINT        0x01

/* This flag, if set, indicates that the buffer object is using valid */
/* physical address used to store the media data */
#define ADSP_AUDIO_BUFFER_FLAG_PHYS_ADDR         0x04

/* This flag, if set, indicates that a media start timestamp has been */
/* set for a buffer. */
#define ADSP_AUDIO_BUFFER_FLAG_START_SET         0x08

/* This flag, if set, indicates that a media stop timestamp has been set */
/* for a buffer. */
#define ADSP_AUDIO_BUFFER_FLAG_STOP_SET          0x10

/* This flag, if set, indicates that a preroll timestamp has been set */
/* for a buffer. */
#define ADSP_AUDIO_BUFFER_FLAG_PREROLL_SET       0x20

/* This flag, if set, indicates that the data in the buffer is a fragment of */
/* a larger block of data, and will be continued by the data in the next */
/* buffer to be delivered. */
#define ADSP_AUDIO_BUFFER_FLAG_CONTINUATION      0x40

struct adsp_audio_buffer {
	u32 addr;		/* Physical Address of buffer */
	u32 max_size;		/* Maximum size of buffer */
	u32 actual_size;	/* Actual size of valid data in the buffer */
	u32 offset;		/* Offset to the first valid byte */
	u32 flags;		/* ADSP_AUDIO_BUFFER_FLAGs that has been set */
	s64 start;		/* Start timestamp, if any */
	s64 stop;		/* Stop timestamp, if any */
	s64 preroll;		/* Preroll timestamp, if any */
} __attribute__ ((packed));



/* ---- audio commands ---- */

/* Command/event response types */
#define ADSP_AUDIO_RESPONSE_COMMAND   0
#define ADSP_AUDIO_RESPONSE_ASYNC     1

struct adsp_command_hdr {
	u32 size;		/* sizeof(cmd) - sizeof(u32) */

	u32 dst;
	u32 src;

	u32 opcode;
	u32 response_type;
	u32 seq_number;

	u32 context;		/* opaque to DSP */
	u32 data;

	u32 padding;
} __attribute__ ((packed));


#define AUDIO_DOMAIN_APP	0
#define AUDIO_DOMAIN_MODEM	1
#define AUDIO_DOMAIN_DSP	2

#define AUDIO_SERVICE_AUDIO	0
#define AUDIO_SERVICE_VIDEO	1 /* really? */

/* adsp audio addresses are (byte order) domain, service, major, minor */
//#define AUDIO_ADDR(maj,min) ( (((maj) & 0xff) << 16) | (((min) & 0xff) << 24) | (1) )

#define AUDIO_ADDR(maj,min,dom) ( (((min) & 0xff) << 24) | (((maj) & 0xff) << 16) | ((AUDIO_SERVICE_AUDIO) << 8) | (dom) )


/* AAC Encoder modes */
#define ADSP_AUDIO_ENC_AAC_LC_ONLY_MODE		0
#define ADSP_AUDIO_ENC_AAC_PLUS_MODE		1
#define ADSP_AUDIO_ENC_ENHANCED_AAC_PLUS_MODE	2

struct adsp_audio_aac_enc_cfg {
	u32 bit_rate;		/* bits per second */
	u32 encoder_mode;	/* ADSP_AUDIO_ENC_* */
} __attribute__ ((packed));

#define ADSP_AUDIO_ENC_SBC_ALLOCATION_METHOD_LOUNDNESS     0
#define ADSP_AUDIO_ENC_SBC_ALLOCATION_METHOD_SNR           1

#define ADSP_AUDIO_ENC_SBC_CHANNEL_MODE_MONO                1
#define ADSP_AUDIO_ENC_SBC_CHANNEL_MODE_STEREO              2
#define ADSP_AUDIO_ENC_SBC_CHANNEL_MODE_DUAL                8
#define ADSP_AUDIO_ENC_SBC_CHANNEL_MODE_JOINT_STEREO        9

struct adsp_audio_sbc_encoder_cfg {
	u32 num_subbands;
	u32 block_len;
	u32 channel_mode;
	u32 allocation_method;
	u32 bit_rate;
} __attribute__ ((packed));

/* AMR NB encoder modes */
#define ADSP_AUDIO_AMR_MR475	0
#define ADSP_AUDIO_AMR_MR515	1
#define ADSP_AUDIO_AMR_MMR59	2
#define ADSP_AUDIO_AMR_MMR67	3
#define ADSP_AUDIO_AMR_MMR74	4
#define ADSP_AUDIO_AMR_MMR795	5
#define ADSP_AUDIO_AMR_MMR102	6
#define ADSP_AUDIO_AMR_MMR122	7

/* The following are valid AMR NB DTX modes */
#define ADSP_AUDIO_AMR_DTX_MODE_OFF		0
#define ADSP_AUDIO_AMR_DTX_MODE_ON_VAD1		1
#define ADSP_AUDIO_AMR_DTX_MODE_ON_VAD2		2
#define ADSP_AUDIO_AMR_DTX_MODE_ON_AUTO		3

/* AMR Encoder configuration */
struct adsp_audio_amr_enc_cfg {
	u32	mode;		/* ADSP_AUDIO_AMR_MR* */
	u32	dtx_mode;	/* ADSP_AUDIO_AMR_DTX_MODE* */
	u32	enable;		/* 1 = enable, 0 = disable */
} __attribute__ ((packed));

struct adsp_audio_qcelp13k_enc_cfg {
	u16	min_rate;
	u16	max_rate;
} __attribute__ ((packed));

struct adsp_audio_evrc_enc_cfg {
	u16	min_rate;
	u16	max_rate;
} __attribute__ ((packed));

union adsp_audio_codec_config {
	struct adsp_audio_amr_enc_cfg amr;
	struct adsp_audio_aac_enc_cfg aac;
	struct adsp_audio_qcelp13k_enc_cfg qcelp13k;
	struct adsp_audio_evrc_enc_cfg evrc;
	struct adsp_audio_sbc_encoder_cfg sbc;
} __attribute__ ((packed));


/* This is the default value. */
#define ADSP_AUDIO_OPEN_STREAM_MODE_NONE		0x0000

/* This bit, if set, indicates that the AVSync mode is activated. */
#define ADSP_AUDIO_OPEN_STREAM_MODE_AVSYNC		0x0001

/* This bit, if set, indicates that the Sample Rate/Channel Mode */
/* Change Notification mode is activated. */
#define ADSP_AUDIO_OPEN_STREAM_MODE_SR_CM_NOTIFY	0x0002

/* This bit, if set, indicates that the sync clock is enabled */
#define  ADSP_AUDIO_OPEN_STREAM_MODE_ENABLE_SYNC_CLOCK	0x0004

/* This bit, if set, indicates that the AUX PCM loopback is enabled */
#define  ADSP_AUDIO_OPEN_STREAM_MODE_AUX_PCM		0x0040

struct adsp_open_command {
	struct adsp_command_hdr hdr;

	u32 device;
	u32 endpoint; /* address */

	u32 stream_context;
	u32 mode;

	u32 buf_max_size;

	union adsp_audio_format format;
	union adsp_audio_codec_config config;
} __attribute__ ((packed));


/* --- audio control and stream session ioctls ---- */

/* Opcode to open a device stream session to capture audio */
#define ADSP_AUDIO_IOCTL_CMD_OPEN_READ			0x0108dd79

/* Opcode to open a device stream session to render audio */
#define ADSP_AUDIO_IOCTL_CMD_OPEN_WRITE			0x0108dd7a

/* Opcode to open a device session, must open a device */
#define ADSP_AUDIO_IOCTL_CMD_OPEN_DEVICE		0x0108dd7b

/* Close an existing stream or device */
#define ADSP_AUDIO_IOCTL_CMD_CLOSE			0x0108d8bc



/* A device switch requires three IOCTL */
/* commands in the following sequence: PREPARE, STANDBY, COMMIT */

/* adsp_audio_device_switch_command structure is needed for */
/* DEVICE_SWITCH_PREPARE */

/* Device switch protocol step #1. Pause old device and */
/* generate silence for the old device. */
#define ADSP_AUDIO_IOCTL_CMD_DEVICE_SWITCH_PREPARE	0x010815c4

/* Device switch protocol step #2. Release old device, */
/* create new device and generate silence for the new device. */

/* When client receives ack for this IOCTL, the client can */
/* start sending IOCTL commands to configure, calibrate and */
/* change filter settings on the new device. */
#define ADSP_AUDIO_IOCTL_CMD_DEVICE_SWITCH_STANDBY	0x010815c5

/* Device switch protocol step #3. Start normal operations on new device */
#define ADSP_AUDIO_IOCTL_CMD_DEVICE_SWITCH_COMMIT	0x01075ee7

struct adsp_device_switch_command {
	struct adsp_command_hdr hdr;
	u32 old_device;
	u32 new_device;
	u8 device_class; /* 0 = i.rx, 1 = i.tx, 2 = e.rx, 3 = e.tx */
	u8 device_type; /* 0 = rx, 1 = tx, 2 = both */
} __attribute__ ((packed));



/* --- audio control session ioctls ---- */

#define ADSP_PATH_RX	0
#define ADSP_PATH_TX	1
#define ADSP_PATH_BOTH	2
#define ADSP_PATH_TX_CNG_DIS 3

struct adsp_audio_dtmf_start_command {
	struct adsp_command_hdr hdr;
	u32 tone1_hz;
	u32 tone2_hz;
	u32 duration_usec;
	s32 gain_mb;
} __attribute__ ((packed));

/* These commands will affect a logical device and all its associated */
/* streams. */

#define ADSP_AUDIO_MAX_EQ_BANDS 12

struct adsp_audio_eq_band {
	u16     band_idx; /* The band index, 0 .. 11 */
	u32     filter_type; /* Filter band type */
	u32     center_freq_hz; /* Filter band center frequency */
	s32     filter_gain; /* Filter band initial gain (dB) */
			/* Range is +12 dB to -12 dB with 1dB increments. */
	s32     q_factor;
		/* Filter band quality factor expressed as q-8 number, */
		/* e.g. 3000/(2^8) */
} __attribute__ ((packed));

struct adsp_audio_eq_stream_config {
	uint32_t  enable; /* Number of consequtive bands specified */
	uint32_t  num_bands;
	struct adsp_audio_eq_band  eq_bands[ADSP_AUDIO_MAX_EQ_BANDS];
} __attribute__ ((packed));

/* set device equalizer */
struct adsp_set_dev_equalizer_command {
	struct adsp_command_hdr hdr;
	u32    device_id;
	u32    enable;
	u32    num_bands;
	struct adsp_audio_eq_band eq_bands[ADSP_AUDIO_MAX_EQ_BANDS];
} __attribute__ ((packed));

/* Set device volume. */
#define ADSP_AUDIO_IOCTL_CMD_SET_DEVICE_VOL		0x0107605c

struct adsp_set_dev_volume_command {
	struct adsp_command_hdr hdr;
	u32 device_id;
	u32 path; /* 0 = rx, 1 = tx, 2 = both */
	s32 volume;
} __attribute__ ((packed));

/* Set Device stereo volume. This command has data payload, */
/* struct adsp_audio_set_dev_stereo_volume_command. */
#define ADSP_AUDIO_IOCTL_SET_DEVICE_STEREO_VOL		0x0108df3e

/* Set L, R cross channel gain for a Device. This command has */
/* data payload, struct adsp_audio_set_dev_x_chan_gain_command. */
#define ADSP_AUDIO_IOCTL_SET_DEVICE_XCHAN_GAIN		0x0108df40

/* Set device mute state. */
#define ADSP_AUDIO_IOCTL_CMD_SET_DEVICE_MUTE		0x0107605f

struct adsp_set_dev_mute_command {
	struct adsp_command_hdr hdr;
	u32 device_id;
	u32 path; /* 0 = rx, 1 = tx, 2 = both */
	u32 mute; /* 1 = mute */
} __attribute__ ((packed));

/* Configure Equalizer for a device. */
/* This command has payload struct adsp_audio_set_dev_equalizer_command. */
#define ADSP_AUDIO_IOCTL_CMD_SET_DEVICE_EQ_CONFIG	0x0108b10e

/* Set configuration data for an algorithm aspect of a device. */
/* This command has payload struct adsp_audio_set_dev_cfg_command. */
#define ADSP_AUDIO_IOCTL_SET_DEVICE_CONFIG		0x0108b6cb

struct adsp_set_dev_cfg_command {
	struct adsp_command_hdr hdr;
	u32 device_id;
	u32 block_id;
	u32 interface_id;
	u32 phys_addr;
	u32 phys_size;
	u32 phys_used;
} __attribute__ ((packed));

/* Set configuration data for all interfaces of a device. */
#define ADSP_AUDIO_IOCTL_SET_DEVICE_CONFIG_TABLE	0x0108b6bf

struct adsp_set_dev_cfg_table_command {
	struct adsp_command_hdr hdr;
	u32 device_id;
	u32 phys_addr;
	u32 phys_size;
	u32 phys_used;
} __attribute__ ((packed));

/* ---- audio stream data commands ---- */

#define ADSP_AUDIO_IOCTL_CMD_DATA_TX			0x0108dd7f
#define ADSP_AUDIO_IOCTL_CMD_DATA_RX			0x0108dd80

struct adsp_buffer_command {
	struct adsp_command_hdr hdr;
	struct adsp_audio_buffer buffer;
} __attribute__ ((packed));



/* ---- audio stream ioctls (only affect a single stream in a session) ---- */

/* Stop stream for audio device. */
#define ADSP_AUDIO_IOCTL_CMD_STREAM_STOP		0x01075c54

/* End of stream reached. Client will not send any more data. */
#define ADSP_AUDIO_IOCTL_CMD_STREAM_EOS			0x0108b150

/* Do sample slipping/stuffing on AAC outputs. The payload of */
/* this command is struct adsp_audio_slip_sample_command. */
#define ADSP_AUDIO_IOCTL_CMD_STREAM_SLIPSAMPLE		0x0108d40e

/* Set stream volume. */
/* This command has data payload, struct adsp_audio_set_volume_command. */
#define ADSP_AUDIO_IOCTL_CMD_SET_STREAM_VOL		0x0108c0de

/* Set stream stereo volume. This command has data payload, */
/* struct adsp_audio_set_stereo_volume_command. */
#define ADSP_AUDIO_IOCTL_SET_STREAM_STEREO_VOL		0x0108dd7c

/* Set L, R cross channel gain for a Stream. This command has */
/* data payload, struct adsp_audio_set_x_chan_gain_command. */
#define ADSP_AUDIO_IOCTL_SET_STREAM_XCHAN_GAIN		0x0108dd7d

/* Set stream mute state. */
/* This command has data payload, struct adsp_audio_set_stream_mute. */
#define ADSP_AUDIO_IOCTL_CMD_SET_STREAM_MUTE		0x0108c0df

/* Reconfigure bit rate information. This command has data */
/* payload, struct adsp_audio_set_bit_rate_command */
#define ADSP_AUDIO_IOCTL_SET_STREAM_BITRATE		0x0108ccf1

/* Set Channel Mapping. This command has data payload, struct */
/* This command has data payload struct adsp_audio_set_channel_map_command. */
#define ADSP_AUDIO_IOCTL_SET_STREAM_CHANNELMAP		0x0108d32a

/* Enable/disable AACPlus SBR. */
/* This command has data payload struct adsp_audio_set_sbr_command */
#define ADSP_AUDIO_IOCTL_SET_STREAM_SBR			0x0108d416

/* Enable/disable WMA Pro Chex and Fex. This command has data payload */
/* struct adsp_audio_stream_set_wma_command. */
#define ADSP_AUDIO_IOCTL_SET_STREAM_WMAPRO		0x0108d417


/* ---- audio session ioctls (affect all streams in a session) --- */

/* Start stream for audio device. */
#define ADSP_AUDIO_IOCTL_CMD_SESSION_START		0x010815c6

/* Stop all stream(s) for audio session as indicated by major id. */
#define ADSP_AUDIO_IOCTL_CMD_SESSION_STOP		0x0108dd7e

/* Pause the data flow for a session as indicated by major id. */
#define ADSP_AUDIO_IOCTL_CMD_SESSION_PAUSE		0x01075ee8

/* Resume the data flow for a session as indicated by major id. */
#define ADSP_AUDIO_IOCTL_CMD_SESSION_RESUME		0x01075ee9

/* Drop any unprocessed data buffers for a session as indicated by major id. */
#define ADSP_AUDIO_IOCTL_CMD_SESSION_FLUSH		0x01075eea

/* Start Stream DTMF tone */
#define ADSP_AUDIO_IOCTL_CMD_SESSION_DTMF_START		0x0108c0dd

/* Stop Stream DTMF tone */
#define ADSP_AUDIO_IOCTL_CMD_SESSION_DTMF_STOP		0x01087554

/* Set Session volume. */
/* This command has data payload, struct adsp_audio_set_volume_command. */
#define ADSP_AUDIO_IOCTL_SET_SESSION_VOL		0x0108d8bd

/* Set session stereo volume. This command has data payload, */
/* struct adsp_audio_set_stereo_volume_command. */
#define ADSP_AUDIO_IOCTL_SET_SESSION_STEREO_VOL		0x0108df3d

/* Set L, R cross channel gain for a session. This command has */
/* data payload, struct adsp_audio_set_x_chan_gain_command. */
#define ADSP_AUDIO_IOCTL_SET_SESSION_XCHAN_GAIN		0x0108df3f

/* Set Session mute state. */
/* This command has data payload, struct adsp_audio_set_mute_command. */
#define ADSP_AUDIO_IOCTL_SET_SESSION_MUTE		0x0108d8be

/* Configure Equalizer for a stream. */
/* This command has payload struct adsp_audio_set_equalizer_command. */
#define ADSP_AUDIO_IOCTL_SET_SESSION_EQ_CONFIG		0x0108c0e0

/* Set Audio Video sync information. */
/* This command has data payload, struct adsp_audio_set_av_sync_command. */
#define ADSP_AUDIO_IOCTL_SET_SESSION_AVSYNC		0x0108d1e2

/* Get Audio Media Session time. */
/* This command returns the audioTime in adsp_audio_unsigned64_event */
#define ADSP_AUDIO_IOCTL_CMD_GET_AUDIO_TIME		0x0108c26c


/* these command structures are used for both STREAM and SESSION ioctls */

struct adsp_set_volume_command {
	struct adsp_command_hdr hdr;
	s32 volume;
} __attribute__ ((packed));
	
struct adsp_set_mute_command {
	struct adsp_command_hdr hdr;
	u32 mute; /* 1 == mute */
} __attribute__ ((packed));


struct adsp_set_equalizer_command {
	struct adsp_command_hdr hdr;
	u32    enable;
	u32    num_bands;
	struct adsp_audio_eq_band eq_bands[ADSP_AUDIO_MAX_EQ_BANDS];
} __attribute__ ((packed));

/* ---- audio events ---- */

/* All IOCTL commands generate an event with the IOCTL opcode as the */
/* event id after the IOCTL command has been executed. */

/* This event is generated after a media stream session is opened. */
#define ADSP_AUDIO_EVT_STATUS_OPEN				0x0108c0d6

/* This event is generated after a media stream  session is closed. */
#define ADSP_AUDIO_EVT_STATUS_CLOSE				0x0108c0d7

/* Asyncronous buffer consumption. This event is generated after a */
/* recived  buffer is consumed during rendering or filled during */
/* capture opeartion. */
#define ADSP_AUDIO_EVT_STATUS_BUF_DONE				0x0108c0d8

/* This event is generated when rendering operation is starving for */
/* data. In order to avoid audio loss at the end of a plauback, the */
/* client should wait for this event before issuing the close command. */
#define ADSP_AUDIO_EVT_STATUS_BUF_UNDERRUN			0x0108c0d9

/* This event is generated during capture operation when there are no */
/* buffers available to copy the captured audio data */
#define ADSP_AUDIO_EVT_STATUS_BUF_OVERFLOW			0x0108c0da

/* This asynchronous event is generated as a result of an input */
/* sample rate change and/or channel mode change detected by the */
/* decoder. The event payload data is an array of 2 uint32 */
/* values containing the sample rate in Hz and channel mode. */
#define ADSP_AUDIO_EVT_SR_CM_CHANGE				0x0108d329

struct adsp_event_hdr {
	u32 evt_handle;		/* DAL common header */
	u32 evt_cookie;
	u32 evt_length;

	u32 src;		/* "source" audio address */
	u32 dst;		/* "destination" audio address */

	u32 event_id;
	u32 response_type;
	u32 seq_number;

	u32 context;		/* opaque to DSP */
	u32 data;

	u32 status;
} __attribute__ ((packed));

struct adsp_buffer_event {
	struct adsp_event_hdr hdr;
	struct adsp_audio_buffer buffer;
} __attribute__ ((packed));


/* ---- audio device IDs ---- */

/* Device direction Rx/Tx flag */
#define ADSP_AUDIO_RX_DEVICE		0x00
#define ADSP_AUDIO_TX_DEVICE		0x01

/* Default RX or TX device */
#define ADSP_AUDIO_DEVICE_ID_DEFAULT		0x1081679

/* Source (TX) devices */
#define ADSP_AUDIO_DEVICE_ID_HANDSET_MIC	0x107ac8d
#define ADSP_AUDIO_DEVICE_ID_HEADSET_MIC	0x1081510
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MIC	0x1081512
#define ADSP_AUDIO_DEVICE_ID_BT_SCO_MIC		0x1081518
#define ADSP_AUDIO_DEVICE_ID_AUXPCM_TX		0x1081518
#define ADSP_AUDIO_DEVICE_ID_TTY_HEADSET_MIC	0x108151b
#define ADSP_AUDIO_DEVICE_ID_I2S_MIC		0x1089bf3

#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_DUAL_MIC	0x108f9c5
#define ADSP_AUDIO_DEVICE_ID_HANDSET_DUAL_MIC		0x108f9c3

/* Special loopback pseudo device to be paired with an RX device */
/* with usage ADSP_AUDIO_DEVICE_USAGE_MIXED_PCM_LOOPBACK */
#define ADSP_AUDIO_DEVICE_ID_MIXED_PCM_LOOPBACK_TX	0x1089bf2

/* Sink (RX) devices */
#define ADSP_AUDIO_DEVICE_ID_HANDSET_SPKR			0x107ac88
#define ADSP_AUDIO_DEVICE_ID_HEADSET_SPKR_MONO			0x1081511
#define ADSP_AUDIO_DEVICE_ID_HEADSET_SPKR_STEREO		0x107ac8a
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO			0x1081513
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO_W_MONO_HEADSET     0x108c508
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_MONO_W_STEREO_HEADSET   0x108c894
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_STEREO			0x1081514
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_STEREO_W_MONO_HEADSET   0x108c895
#define ADSP_AUDIO_DEVICE_ID_SPKR_PHONE_STEREO_W_STEREO_HEADSET	0x108c509
#define ADSP_AUDIO_DEVICE_ID_BT_SCO_SPKR			0x1081519
#define ADSP_AUDIO_DEVICE_ID_AUXPCM_RX				0x1081519
#define ADSP_AUDIO_DEVICE_ID_TTY_HEADSET_SPKR			0x108151c
#define ADSP_AUDIO_DEVICE_ID_I2S_SPKR				0x1089bf4
#define ADSP_AUDIO_DEVICE_ID_NULL_SINK				0x108e512

/* BT A2DP playback device. */
/* This device must be paired with */
/* ADSP_AUDIO_DEVICE_ID_MIXED_PCM_LOOPBACK_TX using  */
/* ADSP_AUDIO_DEVICE_USAGE_MIXED_PCM_LOOPBACK mode */
#define ADSP_AUDIO_DEVICE_ID_BT_A2DP_SPKR	0x108151a

/* Voice Destination identifier - specifically used for */
/* controlling Voice module from the Device Control Session */
#define ADSP_AUDIO_DEVICE_ID_VOICE		0x0108df3c

/*  Audio device usage types. */
/*  This is a bit mask to determine which topology to use in the */
/* device session */
#define ADSP_AUDIO_DEVICE_CONTEXT_VOICE			0x01
#define ADSP_AUDIO_DEVICE_CONTEXT_PLAYBACK		0x02
#define ADSP_AUDIO_DEVICE_CONTEXT_MIXED_RECORD		0x10
#define ADSP_AUDIO_DEVICE_CONTEXT_RECORD		0x20
#define ADSP_AUDIO_DEVICE_CONTEXT_PCM_LOOPBACK		0x40

/* ADSP audio driver return codes */
#define ADSP_AUDIO_STATUS_SUCCESS               0
#define ADSP_AUDIO_STATUS_EUNSUPPORTED          20

#endif
