#ifndef __MACH_QDSP5_V2_QDSP5AUDPPCMDI_H
#define __MACH_QDSP5_V2_QDSP5AUDPPCMDI_H

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

    A U D I O   P O S T   P R O C E S S I N G  I N T E R N A L  C O M M A N D S

GENERAL DESCRIPTION
  This file contains defintions of format blocks of commands
  that are accepted by AUDPP Task

REFERENCES
  None

EXTERNALIZED FUNCTIONS
  None

Copyright(c) 1992-2011, The Linux Foundation. All rights reserved.

This software is licensed under the terms of the GNU General Public
License version 2, as published by the Free Software Foundation, and
may be copied, distributed, and modified under those terms.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*
 * ARM to AUDPPTASK Commands
 *
 * ARM uses three command queues to communicate with AUDPPTASK
 * 1)uPAudPPCmd1Queue : Used for more frequent and shorter length commands
 * 	Location : MEMA
 * 	Buffer Size : 6 words
 * 	No of buffers in a queue : 20 for gaming audio and 5 for other images
 * 2)uPAudPPCmd2Queue : Used for commands which are not much lengthier
 * 	Location : MEMA
 * 	Buffer Size : 23
 * 	No of buffers in a queue : 2
 * 3)uPAudOOCmd3Queue : Used for lengthier and more frequent commands
 * 	Location : MEMA
 * 	Buffer Size : 145
 * 	No of buffers in a queue : 3
 */

/*
 * Commands Related to uPAudPPCmd1Queue
 */

/*
 * Command Structure to enable or disable the active decoders
 */

#define AUDPP_CMD_CFG_DEC_TYPE 		0x0001
#define AUDPP_CMD_CFG_DEC_TYPE_LEN 	sizeof(struct audpp_cmd_cfg_dec_type)

/* Enable the decoder */
#define AUDPP_CMD_DEC_TYPE_M           	0x000F

#define AUDPP_CMD_ENA_DEC_V         	0x4000
#define AUDPP_CMD_DIS_DEC_V        	0x0000
#define AUDPP_CMD_DEC_STATE_M          	0x4000

#define AUDPP_CMD_UPDATDE_CFG_DEC	0x8000
#define AUDPP_CMD_DONT_UPDATE_CFG_DEC	0x0000


/* Type specification of cmd_cfg_dec */

struct audpp_cmd_cfg_dec_type {
	unsigned short cmd_id;
	unsigned short stream_id;
	unsigned short dec_cfg;
	unsigned short dm_mode;
} __attribute__((packed));

/*
 * Command Structure to Pause , Resume and flushes the selected audio decoders
 */

#define AUDPP_CMD_DEC_CTRL		0x0002
#define AUDPP_CMD_DEC_CTRL_LEN		sizeof(struct audpp_cmd_dec_ctrl)

/* Decoder control commands for pause, resume and flush */
#define AUDPP_CMD_FLUSH_V         		0x2000

#define AUDPP_CMD_PAUSE_V		        0x4000
#define AUDPP_CMD_RESUME_V		        0x0000

#define AUDPP_CMD_UPDATE_V		        0x8000
#define AUDPP_CMD_IGNORE_V		        0x0000


/* Type Spec for decoder control command*/

struct audpp_cmd_dec_ctrl{
	unsigned short cmd_id;
	unsigned short stream_id;
	unsigned short dec_ctrl;
} __attribute__((packed));

/*
 * Command Structure to Configure the AVSync FeedBack Mechanism
 */

#define AUDPP_CMD_AVSYNC	0x0003
#define AUDPP_CMD_AVSYNC_LEN	sizeof(struct audpp_cmd_avsync)

struct audpp_cmd_avsync{
	unsigned short cmd_id;
	unsigned short stream_id;
	unsigned short interrupt_interval;
	unsigned short sample_counter_dlsw;
	unsigned short sample_counter_dmsw;
	unsigned short sample_counter_msw;
	unsigned short byte_counter_dlsw;
	unsigned short byte_counter_dmsw;
	unsigned short byte_counter_msw;
} __attribute__((packed));

/*
 * Macros used to store the AV Sync Info from DSP
 */

#define AUDPP_AVSYNC_CH_COUNT 1
#define AUDPP_AVSYNC_NUM_WORDS 6
/* Timeout of 3000ms for AV Sync Query response */
#define AUDPP_AVSYNC_EVENT_TIMEOUT 3000

/*
 * Command Structure to Query AVSync Info from DSP
 */

#define AUDPP_CMD_QUERY_AVSYNC	0x0006

struct audpp_cmd_query_avsync{
	unsigned short cmd_id;
	unsigned short stream_id;
} __attribute__((packed));

/*
 * Command Structure to enable or disable(sleep) the AUDPPTASK
 */

#define AUDPP_CMD_CFG	0x0004
#define AUDPP_CMD_CFG_LEN	sizeof(struct audpp_cmd_cfg)

#define AUDPP_CMD_CFG_SLEEP   				0x0000
#define AUDPP_CMD_CFG_ENABLE  				0xFFFF

struct audpp_cmd_cfg {
	unsigned short cmd_id;
	unsigned short cfg;
} __attribute__((packed));

/*
 * Command Structure to Inject or drop the specified no of samples
 */

