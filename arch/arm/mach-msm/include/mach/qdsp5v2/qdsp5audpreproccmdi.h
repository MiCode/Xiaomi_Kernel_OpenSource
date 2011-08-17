/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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

#ifndef QDSP5AUDPREPROCCMDI_H
#define QDSP5AUDPREPROCCMDI_H

/*
 * AUDIOPREPROC COMMANDS:
 * ARM uses uPAudPreProcAudRecCmdQueue to communicate with AUDPREPROCTASK
 * Location : MEMB
 * Buffer size : 7
 * Number of buffers in a queue : 4
 */

/*
 * Command to enable or disable particular encoder for new interface
 */

#define AUDPREPROC_AUDREC_CMD_ENC_CFG		0x0000
#define	AUDPREPROC_AUDREC_CMD_ENC_CFG_LEN	\
	sizeof(struct audpreproc_audrec_cmd_enc_cfg)
#define AUDREC_TASK_0	0x00 /* SBC / PCM */
#define AUDREC_TASK_1	0x01 /* AAC / PCM / VOICE ENC */

#define ENCODE_ENABLE	0x8000

/* encoder type supported */
#define ENC_TYPE_WAV	0x00
#define ENC_TYPE_AAC	0x01
#define ENC_TYPE_SBC	0x02
#define ENC_TYPE_AMRNB	0x03
#define ENC_TYPE_EVRC	0x04
#define ENC_TYPE_V13K	0x05
#define ENC_TYPE_EXT_WAV   0x0F /* to dynamically configure frame size */

/* structure definitions according to
 * command description of ARM-DSP interface specifications
 */
struct audpreproc_audrec_cmd_enc_cfg {
	unsigned short	cmd_id;
	unsigned short  stream_id;
	unsigned short  audrec_enc_type;
} __attribute__((packed));

/*
 * Command to configure parameters of selected Encoder
 */

#define AUDPREPROC_AUDREC_CMD_PARAM_CFG	0x0001

#define AUDPREPROC_AUDREC_CMD_PARAM_CFG_COMMON_LEN		\
	sizeof(struct audpreproc_audrec_cmd_param_cfg_common)

#define DUAL_MIC_STEREO_RECORDING      2

struct audpreproc_audrec_cmd_param_cfg_common {
	unsigned short cmd_id;
	unsigned short stream_id;
} __attribute__((packed));

/*
 * Command Structure to configure WAV Encoder
 */

#define AUDPREPROC_AUDREC_CMD_PARAM_CFG_WAV_LEN		\
	sizeof(struct audpreproc_audrec_cmd_parm_cfg_wav)

#define AUDREC_CMD_MODE_MONO 0
#define AUDREC_CMD_MODE_STEREO 1

struct audpreproc_audrec_cmd_parm_cfg_wav {
	struct audpreproc_audrec_cmd_param_cfg_common common;
	unsigned short aud_rec_samplerate_idx;
	unsigned short aud_rec_stereo_mode;
	unsigned short aud_rec_frame_size;
} __attribute__((packed));

/*
 * Command Structure to configure AAC Encoder
 */

#define AUDPREPROC_AUDREC_CMD_PARAM_CFG_AAC_LEN		\
	sizeof(struct audpreproc_audrec_cmd_parm_cfg_aac)

struct audpreproc_audrec_cmd_parm_cfg_aac {
	struct audpreproc_audrec_cmd_param_cfg_common common;
	unsigned short aud_rec_samplerate_idx;
	unsigned short aud_rec_stereo_mode;
	signed short   recording_quality;
} __attribute__((packed));

/*
 * Command Structure to configure SBC Encoder
 */

#define AUDPREPROC_AUDREC_CMD_PARAM_CFG_SBC_LEN		\
	sizeof(struct audpreproc_audrec_cmd_parm_cfg_sbc)

/* encoder parameters mask definitions*/

