/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef WCD9XXX_CODEC_COMMON

#define WCD9XXX_CODEC_COMMON

#include "wcd9xxx-resmgr.h"

#define WCD9XXX_CLSH_REQ_ENABLE true
#define WCD9XXX_CLSH_REQ_DISABLE false

#define WCD9XXX_CLSH_EVENT_PRE_DAC 0x01
#define WCD9XXX_CLSH_EVENT_POST_PA 0x02

/* Basic states for Class H state machine.
 * represented as a bit mask within a u8 data type
 * bit 0: EAR mode
 * bit 1: HPH Left mode
 * bit 2: HPH Right mode
 * bit 3: Lineout mode
 * bit 4: Ultrasound mode
 */
#define	WCD9XXX_CLSH_STATE_IDLE 0x00
#define	WCD9XXX_CLSH_STATE_EAR (0x01 << 0)
#define	WCD9XXX_CLSH_STATE_HPHL (0x01 << 1)
#define	WCD9XXX_CLSH_STATE_HPHR (0x01 << 2)
#define	WCD9XXX_CLSH_STATE_LO (0x01 << 3)
#define NUM_CLSH_STATES (0x01 << 4)

#define WCD9XXX_DMIC_SAMPLE_RATE_DIV_2    0x0
#define WCD9XXX_DMIC_SAMPLE_RATE_DIV_3    0x1
#define WCD9XXX_DMIC_SAMPLE_RATE_DIV_4    0x2

#define WCD9XXX_DMIC_B1_CTL_DIV_2 0x00
#define WCD9XXX_DMIC_B1_CTL_DIV_3 0x22
#define WCD9XXX_DMIC_B1_CTL_DIV_4 0x44

#define WCD9XXX_DMIC_B2_CTL_DIV_2 0x00
#define WCD9XXX_DMIC_B2_CTL_DIV_3 0x02
#define WCD9XXX_DMIC_B2_CTL_DIV_4 0x04

#define WCD9XXX_ANC_DMIC_X2_ON    0x1
#define WCD9XXX_ANC_DMIC_X2_OFF   0x0

/* Derived State: Bits 1 and 2 should be set for Headphone stereo */
#define WCD9XXX_CLSH_STATE_HPH_ST (WCD9XXX_CLSH_STATE_HPHL | \
						WCD9XXX_CLSH_STATE_HPHR)

#define WCD9XXX_CLSH_STATE_HPHL_EAR (WCD9XXX_CLSH_STATE_HPHL | \
						WCD9XXX_CLSH_STATE_EAR)
#define WCD9XXX_CLSH_STATE_HPHR_EAR (WCD9XXX_CLSH_STATE_HPHR | \
						WCD9XXX_CLSH_STATE_EAR)

#define WCD9XXX_CLSH_STATE_HPH_ST_EAR (WCD9XXX_CLSH_STATE_HPH_ST | \
						WCD9XXX_CLSH_STATE_EAR)

#define WCD9XXX_CLSH_STATE_HPHL_LO (WCD9XXX_CLSH_STATE_HPHL | \
						WCD9XXX_CLSH_STATE_LO)
#define WCD9XXX_CLSH_STATE_HPHR_LO (WCD9XXX_CLSH_STATE_HPHR | \
						WCD9XXX_CLSH_STATE_LO)

#define WCD9XXX_CLSH_STATE_HPH_ST_LO (WCD9XXX_CLSH_STATE_HPH_ST | \
						WCD9XXX_CLSH_STATE_LO)

#define WCD9XXX_CLSH_STATE_EAR_LO (WCD9XXX_CLSH_STATE_EAR | \
						WCD9XXX_CLSH_STATE_LO)

#define WCD9XXX_CLSH_STATE_HPHL_EAR_LO (WCD9XXX_CLSH_STATE_HPHL | \
						WCD9XXX_CLSH_STATE_EAR | \
						WCD9XXX_CLSH_STATE_LO)
#define WCD9XXX_CLSH_STATE_HPHR_EAR_LO (WCD9XXX_CLSH_STATE_HPHR | \
						WCD9XXX_CLSH_STATE_EAR | \
						WCD9XXX_CLSH_STATE_LO)
#define WCD9XXX_CLSH_STATE_HPH_ST_EAR_LO (WCD9XXX_CLSH_STATE_HPH_ST | \
						WCD9XXX_CLSH_STATE_EAR | \
						WCD9XXX_CLSH_STATE_LO)

struct wcd9xxx_reg_mask_val {
	u16	reg;
	u8	mask;
	u8	val;
};

enum ncp_fclk_level {
	NCP_FCLK_LEVEL_8,
	NCP_FCLK_LEVEL_5,
	NCP_FCLK_LEVEL_MAX,
};

/* Class H data that the codec driver will maintain */
struct wcd9xxx_clsh_cdc_data {
	u8 state;
	int buck_mv;
	bool is_dynamic_vdd_cp;
	int clsh_users;
	int buck_users;
	int ncp_users[NCP_FCLK_LEVEL_MAX];
	struct wcd9xxx_resmgr *resmgr;
};

struct wcd9xxx_anc_header {
	u32 reserved[3];
	u32 num_anc_slots;
};

enum wcd9xxx_buck_volt {
	WCD9XXX_CDC_BUCK_UNSUPPORTED = 0,
	WCD9XXX_CDC_BUCK_MV_1P8 = 1800000,
	WCD9XXX_CDC_BUCK_MV_2P15 = 2150000,
};

extern void wcd9xxx_clsh_fsm(struct snd_soc_codec *codec,
		struct wcd9xxx_clsh_cdc_data *cdc_clsh_d,
		u8 req_state, bool req_type, u8 clsh_event);

extern void wcd9xxx_clsh_init(struct wcd9xxx_clsh_cdc_data *clsh,
			      struct wcd9xxx_resmgr *resmgr);

extern void wcd9xxx_clsh_imped_config(struct snd_soc_codec *codec,
				  int imped);

enum wcd9xxx_codec_event {
	WCD9XXX_CODEC_EVENT_CODEC_UP = 0,
};

struct wcd9xxx_register_save_node {
	struct list_head lh;
	u16 reg;
	u16 value;
};

extern int wcd9xxx_soc_update_bits_push(struct snd_soc_codec *codec,
					struct list_head *lh,
					uint16_t reg, uint8_t mask,
					uint8_t value, int delay);
extern void wcd9xxx_restore_registers(struct snd_soc_codec *codec,
				      struct list_head *lh);
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
	MAD_VBAT_INT_DEST_SELECT_REG,
	MAD_AUDIO_INT_MASK_REG,
	MAD_ULT_INT_MASK_REG,
	MAD_BEACON_INT_MASK_REG,
	MAD_CLIP_INT_MASK_REG,
	MAD_VBAT_INT_MASK_REG,
	MAD_AUDIO_INT_STATUS_REG,
	MAD_ULT_INT_STATUS_REG,
	MAD_BEACON_INT_STATUS_REG,
	MAD_CLIP_INT_STATUS_REG,
	MAD_VBAT_INT_STATUS_REG,
	MAD_AUDIO_INT_CLEAR_REG,
	MAD_ULT_INT_CLEAR_REG,
	MAD_BEACON_INT_CLEAR_REG,
	MAD_CLIP_INT_CLEAR_REG,
	MAD_VBAT_INT_CLEAR_REG,
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
	MAX_CFG_REGISTERS,
};

#endif
