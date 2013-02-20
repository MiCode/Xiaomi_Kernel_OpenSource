/* Copyright (c) 2009-2011, The Linux Foundation. All rights reserved.
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
#ifndef _MACH_QDSP5_V2_AUDIO_ACDBI_H
#define _MACH_QDSP5_V2_AUDIO_ACDBI_H

#define DBOR_SIGNATURE	0x524F4244

#ifdef CONFIG_DEBUG_FS
void acdb_rtc_set_err(u32 ErrCode);
#endif


struct header {
	u32 dbor_signature;
	u32 abid;
	u32 iid;
	u32 data_len;
};

enum {
	ACDB_AGC_BLOCK			= 197,
	ACDB_IIR_BLOCK			= 245,
	ACDB_MBADRC_BLOCK		= 343
};

/* Structure to query for acdb parameter */
struct acdb_get_block {
	u32	acdb_id;
	u32	sample_rate_id;		/* Actual sample rate value */
	u32	interface_id;		/* Interface id's */
	u32	algorithm_block_id;	/* Algorithm block id */
	u32	total_bytes;		/* Length in bytes used by buffer for
						configuration */
	u32	*buf_ptr;		/* Address for storing configuration
						data */
};

struct acdb_agc_block {
	u16	enable_status;
	u16	comp_rlink_static_gain;
	u16	comp_rlink_aig_flag;
	u16	exp_rlink_threshold;
	u16	exp_rlink_slope;
	u16	comp_rlink_threshold;
	u16	comp_rlink_slope;
	u16	comp_rlink_aig_attack_k;
	u16	comp_rlink_aig_leak_down;
	u16	comp_rlink_aig_leak_up;
	u16	comp_rlink_aig_max;
	u16	comp_rlink_aig_min;
	u16	comp_rlink_aig_release_k;
	u16	comp_rlink_aig_sm_leak_rate_fast;
	u16	comp_rlink_aig_sm_leak_rate_slow;
	u16	comp_rlink_attack_k_msw;
	u16	comp_rlink_attack_k_lsw;
	u16	comp_rlink_delay;
	u16	comp_rlink_release_k_msw;
	u16	comp_rlink_release_k_lsw;
	u16	comp_rlink_rms_trav;
};


struct iir_coeff_type {
	u16	b0_lo;
	u16	b0_hi;
	u16	b1_lo;
	u16	b1_hi;
	u16	b2_lo;
	u16	b2_hi;
};

struct iir_coeff_stage_a {
	u16	a1_lo;
	u16	a1_hi;
	u16	a2_lo;
	u16	a2_hi;
};

struct acdb_iir_block {
	u16			enable_flag;
	u16			stage_count;
	struct iir_coeff_type	stages[4];
	struct iir_coeff_stage_a stages_a[4];
	u16			shift_factor[4];
	u16			pan[4];
};



struct mbadrc_band_config_type {
	u16	mbadrc_sub_band_enable;
	u16	mbadrc_sub_mute;
	u16	mbadrc_comp_rms_tav;
	u16	mbadrc_comp_threshold;
	u16	mbadrc_comp_slop;
	u16	mbadrc_comp_attack_msw;
	u16	mbadrc_comp_attack_lsw;
	u16	mbadrc_comp_release_msw;
	u16	mbadrc_comp_release_lsw;
	u16	mbadrc_make_up_gain;
};

struct mbadrc_parameter {
	u16				mbadrc_enable;
	u16				mbadrc_num_bands;
	u16				mbadrc_down_sample_level;
	u16				mbadrc_delay;
};

struct acdb_mbadrc_block {
	u16				ext_buf[196];
	struct mbadrc_band_config_type	band_config[5];
	struct mbadrc_parameter		parameters;
};

struct  acdb_calib_gain_rx {
	u16 audppcalgain;
	u16 reserved;
};

struct acdb_calib_gain_tx {
	u16 audprecalgain;
	u16 reserved;
};

struct acdb_pbe_block {
	s16 realbassmix;
	s16 basscolorcontrol;
	u16 mainchaindelay;
	u16 xoverfltorder;
	u16 bandpassfltorder;
	s16 adrcdelay;
	u16 downsamplelevel;
	u16 comprmstav;
	s16 expthreshold;
	u16 expslope;
	u16 compthreshold;
	u16 compslope;
	u16 cpmpattack_lsw;
	u16 compattack_msw;
	u16 comprelease_lsw;
	u16 comprelease_msw;
	u16 compmakeupgain;
	s16 baselimthreshold;
	s16 highlimthreshold;
	s16 basslimmakeupgain;
	s16 highlimmakeupgain;
	s16 limbassgrc;
	s16 limhighgrc;
	s16 limdelay;
	u16 filter_coeffs[90];
};