#define AUDREC_SBC_ENC_PARAM_VER_MASK				0x000A
#define AUDREC_SBC_ENC_PARAM_ENAHANCED_SBC_BASELINE_VERSION	0x0000
#define AUDREC_SBC_ENC_PARAM_ENAHANCED_SBC_NA_MASK		0x0400
#define AUDREC_SBC_ENC_PARAM_BIT_ALLOC_MASK			0x0008
#define AUDREC_SBC_ENC_PARAM_SNR_MASK				0x0100
#define AUDREC_SBC_ENC_PARAM_MODE_MASK				0x0006
#define AUDREC_SBC_ENC_PARAM_MODE_DUAL_MASK			0x0040
#define AUDREC_SBC_ENC_PARAM_MODE_STEREO_MASK			0x0080
#define AUDREC_SBC_ENC_PARAM_MODE_JOINT_STEREO_MASK		0x00C0
#define AUDREC_SBC_ENC_PARAM_NUM_SUB_BANDS_MASK 		0x0004
#define AUDREC_SBC_ENC_PARAM_NUM_SUB_BANDS_8_MASK		0x0001
#define AUDREC_SBC_ENC_PARAM_NUM_SUB_BLOCKS_MASK		0x0000
#define AUDREC_SBC_ENC_PARAM_NUM_SUB_BLOCKS_4_MASK		0x0000
#define AUDREC_SBC_ENC_PARAM_NUM_SUB_BLOCKS_8_MASK		0x0001
#define AUDREC_SBC_ENC_PARAM_NUM_SUB_BLOCKS_12_MASK		0x0002
#define AUDREC_SBC_ENC_PARAM_NUM_SUB_BLOCKS_16_MASK		0x0003

struct audpreproc_audrec_cmd_parm_cfg_sbc {
	struct audpreproc_audrec_cmd_param_cfg_common common;
	unsigned short aud_rec_sbc_enc_param;
	unsigned short aud_rec_sbc_bit_rate_msw;
	unsigned short aud_rec_sbc_bit_rate_lsw;
} __attribute__((packed));

/*
 * Command Structure to configure AMRNB Encoder
 */

#define AUDPREPROC_AUDREC_CMD_PARAM_CFG_AMRNB_LEN		\
	sizeof(struct audpreproc_audrec_cmd_parm_cfg_amrnb)

#define AMRNB_DTX_MODE_ENABLE		-1
#define AMRNB_DTX_MODE_DISABLE		 0

#define AMRNB_TEST_MODE_ENABLE		-1
#define AMRNB_TEST_MODE_DISABLE		 0

#define AMRNB_USED_MODE_MR475		0x0
#define AMRNB_USED_MODE_MR515		0x1
#define AMRNB_USED_MODE_MR59		0x2
#define AMRNB_USED_MODE_MR67		0x3
#define AMRNB_USED_MODE_MR74		0x4
#define AMRNB_USED_MODE_MR795		0x5
#define AMRNB_USED_MODE_MR102		0x6
#define AMRNB_USED_MODE_MR122		0x7

struct audpreproc_audrec_cmd_parm_cfg_amrnb {
	struct audpreproc_audrec_cmd_param_cfg_common common;
	signed short dtx_mode;
	signed short test_mode;
	unsigned short used_mode;
} __attribute__((packed)) ;

/*
 * Command Structure to configure EVRC Encoder
 */

#define AUDPREPROC_AUDREC_CMD_PARAM_CFG_EVRC_LEN		\
	sizeof(struct audpreproc_audrec_cmd_parm_cfg_evrc)

struct audpreproc_audrec_cmd_parm_cfg_evrc {
	struct audpreproc_audrec_cmd_param_cfg_common common;
	unsigned short enc_min_rate;
	unsigned short enc_max_rate;
	unsigned short rate_modulation_cmd;
} __attribute__((packed));

/*
 * Command Structure to configure QCELP_13K Encoder
 */

#define AUDPREPROC_AUDREC_CMD_PARAM_CFG_QCELP13K_LEN		\
	sizeof(struct audpreproc_audrec_cmd_parm_cfg_qcelp13k)

struct audpreproc_audrec_cmd_parm_cfg_qcelp13k {
	struct audpreproc_audrec_cmd_param_cfg_common common;
	unsigned short enc_min_rate;
	unsigned short enc_max_rate;
	unsigned short rate_modulation_cmd;
	unsigned short reduced_rate_level;
} __attribute__((packed));

/*
 * Command to configure AFE for recording paths
 */
#define AUDPREPROC_AFE_CMD_AUDIO_RECORD_CFG 0x0002