#define AUDPP_CMD_ADJUST_SAMP		0x0005
#define AUDPP_CMD_ADJUST_SAMP_LEN	sizeof(struct audpp_cmd_adjust_samp)

#define AUDPP_CMD_SAMP_DROP		-1
#define AUDPP_CMD_SAMP_INSERT		0x0001

#define AUDPP_CMD_NUM_SAMPLES		0x0001

struct audpp_cmd_adjust_samp {
	unsigned short cmd_id;
	unsigned short object_no;
	signed short sample_insert_or_drop;
	unsigned short num_samples;
} __attribute__((packed));

/*
 * Command Structure to Configure AVSync Feedback Mechanism
 */

#define AUDPP_CMD_ROUTING_MODE      0x0007
#define AUDPP_CMD_ROUTING_MODE_LEN  \
sizeof(struct audpp_cmd_routing_mode)

struct audpp_cmd_routing_mode {
	unsigned short cmd_id;
	unsigned short object_number;
	unsigned short routing_mode;
} __attribute__((packed));

/*
 * Commands Related to uPAudPPCmd2Queue
 */

/*
 * Command Structure to configure Per decoder Parameters (Common)
 */

#define AUDPP_CMD_CFG_ADEC_PARAMS 		0x0000
#define AUDPP_CMD_CFG_ADEC_PARAMS_COMMON_LEN	\
	sizeof(struct audpp_cmd_cfg_adec_params_common)

#define AUDPP_CMD_STATUS_MSG_FLAG_ENA_FCM	0x4000
#define AUDPP_CMD_STATUS_MSG_FLAG_DIS_FCM	0x0000

#define AUDPP_CMD_STATUS_MSG_FLAG_ENA_DCM	0x8000
#define AUDPP_CMD_STATUS_MSG_FLAG_DIS_DCM	0x0000

/* Sampling frequency*/
#define  AUDPP_CMD_SAMP_RATE_96000 	0x0000
#define  AUDPP_CMD_SAMP_RATE_88200 	0x0001
#define  AUDPP_CMD_SAMP_RATE_64000 	0x0002
#define  AUDPP_CMD_SAMP_RATE_48000 	0x0003
#define  AUDPP_CMD_SAMP_RATE_44100 	0x0004
#define  AUDPP_CMD_SAMP_RATE_32000 	0x0005
#define  AUDPP_CMD_SAMP_RATE_24000 	0x0006
#define  AUDPP_CMD_SAMP_RATE_22050 	0x0007
#define  AUDPP_CMD_SAMP_RATE_16000 	0x0008
#define  AUDPP_CMD_SAMP_RATE_12000 	0x0009
#define  AUDPP_CMD_SAMP_RATE_11025 	0x000A
#define  AUDPP_CMD_SAMP_RATE_8000  	0x000B


/*
 * Type specification of cmd_adec_cfg sent to all decoder
 */

struct audpp_cmd_cfg_adec_params_common {
	unsigned short  cmd_id;
	unsigned short  dec_id;
	unsigned short  length;
	unsigned short  reserved;
	unsigned short  input_sampling_frequency;
} __attribute__((packed));

/*
 * Command Structure to configure Per decoder Parameters (Wav)
 */

#define AUDPP_CMD_CFG_ADEC_PARAMS_WAV_LEN \
	sizeof(struct audpp_cmd_cfg_adec_params_wav)


#define	AUDPP_CMD_WAV_STEREO_CFG_MONO	0x0001
#define AUDPP_CMD_WAV_STEREO_CFG_STEREO	0x0002

#define AUDPP_CMD_WAV_PCM_WIDTH_8	0x0000
#define AUDPP_CMD_WAV_PCM_WIDTH_16	0x0001
#define AUDPP_CMD_WAV_PCM_WIDTH_24	0x0002

struct audpp_cmd_cfg_adec_params_wav {
	struct audpp_cmd_cfg_adec_params_common		common;
	unsigned short					stereo_cfg;
	unsigned short					pcm_width;
	unsigned short 					sign;
} __attribute__((packed));

/*
 *  Command Structure for CMD_CFG_DEV_MIXER
 */

#define AUDPP_CMD_CFG_DEV_MIXER_PARAMS_LEN \
	sizeof(struct audpp_cmd_cfg_dev_mixer_params)

#define AUDPP_CMD_CFG_DEV_MIXER            0x0008

#define AUDPP_CMD_CFG_DEV_MIXER_ID_0       0
#define AUDPP_CMD_CFG_DEV_MIXER_ID_1       1
#define AUDPP_CMD_CFG_DEV_MIXER_ID_2       2
#define AUDPP_CMD_CFG_DEV_MIXER_ID_3       3
#define AUDPP_CMD_CFG_DEV_MIXER_ID_4       4
#define AUDPP_CMD_CFG_DEV_MIXER_ID_5       5

#define AUDPP_CMD_CFG_DEV_MIXER_DEV_NONE   0x0000
#define AUDPP_CMD_CFG_DEV_MIXER_DEV_0      \
				(0x1 << AUDPP_CMD_CFG_DEV_MIXER_ID_0)
#define AUDPP_CMD_CFG_DEV_MIXER_DEV_1      \
				(0x1 << AUDPP_CMD_CFG_DEV_MIXER_ID_1)
#define AUDPP_CMD_CFG_DEV_MIXER_DEV_2      \
				(0x1 << AUDPP_CMD_CFG_DEV_MIXER_ID_2)
