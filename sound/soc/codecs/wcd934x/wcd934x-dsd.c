/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mfd/wcd934x/registers.h>
#include <sound/tlv.h>
#include <sound/control.h>
#include "wcd934x-dsd.h"

#define DSD_VOLUME_MAX_0dB      0
#define DSD_VOLUME_MIN_M110dB   -110

#define DSD_VOLUME_RANGE_CHECK(x)   ((x >= DSD_VOLUME_MIN_M110dB) &&\
				     (x <= DSD_VOLUME_MAX_0dB))
#define DSD_VOLUME_STEPS            3
#define DSD_VOLUME_UPDATE_DELAY_MS  30
#define DSD_VOLUME_USLEEP_MARGIN_US 100
#define DSD_VOLUME_STEP_DELAY_US    ((1000 * DSD_VOLUME_UPDATE_DELAY_MS) / \
				     (2 * DSD_VOLUME_STEPS))

#define TAVIL_VERSION_1_0  0
#define TAVIL_VERSION_1_1  1

static const DECLARE_TLV_DB_MINMAX(tavil_dsd_db_scale, DSD_VOLUME_MIN_M110dB,
				   DSD_VOLUME_MAX_0dB);

static const char *const dsd_if_text[] = {
	"ZERO", "RX0", "RX1", "RX2", "RX3", "RX4", "RX5", "RX6", "RX7",
	"DSD_DATA_PAD"
};

static const char * const dsd_filt0_mux_text[] = {
	"ZERO", "DSD_L IF MUX",
};

static const char * const dsd_filt1_mux_text[] = {
	"ZERO", "DSD_R IF MUX",
};

static const struct soc_enum dsd_filt0_mux_enum =
	SOC_ENUM_SINGLE(WCD934X_CDC_DSD0_PATH_CTL, 0,
			ARRAY_SIZE(dsd_filt0_mux_text), dsd_filt0_mux_text);

static const struct soc_enum dsd_filt1_mux_enum =
	SOC_ENUM_SINGLE(WCD934X_CDC_DSD1_PATH_CTL, 0,
			ARRAY_SIZE(dsd_filt1_mux_text), dsd_filt1_mux_text);

static SOC_ENUM_SINGLE_DECL(dsd_l_if_enum, WCD934X_CDC_DSD0_CFG0,
			    2, dsd_if_text);
static SOC_ENUM_SINGLE_DECL(dsd_r_if_enum, WCD934X_CDC_DSD1_CFG0,
			    2, dsd_if_text);

static const struct snd_kcontrol_new dsd_filt0_mux =
		SOC_DAPM_ENUM("DSD Filt0 Mux", dsd_filt0_mux_enum);

static const struct snd_kcontrol_new dsd_filt1_mux =
		SOC_DAPM_ENUM("DSD Filt1 Mux", dsd_filt1_mux_enum);

static const struct snd_kcontrol_new dsd_l_if_mux =
		SOC_DAPM_ENUM("DSD Left If Mux", dsd_l_if_enum);
static const struct snd_kcontrol_new dsd_r_if_mux =
		SOC_DAPM_ENUM("DSD Right If Mux", dsd_r_if_enum);

static const struct snd_soc_dapm_route tavil_dsd_audio_map[] = {
	{"DSD_L IF MUX", "RX0", "CDC_IF RX0 MUX"},
	{"DSD_L IF MUX", "RX1", "CDC_IF RX1 MUX"},
	{"DSD_L IF MUX", "RX2", "CDC_IF RX2 MUX"},
	{"DSD_L IF MUX", "RX3", "CDC_IF RX3 MUX"},
	{"DSD_L IF MUX", "RX4", "CDC_IF RX4 MUX"},
	{"DSD_L IF MUX", "RX5", "CDC_IF RX5 MUX"},
	{"DSD_L IF MUX", "RX6", "CDC_IF RX6 MUX"},
	{"DSD_L IF MUX", "RX7", "CDC_IF RX7 MUX"},

	{"DSD_FILTER_0", NULL, "DSD_L IF MUX"},
	{"DSD_FILTER_0", NULL, "RX INT1 NATIVE SUPPLY"},
	{"RX INT1 MIX3", "DSD HPHL Switch", "DSD_FILTER_0"},

	{"DSD_R IF MUX", "RX0", "CDC_IF RX0 MUX"},
	{"DSD_R IF MUX", "RX1", "CDC_IF RX1 MUX"},
	{"DSD_R IF MUX", "RX2", "CDC_IF RX2 MUX"},
	{"DSD_R IF MUX", "RX3", "CDC_IF RX3 MUX"},
	{"DSD_R IF MUX", "RX4", "CDC_IF RX4 MUX"},
	{"DSD_R IF MUX", "RX5", "CDC_IF RX5 MUX"},
	{"DSD_R IF MUX", "RX6", "CDC_IF RX6 MUX"},
	{"DSD_R IF MUX", "RX7", "CDC_IF RX7 MUX"},

	{"DSD_FILTER_1", NULL, "DSD_R IF MUX"},
	{"DSD_FILTER_1", NULL, "RX INT2 NATIVE SUPPLY"},
	{"RX INT2 MIX3", "DSD HPHR Switch", "DSD_FILTER_1"},
};