#define AUDPREPROC_AFE_CMD_AUDIO_RECORD_CFG_LEN		\
	sizeof(struct audpreproc_afe_cmd_audio_record_cfg)

#define AUDIO_RECORDING_TURN_ON		0xFFFF
#define AUDIO_RECORDING_TURN_OFF	0x0000

#define AUDPP_A2DP_PIPE_SOURCE_MIX_MASK		0x0020
#define VOICE_DL_SOURCE_MIX_MASK		0x0010
#define VOICE_UL_SOURCE_MIX_MASK		0x0008
#define FM_SOURCE_MIX_MASK			0x0004
#define AUX_CODEC_TX_SOURCE_MIX_MASK		0x0002
#define INTERNAL_CODEC_TX_SOURCE_MIX_MASK	0x0001

struct audpreproc_afe_cmd_audio_record_cfg {
	unsigned short cmd_id;
	unsigned short stream_id;
	unsigned short destination_activity;
	unsigned short source_mix_mask;
	unsigned short pipe_id;
	unsigned short reserved;
} __attribute__((packed));

/*
 * Command to configure Tunnel(RT) or Non-Tunnel(FTRT) mode
 */
#define AUDPREPROC_AUDREC_CMD_ROUTING_MODE 0x0003
#define	AUDPREPROC_AUDREC_CMD_ROUTING_MODE_LEN	\
	sizeof(struct audpreproc_audrec_cmd_routing_mode)

#define AUDIO_ROUTING_MODE_FTRT		0x0001
#define AUDIO_ROUTING_MODE_RT		0x0002

struct audpreproc_audrec_cmd_routing_mode {
	unsigned short cmd_id;
	unsigned short stream_id;
	unsigned short routing_mode;
} __attribute__((packed));

/*
 * Command to configure DSP for topology where resampler moved
 * in front of pre processing chain
 */
#define AUDPREPROC_AUDREC_CMD_ENC_CFG_2		0x0004
#define	AUDPREPROC_AUDREC_CMD_ENC_CFG_2_LEN	\
	sizeof(struct audpreproc_audrec_cmd_enc_cfg_2)


struct audpreproc_audrec_cmd_enc_cfg_2 {
	unsigned short	cmd_id;
	unsigned short  stream_id;
	unsigned short  audrec_enc_type;
} __attribute__((packed));

/*
 * AUDIOPREPROC COMMANDS:
 * ARM uses uPAudPreProcCmdQueue to communicate with AUDPREPROCTASK
 * Location : MEMB
 * Buffer size : 52
 * Number of buffers in a queue : 3
 */

/*
 * Command to configure the parameters of AGC
 */

#define	AUDPREPROC_CMD_CFG_AGC_PARAMS	0x0000
#define	AUDPREPROC_CMD_CFG_AGC_PARAMS_LEN	\
	sizeof(struct audpreproc_cmd_cfg_agc_params)

#define	AUDPREPROC_CMD_TX_AGC_PARAM_MASK_COMP_SLOPE	0x0200
#define	AUDPREPROC_CMD_TX_AGC_PARAM_MASK_COMP_TH	0x0400
#define	AUDPREPROC_CMD_TX_AGC_PARAM_MASK_EXP_SLOPE	0x0800
#define	AUDPREPROC_CMD_TX_AGC_PARAM_MASK_EXP_TH		0x1000
#define	AUDPREPROC_CMD_TX_AGC_PARAM_MASK_COMP_AIG_FLAG		0x2000
#define	AUDPREPROC_CMD_TX_AGC_PARAM_MASK_COMP_STATIC_GAIN	0x4000
#define	AUDPREPROC_CMD_TX_AGC_PARAM_MASK_TX_AGC_ENA_FLAG	0x8000

#define	AUDPREPROC_CMD_TX_AGC_ENA_FLAG_ENA	-1
#define	AUDPREPROC_CMD_TX_AGC_ENA_FLAG_DIS	0x0000

#define	AUDPREPROC_CMD_ADP_GAIN_FLAG_ENA_ADP_GAIN	-1
#define	AUDPREPROC_CMD_ADP_GAIN_FLAG_ENA_STATIC_GAIN	0x0000