#define AUDPP_CMD_CFG_DEV_MIXER_DEV_3      \
				(0x1 << AUDPP_CMD_CFG_DEV_MIXER_ID_3)
#define AUDPP_CMD_CFG_DEV_MIXER_DEV_4      \
				(0x1 << AUDPP_CMD_CFG_DEV_MIXER_ID_4)
#define AUDPP_CMD_CFG_DEV_MIXER_DEV_5      \
				(0x1 << AUDPP_CMD_CFG_DEV_MIXER_ID_5)

struct audpp_cmd_cfg_dev_mixer_params {
	unsigned short cmd_id;
	unsigned short stream_id;
	unsigned short mixer_cmd;
} __attribute__((packed));


/*
 * Command Structure to configure Per decoder Parameters (ADPCM)
 */

#define AUDPP_CMD_CFG_ADEC_PARAMS_ADPCM_LEN \
	sizeof(struct audpp_cmd_cfg_adec_params_adpcm)


#define	AUDPP_CMD_ADPCM_STEREO_CFG_MONO		0x0001
#define AUDPP_CMD_ADPCM_STEREO_CFG_STEREO	0x0002

struct audpp_cmd_cfg_adec_params_adpcm {
	struct audpp_cmd_cfg_adec_params_common		common;
	unsigned short					stereo_cfg;
	unsigned short 					block_size;
} __attribute__((packed));

/*
 * Command Structure to configure Per decoder Parameters (WMA)
 */

#define AUDPP_CMD_CFG_ADEC_PARAMS_WMA_LEN	\
	sizeof(struct audpp_cmd_cfg_adec_params_wma)

struct audpp_cmd_cfg_adec_params_wma {
	struct audpp_cmd_cfg_adec_params_common    common;
	unsigned short 	armdatareqthr;
	unsigned short 	channelsdecoded;
	unsigned short 	wmabytespersec;
	unsigned short	wmasamplingfreq;
	unsigned short	wmaencoderopts;
} __attribute__((packed));


/*
 * Command Structure to configure Per decoder Parameters (MP3)
 */

#define AUDPP_CMD_CFG_ADEC_PARAMS_MP3_LEN	\
	sizeof(struct audpp_cmd_cfg_adec_params_mp3)

struct audpp_cmd_cfg_adec_params_mp3 {
	struct audpp_cmd_cfg_adec_params_common    common;
} __attribute__((packed));


/*
 * Command Structure to configure Per decoder Parameters (AAC)
 */

#define AUDPP_CMD_CFG_ADEC_PARAMS_AAC_LEN	\
	sizeof(struct audpp_cmd_cfg_adec_params_aac)


#define AUDPP_CMD_AAC_FORMAT_ADTS		-1
#define	AUDPP_CMD_AAC_FORMAT_RAW		0x0000
#define	AUDPP_CMD_AAC_FORMAT_PSUEDO_RAW		0x0001
#define	AUDPP_CMD_AAC_FORMAT_LOAS		0x0002

#define AUDPP_CMD_AAC_AUDIO_OBJECT_LC		0x0002
#define AUDPP_CMD_AAC_AUDIO_OBJECT_LTP		0x0004
#define AUDPP_CMD_AAC_AUDIO_OBJECT_ERLC	0x0011

#define AUDPP_CMD_AAC_SBR_ON_FLAG_ON		0x0001
#define AUDPP_CMD_AAC_SBR_ON_FLAG_OFF		0x0000

#define AUDPP_CMD_AAC_SBR_PS_ON_FLAG_ON		0x0001
#define AUDPP_CMD_AAC_SBR_PS_ON_FLAG_OFF	0x0000

struct audpp_cmd_cfg_adec_params_aac {
	struct audpp_cmd_cfg_adec_params_common	common;
	signed short			format;
	unsigned short			audio_object;
	unsigned short			ep_config;
	unsigned short                  aac_section_data_resilience_flag;
	unsigned short                  aac_scalefactor_data_resilience_flag;
	unsigned short                  aac_spectral_data_resilience_flag;
	unsigned short                  sbr_on_flag;
	unsigned short                  sbr_ps_on_flag;
	unsigned short                  channel_configuration;
} __attribute__((packed));

/*
 * Command Structure to configure Per decoder Parameters (V13K)
 */

#define AUDPP_CMD_CFG_ADEC_PARAMS_V13K_LEN	\
	sizeof(struct audpp_cmd_cfg_adec_params_v13k)


#define AUDPP_CMD_STEREO_CFG_MONO		0x0001
#define AUDPP_CMD_STEREO_CFG_STEREO		0x0002

struct audpp_cmd_cfg_adec_params_v13k {
	struct audpp_cmd_cfg_adec_params_common    	common;
	unsigned short			stereo_cfg;
} __attribute__((packed));

#define AUDPP_CMD_CFG_ADEC_PARAMS_EVRC_LEN \
	sizeof(struct audpp_cmd_cfg_adec_params_evrc)

struct audpp_cmd_cfg_adec_params_evrc {
	struct audpp_cmd_cfg_adec_params_common common;
	unsigned short stereo_cfg;
} __attribute__ ((packed));

/*
 * Command Structure to configure Per decoder Parameters (AMRWB)
 */

