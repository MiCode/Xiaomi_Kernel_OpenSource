/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#ifndef WCD9335_H
#define WCD9335_H

#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/apr_audio-v2.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>
#include "wcd-mbhc-v2.h"

#define TASHA_REG_VAL(reg, val)      {reg, 0, val}

#define TASHA_REGISTER_START_OFFSET  0x800
#define TASHA_SB_PGD_PORT_RX_BASE   0x40
#define TASHA_SB_PGD_PORT_TX_BASE   0x50

#define TASHA_ZDET_SUPPORTED true
/* z value defined in milliohm */
#define TASHA_ZDET_VAL_32	32000
#define TASHA_ZDET_VAL_400	400000
#define TASHA_ZDET_VAL_1200	1200000
#define TASHA_ZDET_VAL_100K	100000000
/* z floating defined in ohms */
#define TASHA_ZDET_FLOATING_IMPEDANCE 0x0FFFFFFE

#define WCD9335_DMIC_CLK_DIV_2  0x0
#define WCD9335_DMIC_CLK_DIV_3  0x1
#define WCD9335_DMIC_CLK_DIV_4  0x2
#define WCD9335_DMIC_CLK_DIV_6  0x3
#define WCD9335_DMIC_CLK_DIV_8  0x4
#define WCD9335_DMIC_CLK_DIV_16  0x5
#define WCD9335_DMIC_CLK_DRIVE_DEFAULT 0x02

#define WCD9335_ANC_DMIC_X2_FULL_RATE 1
#define WCD9335_ANC_DMIC_X2_HALF_RATE 0

/* Number of input and output Slimbus port */
enum {
	TASHA_RX0 = 0,
	TASHA_RX1,
	TASHA_RX2,
	TASHA_RX3,
	TASHA_RX4,
	TASHA_RX5,
	TASHA_RX6,
	TASHA_RX7,
	TASHA_RX8,
	TASHA_RX9,
	TASHA_RX10,
	TASHA_RX11,
	TASHA_RX12,
	TASHA_RX_MAX,
};

enum {
	TASHA_TX0 = 0,
	TASHA_TX1,
	TASHA_TX2,
	TASHA_TX3,
	TASHA_TX4,
	TASHA_TX5,
	TASHA_TX6,
	TASHA_TX7,
	TASHA_TX8,
	TASHA_TX9,
	TASHA_TX10,
	TASHA_TX11,
	TASHA_TX12,
	TASHA_TX13,
	TASHA_TX14,
	TASHA_TX15,
	TASHA_TX_MAX,
};

enum {
	/* INTR_REG 0 */
	WCD9335_IRQ_FLL_LOCK_LOSS = 1,
	WCD9335_IRQ_HPH_PA_OCPL_FAULT,
	WCD9335_IRQ_HPH_PA_OCPR_FAULT,
	WCD9335_IRQ_EAR_PA_OCP_FAULT,
	WCD9335_IRQ_HPH_PA_CNPL_COMPLETE,
	WCD9335_IRQ_HPH_PA_CNPR_COMPLETE,
	WCD9335_IRQ_EAR_PA_CNP_COMPLETE,
	/* INTR_REG 1 */
	WCD9335_IRQ_MBHC_SW_DET,
	WCD9335_IRQ_MBHC_ELECT_INS_REM_DET,
	WCD9335_IRQ_MBHC_BUTTON_PRESS_DET,
	WCD9335_IRQ_MBHC_BUTTON_RELEASE_DET,
	WCD9335_IRQ_MBHC_ELECT_INS_REM_LEG_DET,
	WCD9335_IRQ_RESERVED_0,
	WCD9335_IRQ_RESERVED_1,
	WCD9335_IRQ_RESERVED_2,
	/* INTR_REG 2 */
	WCD9335_IRQ_LINE_PA1_CNP_COMPLETE,
	WCD9335_IRQ_LINE_PA2_CNP_COMPLETE,
	WCD9335_IRQ_LINE_PA3_CNP_COMPLETE,
	WCD9335_IRQ_LINE_PA4_CNP_COMPLETE,
	WCD9335_IRQ_SOUNDWIRE,
	WCD9335_IRQ_VDD_DIG_RAMP_COMPLETE,
	WCD9335_IRQ_RCO_ERROR,
	WCD9335_IRQ_SVA_ERROR,
	/* INTR_REG 3 */
	WCD9335_IRQ_MAD_AUDIO,
	WCD9335_IRQ_MAD_BEACON,
	WCD9335_IRQ_MAD_ULTRASOUND,
	WCD9335_IRQ_VBAT_ATTACK,
	WCD9335_IRQ_VBAT_RESTORE,
	WCD9335_IRQ_SVA_OUTBOX1,
	WCD9335_IRQ_SVA_OUTBOX2,
	WCD9335_NUM_IRQS,
};

enum wcd9335_codec_event {
	WCD9335_CODEC_EVENT_CODEC_UP = 0,
};

enum tasha_on_demand_supply {
	ON_DEMAND_MICBIAS = 0,
	ON_DEMAND_SUPPLIES_MAX,
};

/* structure used to put the defined
 * ondemand supply for codec
 * and count being used.
 */
struct on_demand_supply {
	struct regulator *supply;
	int ondemand_supply_count;
};

/* Dai data structure holds the
 * dai specific info like rate,
 * channel number etc.
 */
struct tasha_codec_dai_data {
	u32 rate;
	u32 *ch_num;
	u32 ch_act;
	u32 ch_tot;
};

/* Structure used to update codec
 * register defaults after reset
 */
struct tasha_reg_mask_val {
	u16 reg;
	u8 mask;
	u8 val;
};

/* Selects compander and smart boost settings
 * for a given speaker mode
 */
enum {
	SPKR_MODE_DEFAULT,
	SPKR_MODE_1,          /* COMP Gain = 12dB, Smartboost Max = 5.5V */
};

/*
 * Rx path gain offsets
 */
enum {
	RX_GAIN_OFFSET_M1P5_DB,
	RX_GAIN_OFFSET_0_DB,
};

extern void *tasha_get_afe_config(struct snd_soc_codec *codec,
				  enum afe_config_type config_type);
extern int tasha_cdc_mclk_enable(struct snd_soc_codec *codec, int enable,
				 bool dapm);
extern int tasha_cdc_mclk_tx_enable(struct snd_soc_codec *codec, int enable,
				    bool dapm);
extern int tasha_enable_efuse_sensing(struct snd_soc_codec *codec);
extern int tasha_mbhc_hs_detect(struct snd_soc_codec *codec,
				struct wcd_mbhc_config *mbhc_cfg);
extern void tasha_mbhc_hs_detect_exit(struct snd_soc_codec *codec);
extern void tasha_mbhc_zdet_gpio_ctrl(
		int (*zdet_gpio_cb)(struct snd_soc_codec *codec, bool high),
		struct snd_soc_codec *codec);
extern int tasha_codec_info_create_codec_entry(struct snd_info_entry *,
					       struct snd_soc_codec *);
extern void tasha_event_register(
	int (*machine_event_cb)(struct snd_soc_codec *codec,
				enum wcd9335_codec_event),
	struct snd_soc_codec *codec);
extern int tasha_codec_enable_standalone_micbias(struct snd_soc_codec *codec,
						 int micb_num,
						 bool enable);
extern int tasha_set_spkr_mode(struct snd_soc_codec *codec, int mode);
extern int tasha_set_spkr_gain_offset(struct snd_soc_codec *codec, int offset);
extern enum codec_variant tasha_codec_ver(void);
#endif