#define	AUDPREPROC_CMD_PARAM_MASK_RMS_TAY	0x0010
#define	AUDPREPROC_CMD_PARAM_MASK_RELEASEK	0x0020
#define	AUDPREPROC_CMD_PARAM_MASK_DELAY		0x0040
#define	AUDPREPROC_CMD_PARAM_MASK_ATTACKK	0x0080
#define	AUDPREPROC_CMD_PARAM_MASK_LEAKRATE_SLOW	0x0100
#define	AUDPREPROC_CMD_PARAM_MASK_LEAKRATE_FAST	0x0200
#define	AUDPREPROC_CMD_PARAM_MASK_AIG_RELEASEK 	0x0400
#define	AUDPREPROC_CMD_PARAM_MASK_AIG_MIN	0x0800
#define	AUDPREPROC_CMD_PARAM_MASK_AIG_MAX	0x1000
#define	AUDPREPROC_CMD_PARAM_MASK_LEAK_UP	0x2000
#define	AUDPREPROC_CMD_PARAM_MASK_LEAK_DOWN	0x4000
#define	AUDPREPROC_CMD_PARAM_MASK_AIG_ATTACKK	0x8000

struct audpreproc_cmd_cfg_agc_params {
	unsigned short	cmd_id;
	unsigned short 	stream_id;
	unsigned short	tx_agc_param_mask;
	signed short	tx_agc_enable_flag;
	unsigned short	comp_rlink_static_gain;
	signed short	comp_rlink_aig_flag;
	unsigned short	expander_rlink_th;
	unsigned short	expander_rlink_slope;
	unsigned short	compressor_rlink_th;
	unsigned short	compressor_rlink_slope;
	unsigned short	tx_adc_agc_param_mask;
	unsigned short	comp_rlink_aig_attackk;
	unsigned short	comp_rlink_aig_leak_down;
	unsigned short	comp_rlink_aig_leak_up;
	unsigned short	comp_rlink_aig_max;
	unsigned short	comp_rlink_aig_min;
	unsigned short	comp_rlink_aig_releasek;
	unsigned short	comp_rlink_aig_leakrate_fast;
	unsigned short	comp_rlink_aig_leakrate_slow;
	unsigned short	comp_rlink_attackk_msw;
	unsigned short	comp_rlink_attackk_lsw;
	unsigned short	comp_rlink_delay;
	unsigned short	comp_rlink_releasek_msw;
	unsigned short	comp_rlink_releasek_lsw;
	unsigned short	comp_rlink_rms_tav;
} __attribute__((packed));

/*
 * Command to configure the params of Advanved AGC
 */

#define	AUDPREPROC_CMD_CFG_AGC_PARAMS_2		0x0001
#define	AUDPREPROC_CMD_CFG_AGC_PARAMS_2_LEN		\
	sizeof(struct audpreproc_cmd_cfg_agc_params_2)

#define	AUDPREPROC_CMD_2_TX_AGC_ENA_FLAG_ENA	-1;
#define	AUDPREPROC_CMD_2_TX_AGC_ENA_FLAG_DIS	0x0000;

struct audpreproc_cmd_cfg_agc_params_2 {
	unsigned short	cmd_id;
	unsigned short 	stream_id;
	unsigned short	agc_param_mask;
	signed short	tx_agc_enable_flag;
	unsigned short	comp_rlink_static_gain;
	unsigned short	exp_rlink_th;
	unsigned short	exp_rlink_slope;
	unsigned short	comp_rlink_th;
	unsigned short	comp_rlink_slope;
	unsigned short	comp_rlink_rms_tav;
	unsigned short	comp_rlink_down_samp_mask;
	unsigned short	comp_rlink_attackk_msw;
	unsigned short	comp_rlink_attackk_lsw;
	unsigned short	comp_rlink_releasek_msw;
	unsigned short	comp_rlink_releasek_lsw;
	unsigned short	comp_rlink_delay;
	unsigned short	comp_rlink_makeup_gain;
} __attribute__((packed));

/*
 * Command to configure params for ns
 */

#define	AUDPREPROC_CMD_CFG_NS_PARAMS		0x0002
#define	AUDPREPROC_CMD_CFG_NS_PARAMS_LEN	\
	sizeof(struct audpreproc_cmd_cfg_ns_params)