struct acdb_rmc_block  {
	s16 rmc_enable;
	u16 rmc_ipw_length_ms;
	u16 rmc_detect_start_threshdb;
	u16 rmc_peak_length_ms;
	s16 rmc_init_pulse_threshdb;
	u16 rmc_init_pulse_length_ms;
	u16 rmc_total_int_length_ms;
	u16 rmc_rampupdn_length_ms;
	u16 rmc_delay_length_ms;
	u16 reserved00;
	u16 reserved01;
	s16 reserved02;
	s16 reserved03;
	s16 reserved04;
};

struct acdb_fluence_block {
	u16 csmode;
	u16 cs_tuningMode;
	u16 cs_echo_path_delay_by_80;
	u16 cs_echo_path_delay;
	u16 af1_twoalpha;
	u16 af1_erl;
	u16 af1_taps;
	u16 af1_preset_coefs;
	u16 af1_offset;
	u16 af2_twoalpha;
	u16 af2_erl;
	u16 af2_taps;
	u16 af2_preset_coefs;
	u16 af2_offset;
	u16 pcd_twoalpha;
	u16 pcd_offset;
	u16 cspcd_threshold;
	u16 wgthreshold;
	u16 mpthreshold;
	u16 sf_init_table_0[8];
	u16 sf_init_table_1[8];
	u16 sf_taps;
	u16 sf_twoalpha;
	u16 dnns_echoalpharev;
	u16 dnns_echoycomp;
	u16 dnns_wbthreshold;
	u16 dnns_echogammahi;
	u16 dnns_echogammalo;
	u16 dnns_noisegammas;
	u16 dnns_noisegamman;
	u16 dnns_noisegainmins;
	u16 dnns_noisegainminn;
	u16 dnns_noisebiascomp;
	u16 dnns_acthreshold;
	u16 wb_echo_ratio_2mic;
	u16 wb_gamma_e;
	u16 wb_gamma_nn;
	u16 wb_gamma_sn;
	u16 vcodec_delay0;
	u16 vcodec_delay1;
	u16 vcodec_len0;
	u16 vcodec_len1;
	u16 vcodec_thr0;
	u16 vcodec_thr1;
	u16 fixcalfactorleft;
	u16 fixcalfactorright;
	u16 csoutputgain;
	u16 enh_meu_1;
	u16 enh_meu_2;
	u16 fixed_over_est;
	u16 rx_nlpp_limit;
	u16 rx_nlpp_gain;
	u16 wnd_threshold;
	u16 wnd_ns_hover;
	u16 wnd_pwr_smalpha;
	u16 wnd_det_esmalpha;
	u16 wnd_ns_egoffset;
	u16 wnd_sm_ratio;
	u16 wnd_det_coefs[5];
	u16 wnd_th1;
	u16 wnd_th2;
	u16 wnd_fq;
	u16 wnd_dfc;
	u16 wnd_sm_alphainc;
	u16 wnd_sm_alphsdec;
	u16 lvnv_spdet_far;
	u16 lvnv_spdet_mic;
	u16 lvnv_spdet_xclip;
	u16 dnns_nl_atten;
	u16 dnns_cni_level;
	u16 dnns_echogammaalpha;
	u16 dnns_echogammarescue;
	u16 dnns_echogammadt;
	u16 mf_noisegammafac;
	u16 e_noisegammafac;
	u16 dnns_noisegammainit;
	u16 sm_noisegammas;
	u16 wnd_noisegamman;
	u16 af_taps_bg_spkr;
	u16 af_erl_bg_spkr;
	u16 minimum_erl_bg;
	u16 erl_step_bg;
	u16 upprisecalpha;
	u16 upprisecthresh;
	u16 uppriwindbias;
	u16 e_pcd_threshold;
	u16 nv_maxvadcount;
	u16 crystalspeechreserved[38];
	u16 cs_speaker[7];
	u16 ns_fac;
	u16 ns_blocksize;
	u16 is_bias;
	u16 is_bias_inp;
	u16 sc_initb;
	u16 ac_resetb;
	u16 sc_avar;
	u16 is_hover[5];
	u16 is_cf_level;
	u16 is_cf_ina;
	u16 is_cf_inb;
	u16 is_cf_a;
	u16 is_cf_b;
	u16 sc_th;
	u16 sc_pscale;
	u16 sc_nc;
	u16 sc_hover;
	u16 sc_alphas;
	u16 sc_cfac;
	u16 sc_sdmax;
	u16 sc_sdmin;
	u16 sc_initl;
	u16 sc_maxval;
	u16 sc_spmin;
	u16 is_ec_th;
	u16 is_fx_dl;
	u16 coeffs_iva_filt_0[32];
	u16 coeffs_iva_filt_1[32];
};

s32 acdb_get_calibration_data(struct acdb_get_block *get_block);
void fluence_feature_update(int enable, int stream_id);
#endif
