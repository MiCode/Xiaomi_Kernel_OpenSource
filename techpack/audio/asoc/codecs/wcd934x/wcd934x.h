/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 */
#ifndef WCD934X_H
#define WCD934X_H

#include <dsp/apr_audio-v2.h>
#include "wcd934x-dsp-cntl.h"
#include <asoc/wcd9xxx-slimslave.h>
#include <asoc/wcd9xxx-common-v2.h>
#include <asoc/wcd-mbhc-v2.h>

#define WCD934X_REGISTER_START_OFFSET  0x800
#define WCD934X_SB_PGD_PORT_RX_BASE   0x40
#define WCD934X_SB_PGD_PORT_TX_BASE   0x50
#define WCD934X_RX_PORT_START_NUMBER  16

#define WCD934X_DMIC_CLK_DIV_2  0x0
#define WCD934X_DMIC_CLK_DIV_3  0x1
#define WCD934X_DMIC_CLK_DIV_4  0x2
#define WCD934X_DMIC_CLK_DIV_6  0x3
#define WCD934X_DMIC_CLK_DIV_8  0x4
#define WCD934X_DMIC_CLK_DIV_16  0x5
#define WCD934X_DMIC_CLK_DRIVE_DEFAULT 0x02

#define WCD934X_ANC_DMIC_X2_FULL_RATE 1
#define WCD934X_ANC_DMIC_X2_HALF_RATE 0

#define TAVIL_MAX_MICBIAS 4
#define TAVIL_NUM_INTERPOLATORS 9
#define MAX_ON_DEMAND_SUPPLY_NAME_LENGTH    64

/* Convert from vout ctl to micbias voltage in mV */
#define  WCD_VOUT_CTL_TO_MICB(v)  (1000 + v * 50)

/* Feature masks to distinguish codec version */
#define DSD_DISABLED_MASK   0
#define SLNQ_DISABLED_MASK  1

#define DSD_DISABLED   (1 << DSD_DISABLED_MASK)
#define SLNQ_DISABLED  (1 << SLNQ_DISABLED_MASK)

/* Number of input and output Slimbus port */
enum {
	WCD934X_RX0 = 0,
	WCD934X_RX1,
	WCD934X_RX2,
	WCD934X_RX3,
	WCD934X_RX4,
	WCD934X_RX5,
	WCD934X_RX6,
	WCD934X_RX7,
	WCD934X_RX_MAX,
};

enum {
	WCD934X_TX0 = 0,
	WCD934X_TX1,
	WCD934X_TX2,
	WCD934X_TX3,
	WCD934X_TX4,
	WCD934X_TX5,
	WCD934X_TX6,
	WCD934X_TX7,
	WCD934X_TX8,
	WCD934X_TX9,
	WCD934X_TX10,
	WCD934X_TX11,
	WCD934X_TX12,
	WCD934X_TX13,
	WCD934X_TX14,
	WCD934X_TX15,
	WCD934X_TX_MAX,
};

enum {
	INTERP_EAR = 0,
	INTERP_HPHL,
	INTERP_HPHR,
	INTERP_LO1,
	INTERP_LO2,
	INTERP_LO3_NA, /* LO3 not avalible in Tavil*/
	INTERP_LO4_NA,
	INTERP_SPKR1,
	INTERP_SPKR2,
	INTERP_MAX,
};

/*
 * Selects compander and smart boost settings
 * for a given speaker mode
 */
enum {
	WCD934X_SPKR_MODE_DEFAULT,
	WCD934X_SPKR_MODE_1, /* COMP Gain = 12dB, Smartboost Max = 5.5V */
};

/*
 * Rx path gain offsets
 */
enum {
	WCD934X_RX_GAIN_OFFSET_M1P5_DB,
	WCD934X_RX_GAIN_OFFSET_0_DB,
};

/*
 * Dai data structure holds the
 * dai specific info like rate,
 * channel number etc.
 */
struct tavil_codec_dai_data {
	u32 rate;
	u32 *ch_num;
	u32 ch_act;
	u32 ch_tot;
};

/*
 * Structure used to update codec
 * register defaults after reset
 */
struct tavil_reg_mask_val {
	u16 reg;
	u8 mask;
	u8 val;
};

#if IS_ENABLED(CONFIG_SND_SOC_WCD934X)
extern void *tavil_get_afe_config(struct snd_soc_component *component,
				  enum afe_config_type config_type);
extern int tavil_cdc_mclk_enable(struct snd_soc_component *component,
				 bool enable);
extern int tavil_cdc_mclk_tx_enable(struct snd_soc_component *component,
				    bool enable);
extern int tavil_set_spkr_mode(struct snd_soc_component *component, int mode);
extern int tavil_set_spkr_gain_offset(struct snd_soc_component *component,
				      int offset);
extern struct wcd_dsp_cntl *tavil_get_wcd_dsp_cntl(struct device *dev);
extern int wcd934x_get_micb_vout_ctl_val(u32 micb_mv);
extern int tavil_micbias_control(struct snd_soc_component *component,
				 int micb_num,
				 int req, bool is_dapm);
extern int tavil_mbhc_micb_adjust_voltage(struct snd_soc_component *component,
					  int req_volt,
					  int micb_num);
extern struct wcd934x_mbhc *tavil_soc_get_mbhc(
				struct snd_soc_component *component);
extern int tavil_codec_enable_interp_clk(struct snd_soc_component *component,
					 int event, int intp_idx);
extern struct tavil_dsd_config *tavil_get_dsd_config(
				struct snd_soc_component *component);
extern int tavil_codec_info_create_codec_entry(
				struct snd_info_entry *codec_root,
				struct snd_soc_component *component);
#else
extern void *tavil_get_afe_config(struct snd_soc_component *component,
				  enum afe_config_type config_type)
{
	return NULL;
}
extern int tavil_cdc_mclk_enable(struct snd_soc_component *component,
				 bool enable)
{
	return 0;
}
extern int tavil_cdc_mclk_tx_enable(struct snd_soc_component *component,
				    bool enable)
{
	return 0;
}
extern int tavil_set_spkr_mode(struct snd_soc_component *component, int mode)
{
	return 0;
}
extern int tavil_set_spkr_gain_offset(struct snd_soc_component *component,
				      int offset)
{
	return 0;
}
extern struct wcd_dsp_cntl *tavil_get_wcd_dsp_cntl(struct device *dev)
{
	return NULL;
}
extern int wcd934x_get_micb_vout_ctl_val(u32 micb_mv)
{
	return 0;
}
extern int tavil_micbias_control(struct snd_soc_component *component,
				 int micb_num,
				 int req, bool is_dapm)
{
	return 0;
}
extern int tavil_mbhc_micb_adjust_voltage(struct snd_soc_component *component,
					  int req_volt,
					  int micb_num)
{
	return 0;
}
extern struct wcd934x_mbhc *tavil_soc_get_mbhc(
				struct snd_soc_component *component)
{
	return NULL;
}
extern int tavil_codec_enable_interp_clk(struct snd_soc_component *component,
					 int event, int intp_idx)
{
	return 0;
}
extern struct tavil_dsd_config *tavil_get_dsd_config(
				struct snd_soc_component *component)
{
	return NULL;
}
extern int tavil_codec_info_create_codec_entry(
				struct snd_info_entry *codec_root,
				struct snd_soc_component *component)
{
	return 0;
}

#endif

#endif