#define	AUDPREPROC_CMD_EC_MODE_NLMS_ENA	0x0001
#define	AUDPREPROC_CMD_EC_MODE_NLMS_DIS 	0x0000
#define	AUDPREPROC_CMD_EC_MODE_DES_ENA	0x0002
#define	AUDPREPROC_CMD_EC_MODE_DES_DIS	0x0000
#define	AUDPREPROC_CMD_EC_MODE_NS_ENA	0x0004
#define	AUDPREPROC_CMD_EC_MODE_NS_DIS	0x0000
#define	AUDPREPROC_CMD_EC_MODE_CNI_ENA	0x0008
#define	AUDPREPROC_CMD_EC_MODE_CNI_DIS	0x0000

#define	AUDPREPROC_CMD_EC_MODE_NLES_ENA	0x0010
#define	AUDPREPROC_CMD_EC_MODE_NLES_DIS	0x0000
#define	AUDPREPROC_CMD_EC_MODE_HB_ENA	0x0020
#define	AUDPREPROC_CMD_EC_MODE_HB_DIS	0x0000
#define	AUDPREPROC_CMD_EC_MODE_VA_ENA	0x0040
#define	AUDPREPROC_CMD_EC_MODE_VA_DIS	0x0000
#define	AUDPREPROC_CMD_EC_MODE_PCD_ENA	0x0080
#define	AUDPREPROC_CMD_EC_MODE_PCD_DIS	0x0000
#define	AUDPREPROC_CMD_EC_MODE_FEHI_ENA	0x0100
#define	AUDPREPROC_CMD_EC_MODE_FEHI_DIS 	0x0000
#define	AUDPREPROC_CMD_EC_MODE_NEHI_ENA	0x0200
#define	AUDPREPROC_CMD_EC_MODE_NEHI_DIS 	0x0000
#define	AUDPREPROC_CMD_EC_MODE_NLPP_ENA	0x0400
#define	AUDPREPROC_CMD_EC_MODE_NLPP_DIS	0x0000
#define	AUDPREPROC_CMD_EC_MODE_FNE_ENA	0x0800
#define	AUDPREPROC_CMD_EC_MODE_FNE_DIS	0x0000
#define	AUDPREPROC_CMD_EC_MODE_PRENLMS_ENA 	0x1000
#define	AUDPREPROC_CMD_EC_MODE_PRENLMS_DIS 	0x0000

struct audpreproc_cmd_cfg_ns_params {
	unsigned short	cmd_id;
	unsigned short  stream_id;
	unsigned short	ec_mode_new;
	unsigned short	dens_gamma_n;
	unsigned short	dens_nfe_block_size;
	unsigned short	dens_limit_ns;
	unsigned short	dens_limit_ns_d;
	unsigned short	wb_gamma_e;
	unsigned short	wb_gamma_n;
} __attribute__((packed));

/*
 * Command to configure parameters for IIR tuning filter
 */

#define	AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS		0x0003
#define	AUDPREPROC_CMD_CFG_IIR_TUNING_FILTER_PARAMS_LEN	\
	sizeof(struct audpreproc_cmd_cfg_iir_tuning_filter_params)

#define	AUDPREPROC_CMD_IIR_ACTIVE_FLAG_DIS	0x0000
#define	AUDPREPROC_CMD_IIR_ACTIVE_FLAG_ENA	0x0001

struct audpreproc_cmd_cfg_iir_tuning_filter_params {
	unsigned short	cmd_id;
	unsigned short  stream_id;
	unsigned short	active_flag;
	unsigned short	num_bands;

	unsigned short	numerator_coeff_b0_filter0_lsw;
	unsigned short	numerator_coeff_b0_filter0_msw;
	unsigned short	numerator_coeff_b1_filter0_lsw;
	unsigned short	numerator_coeff_b1_filter0_msw;
	unsigned short	numerator_coeff_b2_filter0_lsw;
	unsigned short	numerator_coeff_b2_filter0_msw;