#define AUDPP_CMD_CFG_ADEC_PARAMS_AMRWB_LEN \
	sizeof(struct audpp_cmd_cfg_adec_params_amrwb)

struct audpp_cmd_cfg_adec_params_amrwb {
	struct audpp_cmd_cfg_adec_params_common    	common;
	unsigned short			stereo_cfg;
} __attribute__((packed));

/*
 * Command Structure to configure Per decoder Parameters (WMAPRO)
 */

#define AUDPP_CMD_CFG_ADEC_PARAMS_WMAPRO_LEN	\
	sizeof(struct audpp_cmd_cfg_adec_params_wmapro)

struct audpp_cmd_cfg_adec_params_wmapro {
	struct audpp_cmd_cfg_adec_params_common    common;
	unsigned short 	armdatareqthr;
	uint8_t         validbitspersample;
	uint8_t         numchannels;
	unsigned short  formattag;
	unsigned short  samplingrate;
	unsigned short  avgbytespersecond;
	unsigned short  asfpacketlength;
	unsigned short 	channelmask;
	unsigned short 	encodeopt;
	unsigned short	advancedencodeopt;
	uint32_t	advancedencodeopt2;
} __attribute__((packed));

/*
 * Command Structure to configure the  HOST PCM interface
 */

#define AUDPP_CMD_PCM_INTF	0x0001
#define AUDPP_CMD_PCM_INTF_2	0x0002
#define AUDPP_CMD_PCM_INTF_LEN	sizeof(struct audpp_cmd_pcm_intf)

#define AUDPP_CMD_PCM_INTF_MONO_V		        0x0001
#define AUDPP_CMD_PCM_INTF_STEREO_V         	0x0002

/* These two values differentiate the two types of commands that could be issued
 * Interface configuration command and Buffer update command */

#define AUDPP_CMD_PCM_INTF_CONFIG_CMD_V	       	0x0000
#define AUDPP_CMD_PCM_INTF_BUFFER_CMD_V	        -1

#define AUDPP_CMD_PCM_INTF_RX_ENA_M              0x000F
#define AUDPP_CMD_PCM_INTF_RX_ENA_ARMTODSP_V     0x0008
#define AUDPP_CMD_PCM_INTF_RX_ENA_DSPTOARM_V     0x0004

/* These flags control the enabling and disabling of the interface together
 *  with host interface bit mask. */

#define AUDPP_CMD_PCM_INTF_ENA_V            -1
#define AUDPP_CMD_PCM_INTF_DIS_V            0x0000


#define  AUDPP_CMD_PCM_INTF_FULL_DUPLEX           0x0
#define  AUDPP_CMD_PCM_INTF_HALF_DUPLEX_TODSP     0x1


#define  AUDPP_CMD_PCM_INTF_OBJECT_NUM           0x5
#define  AUDPP_CMD_PCM_INTF_COMMON_OBJECT_NUM    0x6

struct audpp_cmd_pcm_intf {
	unsigned short  cmd_id;
	unsigned short  stream;
	unsigned short  stream_id;
	signed short  config;
	unsigned short  intf_type;

	/* DSP -> ARM Configuration */
	unsigned short  read_buf1LSW;
	unsigned short  read_buf1MSW;
	unsigned short  read_buf1_len;

	unsigned short  read_buf2LSW;
	unsigned short  read_buf2MSW;
	unsigned short  read_buf2_len;
	/*   0:HOST_PCM_INTF disable
	**  0xFFFF: HOST_PCM_INTF enable
	*/
	signed short  dsp_to_arm_flag;
	unsigned short  partition_number;

	/* ARM -> DSP Configuration */
	unsigned short  write_buf1LSW;
	unsigned short  write_buf1MSW;
	unsigned short  write_buf1_len;

	unsigned short  write_buf2LSW;
	unsigned short  write_buf2MSW;
	unsigned short  write_buf2_len;

	/*   0:HOST_PCM_INTF disable
	**  0xFFFF: HOST_PCM_INTF enable
	*/
	signed short  arm_to_rx_flag;
	unsigned short  weight_decoder_to_rx;
	unsigned short  weight_arm_to_rx;

	unsigned short  partition_number_arm_to_dsp;
	unsigned short  sample_rate;
	unsigned short  channel_mode;
} __attribute__((packed));

/*
 **  BUFFER UPDATE COMMAND
 */
#define AUDPP_CMD_PCM_INTF_SEND_BUF_PARAMS_LEN	\
	sizeof(struct audpp_cmd_pcm_intf_send_buffer)

struct audpp_cmd_pcm_intf_send_buffer {
	unsigned short  cmd_id;
	unsigned short  stream;
	unsigned short  stream_id;
	/* set config = 0xFFFF for configuration*/
	signed short  config;
	unsigned short  intf_type;
	unsigned short  dsp_to_arm_buf_id;
	unsigned short  arm_to_dsp_buf_id;
	unsigned short  arm_to_dsp_buf_len;
} __attribute__((packed));


/*
 * Commands Related to uPAudPPCmd3Queue
 */

/*
 * Command Structure to configure post processing params (Commmon)
 */

#define AUDPP_CMD_CFG_OBJECT_PARAMS		0x0000
#define AUDPP_CMD_CFG_OBJECT_PARAMS_COMMON_LEN		\
	sizeof(struct audpp_cmd_cfg_object_params_common)

