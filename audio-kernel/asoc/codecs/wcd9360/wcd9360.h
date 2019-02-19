/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#ifndef __WCD9360_H__
#define __WCD9360_H__

#include <dsp/apr_audio-v2.h>
#include "../wcd9xxx-slimslave.h"
#include "../wcd9xxx-common-v2.h"

#define WCD9360_REGISTER_START_OFFSET  0x800
#define WCD9360_SB_PGD_PORT_RX_BASE   0x40
#define WCD9360_SB_PGD_PORT_TX_BASE   0x50
#define WCD9360_RX_PORT_START_NUMBER  16

#define WCD9360_DMIC_CLK_DIV_2  0x0
#define WCD9360_DMIC_CLK_DIV_3  0x1
#define WCD9360_DMIC_CLK_DIV_4  0x2
#define WCD9360_DMIC_CLK_DIV_6  0x3
#define WCD9360_DMIC_CLK_DIV_8  0x4
#define WCD9360_DMIC_CLK_DIV_16  0x5
#define WCD9360_DMIC_CLK_DRIVE_DEFAULT 0x02

#define WCD9360_ANC_DMIC_X2_FULL_RATE 1
#define WCD9360_ANC_DMIC_X2_HALF_RATE 0

#define PAHU_MAX_MICBIAS 4
#define PAHU_NUM_INTERPOLATORS 10
#define MAX_ON_DEMAND_SUPPLY_NAME_LENGTH    64

/* Convert from vout ctl to micbias voltage in mV */
#define  WCD_VOUT_CTL_TO_MICB(v)  (1000 + v * 50)


/* Number of input and output Slimbus port */
enum {
	WCD9360_RX0 = 0,
	WCD9360_RX1,
	WCD9360_RX2,
	WCD9360_RX3,
	WCD9360_RX4,
	WCD9360_RX5,
	WCD9360_RX6,
	WCD9360_RX7,
	WCD9360_RX_MAX,
};

enum {
	WCD9360_TX0 = 0,
	WCD9360_TX1,
	WCD9360_TX2,
	WCD9360_TX3,
	WCD9360_TX4,
	WCD9360_TX5,
	WCD9360_TX6,
	WCD9360_TX7,
	WCD9360_TX8,
	WCD9360_TX9,
	WCD9360_TX10,
	WCD9360_TX11,
	WCD9360_TX12,
	WCD9360_TX13,
	WCD9360_TX14,
	WCD9360_TX15,
	WCD9360_TX_MAX,
};

/*
 * Selects compander and smart boost settings
 * for a given speaker mode
 */
enum {
	WCD9360_SPKR_MODE_DEFAULT,
	WCD9360_SPKR_MODE_1, /* COMP Gain = 12dB, Smartboost Max = 5.5V */
};

/*
 * Rx path gain offsets
 */
enum {
	WCD9360_RX_GAIN_OFFSET_M1P5_DB,
	WCD9360_RX_GAIN_OFFSET_0_DB,
};

enum {
	WCD9360_MIC_BIAS_1 = 1,
	WCD9360_MIC_BIAS_2,
	WCD9360_MIC_BIAS_3,
	WCD9360_MIC_BIAS_4
};

enum {
	WCD9360_MICB_PULLUP_ENABLE,
	WCD9360_MICB_PULLUP_DISABLE,
	WCD9360_MICB_ENABLE,
	WCD9360_MICB_DISABLE,
};

/*
 * Dai data structure holds the
 * dai specific info like rate,
 * channel number etc.
 */
struct pahu_codec_dai_data {
	u32 rate;
	u32 *ch_num;
	u32 ch_act;
	u32 ch_tot;
};

/*
 * Structure used to update codec
 * register defaults after reset
 */
struct pahu_reg_mask_val {
	u16 reg;
	u8 mask;
	u8 val;
};

#if IS_ENABLED(CONFIG_SND_SOC_WCD9360)
extern void *pahu_get_afe_config(struct snd_soc_codec *codec,
				  enum afe_config_type config_type);
extern int pahu_cdc_mclk_enable(struct snd_soc_codec *codec, bool enable);
extern int pahu_cdc_mclk_tx_enable(struct snd_soc_codec *codec, bool enable);
extern int pahu_set_spkr_mode(struct snd_soc_codec *codec, int mode);
extern int pahu_set_spkr_gain_offset(struct snd_soc_codec *codec, int offset);
extern void *pahu_get_wcd_dsp_cntl(struct device *dev);
extern int wcd9360_get_micb_vout_ctl_val(u32 micb_mv);
extern int pahu_codec_info_create_codec_entry(
				struct snd_info_entry *codec_root,
				struct snd_soc_codec *codec);
#else
extern void *pahu_get_afe_config(struct snd_soc_codec *codec,
				  enum afe_config_type config_type)
{
	return NULL;
}
extern int pahu_cdc_mclk_enable(struct snd_soc_codec *codec, bool enable)
{
	return 0;
}
extern int pahu_cdc_mclk_tx_enable(struct snd_soc_codec *codec, bool enable)
{
	return 0;
}
extern int pahu_set_spkr_mode(struct snd_soc_codec *codec, int mode)
{
	return 0;
}
extern int pahu_set_spkr_gain_offset(struct snd_soc_codec *codec, int offset)
{
	return 0;
}
extern void *pahu_get_wcd_dsp_cntl(struct device *dev)
{
	return NULL;
}
extern int wcd9360_get_micb_vout_ctl_val(u32 micb_mv)
{
	return 0;
}
extern int pahu_codec_info_create_codec_entry(
				struct snd_info_entry *codec_root,
				struct snd_soc_codec *codec)
{
	return 0;
}
#endif

#endif