static bool is_valid_dsd_interpolator(int interp_num)
{
	if ((interp_num == INTERP_HPHL) || (interp_num == INTERP_HPHR) ||
	    (interp_num == INTERP_LO1) || (interp_num == INTERP_LO2))
		return true;

	return false;
}

/**
 * tavil_dsd_set_mixer_value - Set DSD HPH/LO mixer value
 *
 * @dsd_conf: pointer to dsd config
 * @interp_num: Interpolator number (HPHL/R, LO1/2)
 * @sw_value: Mixer switch value
 *
 * Returns 0 on success or -EINVAL on failure
 */
int tavil_dsd_set_mixer_value(struct tavil_dsd_config *dsd_conf,
			      int interp_num, int sw_value)
{
	if (!dsd_conf)
		return -EINVAL;

	if (!is_valid_dsd_interpolator(interp_num))
		return -EINVAL;

	dsd_conf->dsd_interp_mixer[interp_num] = !!sw_value;

	return 0;
}
EXPORT_SYMBOL(tavil_dsd_set_mixer_value);

/**
 * tavil_dsd_get_current_mixer_value - Get DSD HPH/LO mixer value
 *
 * @dsd_conf: pointer to dsd config
 * @interp_num: Interpolator number (HPHL/R, LO1/2)
 *
 * Returns current mixer val for success or -EINVAL for failure
 */
int tavil_dsd_get_current_mixer_value(struct tavil_dsd_config *dsd_conf,
				      int interp_num)
{
	if (!dsd_conf)
		return -EINVAL;

	if (!is_valid_dsd_interpolator(interp_num))
		return -EINVAL;

	return dsd_conf->dsd_interp_mixer[interp_num];
}
EXPORT_SYMBOL(tavil_dsd_get_current_mixer_value);

/**
 * tavil_dsd_set_out_select - DSD0/1 out select to HPH or LO
 *
 * @dsd_conf: pointer to dsd config
 * @interp_num: Interpolator number (HPHL/R, LO1/2)
 *
 * Returns 0 for success or -EINVAL for failure
 */
int tavil_dsd_set_out_select(struct tavil_dsd_config *dsd_conf,
			     int interp_num)
{
	unsigned int reg, val;
	struct snd_soc_codec *codec;

	if (!dsd_conf || !dsd_conf->codec)
		return -EINVAL;

	codec = dsd_conf->codec;

	if (!is_valid_dsd_interpolator(interp_num)) {
		dev_err(codec->dev, "%s: Invalid Interpolator: %d for DSD\n",
			__func__, interp_num);
		return -EINVAL;
	}

	switch (interp_num) {
	case INTERP_HPHL:
		reg = WCD934X_CDC_DSD0_CFG0;
		val = 0x00;
		break;
	case INTERP_HPHR:
		reg = WCD934X_CDC_DSD1_CFG0;
		val = 0x00;
		break;
	case INTERP_LO1:
		reg = WCD934X_CDC_DSD0_CFG0;
		val = 0x02;
		break;
	case INTERP_LO2:
		reg = WCD934X_CDC_DSD1_CFG0;
		val = 0x02;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, reg, 0x02, val);

	return 0;
}
EXPORT_SYMBOL(tavil_dsd_set_out_select);

/**
 * tavil_dsd_reset - Reset DSD block
 *
 * @dsd_conf: pointer to dsd config
 *
 */
