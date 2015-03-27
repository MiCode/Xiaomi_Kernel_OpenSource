/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _WCD9XXX_COMMON_V2

#define _WCD9XXX_COMMON_V2

#define CLSH_REQ_ENABLE true
#define CLSH_REQ_DISABLE false

#define WCD_CLSH_EVENT_PRE_DAC 0x01
#define WCD_CLSH_EVENT_POST_PA 0x02

/*
 * Basic states for Class H state machine.
 * represented as a bit mask within a u8 data type
 * bit 0: EAR mode
 * bit 1: HPH Left mode
 * bit 2: HPH Right mode
 * bit 3: Lineout mode
 */
#define	WCD_CLSH_STATE_IDLE 0x00
#define	WCD_CLSH_STATE_EAR (0x01 << 0)
#define	WCD_CLSH_STATE_HPHL (0x01 << 1)
#define	WCD_CLSH_STATE_HPHR (0x01 << 2)
#define	WCD_CLSH_STATE_LO (0x01 << 3)
#define WCD_CLSH_STATE_MAX 4
#define NUM_CLSH_STATES (0x01 << WCD_CLSH_STATE_MAX)


/* Derived State: Bits 1 and 2 should be set for Headphone stereo */
#define WCD_CLSH_STATE_HPH_ST (WCD_CLSH_STATE_HPHL | \
			       WCD_CLSH_STATE_HPHR)

#define WCD_CLSH_STATE_HPHL_LO (WCD_CLSH_STATE_HPHL | \
				    WCD_CLSH_STATE_LO)
#define WCD_CLSH_STATE_HPHR_LO (WCD_CLSH_STATE_HPHR | \
				    WCD_CLSH_STATE_LO)
#define WCD_CLSH_STATE_HPH_ST_LO (WCD_CLSH_STATE_HPH_ST | \
				      WCD_CLSH_STATE_LO)
#define WCD_CLSH_STATE_EAR_LO (WCD_CLSH_STATE_EAR | \
				   WCD_CLSH_STATE_LO)
#define WCD_CLSH_STATE_HPHL_EAR (WCD_CLSH_STATE_HPHL | \
				     WCD_CLSH_STATE_EAR)
#define WCD_CLSH_STATE_HPHR_EAR (WCD_CLSH_STATE_HPHR | \
				     WCD_CLSH_STATE_EAR)
#define WCD_CLSH_STATE_HPH_ST_EAR (WCD_CLSH_STATE_HPH_ST | \
				       WCD_CLSH_STATE_EAR)

enum {
	CLS_H_NORMAL = 0, /* Class-H Default */
	CLS_H_HIFI, /* Class-H HiFi */
	CLS_H_LP, /* Class-H Low Power */
	CLS_AB, /* Class-AB */
	CLS_NONE, /* None of the above modes */
};

/* Class H data that the codec driver will maintain */
struct wcd_clsh_cdc_data {
	u8 state;
	int flyback_users;
	int buck_users;
	int clsh_users;
	int interpolator_modes[WCD_CLSH_STATE_MAX];
};

struct wcd_mad_audio_header {
	u32 reserved[3];
	u32 num_reg_cfg;
};

struct wcd_mad_microphone_info {
	uint8_t input_microphone;
	uint8_t cycle_time;
	uint8_t settle_time;
	uint8_t padding;
} __packed;

struct wcd_mad_micbias_info {
	uint8_t micbias;
	uint8_t k_factor;
	uint8_t external_bypass_capacitor;
	uint8_t internal_biasing;
	uint8_t cfilter;
	uint8_t padding[3];
} __packed;

struct wcd_mad_rms_audio_beacon_info {
	uint8_t rms_omit_samples;
	uint8_t rms_comp_time;
	uint8_t detection_mechanism;
	uint8_t rms_diff_threshold;
	uint8_t rms_threshold_lsb;
	uint8_t rms_threshold_msb;
	uint8_t padding[2];
	uint8_t iir_coefficients[36];
} __packed;

struct wcd_mad_rms_ultrasound_info {
	uint8_t rms_comp_time;
	uint8_t detection_mechanism;
	uint8_t rms_diff_threshold;
	uint8_t rms_threshold_lsb;
	uint8_t rms_threshold_msb;
	uint8_t padding[3];
	uint8_t iir_coefficients[36];
} __packed;

struct wcd_mad_audio_cal {
	uint32_t version;
	struct wcd_mad_microphone_info microphone_info;
	struct wcd_mad_micbias_info micbias_info;
	struct wcd_mad_rms_audio_beacon_info audio_info;
	struct wcd_mad_rms_audio_beacon_info beacon_info;
	struct wcd_mad_rms_ultrasound_info ultrasound_info;
} __packed;

extern void wcd_clsh_fsm(struct snd_soc_codec *codec,
		struct wcd_clsh_cdc_data *cdc_clsh_d,
		u8 clsh_event, u8 req_state,
		int int_mode);

extern void wcd_clsh_init(struct wcd_clsh_cdc_data *clsh);

#endif