#define AUDPP_CMD_OBJ0_UPDATE		0x8000
#define AUDPP_CMD_OBJ0_DONT_UPDATE	0x0000


#define AUDPP_CMD_OBJ2_UPDATE		0x8000
#define AUDPP_CMD_OBJ2_DONT_UPDATE	0x0000

#define AUDPP_CMD_OBJ3_UPDATE		0x8000
#define AUDPP_CMD_OBJ3_DONT_UPDATE	0x0000

#define AUDPP_CMD_OBJ4_UPDATE		0x8000
#define AUDPP_CMD_OBJ4_DONT_UPDATE	0x0000

#define AUDPP_CMD_HPCM_UPDATE		0x8000
#define AUDPP_CMD_HPCM_DONT_UPDATE	0x0000

#define AUDPP_CMD_COMMON_CFG_UPDATE		0x8000
#define AUDPP_CMD_COMMON_CFG_DONT_UPDATE	0x0000

#define AUDPP_CMD_POPP_STREAM   0xFFFF
#define AUDPP_CMD_COPP_STREAM   0x0000

struct audpp_cmd_cfg_object_params_common{
	unsigned short  cmd_id;
	unsigned short	stream;
	unsigned short	stream_id;
	unsigned short	obj_cfg;
	unsigned short	command_type;
} __attribute__((packed));

/*
 * Command Structure to configure post processing params (Volume)
 */
#define AUDPP_CMD_VOLUME_PAN		0
#define AUDPP_CMD_CFG_OBJECT_PARAMS_VOLUME_LEN		\
	sizeof(struct audpp_cmd_cfg_object_params_volume)

struct audpp_cmd_cfg_object_params_volume {
	struct audpp_cmd_cfg_object_params_common 	common;
	unsigned short					volume;
	unsigned short					pan;
} __attribute__((packed));

/*
 * Command Structure to configure post processing params (PCM Filter)
 */

struct numerator {
	unsigned short			numerator_b0_filter_lsw;
	unsigned short			numerator_b0_filter_msw;
	unsigned short			numerator_b1_filter_lsw;
	unsigned short			numerator_b1_filter_msw;
	unsigned short			numerator_b2_filter_lsw;
	unsigned short			numerator_b2_filter_msw;
} __attribute__((packed));

struct denominator {
	unsigned short			denominator_a0_filter_lsw;
	unsigned short			denominator_a0_filter_msw;
	unsigned short			denominator_a1_filter_lsw;
	unsigned short			denominator_a1_filter_msw;
} __attribute__((packed));

struct shift_factor {
	unsigned short			shift_factor_0;
} __attribute__((packed));

struct pan {
	unsigned short			pan_filter_0;
} __attribute__((packed));

struct filter_1 {
	struct numerator		numerator_filter;
	struct denominator		denominator_filter;
	struct shift_factor		shift_factor_filter;
	struct pan			pan_filter;
} __attribute__((packed));

struct filter_2 {
	struct numerator		numerator_filter[2];
	struct denominator		denominator_filter[2];
	struct shift_factor		shift_factor_filter[2];
	struct pan			pan_filter[2];
} __attribute__((packed));

struct filter_3 {
	struct numerator		numerator_filter[3];
	struct denominator		denominator_filter[3];
	struct shift_factor		shift_factor_filter[3];
	struct pan			pan_filter[3];
} __attribute__((packed));

struct filter_4 {
	struct numerator		numerator_filter[4];
	struct denominator		denominator_filter[4];
	struct shift_factor		shift_factor_filter[4];
	struct pan			pan_filter[4];
} __attribute__((packed));

#define AUDPP_CMD_IIR_TUNING_FILTER	1
#define AUDPP_CMD_CFG_OBJECT_PARAMS_PCM_LEN		\
	sizeof(struct audpp_cmd_cfg_object_params_pcm)


struct audpp_cmd_cfg_object_params_pcm {
	struct audpp_cmd_cfg_object_params_common 	common;
	signed short				active_flag;
	unsigned short 				num_bands;
	union {
		struct filter_1			filter_1_params;
		struct filter_2			filter_2_params;
		struct filter_3			filter_3_params;
		struct filter_4			filter_4_params;
	} __attribute__((packed)) params_filter;
} __attribute__((packed));

#define AUDPP_CMD_CALIB_GAIN_RX         15
#define AUDPP_CMD_CFG_CAL_GAIN_LEN sizeof(struct audpp_cmd_cfg_cal_gain)


struct audpp_cmd_cfg_cal_gain {
	struct audpp_cmd_cfg_object_params_common common;
	unsigned short audppcalgain;
	unsigned short reserved;
} __attribute__((packed));


/*
 * Command Structure to configure post processing parameters (equalizer)
 */
#define AUDPP_CMD_EQUALIZER		2
#define AUDPP_CMD_CFG_OBJECT_PARAMS_EQALIZER_LEN		\
	sizeof(struct audpp_cmd_cfg_object_params_eqalizer)

struct eq_numerator {
	unsigned short			numerator_coeff_0_lsw;
	unsigned short			numerator_coeff_0_msw;
	unsigned short			numerator_coeff_1_lsw;
	unsigned short			numerator_coeff_1_msw;
	unsigned short			numerator_coeff_2_lsw;
	unsigned short			numerator_coeff_2_msw;
} __attribute__((packed));