void tavil_dsd_reset(struct tavil_dsd_config *dsd_conf)
{
	if (!dsd_conf || !dsd_conf->codec)
		return;

	snd_soc_update_bits(dsd_conf->codec, WCD934X_CDC_DSD0_PATH_CTL,
			    0x02, 0x02);
	snd_soc_update_bits(dsd_conf->codec, WCD934X_CDC_DSD0_PATH_CTL,
			    0x01, 0x00);
	snd_soc_update_bits(dsd_conf->codec, WCD934X_CDC_DSD1_PATH_CTL,
			    0x02, 0x02);
	snd_soc_update_bits(dsd_conf->codec, WCD934X_CDC_DSD1_PATH_CTL,
			    0x01, 0x00);
}
EXPORT_SYMBOL(tavil_dsd_reset);

/**
 * tavil_dsd_set_interp_rate - Set interpolator rate for DSD
 *
 * @dsd_conf: pointer to dsd config
 * @rx_port: RX port number
 * @sample_rate: Sample rate of the RX interpolator
 * @sample_rate_val: Interpolator rate value
 */
void tavil_dsd_set_interp_rate(struct tavil_dsd_config *dsd_conf, u16 rx_port,
			       u32 sample_rate, u8 sample_rate_val)
{
	u8 dsd_inp_sel;
	u8 dsd0_inp, dsd1_inp;
	u8 val0, val1;
	u8 dsd0_out_sel, dsd1_out_sel;
	u16 int_fs_reg, interp_num = 0;
	struct snd_soc_codec *codec;

	if (!dsd_conf || !dsd_conf->codec)
		return;

	codec = dsd_conf->codec;

	dsd_inp_sel = DSD_INP_SEL_RX0 + rx_port - WCD934X_RX_PORT_START_NUMBER;

	val0 = snd_soc_read(codec, WCD934X_CDC_DSD0_CFG0);
	val1 = snd_soc_read(codec, WCD934X_CDC_DSD1_CFG0);
	dsd0_inp = (val0 & 0x3C) >> 2;
	dsd1_inp = (val1 & 0x3C) >> 2;
	dsd0_out_sel = (val0 & 0x02) >> 1;
	dsd1_out_sel = (val1 & 0x02) >> 1;

	/* Set HPHL or LO1 interp rate based on out select */
	if (dsd_inp_sel == dsd0_inp) {
		interp_num = dsd0_out_sel ? INTERP_LO1 : INTERP_HPHL;
		dsd_conf->base_sample_rate[DSD0] = sample_rate;
	}

	/* Set HPHR or LO2 interp rate based on out select */
	if (dsd_inp_sel == dsd1_inp) {
		interp_num = dsd1_out_sel ? INTERP_LO2 : INTERP_HPHR;
		dsd_conf->base_sample_rate[DSD1] = sample_rate;
	}

	if (interp_num) {
		int_fs_reg = WCD934X_CDC_RX0_RX_PATH_CTL + 20 * interp_num;
		if ((snd_soc_read(codec, int_fs_reg) & 0x0f) < 0x09) {
			dev_dbg(codec->dev, "%s: Set Interp %d to sample_rate val 0x%x\n",
				__func__, interp_num, sample_rate_val);
			snd_soc_update_bits(codec, int_fs_reg, 0x0F,
					    sample_rate_val);
		}
	}
}
EXPORT_SYMBOL(tavil_dsd_set_interp_rate);

static int tavil_set_dsd_mode(struct snd_soc_codec *codec, int dsd_num,
			      u8 *pcm_rate_val)
{
	unsigned int dsd_out_sel_reg;
	u8 dsd_mode;
	u32 sample_rate;
	struct tavil_dsd_config *dsd_conf = tavil_get_dsd_config(codec);

	if (!dsd_conf)
		return -EINVAL;

	if ((dsd_num < 0) || (dsd_num > 1))
		return -EINVAL;

	sample_rate = dsd_conf->base_sample_rate[dsd_num];
	dsd_out_sel_reg = WCD934X_CDC_DSD0_CFG0 + dsd_num * 16;

	switch (sample_rate) {
	case 176400:
		dsd_mode = 0; /* DSD_64 */
		*pcm_rate_val = 0xb;
		break;
	case 352800:
		dsd_mode = 1; /* DSD_128 */
		*pcm_rate_val = 0xc;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid DSD rate: %d\n",
			__func__, sample_rate);
		return -EINVAL;
	}

	snd_soc_update_bits(codec, dsd_out_sel_reg, 0x01, dsd_mode);

	return 0;
}

