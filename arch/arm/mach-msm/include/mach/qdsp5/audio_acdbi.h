/* Copyright (c) 2012 The Linux Foundation. All rights reserved.
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

void acdb_rtc_set_err(u32 err_code);


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

struct acdb_ns_tx_block {
	unsigned short	ec_mode_new;
	unsigned short	dens_gamma_n;
	unsigned short	dens_nfe_block_size;
	unsigned short	dens_limit_ns;
	unsigned short	dens_limit_ns_d;
	unsigned short	wb_gamma_e;
	unsigned short	wb_gamma_n;
};

#endif