struct eq_denominator {
	unsigned short			denominator_coeff_0_lsw;
	unsigned short			denominator_coeff_0_msw;
	unsigned short			denominator_coeff_1_lsw;
	unsigned short			denominator_coeff_1_msw;
} __attribute__((packed));

struct eq_shiftfactor {
	unsigned short			shift_factor;
} __attribute__((packed));

struct eq_coeff_1 {
	struct eq_numerator	numerator;
	struct eq_denominator	denominator;
	struct eq_shiftfactor	shiftfactor;
} __attribute__((packed));

struct eq_coeff_2 {
	struct eq_numerator	numerator[2];
	struct eq_denominator	denominator[2];
	struct eq_shiftfactor	shiftfactor[2];
} __attribute__((packed));

struct eq_coeff_3 {
	struct eq_numerator	numerator[3];
	struct eq_denominator	denominator[3];
	struct eq_shiftfactor	shiftfactor[3];
} __attribute__((packed));

struct eq_coeff_4 {
	struct eq_numerator	numerator[4];
	struct eq_denominator	denominator[4];
	struct eq_shiftfactor	shiftfactor[4];
} __attribute__((packed));

struct eq_coeff_5 {
	struct eq_numerator	numerator[5];
	struct eq_denominator	denominator[5];
	struct eq_shiftfactor	shiftfactor[5];
} __attribute__((packed));

struct eq_coeff_6 {
	struct eq_numerator	numerator[6];
	struct eq_denominator	denominator[6];
	struct eq_shiftfactor	shiftfactor[6];
} __attribute__((packed));

struct eq_coeff_7 {
	struct eq_numerator	numerator[7];
	struct eq_denominator	denominator[7];
	struct eq_shiftfactor	shiftfactor[7];
} __attribute__((packed));

struct eq_coeff_8 {
	struct eq_numerator	numerator[8];
	struct eq_denominator	denominator[8];
	struct eq_shiftfactor	shiftfactor[8];
} __attribute__((packed));

struct eq_coeff_9 {
	struct eq_numerator	numerator[9];
	struct eq_denominator	denominator[9];
	struct eq_shiftfactor	shiftfactor[9];
} __attribute__((packed));

struct eq_coeff_10 {
	struct eq_numerator	numerator[10];
	struct eq_denominator	denominator[10];
	struct eq_shiftfactor	shiftfactor[10];
} __attribute__((packed));

struct eq_coeff_11 {
	struct eq_numerator	numerator[11];
	struct eq_denominator	denominator[11];
	struct eq_shiftfactor	shiftfactor[11];
} __attribute__((packed));

struct eq_coeff_12 {
	struct eq_numerator	numerator[12];
	struct eq_denominator	denominator[12];
	struct eq_shiftfactor	shiftfactor[12];
} __attribute__((packed));


struct audpp_cmd_cfg_object_params_eqalizer {
	struct audpp_cmd_cfg_object_params_common 	common;
	signed short				eq_flag;
	unsigned short				num_bands;
	union {
		struct eq_coeff_1	eq_coeffs_1;
		struct eq_coeff_2	eq_coeffs_2;
		struct eq_coeff_3	eq_coeffs_3;
		struct eq_coeff_4	eq_coeffs_4;
		struct eq_coeff_5	eq_coeffs_5;
		struct eq_coeff_6	eq_coeffs_6;
		struct eq_coeff_7	eq_coeffs_7;
		struct eq_coeff_8	eq_coeffs_8;
		struct eq_coeff_9	eq_coeffs_9;
		struct eq_coeff_10	eq_coeffs_10;
		struct eq_coeff_11	eq_coeffs_11;
		struct eq_coeff_12	eq_coeffs_12;
	} __attribute__((packed)) eq_coeff;
} __attribute__((packed));

/*
 * Command Structure to configure post processing parameters (ADRC)
 */
#define AUDPP_CMD_ADRC			3
#define AUDPP_CMD_CFG_OBJECT_PARAMS_ADRC_LEN		\
	sizeof(struct audpp_cmd_cfg_object_params_adrc)


#define AUDPP_CMD_ADRC_FLAG_DIS		0x0000
#define AUDPP_CMD_ADRC_FLAG_ENA		-1
#define AUDPP_CMD_PBE_FLAG_DIS		0x0000
#define AUDPP_CMD_PBE_FLAG_ENA		-1

struct audpp_cmd_cfg_object_params_adrc {
	struct audpp_cmd_cfg_object_params_common 	common;
	signed short		adrc_flag;
	unsigned short	compression_th;
	unsigned short	compression_slope;
	unsigned short	rms_time;
	unsigned short	attack_const_lsw;
	unsigned short	attack_const_msw;
	unsigned short	release_const_lsw;
	unsigned short	release_const_msw;
	unsigned short	adrc_delay;
};

/*
 * Command Structure to configure post processing parameters (MB - ADRC)
 */
#define AUDPP_CMD_MBADRC		10
#define	AUDPP_MAX_MBADRC_BANDS		5

struct adrc_config {
	uint16_t subband_enable;
	uint16_t adrc_sub_mute;
	uint16_t rms_time;
	uint16_t compression_th;
	uint16_t compression_slope;
	uint16_t attack_const_lsw;
	uint16_t attack_const_msw;
	uint16_t release_const_lsw;
	uint16_t release_const_msw;
	uint16_t makeup_gain;
};