static void tavil_dsd_data_pull(struct snd_soc_codec *codec, int dsd_num,
				u8 pcm_rate_val, bool enable)
{
	u8 clk_en, mute_en;
	u8 dsd_inp_sel;

	if (enable) {
		clk_en = 0x20;
		mute_en = 0x10;
	} else {
		clk_en = 0x00;
		mute_en = 0x00;
	}

	if (dsd_num & 0x01) {
		snd_soc_update_bits(codec, WCD934X_CDC_RX7_RX_PATH_MIX_CTL,
				    0x20, clk_en);
		dsd_inp_sel = (snd_soc_read(codec, WCD934X_CDC_DSD0_CFG0) &
				0x3C) >> 2;
		dsd_inp_sel = (enable) ? dsd_inp_sel : 0;
		if (dsd_inp_sel < 9) {
			snd_soc_update_bits(codec,
					WCD934X_CDC_RX_INP_MUX_RX_INT7_CFG1,
					0x0F, dsd_inp_sel);
			snd_soc_update_bits(codec,
					WCD934X_CDC_RX7_RX_PATH_MIX_CTL,
					0x0F, pcm_rate_val);
			snd_soc_update_bits(codec,
					WCD934X_CDC_RX7_RX_PATH_MIX_CTL,
					0x10, mute_en);
		}
	}
	if (dsd_num & 0x02) {
		snd_soc_update_bits(codec, WCD934X_CDC_RX8_RX_PATH_MIX_CTL,
				    0x20, clk_en);
		dsd_inp_sel = (snd_soc_read(codec, WCD934X_CDC_DSD1_CFG0) &
				0x3C) >> 2;
		dsd_inp_sel = (enable) ? dsd_inp_sel : 0;
		if (dsd_inp_sel < 9) {
			snd_soc_update_bits(codec,
					WCD934X_CDC_RX_INP_MUX_RX_INT8_CFG1,
					0x0F, dsd_inp_sel);
			snd_soc_update_bits(codec,
					WCD934X_CDC_RX8_RX_PATH_MIX_CTL,
					0x0F, pcm_rate_val);
			snd_soc_update_bits(codec,
					WCD934X_CDC_RX8_RX_PATH_MIX_CTL,
					0x10, mute_en);
		}
	}
}

static void tavil_dsd_update_volume(struct tavil_dsd_config *dsd_conf)
{
	snd_soc_update_bits(dsd_conf->codec, WCD934X_CDC_TOP_TOP_CFG0,
			    0x01, 0x01);
	snd_soc_update_bits(dsd_conf->codec, WCD934X_CDC_TOP_TOP_CFG0,
			    0x01, 0x00);
}

