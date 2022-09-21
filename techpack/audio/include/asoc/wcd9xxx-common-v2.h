/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _WCD9XXX_COMMON_V2

#define _WCD9XXX_COMMON_V2

#define CLSH_REQ_ENABLE true
#define CLSH_REQ_DISABLE false

#define WCD_CLSH_EVENT_PRE_DAC 0x01
#define WCD_CLSH_EVENT_POST_PA 0x02
#define MAX_VBAT_MONITOR_WRITES 17
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

/*
 * Though number of CLSH states are 4, max state shoulbe be 5
 * because state array index starts from 1.
 */
#define WCD_CLSH_STATE_MAX 5
#define NUM_CLSH_STATES_V2 (0x01 << WCD_CLSH_STATE_MAX)


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
	CLS_AB, /* Class-AB Low HIFI*/
	CLS_H_LOHIFI, /* LoHIFI */
	CLS_H_ULP, /* Ultra Low power */
	CLS_AB_HIFI, /* Class-AB */
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

struct wcd9xxx_anc_header {
	u32 reserved[3];
	u32 num_anc_slots;
};

struct vbat_monitor_reg {
	u32 size;
	u32 writes[MAX_VBAT_MONITOR_WRITES];
} __packed;

struct wcd_reg_mask_val {
	u16	reg;
	u8	mask;
	u8	val;
};

extern void wcd_clsh_fsm(struct snd_soc_component *component,
		struct wcd_clsh_cdc_data *cdc_clsh_d,
		u8 clsh_event, u8 req_state,
		int int_mode);

extern void wcd_clsh_init(struct wcd_clsh_cdc_data *clsh);
extern int wcd_clsh_get_clsh_state(struct wcd_clsh_cdc_data *clsh);
extern void wcd_clsh_imped_config(struct snd_soc_component *component,
		int imped, bool reset);

enum {
	RESERVED = 0,
	AANC_LPF_FF_FB = 1,
	AANC_LPF_COEFF_MSB,
	AANC_LPF_COEFF_LSB,
	HW_MAD_AUDIO_ENABLE,
	HW_MAD_ULTR_ENABLE,
	HW_MAD_BEACON_ENABLE,
	HW_MAD_AUDIO_SLEEP_TIME,
	HW_MAD_ULTR_SLEEP_TIME,
	HW_MAD_BEACON_SLEEP_TIME,
	HW_MAD_TX_AUDIO_SWITCH_OFF,
	HW_MAD_TX_ULTR_SWITCH_OFF,
	HW_MAD_TX_BEACON_SWITCH_OFF,
	MAD_AUDIO_INT_DEST_SELECT_REG,
	MAD_ULT_INT_DEST_SELECT_REG,
	MAD_BEACON_INT_DEST_SELECT_REG,
	MAD_CLIP_INT_DEST_SELECT_REG,
	VBAT_INT_DEST_SELECT_REG,
	MAD_AUDIO_INT_MASK_REG,
	MAD_ULT_INT_MASK_REG,
	MAD_BEACON_INT_MASK_REG,
	MAD_CLIP_INT_MASK_REG,
	VBAT_INT_MASK_REG,
	MAD_AUDIO_INT_STATUS_REG,
	MAD_ULT_INT_STATUS_REG,
	MAD_BEACON_INT_STATUS_REG,
	MAD_CLIP_INT_STATUS_REG,
	VBAT_INT_STATUS_REG,
	MAD_AUDIO_INT_CLEAR_REG,
	MAD_ULT_INT_CLEAR_REG,
	MAD_BEACON_INT_CLEAR_REG,
	MAD_CLIP_INT_CLEAR_REG,
	VBAT_INT_CLEAR_REG,
	SB_PGD_PORT_TX_WATERMARK_N,
	SB_PGD_PORT_TX_ENABLE_N,
	SB_PGD_PORT_RX_WATERMARK_N,
	SB_PGD_PORT_RX_ENABLE_N,
	SB_PGD_TX_PORTn_MULTI_CHNL_0,
	SB_PGD_TX_PORTn_MULTI_CHNL_1,
	SB_PGD_RX_PORTn_MULTI_CHNL_0,
	SB_PGD_RX_PORTn_MULTI_CHNL_1,
	AANC_FF_GAIN_ADAPTIVE,
	AANC_FFGAIN_ADAPTIVE_EN,
	AANC_GAIN_CONTROL,
	SPKR_CLIP_PIPE_BANK_SEL,
	SPKR_CLIPDET_VAL0,
	SPKR_CLIPDET_VAL1,
	SPKR_CLIPDET_VAL2,
	SPKR_CLIPDET_VAL3,
	SPKR_CLIPDET_VAL4,
	SPKR_CLIPDET_VAL5,
	SPKR_CLIPDET_VAL6,
	SPKR_CLIPDET_VAL7,
	VBAT_RELEASE_INT_DEST_SELECT_REG,
	VBAT_RELEASE_INT_MASK_REG,
	VBAT_RELEASE_INT_STATUS_REG,
	VBAT_RELEASE_INT_CLEAR_REG,
	MAD2_CLIP_INT_DEST_SELECT_REG,
	MAD2_CLIP_INT_MASK_REG,
	MAD2_CLIP_INT_STATUS_REG,
	MAD2_CLIP_INT_CLEAR_REG,
	SPKR2_CLIP_PIPE_BANK_SEL,
	SPKR2_CLIPDET_VAL0,
	SPKR2_CLIPDET_VAL1,
	SPKR2_CLIPDET_VAL2,
	SPKR2_CLIPDET_VAL3,
	SPKR2_CLIPDET_VAL4,
	SPKR2_CLIPDET_VAL5,
	SPKR2_CLIPDET_VAL6,
	SPKR2_CLIPDET_VAL7,
	MAX_CFG_REGISTERS,
};

#endif