struct audpp_cmd_cfg_object_params_mbadrc {
	struct audpp_cmd_cfg_object_params_common 	common;
	uint16_t enable;
	uint16_t num_bands;
	uint16_t down_samp_level;
	uint16_t adrc_delay;
	uint16_t ext_buf_size;
	uint16_t ext_partition;
	uint16_t ext_buf_msw;
	uint16_t ext_buf_lsw;
	struct adrc_config adrc_band[AUDPP_MAX_MBADRC_BANDS];
} __attribute__((packed));

/*
 * Command Structure to configure post processing parameters(Spectrum Analizer)
 */
#define AUDPP_CMD_SPECTROGRAM		4
#define AUDPP_CMD_CFG_OBJECT_PARAMS_SPECTRAM_LEN		\
	sizeof(struct audpp_cmd_cfg_object_params_spectram)


struct audpp_cmd_cfg_object_params_spectram {
	struct audpp_cmd_cfg_object_params_common 	common;
	unsigned short				sample_interval;
	unsigned short				num_coeff;
} __attribute__((packed));

/*
 * Command Structure to configure post processing parameters (QConcert)
 */
#define AUDPP_CMD_QCONCERT		5
#define AUDPP_CMD_CFG_OBJECT_PARAMS_QCONCERT_LEN		\
	sizeof(struct audpp_cmd_cfg_object_params_qconcert)


#define AUDPP_CMD_QCON_ENA_FLAG_ENA		-1
#define AUDPP_CMD_QCON_ENA_FLAG_DIS		0x0000

#define AUDPP_CMD_QCON_OP_MODE_HEADPHONE	-1
#define AUDPP_CMD_QCON_OP_MODE_SPEAKER_FRONT	0x0000
#define AUDPP_CMD_QCON_OP_MODE_SPEAKER_SIDE	0x0001
#define AUDPP_CMD_QCON_OP_MODE_SPEAKER_DESKTOP	0x0002

#define AUDPP_CMD_QCON_GAIN_UNIT			0x7FFF
#define AUDPP_CMD_QCON_GAIN_SIX_DB			0x4027


#define AUDPP_CMD_QCON_EXPANSION_MAX		0x7FFF


struct audpp_cmd_cfg_object_params_qconcert {
	struct audpp_cmd_cfg_object_params_common 	common;
	signed short				enable_flag;
	signed short				op_mode;
	signed short				gain;
	signed short				expansion;
	signed short				delay;
	unsigned short				stages_per_mode;
	unsigned short				reverb_enable;
	unsigned short				decay_msw;
	unsigned short				decay_lsw;
	unsigned short				decay_time_ratio_msw;
	unsigned short				decay_time_ratio_lsw;
	unsigned short				reflection_delay_time;
	unsigned short				late_reverb_gain;
	unsigned short				late_reverb_delay;
	unsigned short                          delay_buff_size_msw;
	unsigned short                          delay_buff_size_lsw;
	unsigned short                          partition_num;
	unsigned short                          delay_buff_start_msw;
	unsigned short                          delay_buff_start_lsw;
} __attribute__((packed));

/*
 * Command Structure to configure post processing parameters (Side Chain)
 */
#define AUDPP_CMD_SIDECHAIN_TUNING_FILTER	6
#define AUDPP_CMD_CFG_OBJECT_PARAMS_SIDECHAIN_LEN		\
	sizeof(struct audpp_cmd_cfg_object_params_sidechain)


#define AUDPP_CMD_SIDECHAIN_ACTIVE_FLAG_DIS	0x0000
#define AUDPP_CMD_SIDECHAIN_ACTIVE_FLAG_ENA	-1

struct audpp_cmd_cfg_object_params_sidechain {
	struct audpp_cmd_cfg_object_params_common 	common;
	signed short				active_flag;
	unsigned short				num_bands;
	union {
		struct filter_1			filter_1_params;
		struct filter_2			filter_2_params;
		struct filter_3			filter_3_params;
		struct filter_4			filter_4_params;
	} __attribute__((packed)) params_filter;
} __attribute__((packed));


/*
 * Command Structure to configure post processing parameters (QAFX)
 */
#define AUDPP_CMD_QAFX			8
#define AUDPP_CMD_CFG_OBJECT_PARAMS_QAFX_LEN		\
	sizeof(struct audpp_cmd_cfg_object_params_qafx)

#define AUDPP_CMD_QAFX_ENA_DISA		0x0000
#define AUDPP_CMD_QAFX_ENA_ENA_CFG	-1
#define AUDPP_CMD_QAFX_ENA_DIS_CFG	0x0001

#define AUDPP_CMD_QAFX_CMD_TYPE_ENV	0x0100
#define AUDPP_CMD_QAFX_CMD_TYPE_OBJ	0x0010
#define AUDPP_CMD_QAFX_CMD_TYPE_QUERY	0x1000

#define AUDPP_CMD_QAFX_CMDS_ENV_OP_MODE	0x0100
#define AUDPP_CMD_QAFX_CMDS_ENV_LIS_POS	0x0101
#define AUDPP_CMD_QAFX_CMDS_ENV_LIS_ORI	0x0102
#define AUDPP_CMD_QAFX_CMDS_ENV_LIS_VEL	0X0103
#define AUDPP_CMD_QAFX_CMDS_ENV_ENV_RES	0x0107