static int tavil_enable_dsd(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tavil_dsd_config *dsd_conf = tavil_get_dsd_config(codec);
	int rc, clk_users;
	int interp_idx;
	u8 pcm_rate_val;

	if (!dsd_conf) {
		dev_err(codec->dev, "%s: null dsd_config pointer\n", __func__);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: DSD%d, event: %d\n", __func__,
		w->shift, event);

	if (w->shift == DSD0) {
		/* Read out select */
		if (snd_soc_read(codec, WCD934X_CDC_DSD0_CFG0) & 0x02)
			interp_idx = INTERP_LO1;
		else
			interp_idx = INTERP_HPHL;
	} else if (w->shift == DSD1) {
		/* Read out select */
		if (snd_soc_read(codec, WCD934X_CDC_DSD1_CFG0) & 0x02)
			interp_idx = INTERP_LO2;
		else
			interp_idx = INTERP_HPHR;
	} else {
		dev_err(codec->dev, "%s: Unsupported DSD:%d\n",
			__func__, w->shift);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		clk_users = tavil_codec_enable_interp_clk(codec, event,
							  interp_idx);

		rc = tavil_set_dsd_mode(codec, w->shift, &pcm_rate_val);
		if (rc)
			return rc;

		tavil_dsd_data_pull(codec, (1 << w->shift), pcm_rate_val,
				    true);

		snd_soc_update_bits(codec,
				    WCD934X_CDC_CLK_RST_CTRL_DSD_CONTROL, 0x01,
				    0x01);
		if (w->shift == DSD0) {
			snd_soc_update_bits(codec, WCD934X_CDC_DSD0_PATH_CTL,
					    0x02, 0x02);
			snd_soc_update_bits(codec, WCD934X_CDC_DSD0_PATH_CTL,
					    0x02, 0x00);
			snd_soc_update_bits(codec, WCD934X_CDC_DSD0_PATH_CTL,
					    0x01, 0x01);
			/* Apply Gain */
			snd_soc_write(codec, WCD934X_CDC_DSD0_CFG1,
				      dsd_conf->volume[DSD0]);
			if (dsd_conf->version == TAVIL_VERSION_1_1)
				tavil_dsd_update_volume(dsd_conf);

		} else if (w->shift == DSD1) {
			snd_soc_update_bits(codec, WCD934X_CDC_DSD1_PATH_CTL,
					    0x02, 0x02);
			snd_soc_update_bits(codec, WCD934X_CDC_DSD1_PATH_CTL,
					    0x02, 0x00);
			snd_soc_update_bits(codec, WCD934X_CDC_DSD1_PATH_CTL,
					    0x01, 0x01);
			/* Apply Gain */
			snd_soc_write(codec, WCD934X_CDC_DSD1_CFG1,
				      dsd_conf->volume[DSD1]);
			if (dsd_conf->version == TAVIL_VERSION_1_1)
				tavil_dsd_update_volume(dsd_conf);
		}
		/* 10msec sleep required after DSD clock is set */
		usleep_range(10000, 10100);

		if (clk_users > 1) {
			snd_soc_update_bits(codec, WCD934X_ANA_RX_SUPPLIES,
					    0x02, 0x02);
			if (w->shift == DSD0)
				snd_soc_update_bits(codec,
						    WCD934X_CDC_DSD0_CFG2,
						    0x04, 0x00);
			if (w->shift == DSD1)
				snd_soc_update_bits(codec,
						    WCD934X_CDC_DSD1_CFG2,
						    0x04, 0x00);

		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (w->shift == DSD0) {
			snd_soc_update_bits(codec, WCD934X_CDC_DSD0_CFG2,
					    0x04, 0x04);
			snd_soc_update_bits(codec, WCD934X_CDC_DSD0_PATH_CTL,
					    0x01, 0x00);
		} else if (w->shift == DSD1) {
			snd_soc_update_bits(codec, WCD934X_CDC_DSD1_CFG2,
					    0x04, 0x04);
			snd_soc_update_bits(codec, WCD934X_CDC_DSD1_PATH_CTL,
					    0x01, 0x00);
		}

		tavil_codec_enable_interp_clk(codec, event, interp_idx);

		if (!(snd_soc_read(codec, WCD934X_CDC_DSD0_PATH_CTL) & 0x01) &&
		    !(snd_soc_read(codec, WCD934X_CDC_DSD1_PATH_CTL) & 0x01)) {
			snd_soc_update_bits(codec,
					WCD934X_CDC_CLK_RST_CTRL_DSD_CONTROL,
					0x01, 0x00);
			tavil_dsd_data_pull(codec, 0x03, 0x04, false);
			tavil_dsd_reset(dsd_conf);
		}
		break;
	}

	return 0;
}

static int tavil_dsd_vol_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = DSD_VOLUME_MIN_M110dB;
	uinfo->value.integer.max = DSD_VOLUME_MAX_0dB;

	return 0;
}