	unsigned short	numerator_coeff_b0_filter1_lsw;
	unsigned short	numerator_coeff_b0_filter1_msw;
	unsigned short	numerator_coeff_b1_filter1_lsw;
	unsigned short	numerator_coeff_b1_filter1_msw;
	unsigned short	numerator_coeff_b2_filter1_lsw;
	unsigned short	numerator_coeff_b2_filter1_msw;

	unsigned short	numerator_coeff_b0_filter2_lsw;
	unsigned short	numerator_coeff_b0_filter2_msw;
	unsigned short	numerator_coeff_b1_filter2_lsw;
	unsigned short	numerator_coeff_b1_filter2_msw;
	unsigned short	numerator_coeff_b2_filter2_lsw;
	unsigned short	numerator_coeff_b2_filter2_msw;

	unsigned short	numerator_coeff_b0_filter3_lsw;
	unsigned short	numerator_coeff_b0_filter3_msw;
	unsigned short	numerator_coeff_b1_filter3_lsw;
	unsigned short	numerator_coeff_b1_filter3_msw;
	unsigned short	numerator_coeff_b2_filter3_lsw;
	unsigned short	numerator_coeff_b2_filter3_msw;

	unsigned short 	denominator_coeff_a0_filter0_lsw;
	unsigned short 	denominator_coeff_a0_filter0_msw;
	unsigned short 	denominator_coeff_a1_filter0_lsw;
	unsigned short 	denominator_coeff_a1_filter0_msw;

	unsigned short 	denominator_coeff_a0_filter1_lsw;
	unsigned short 	denominator_coeff_a0_filter1_msw;
	unsigned short 	denominator_coeff_a1_filter1_lsw;
	unsigned short 	denominator_coeff_a1_filter1_msw;

	unsigned short	denominator_coeff_a0_filter2_lsw;
	unsigned short	denominator_coeff_a0_filter2_msw;
	unsigned short	denominator_coeff_a1_filter2_lsw;
	unsigned short	denominator_coeff_a1_filter2_msw;

	unsigned short	denominator_coeff_a0_filter3_lsw;
	unsigned short	denominator_coeff_a0_filter3_msw;
	unsigned short 	denominator_coeff_a1_filter3_lsw;
	unsigned short 	denominator_coeff_a1_filter3_msw;

	unsigned short	shift_factor_filter0;
	unsigned short	shift_factor_filter1;
	unsigned short	shift_factor_filter2;
	unsigned short	shift_factor_filter3;

	unsigned short	pan_of_filter0;
	unsigned short	pan_of_filter1;
	unsigned short	pan_of_filter2;
	unsigned short	pan_of_filter3;
} __attribute__((packed));

/*
 * Command to configure parameters for calibration gain rx
 */

#define AUDPREPROC_CMD_CFG_CAL_GAIN_PARAMS 0x0004
#define AUDPREPROC_CMD_CFG_CAL_GAIN_LEN    \
	sizeof(struct audpreproc_cmd_cfg_cal_gain)

struct audpreproc_cmd_cfg_cal_gain {
	unsigned short  cmd_id;
	unsigned short  stream_id;
	unsigned short  audprecalgain;
	unsigned short  reserved;
}  __attribute__((packed));

#define AUDPREPROC_CMD_CFG_LVNV_PARMS	0x0006
#define AUDPREPROC_CMD_CFG_LVNV_PARMS_LEN	\
		sizeof(struct audpreproc_cmd_cfg_lvnv_param)

struct audpreproc_cmd_cfg_lvnv_param {
	unsigned short cmd_id;
	unsigned short stream_id;
	unsigned short cs_mode;
	unsigned short lvnv_ext_buf_size;
	unsigned short lvnv_ext_partition;
	unsigned short lvnv_ext_buf_start_lsw;
	unsigned short lvnv_ext_buf_start_msw;
};

#define AUDPREPROC_CMD_FEAT_QUERY_PARAMS 0x0005

struct rtc_audpreproc_read_data {
	unsigned short	cmd_id;
	unsigned short	stream_id;
	unsigned short  feature_id;
	unsigned short  extbufsizemsw;
	unsigned short  extbufsizelsw;
	unsigned short  extpart;
	unsigned short  extbufstartmsw;
	unsigned short	extbufstartlsw;
} __attribute__((packed)) ;

#endif /* QDSP5AUDPREPROCCMDI_H */