#define AUDPP_CMD_QAFX_CMDS_OBJ_SAMP_FREQ	0x0010
#define AUDPP_CMD_QAFX_CMDS_OBJ_VOL		0x0011
#define AUDPP_CMD_QAFX_CMDS_OBJ_DIST		0x0012
#define AUDPP_CMD_QAFX_CMDS_OBJ_POS		0x0013
#define AUDPP_CMD_QAFX_CMDS_OBJ_VEL		0x0014


struct audpp_cmd_cfg_object_params_qafx {
	struct audpp_cmd_cfg_object_params_common 	common;
	signed short				enable;
	unsigned short				command_type;
	unsigned short				num_commands;
	unsigned short				commands;
} __attribute__((packed));

/*
 * Command Structure to enable , disable or configure the reverberation effect
 * (REVERB) (Common)
 */

#define AUDPP_CMD_REVERB_CONFIG		0x0001
#define	AUDPP_CMD_REVERB_CONFIG_COMMON_LEN	\
	sizeof(struct audpp_cmd_reverb_config_common)

#define AUDPP_CMD_ENA_ENA	0xFFFF
#define AUDPP_CMD_ENA_DIS	0x0000
#define AUDPP_CMD_ENA_CFG	0x0001

#define AUDPP_CMD_CMD_TYPE_ENV		0x0104
#define AUDPP_CMD_CMD_TYPE_OBJ		0x0015
#define AUDPP_CMD_CMD_TYPE_QUERY	0x1000


struct audpp_cmd_reverb_config_common {
	unsigned short			cmd_id;
	unsigned short			enable;
	unsigned short			cmd_type;
} __attribute__((packed));

/*
 * Command Structure to enable , disable or configure the reverberation effect
 * (ENV-0x0104)
 */

#define	AUDPP_CMD_REVERB_CONFIG_ENV_104_LEN	\
	sizeof(struct audpp_cmd_reverb_config_env_104)

struct audpp_cmd_reverb_config_env_104 {
	struct audpp_cmd_reverb_config_common	common;
	unsigned short			env_gain;
	unsigned short			decay_msw;
	unsigned short			decay_lsw;
	unsigned short			decay_timeratio_msw;
	unsigned short			decay_timeratio_lsw;
	unsigned short			delay_time;
	unsigned short			reverb_gain;
	unsigned short			reverb_delay;
} __attribute__((packed));

/*
 * Command Structure to enable , disable or configure the reverberation effect
 * (ENV-0x0015)
 */

#define	AUDPP_CMD_REVERB_CONFIG_ENV_15_LEN	\
	sizeof(struct audpp_cmd_reverb_config_env_15)

struct audpp_cmd_reverb_config_env_15 {
	struct audpp_cmd_reverb_config_common	common;
	unsigned short			object_num;
	unsigned short			absolute_gain;
} __attribute__((packed));

#define AUDPP_CMD_PBE                   16
#define AUDPP_CMD_CFG_PBE_LEN sizeof(struct audpp_cmd_cfg_pbe)

struct audpp_cmd_cfg_pbe {
	struct audpp_cmd_cfg_object_params_common       common;
	unsigned short pbe_enable;
	signed short   realbassmix;
	signed short   basscolorcontrol;
	unsigned short mainchaindelay;
	unsigned short xoverfltorder;
	unsigned short bandpassfltorder;
	signed short   adrcdelay;
	unsigned short downsamplelevel;
	unsigned short comprmstav;
	signed short   expthreshold;
	unsigned short expslope;
	unsigned short compthreshold;
	unsigned short compslope;
	unsigned short cpmpattack_lsw;
	unsigned short compattack_msw;
	unsigned short comprelease_lsw;
	unsigned short comprelease_msw;
	unsigned short compmakeupgain;
	signed short   baselimthreshold;
	signed short   highlimthreshold;
	signed short   basslimmakeupgain;
	signed short   highlimmakeupgain;
	signed short   limbassgrc;
	signed short   limhighgrc;
	signed short   limdelay;
	unsigned short filter_coeffs[90];
	unsigned short extbuffsize_lsw;
	unsigned short extbuffsize_msw;
	unsigned short extpartition;
	unsigned short extbuffstart_lsw;
	unsigned short extbuffstart_msw;
} __attribute__((packed));

#define AUDPP_CMD_PP_FEAT_QUERY_PARAMS  0x0002

struct audpp_cmd_cfg_object_params_volpan {
	struct audpp_cmd_cfg_object_params_common       common;
	u16 volume ;
	u16 pan;
};

struct rtc_audpp_read_data {
	unsigned short  cmd_id;
	unsigned short  obj_id;
	unsigned short  route_id;
	unsigned short  feature_id;
	unsigned short  extbufsizemsw;
	unsigned short  extbufsizelsw;
	unsigned short	extpart;
	unsigned short	extbufstartmsw;
	unsigned short	extbufstartlsw;
} __attribute__((packed)) ;

#define AUDPP_CMD_SAMPLING_FREQUENCY	7
#define AUDPP_CMD_QRUMBLE		9

#endif /* __MACH_QDSP5_V2_QDSP5AUDPPCMDI_H */