static int tavil_dsd_vol_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_dsd_config *dsd_conf = tavil_get_dsd_config(codec);
	int nv[DSD_MAX], cv[DSD_MAX];
	int step_size, nv1;
	int i, dsd_idx;

	if (!dsd_conf)
		return 0;

	mutex_lock(&dsd_conf->vol_mutex);

	for (dsd_idx = DSD0; dsd_idx < DSD_MAX; dsd_idx++) {
		cv[dsd_idx] = dsd_conf->volume[dsd_idx];
		nv[dsd_idx] = ucontrol->value.integer.value[dsd_idx];
	}

	if ((!DSD_VOLUME_RANGE_CHECK(nv[DSD0])) ||
	    (!DSD_VOLUME_RANGE_CHECK(nv[DSD1])))
		goto done;

	for (dsd_idx = DSD0; dsd_idx < DSD_MAX; dsd_idx++) {
		if (cv[dsd_idx] == nv[dsd_idx])
			continue;

		dev_dbg(codec->dev, "%s: DSD%d cur.vol: %d, new vol: %d\n",
			__func__, dsd_idx, cv[dsd_idx], nv[dsd_idx]);

		step_size =  (nv[dsd_idx] - cv[dsd_idx]) /
			      DSD_VOLUME_STEPS;

		nv1 = cv[dsd_idx];

		for (i = 0; i < DSD_VOLUME_STEPS; i++) {
			nv1 += step_size;
			snd_soc_write(codec,
				      WCD934X_CDC_DSD0_CFG1 + 16 * dsd_idx,
				      nv1);
			if (dsd_conf->version == TAVIL_VERSION_1_1)
				tavil_dsd_update_volume(dsd_conf);

			/* sleep required after each volume step */
			usleep_range(DSD_VOLUME_STEP_DELAY_US,
				     (DSD_VOLUME_STEP_DELAY_US +
				      DSD_VOLUME_USLEEP_MARGIN_US));
		}
		if (nv1 != nv[dsd_idx]) {
			snd_soc_write(codec,
				      WCD934X_CDC_DSD0_CFG1 + 16 * dsd_idx,
				      nv[dsd_idx]);

			if (dsd_conf->version == TAVIL_VERSION_1_1)
				tavil_dsd_update_volume(dsd_conf);
		}

		dsd_conf->volume[dsd_idx] = nv[dsd_idx];
	}

done:
	mutex_unlock(&dsd_conf->vol_mutex);

	return 0;
}

static int tavil_dsd_vol_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tavil_dsd_config *dsd_conf = tavil_get_dsd_config(codec);

	if (dsd_conf) {
		ucontrol->value.integer.value[0] = dsd_conf->volume[DSD0];
		ucontrol->value.integer.value[1] = dsd_conf->volume[DSD1];
	}

	return 0;
}

static const struct snd_kcontrol_new tavil_dsd_vol_controls[] = {
	{
	   .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	   .access = (SNDRV_CTL_ELEM_ACCESS_READWRITE |
		      SNDRV_CTL_ELEM_ACCESS_TLV_READ),
	   .name = "DSD Volume",
	   .info = tavil_dsd_vol_info,
	   .get = tavil_dsd_vol_get,
	   .put = tavil_dsd_vol_put,
	   .tlv = { .p = tavil_dsd_db_scale },
	},
};

static const struct snd_soc_dapm_widget tavil_dsd_widgets[] = {
	SND_SOC_DAPM_MUX("DSD_L IF MUX", SND_SOC_NOPM, 0, 0, &dsd_l_if_mux),
	SND_SOC_DAPM_MUX_E("DSD_FILTER_0", SND_SOC_NOPM, 0, 0, &dsd_filt0_mux,
			   tavil_enable_dsd,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("DSD_R IF MUX", SND_SOC_NOPM, 0, 0, &dsd_r_if_mux),
	SND_SOC_DAPM_MUX_E("DSD_FILTER_1", SND_SOC_NOPM, 1, 0, &dsd_filt1_mux,
			   tavil_enable_dsd,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

/**
 * tavil_dsd_post_ssr_init - DSD intialization after subsystem restart
 *
 * @codec: pointer to snd_soc_codec
 *
 * Returns 0 on success or error on failure
 */
int tavil_dsd_post_ssr_init(struct tavil_dsd_config *dsd_conf)
{
	struct snd_soc_codec *codec;

	if (!dsd_conf || !dsd_conf->codec)
		return -EINVAL;

	codec = dsd_conf->codec;
	/* Disable DSD Interrupts */
	snd_soc_update_bits(codec, WCD934X_INTR_CODEC_MISC_MASK, 0x08, 0x08);

	/* DSD registers init */
	if (dsd_conf->version == TAVIL_VERSION_1_0) {
		snd_soc_update_bits(codec, WCD934X_CDC_DSD0_CFG2, 0x02, 0x00);
		snd_soc_update_bits(codec, WCD934X_CDC_DSD1_CFG2, 0x02, 0x00);
	}
	/* DSD0: Mute EN */
	snd_soc_update_bits(codec, WCD934X_CDC_DSD0_CFG2, 0x04, 0x04);
	/* DSD1: Mute EN */
	snd_soc_update_bits(codec, WCD934X_CDC_DSD1_CFG2, 0x04, 0x04);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD0_DEBUG_CFG3, 0x10,
			    0x10);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD1_DEBUG_CFG3, 0x10,
			    0x10);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD0_DEBUG_CFG0, 0x0E,
			    0x0A);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD1_DEBUG_CFG0, 0x0E,
			    0x0A);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD0_DEBUG_CFG1, 0x07,
			    0x04);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD1_DEBUG_CFG1, 0x07,
			    0x04);

	/* Enable DSD Interrupts */
	snd_soc_update_bits(codec, WCD934X_INTR_CODEC_MISC_MASK, 0x08, 0x00);

	return 0;
}
EXPORT_SYMBOL(tavil_dsd_post_ssr_init);

/**
 * tavil_dsd_init - DSD intialization
 *
 * @codec: pointer to snd_soc_codec
 *
 * Returns pointer to tavil_dsd_config for success or NULL for failure
 */
struct tavil_dsd_config *tavil_dsd_init(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm;
	struct tavil_dsd_config *dsd_conf;
	u8 val;

	if (!codec)
		return NULL;

	dapm = snd_soc_codec_get_dapm(codec);

	/* Read efuse register to check if DSD is supported */
	val = snd_soc_read(codec, WCD934X_CHIP_TIER_CTRL_EFUSE_VAL_OUT14);
	if (val & 0x80) {
		dev_info(codec->dev, "%s: DSD unsupported for this codec version\n",
			 __func__);
		return NULL;
	}

	dsd_conf = devm_kzalloc(codec->dev, sizeof(struct tavil_dsd_config),
				GFP_KERNEL);
	if (!dsd_conf)
		return NULL;

	dsd_conf->codec = codec;

	/* Read version */
	dsd_conf->version = snd_soc_read(codec,
					 WCD934X_CHIP_TIER_CTRL_CHIP_ID_BYTE0);
	/* DSD registers init */
	if (dsd_conf->version == TAVIL_VERSION_1_0) {
		snd_soc_update_bits(codec, WCD934X_CDC_DSD0_CFG2, 0x02, 0x00);
		snd_soc_update_bits(codec, WCD934X_CDC_DSD1_CFG2, 0x02, 0x00);
	}
	/* DSD0: Mute EN */
	snd_soc_update_bits(codec, WCD934X_CDC_DSD0_CFG2, 0x04, 0x04);
	/* DSD1: Mute EN */
	snd_soc_update_bits(codec, WCD934X_CDC_DSD1_CFG2, 0x04, 0x04);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD0_DEBUG_CFG3, 0x10,
			    0x10);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD1_DEBUG_CFG3, 0x10,
			    0x10);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD0_DEBUG_CFG0, 0x0E,
			    0x0A);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD1_DEBUG_CFG0, 0x0E,
			    0x0A);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD0_DEBUG_CFG1, 0x07,
			    0x04);
	snd_soc_update_bits(codec, WCD934X_CDC_DEBUG_DSD1_DEBUG_CFG1, 0x07,
			    0x04);

	snd_soc_dapm_new_controls(dapm, tavil_dsd_widgets,
				  ARRAY_SIZE(tavil_dsd_widgets));

	snd_soc_dapm_add_routes(dapm, tavil_dsd_audio_map,
				ARRAY_SIZE(tavil_dsd_audio_map));

	mutex_init(&dsd_conf->vol_mutex);
	dsd_conf->volume[DSD0] = DSD_VOLUME_MAX_0dB;
	dsd_conf->volume[DSD1] = DSD_VOLUME_MAX_0dB;

	snd_soc_add_codec_controls(codec, tavil_dsd_vol_controls,
				   ARRAY_SIZE(tavil_dsd_vol_controls));

	/* Enable DSD Interrupts */
	snd_soc_update_bits(codec, WCD934X_INTR_CODEC_MISC_MASK, 0x08, 0x00);

	return dsd_conf;
}
EXPORT_SYMBOL(tavil_dsd_init);

/**
 * tavil_dsd_deinit - DSD de-intialization
 *
 * @dsd_conf: pointer to tavil_dsd_config
 */
void tavil_dsd_deinit(struct tavil_dsd_config *dsd_conf)
{
	struct snd_soc_codec *codec;

	if (!dsd_conf)
		return;

	codec = dsd_conf->codec;

	mutex_destroy(&dsd_conf->vol_mutex);

	/* Disable DSD Interrupts */
	snd_soc_update_bits(codec, WCD934X_INTR_CODEC_MISC_MASK, 0x08, 0x08);

	devm_kfree(codec->dev, dsd_conf);
}
EXPORT_SYMBOL(tavil_dsd_deinit);
