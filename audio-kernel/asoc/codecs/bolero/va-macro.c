/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "bolero-cdc.h"
#include "bolero-cdc-registers.h"

#define VA_MACRO_MAX_OFFSET 0x1000

#define VA_MACRO_NUM_DECIMATORS 8

#define VA_MACRO_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define VA_MACRO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE)

#define  TX_HPF_CUT_OFF_FREQ_MASK	0x60
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

#define VA_MACRO_DMIC_SAMPLE_RATE_UNDEFINED 0
#define VA_MACRO_MCLK_FREQ 9600000
#define VA_MACRO_TX_PATH_OFFSET 0x80
#define VA_MACRO_TX_DMIC_CLK_DIV_MASK 0x0E
#define VA_MACRO_TX_DMIC_CLK_DIV_SHFT 0x01

#define BOLERO_CDC_VA_TX_UNMUTE_DELAY_MS	40
#define MAX_RETRY_ATTEMPTS 500

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static int va_tx_unmute_delay = BOLERO_CDC_VA_TX_UNMUTE_DELAY_MS;
module_param(va_tx_unmute_delay, int, 0664);
MODULE_PARM_DESC(va_tx_unmute_delay, "delay to unmute the tx path");

enum {
	VA_MACRO_AIF_INVALID = 0,
	VA_MACRO_AIF1_CAP,
	VA_MACRO_AIF2_CAP,
	VA_MACRO_MAX_DAIS,
};

enum {
	VA_MACRO_DEC0,
	VA_MACRO_DEC1,
	VA_MACRO_DEC2,
	VA_MACRO_DEC3,
	VA_MACRO_DEC4,
	VA_MACRO_DEC5,
	VA_MACRO_DEC6,
	VA_MACRO_DEC7,
	VA_MACRO_DEC_MAX,
};

enum {
	VA_MACRO_CLK_DIV_2,
	VA_MACRO_CLK_DIV_3,
	VA_MACRO_CLK_DIV_4,
	VA_MACRO_CLK_DIV_6,
	VA_MACRO_CLK_DIV_8,
	VA_MACRO_CLK_DIV_16,
};

struct va_mute_work {
	struct va_macro_priv *va_priv;
	u32 decimator;
	struct delayed_work dwork;
};

struct hpf_work {
	struct va_macro_priv *va_priv;
	u8 decimator;
	u8 hpf_cut_off_freq;
	struct delayed_work dwork;
};

struct va_macro_priv {
	struct device *dev;
	bool dec_active[VA_MACRO_NUM_DECIMATORS];
	bool va_without_decimation;
	struct clk *va_core_clk;
	struct mutex mclk_lock;
	struct snd_soc_codec *codec;
	struct hpf_work va_hpf_work[VA_MACRO_NUM_DECIMATORS];
	struct va_mute_work va_mute_dwork[VA_MACRO_NUM_DECIMATORS];
	unsigned long active_ch_mask[VA_MACRO_MAX_DAIS];
	unsigned long active_ch_cnt[VA_MACRO_MAX_DAIS];
	s32 dmic_0_1_clk_cnt;
	s32 dmic_2_3_clk_cnt;
	s32 dmic_4_5_clk_cnt;
	s32 dmic_6_7_clk_cnt;
	u16 dmic_clk_div;
	u16 va_mclk_users;
	char __iomem *va_io_base;
	struct regulator *micb_supply;
	u32 micb_voltage;
	u32 micb_current;
	int micb_users;
};

static bool va_macro_get_data(struct snd_soc_codec *codec,
			      struct device **va_dev,
			      struct va_macro_priv **va_priv,
			      const char *func_name)
{
	*va_dev = bolero_get_device_ptr(codec->dev, VA_MACRO);
	if (!(*va_dev)) {
		dev_err(codec->dev,
			"%s: null device for macro!\n", func_name);
		return false;
	}
	*va_priv = dev_get_drvdata((*va_dev));
	if (!(*va_priv) || !(*va_priv)->codec) {
		dev_err(codec->dev,
			"%s: priv is null for macro!\n", func_name);
		return false;
	}
	return true;
}

static int va_macro_mclk_enable(struct va_macro_priv *va_priv,
				 bool mclk_enable, bool dapm)
{
	struct regmap *regmap = dev_get_regmap(va_priv->dev->parent, NULL);
	int ret = 0;

	if (regmap == NULL) {
		dev_err(va_priv->dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(va_priv->dev, "%s: mclk_enable = %u, dapm = %d clk_users= %d\n",
		__func__, mclk_enable, dapm, va_priv->va_mclk_users);

	mutex_lock(&va_priv->mclk_lock);
	if (mclk_enable) {
		if (va_priv->va_mclk_users == 0) {
			ret = bolero_request_clock(va_priv->dev,
						VA_MACRO, MCLK_MUX0, true);
			if (ret < 0) {
				dev_err(va_priv->dev,
					"%s: va request clock en failed\n",
					__func__);
				goto exit;
			}
			regcache_mark_dirty(regmap);
			regcache_sync_region(regmap,
					VA_START_OFFSET,
					VA_MAX_OFFSET);
			regmap_update_bits(regmap,
				BOLERO_CDC_VA_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x01);
			regmap_update_bits(regmap,
				BOLERO_CDC_VA_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x01);
			regmap_update_bits(regmap,
				BOLERO_CDC_VA_TOP_CSR_TOP_CFG0,
				0x02, 0x02);
		}
		va_priv->va_mclk_users++;
	} else {
		if (va_priv->va_mclk_users <= 0) {
			dev_err(va_priv->dev, "%s: clock already disabled\n",
			__func__);
			va_priv->va_mclk_users = 0;
			goto exit;
		}
		va_priv->va_mclk_users--;
		if (va_priv->va_mclk_users == 0) {
			regmap_update_bits(regmap,
				BOLERO_CDC_VA_TOP_CSR_TOP_CFG0,
				0x02, 0x00);
			regmap_update_bits(regmap,
				BOLERO_CDC_VA_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x00);
			regmap_update_bits(regmap,
				BOLERO_CDC_VA_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x00);
			bolero_request_clock(va_priv->dev,
					VA_MACRO, MCLK_MUX0, false);
		}
	}
exit:
	mutex_unlock(&va_priv->mclk_lock);
	return ret;
}

static int va_macro_event_handler(struct snd_soc_codec *codec, u16 event,
				  u32 data)
{
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;
	int retry_cnt = MAX_RETRY_ATTEMPTS;

	if (!va_macro_get_data(codec, &va_dev, &va_priv, __func__))
		return -EINVAL;

	switch (event) {
	case BOLERO_MACRO_EVT_WAIT_VA_CLK_RESET:
		while ((va_priv->va_mclk_users != 0) && (retry_cnt != 0)) {
			dev_dbg_ratelimited(va_dev, "%s:retry_cnt: %d\n",
				__func__, retry_cnt);
			/*
			 * Userspace takes 10 seconds to close
			 * the session when pcm_start fails due to concurrency
			 * with PDR/SSR. Loop and check every 20ms till 10
			 * seconds for va_mclk user count to get reset to 0
			 * which ensures userspace teardown is done and SSR
			 * powerup seq can proceed.
			 */
			msleep(20);
			retry_cnt--;
		}
		if (retry_cnt == 0)
			dev_err(va_dev,
				"%s: va_mclk_users is non-zero still, audio SSR fail!!\n",
				__func__);
		break;
	default:
		break;
	}
	return 0;
}

static int va_macro_mclk_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(codec, &va_dev, &va_priv, __func__))
		return -EINVAL;

	dev_dbg(va_dev, "%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = va_macro_mclk_enable(va_priv, 1, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		va_macro_mclk_enable(va_priv, 0, true);
		break;
	default:
		dev_err(va_priv->dev,
			"%s: invalid DAPM event %d\n", __func__, event);
		ret = -EINVAL;
	}
	return ret;
}

static int va_macro_mclk_ctrl(struct device *dev, bool enable)
{
	struct va_macro_priv *va_priv = dev_get_drvdata(dev);
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(va_priv->va_core_clk);
		if (ret < 0) {
			dev_err(dev, "%s:va mclk enable failed\n", __func__);
			goto exit;
		}
	} else {
		clk_disable_unprepare(va_priv->va_core_clk);
	}

exit:
	return ret;
}

static void va_macro_tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct va_macro_priv *va_priv;
	struct snd_soc_codec *codec;
	u16 dec_cfg_reg, hpf_gate_reg;
	u8 hpf_cut_off_freq;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	va_priv = hpf_work->va_priv;
	codec = va_priv->codec;
	hpf_cut_off_freq = hpf_work->hpf_cut_off_freq;

	dec_cfg_reg = BOLERO_CDC_VA_TX0_TX_PATH_CFG0 +
			VA_MACRO_TX_PATH_OFFSET * hpf_work->decimator;
	hpf_gate_reg = BOLERO_CDC_VA_TX0_TX_PATH_SEC2 +
			VA_MACRO_TX_PATH_OFFSET * hpf_work->decimator;

	dev_dbg(va_priv->dev, "%s: decimator %u hpf_cut_of_freq 0x%x\n",
		__func__, hpf_work->decimator, hpf_cut_off_freq);

	snd_soc_update_bits(codec, dec_cfg_reg, TX_HPF_CUT_OFF_FREQ_MASK,
			    hpf_cut_off_freq << 5);
	snd_soc_update_bits(codec, hpf_gate_reg, 0x03, 0x02);
	/* Minimum 1 clk cycle delay is required as per HW spec */
	usleep_range(1000, 1010);
	snd_soc_update_bits(codec, hpf_gate_reg, 0x03, 0x01);
}

static void va_macro_mute_update_callback(struct work_struct *work)
{
	struct va_mute_work *va_mute_dwork;
	struct snd_soc_codec *codec = NULL;
	struct va_macro_priv *va_priv;
	struct delayed_work *delayed_work;
	u16 tx_vol_ctl_reg, decimator;

	delayed_work = to_delayed_work(work);
	va_mute_dwork = container_of(delayed_work, struct va_mute_work, dwork);
	va_priv = va_mute_dwork->va_priv;
	codec = va_priv->codec;
	decimator = va_mute_dwork->decimator;

	tx_vol_ctl_reg =
		BOLERO_CDC_VA_TX0_TX_PATH_CTL +
			VA_MACRO_TX_PATH_OFFSET * decimator;
	snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x00);
	dev_dbg(va_priv->dev, "%s: decimator %u unmute\n",
		__func__, decimator);
}

static int va_macro_put_dec_enum(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	u16 mic_sel_reg;

	val = ucontrol->value.enumerated.item[0];
	if (val > e->items - 1)
		return -EINVAL;

	dev_dbg(codec->dev, "%s: wname: %s, val: 0x%x\n", __func__,
		widget->name, val);

	switch (e->reg) {
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX0_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX1_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX1_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX2_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX2_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX3_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX3_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX4_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX4_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX5_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX5_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX6_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX6_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX7_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX7_TX_PATH_CFG0;
		break;
	default:
		dev_err(codec->dev, "%s: e->reg: 0x%x not expected\n",
			__func__, e->reg);
		return -EINVAL;
	}
	/* DMIC selected */
	if (val != 0)
		snd_soc_update_bits(codec, mic_sel_reg, 1 << 7, 1 << 7);

	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static int va_macro_tx_mixer_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 dec_id = mixer->shift;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(codec, &va_dev, &va_priv, __func__))
		return -EINVAL;

	if (test_bit(dec_id, &va_priv->active_ch_mask[dai_id]))
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int va_macro_tx_mixer_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct snd_soc_dapm_update *update = NULL;
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 dec_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(codec, &va_dev, &va_priv, __func__))
		return -EINVAL;

	if (enable) {
		set_bit(dec_id, &va_priv->active_ch_mask[dai_id]);
		va_priv->active_ch_cnt[dai_id]++;
	} else {
		clear_bit(dec_id, &va_priv->active_ch_mask[dai_id]);
		va_priv->active_ch_cnt[dai_id]--;
	}

	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, update);

	return 0;
}

static int va_macro_enable_dmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u8  dmic_clk_en = 0x01;
	u16 dmic_clk_reg;
	s32 *dmic_clk_cnt;
	unsigned int dmic;
	int ret;
	char *wname;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(codec, &va_dev, &va_priv, __func__))
		return -EINVAL;

	wname = strpbrk(w->name, "01234567");
	if (!wname) {
		dev_err(va_dev, "%s: widget not found\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(wname, 10, &dmic);
	if (ret < 0) {
		dev_err(va_dev, "%s: Invalid DMIC line on the codec\n",
			__func__);
		return -EINVAL;
	}

	switch (dmic) {
	case 0:
	case 1:
		dmic_clk_cnt = &(va_priv->dmic_0_1_clk_cnt);
		dmic_clk_reg = BOLERO_CDC_VA_TOP_CSR_DMIC0_CTL;
		break;
	case 2:
	case 3:
		dmic_clk_cnt = &(va_priv->dmic_2_3_clk_cnt);
		dmic_clk_reg = BOLERO_CDC_VA_TOP_CSR_DMIC1_CTL;
		break;
	case 4:
	case 5:
		dmic_clk_cnt = &(va_priv->dmic_4_5_clk_cnt);
		dmic_clk_reg = BOLERO_CDC_VA_TOP_CSR_DMIC2_CTL;
		break;
	case 6:
	case 7:
		dmic_clk_cnt = &(va_priv->dmic_6_7_clk_cnt);
		dmic_clk_reg = BOLERO_CDC_VA_TOP_CSR_DMIC3_CTL;
		break;
	default:
		dev_err(va_dev, "%s: Invalid DMIC Selection\n",
			__func__);
		return -EINVAL;
	}
	dev_dbg(va_dev, "%s: event %d DMIC%d dmic_clk_cnt %d\n",
		__func__, event,  dmic, *dmic_clk_cnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		(*dmic_clk_cnt)++;
		if (*dmic_clk_cnt == 1) {
			snd_soc_update_bits(codec,
					BOLERO_CDC_VA_TOP_CSR_DMIC_CFG,
					0x80, 0x00);
			snd_soc_update_bits(codec, dmic_clk_reg,
					VA_MACRO_TX_DMIC_CLK_DIV_MASK,
					va_priv->dmic_clk_div <<
					VA_MACRO_TX_DMIC_CLK_DIV_SHFT);
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, dmic_clk_en);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		(*dmic_clk_cnt)--;
		if (*dmic_clk_cnt  == 0) {
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, 0);
		}
		break;
	}

	return 0;
}

static int va_macro_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	unsigned int decimator;
	u16 tx_vol_ctl_reg, dec_cfg_reg, hpf_gate_reg;
	u16 tx_gain_ctl_reg;
	u8 hpf_cut_off_freq;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(codec, &va_dev, &va_priv, __func__))
		return -EINVAL;

	decimator = w->shift;

	dev_dbg(va_dev, "%s(): widget = %s decimator = %u\n", __func__,
		w->name, decimator);

	tx_vol_ctl_reg = BOLERO_CDC_VA_TX0_TX_PATH_CTL +
				VA_MACRO_TX_PATH_OFFSET * decimator;
	hpf_gate_reg = BOLERO_CDC_VA_TX0_TX_PATH_SEC2 +
				VA_MACRO_TX_PATH_OFFSET * decimator;
	dec_cfg_reg = BOLERO_CDC_VA_TX0_TX_PATH_CFG0 +
				VA_MACRO_TX_PATH_OFFSET * decimator;
	tx_gain_ctl_reg = BOLERO_CDC_VA_TX0_TX_VOL_CTL +
				VA_MACRO_TX_PATH_OFFSET * decimator;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enable TX PGA Mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Enable TX CLK */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x20, 0x20);
		snd_soc_update_bits(codec, hpf_gate_reg, 0x01, 0x00);

		hpf_cut_off_freq = (snd_soc_read(codec, dec_cfg_reg) &
				   TX_HPF_CUT_OFF_FREQ_MASK) >> 5;
		va_priv->va_hpf_work[decimator].hpf_cut_off_freq =
							hpf_cut_off_freq;

		if (hpf_cut_off_freq != CF_MIN_3DB_150HZ) {
			snd_soc_update_bits(codec, dec_cfg_reg,
					    TX_HPF_CUT_OFF_FREQ_MASK,
					    CF_MIN_3DB_150HZ << 5);
			snd_soc_update_bits(codec, hpf_gate_reg, 0x02, 0x02);
			/*
			 * Minimum 1 clk cycle delay is required as per HW spec
			 */
			usleep_range(1000, 1010);
			snd_soc_update_bits(codec, hpf_gate_reg, 0x02, 0x00);
		}
		/* schedule work queue to Remove Mute */
		schedule_delayed_work(&va_priv->va_mute_dwork[decimator].dwork,
				      msecs_to_jiffies(va_tx_unmute_delay));
		if (va_priv->va_hpf_work[decimator].hpf_cut_off_freq !=
							CF_MIN_3DB_150HZ)
			schedule_delayed_work(
					&va_priv->va_hpf_work[decimator].dwork,
					msecs_to_jiffies(300));
		/* apply gain after decimator is enabled */
		snd_soc_write(codec, tx_gain_ctl_reg,
			      snd_soc_read(codec, tx_gain_ctl_reg));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		hpf_cut_off_freq =
			va_priv->va_hpf_work[decimator].hpf_cut_off_freq;
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x10);
		if (cancel_delayed_work_sync(
		    &va_priv->va_hpf_work[decimator].dwork)) {
			if (hpf_cut_off_freq != CF_MIN_3DB_150HZ) {
				snd_soc_update_bits(codec, dec_cfg_reg,
						    TX_HPF_CUT_OFF_FREQ_MASK,
						    hpf_cut_off_freq << 5);
				snd_soc_update_bits(codec, hpf_gate_reg,
						    0x02, 0x02);
				/*
				 * Minimum 1 clk cycle delay is required
				 * as per HW spec
				 */
				usleep_range(1000, 1010);
				snd_soc_update_bits(codec, hpf_gate_reg,
						    0x02, 0x00);
			}
		}
		cancel_delayed_work_sync(
				&va_priv->va_mute_dwork[decimator].dwork);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Disable TX CLK */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x20, 0x00);
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x10, 0x00);
		break;
	}
	return 0;
}

static int va_macro_enable_micbias(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;
	int ret = 0;

	if (!va_macro_get_data(codec, &va_dev, &va_priv, __func__))
		return -EINVAL;

	if (!va_priv->micb_supply) {
		dev_err(va_dev,
			"%s:regulator not provided in dtsi\n", __func__);
		return -EINVAL;
	}
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (va_priv->micb_users++ > 0)
			return 0;
		ret = regulator_set_voltage(va_priv->micb_supply,
				      va_priv->micb_voltage,
				      va_priv->micb_voltage);
		if (ret) {
			dev_err(va_dev, "%s: Setting voltage failed, err = %d\n",
				__func__, ret);
			return ret;
		}
		ret = regulator_set_load(va_priv->micb_supply,
					 va_priv->micb_current);
		if (ret) {
			dev_err(va_dev, "%s: Setting current failed, err = %d\n",
				__func__, ret);
			return ret;
		}
		ret = regulator_enable(va_priv->micb_supply);
		if (ret) {
			dev_err(va_dev, "%s: regulator enable failed, err = %d\n",
				__func__, ret);
			return ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (--va_priv->micb_users > 0)
			return 0;
		if (va_priv->micb_users < 0) {
			va_priv->micb_users = 0;
			dev_dbg(va_dev, "%s: regulator already disabled\n",
				__func__);
			return 0;
		}
		ret = regulator_disable(va_priv->micb_supply);
		if (ret) {
			dev_err(va_dev, "%s: regulator disable failed, err = %d\n",
				__func__, ret);
			return ret;
		}
		regulator_set_voltage(va_priv->micb_supply, 0,
				va_priv->micb_voltage);
		regulator_set_load(va_priv->micb_supply, 0);
		break;
	}
	return 0;
}

static int va_macro_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	int tx_fs_rate = -EINVAL;
	struct snd_soc_codec *codec = dai->codec;
	u32 decimator, sample_rate;
	u16 tx_fs_reg = 0;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(codec, &va_dev, &va_priv, __func__))
		return -EINVAL;

	dev_dbg(va_dev,
		"%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
		dai->name, dai->id, params_rate(params),
		params_channels(params));

	sample_rate = params_rate(params);
	switch (sample_rate) {
	case 8000:
		tx_fs_rate = 0;
		break;
	case 16000:
		tx_fs_rate = 1;
		break;
	case 32000:
		tx_fs_rate = 3;
		break;
	case 48000:
		tx_fs_rate = 4;
		break;
	case 96000:
		tx_fs_rate = 5;
		break;
	case 192000:
		tx_fs_rate = 6;
		break;
	case 384000:
		tx_fs_rate = 7;
		break;
	default:
		dev_err(va_dev, "%s: Invalid TX sample rate: %d\n",
			__func__, params_rate(params));
		return -EINVAL;
	}
	for_each_set_bit(decimator, &va_priv->active_ch_mask[dai->id],
			 VA_MACRO_DEC_MAX) {
		if (decimator >= 0) {
			tx_fs_reg = BOLERO_CDC_VA_TX0_TX_PATH_CTL +
				    VA_MACRO_TX_PATH_OFFSET * decimator;
			dev_dbg(va_dev, "%s: set DEC%u rate to %u\n",
				__func__, decimator, sample_rate);
			snd_soc_update_bits(codec, tx_fs_reg, 0x0F,
					    tx_fs_rate);
		} else {
			dev_err(va_dev,
				"%s: ERROR: Invalid decimator: %d\n",
				__func__, decimator);
			return -EINVAL;
		}
	}
	return 0;
}

static int va_macro_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(codec, &va_dev, &va_priv, __func__))
		return -EINVAL;

	switch (dai->id) {
	case VA_MACRO_AIF1_CAP:
	case VA_MACRO_AIF2_CAP:
		*tx_slot = va_priv->active_ch_mask[dai->id];
		*tx_num = va_priv->active_ch_cnt[dai->id];
		break;
	default:
		dev_err(va_dev, "%s: Invalid AIF\n", __func__);
		break;
	}
	return 0;
}

static struct snd_soc_dai_ops va_macro_dai_ops = {
	.hw_params = va_macro_hw_params,
	.get_channel_map = va_macro_get_channel_map,
};

static struct snd_soc_dai_driver va_macro_dai[] = {
	{
		.name = "va_macro_tx1",
		.id = VA_MACRO_AIF1_CAP,
		.capture = {
			.stream_name = "VA_AIF1 Capture",
			.rates = VA_MACRO_RATES,
			.formats = VA_MACRO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &va_macro_dai_ops,
	},
	{
		.name = "va_macro_tx2",
		.id = VA_MACRO_AIF2_CAP,
		.capture = {
			.stream_name = "VA_AIF2 Capture",
			.rates = VA_MACRO_RATES,
			.formats = VA_MACRO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &va_macro_dai_ops,
	},
};

#define STRING(name) #name
#define VA_MACRO_DAPM_ENUM(name, reg, offset, text) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM(STRING(name), name##_enum)

#define VA_MACRO_DAPM_ENUM_EXT(name, reg, offset, text, getname, putname) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM_EXT(STRING(name), name##_enum, getname, putname)

#define VA_MACRO_DAPM_MUX(name, shift, kctl) \
		SND_SOC_DAPM_MUX(name, SND_SOC_NOPM, shift, 0, &kctl##_mux)

static const char * const adc_mux_text[] = {
	"MSM_DMIC", "SWR_MIC"
};

VA_MACRO_DAPM_ENUM(va_dec0, BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec1, BOLERO_CDC_VA_INP_MUX_ADC_MUX1_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec2, BOLERO_CDC_VA_INP_MUX_ADC_MUX2_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec3, BOLERO_CDC_VA_INP_MUX_ADC_MUX3_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec4, BOLERO_CDC_VA_INP_MUX_ADC_MUX4_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec5, BOLERO_CDC_VA_INP_MUX_ADC_MUX5_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec6, BOLERO_CDC_VA_INP_MUX_ADC_MUX6_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec7, BOLERO_CDC_VA_INP_MUX_ADC_MUX7_CFG1,
		   0, adc_mux_text);

static const char * const dmic_mux_text[] = {
	"ZERO", "DMIC0", "DMIC1", "DMIC2", "DMIC3",
	"DMIC4", "DMIC5", "DMIC6", "DMIC7"
};

VA_MACRO_DAPM_ENUM_EXT(va_dmic0, BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic1, BOLERO_CDC_VA_INP_MUX_ADC_MUX1_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic2, BOLERO_CDC_VA_INP_MUX_ADC_MUX2_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic3, BOLERO_CDC_VA_INP_MUX_ADC_MUX3_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic4, BOLERO_CDC_VA_INP_MUX_ADC_MUX4_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic5, BOLERO_CDC_VA_INP_MUX_ADC_MUX5_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic6, BOLERO_CDC_VA_INP_MUX_ADC_MUX6_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic7, BOLERO_CDC_VA_INP_MUX_ADC_MUX7_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

static const char * const smic_mux_text[] = {
	"ZERO", "ADC0", "ADC1", "ADC2", "ADC3",
	"SWR_DMIC0", "SWR_DMIC1", "SWR_DMIC2", "SWR_DMIC3",
	"SWR_DMIC4", "SWR_DMIC5", "SWR_DMIC6", "SWR_DMIC7"
};

VA_MACRO_DAPM_ENUM_EXT(va_smic0, BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic1, BOLERO_CDC_VA_INP_MUX_ADC_MUX1_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic2, BOLERO_CDC_VA_INP_MUX_ADC_MUX2_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic3, BOLERO_CDC_VA_INP_MUX_ADC_MUX3_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic4, BOLERO_CDC_VA_INP_MUX_ADC_MUX4_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic5, BOLERO_CDC_VA_INP_MUX_ADC_MUX5_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic6, BOLERO_CDC_VA_INP_MUX_ADC_MUX6_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic7, BOLERO_CDC_VA_INP_MUX_ADC_MUX7_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

static const struct snd_kcontrol_new va_aif1_cap_mixer[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, VA_MACRO_DEC2, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, VA_MACRO_DEC3, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC4", SND_SOC_NOPM, VA_MACRO_DEC4, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC5", SND_SOC_NOPM, VA_MACRO_DEC5, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC6", SND_SOC_NOPM, VA_MACRO_DEC6, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC7", SND_SOC_NOPM, VA_MACRO_DEC7, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new va_aif2_cap_mixer[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, VA_MACRO_DEC2, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, VA_MACRO_DEC3, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC4", SND_SOC_NOPM, VA_MACRO_DEC4, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC5", SND_SOC_NOPM, VA_MACRO_DEC5, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC6", SND_SOC_NOPM, VA_MACRO_DEC6, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC7", SND_SOC_NOPM, VA_MACRO_DEC7, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_soc_dapm_widget va_macro_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_OUT("VA_AIF1 CAP", "VA_AIF1 Capture", 0,
		SND_SOC_NOPM, VA_MACRO_AIF1_CAP, 0),

	SND_SOC_DAPM_AIF_OUT("VA_AIF2 CAP", "VA_AIF2 Capture", 0,
		SND_SOC_NOPM, VA_MACRO_AIF2_CAP, 0),

	SND_SOC_DAPM_MIXER("VA_AIF1_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF1_CAP, 0,
		va_aif1_cap_mixer, ARRAY_SIZE(va_aif1_cap_mixer)),

	SND_SOC_DAPM_MIXER("VA_AIF2_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF2_CAP, 0,
		va_aif2_cap_mixer, ARRAY_SIZE(va_aif2_cap_mixer)),


	VA_MACRO_DAPM_MUX("VA DMIC MUX0", 0, va_dmic0),
	VA_MACRO_DAPM_MUX("VA DMIC MUX1", 0, va_dmic1),
	VA_MACRO_DAPM_MUX("VA DMIC MUX2", 0, va_dmic2),
	VA_MACRO_DAPM_MUX("VA DMIC MUX3", 0, va_dmic3),
	VA_MACRO_DAPM_MUX("VA DMIC MUX4", 0, va_dmic4),
	VA_MACRO_DAPM_MUX("VA DMIC MUX5", 0, va_dmic5),
	VA_MACRO_DAPM_MUX("VA DMIC MUX6", 0, va_dmic6),
	VA_MACRO_DAPM_MUX("VA DMIC MUX7", 0, va_dmic7),

	VA_MACRO_DAPM_MUX("VA SMIC MUX0", 0, va_smic0),
	VA_MACRO_DAPM_MUX("VA SMIC MUX1", 0, va_smic1),
	VA_MACRO_DAPM_MUX("VA SMIC MUX2", 0, va_smic2),
	VA_MACRO_DAPM_MUX("VA SMIC MUX3", 0, va_smic3),
	VA_MACRO_DAPM_MUX("VA SMIC MUX4", 0, va_smic4),
	VA_MACRO_DAPM_MUX("VA SMIC MUX5", 0, va_smic5),
	VA_MACRO_DAPM_MUX("VA SMIC MUX6", 0, va_smic6),
	VA_MACRO_DAPM_MUX("VA SMIC MUX7", 0, va_smic7),

	SND_SOC_DAPM_MICBIAS_E("VA MIC BIAS1", SND_SOC_NOPM, 0, 0,
			       va_macro_enable_micbias,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC0", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC5", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC6", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC7", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("VA SWR_ADC0"),
	SND_SOC_DAPM_INPUT("VA SWR_ADC1"),
	SND_SOC_DAPM_INPUT("VA SWR_ADC2"),
	SND_SOC_DAPM_INPUT("VA SWR_ADC3"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC0"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC1"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC2"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC3"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC4"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC5"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC6"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC7"),

	SND_SOC_DAPM_MUX_E("VA DEC0 MUX", SND_SOC_NOPM, VA_MACRO_DEC0, 0,
			   &va_dec0_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC1 MUX", SND_SOC_NOPM, VA_MACRO_DEC1, 0,
			   &va_dec1_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC2 MUX", SND_SOC_NOPM, VA_MACRO_DEC2, 0,
			   &va_dec2_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC3 MUX", SND_SOC_NOPM, VA_MACRO_DEC3, 0,
			   &va_dec3_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC4 MUX", SND_SOC_NOPM, VA_MACRO_DEC4, 0,
			   &va_dec4_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC5 MUX", SND_SOC_NOPM, VA_MACRO_DEC5, 0,
			   &va_dec5_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC6 MUX", SND_SOC_NOPM, VA_MACRO_DEC6, 0,
			   &va_dec6_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC7 MUX", SND_SOC_NOPM, VA_MACRO_DEC7, 0,
			   &va_dec7_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("VA_MCLK", -1, SND_SOC_NOPM, 0, 0,
			      va_macro_mclk_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_widget va_macro_wod_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("VA_MCLK", -1, SND_SOC_NOPM, 0, 0,
			      va_macro_mclk_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route va_audio_map[] = {
	{"VA_AIF1 CAP", NULL, "VA_MCLK"},
	{"VA_AIF2 CAP", NULL, "VA_MCLK"},

	{"VA_AIF1 CAP", NULL, "VA_AIF1_CAP Mixer"},
	{"VA_AIF2 CAP", NULL, "VA_AIF2_CAP Mixer"},

	{"VA_AIF1_CAP Mixer", "DEC0", "VA DEC0 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC1", "VA DEC1 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC2", "VA DEC2 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC3", "VA DEC3 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC4", "VA DEC4 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC5", "VA DEC5 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC6", "VA DEC6 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC7", "VA DEC7 MUX"},

	{"VA_AIF2_CAP Mixer", "DEC0", "VA DEC0 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC1", "VA DEC1 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC2", "VA DEC2 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC3", "VA DEC3 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC4", "VA DEC4 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC5", "VA DEC5 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC6", "VA DEC6 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC7", "VA DEC7 MUX"},

	{"VA DEC0 MUX", "MSM_DMIC", "VA DMIC MUX0"},
	{"VA DMIC MUX0", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX0", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX0", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX0", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX0", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX0", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX0", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX0", "DMIC7", "VA DMIC7"},

	{"VA DEC0 MUX", "SWR_MIC", "VA SMIC MUX0"},
	{"VA SMIC MUX0", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX0", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX0", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX0", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX0", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX0", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX0", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX0", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX0", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX0", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX0", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX0", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC1 MUX", "MSM_DMIC", "VA DMIC MUX1"},
	{"VA DMIC MUX1", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX1", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX1", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX1", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX1", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX1", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX1", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX1", "DMIC7", "VA DMIC7"},

	{"VA DEC1 MUX", "SWR_MIC", "VA SMIC MUX1"},
	{"VA SMIC MUX1", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX1", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX1", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX1", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX1", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX1", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX1", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX1", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX1", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX1", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX1", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX1", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC2 MUX", "MSM_DMIC", "VA DMIC MUX2"},
	{"VA DMIC MUX2", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX2", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX2", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX2", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX2", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX2", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX2", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX2", "DMIC7", "VA DMIC7"},

	{"VA DEC2 MUX", "SWR_MIC", "VA SMIC MUX2"},
	{"VA SMIC MUX2", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX2", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX2", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX2", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX2", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX2", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX2", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX2", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX2", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX2", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX2", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX2", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC3 MUX", "MSM_DMIC", "VA DMIC MUX3"},
	{"VA DMIC MUX3", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX3", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX3", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX3", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX3", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX3", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX3", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX3", "DMIC7", "VA DMIC7"},

	{"VA DEC3 MUX", "SWR_MIC", "VA SMIC MUX3"},
	{"VA SMIC MUX3", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX3", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX3", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX3", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX3", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX3", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX3", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX3", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX3", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX3", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX3", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX3", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC4 MUX", "MSM_DMIC", "VA DMIC MUX4"},
	{"VA DMIC MUX4", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX4", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX4", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX4", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX4", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX4", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX4", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX4", "DMIC7", "VA DMIC7"},

	{"VA DEC4 MUX", "SWR_MIC", "VA SMIC MUX4"},
	{"VA SMIC MUX4", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX4", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX4", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX4", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX4", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX4", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX4", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX4", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX4", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX4", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX4", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX4", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC5 MUX", "MSM_DMIC", "VA DMIC MUX5"},
	{"VA DMIC MUX5", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX5", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX5", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX5", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX5", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX5", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX5", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX5", "DMIC7", "VA DMIC7"},

	{"VA DEC5 MUX", "SWR_MIC", "VA SMIC MUX5"},
	{"VA SMIC MUX5", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX5", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX5", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX5", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX5", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX5", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX5", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX5", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX5", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX5", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX5", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX5", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC6 MUX", "MSM_DMIC", "VA DMIC MUX6"},
	{"VA DMIC MUX6", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX6", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX6", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX6", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX6", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX6", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX6", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX6", "DMIC7", "VA DMIC7"},

	{"VA DEC6 MUX", "SWR_MIC", "VA SMIC MUX6"},
	{"VA SMIC MUX6", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX6", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX6", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX6", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX6", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX6", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX6", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX6", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX6", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX6", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX6", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX6", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC7 MUX", "MSM_DMIC", "VA DMIC MUX7"},
	{"VA DMIC MUX7", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX7", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX7", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX7", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX7", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX7", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX7", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX7", "DMIC7", "VA DMIC7"},

	{"VA DEC7 MUX", "SWR_MIC", "VA SMIC MUX7"},
	{"VA SMIC MUX7", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX7", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX7", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX7", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX7", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX7", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX7", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX7", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX7", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX7", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX7", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX7", "SWR_DMIC7", "VA SWR_MIC7"},
};

static const struct snd_kcontrol_new va_macro_snd_controls[] = {
	SOC_SINGLE_SX_TLV("VA_DEC0 Volume",
			  BOLERO_CDC_VA_TX0_TX_VOL_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("VA_DEC1 Volume",
			  BOLERO_CDC_VA_TX1_TX_VOL_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("VA_DEC2 Volume",
			  BOLERO_CDC_VA_TX2_TX_VOL_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("VA_DEC3 Volume",
			  BOLERO_CDC_VA_TX3_TX_VOL_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("VA_DEC4 Volume",
			  BOLERO_CDC_VA_TX4_TX_VOL_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("VA_DEC5 Volume",
			  BOLERO_CDC_VA_TX5_TX_VOL_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("VA_DEC6 Volume",
			  BOLERO_CDC_VA_TX6_TX_VOL_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("VA_DEC7 Volume",
			  BOLERO_CDC_VA_TX7_TX_VOL_CTL,
			  0, -84, 40, digital_gain),
};

static int va_macro_validate_dmic_sample_rate(u32 dmic_sample_rate,
				      struct va_macro_priv *va_priv)
{
	u32 div_factor;
	u32 mclk_rate = VA_MACRO_MCLK_FREQ;

	if (dmic_sample_rate == VA_MACRO_DMIC_SAMPLE_RATE_UNDEFINED ||
	    mclk_rate % dmic_sample_rate != 0)
		goto undefined_rate;

	div_factor = mclk_rate / dmic_sample_rate;

	switch (div_factor) {
	case 2:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_2;
		break;
	case 3:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_3;
		break;
	case 4:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_4;
		break;
	case 6:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_6;
		break;
	case 8:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_8;
		break;
	case 16:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_16;
		break;
	default:
		/* Any other DIV factor is invalid */
		goto undefined_rate;
	}

	/* Valid dmic DIV factors */
	dev_dbg(va_priv->dev, "%s: DMIC_DIV = %u, mclk_rate = %u\n",
		__func__, div_factor, mclk_rate);

	return dmic_sample_rate;

undefined_rate:
	dev_dbg(va_priv->dev, "%s: Invalid rate %d, for mclk %d\n",
		 __func__, dmic_sample_rate, mclk_rate);
	dmic_sample_rate = VA_MACRO_DMIC_SAMPLE_RATE_UNDEFINED;

	return dmic_sample_rate;
}

static int va_macro_init(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int ret, i;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	va_dev = bolero_get_device_ptr(codec->dev, VA_MACRO);
	if (!va_dev) {
		dev_err(codec->dev,
			"%s: null device for macro!\n", __func__);
		return -EINVAL;
	}
	va_priv = dev_get_drvdata(va_dev);
	if (!va_priv) {
		dev_err(codec->dev,
			"%s: priv is null for macro!\n", __func__);
		return -EINVAL;
	}

	if (va_priv->va_without_decimation) {
		ret = snd_soc_dapm_new_controls(dapm, va_macro_wod_dapm_widgets,
					ARRAY_SIZE(va_macro_wod_dapm_widgets));
		if (ret < 0) {
			dev_err(va_dev,
				"%s: Failed to add without dec controls\n",
				__func__);
			return ret;
		}
		va_priv->codec = codec;
		return 0;
	}

	ret = snd_soc_dapm_new_controls(dapm, va_macro_dapm_widgets,
					ARRAY_SIZE(va_macro_dapm_widgets));
	if (ret < 0) {
		dev_err(va_dev, "%s: Failed to add controls\n", __func__);
		return ret;
	}

	ret = snd_soc_dapm_add_routes(dapm, va_audio_map,
					ARRAY_SIZE(va_audio_map));
	if (ret < 0) {
		dev_err(va_dev, "%s: Failed to add routes\n", __func__);
		return ret;
	}

	ret = snd_soc_dapm_new_widgets(dapm->card);
	if (ret < 0) {
		dev_err(va_dev, "%s: Failed to add widgets\n", __func__);
		return ret;
	}
	ret = snd_soc_add_codec_controls(codec, va_macro_snd_controls,
				   ARRAY_SIZE(va_macro_snd_controls));
	if (ret < 0) {
		dev_err(va_dev, "%s: Failed to add snd_ctls\n", __func__);
		return ret;
	}

	snd_soc_dapm_ignore_suspend(dapm, "VA_AIF1 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "VA_AIF2 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_ADC0");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_ADC1");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_ADC2");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_ADC3");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC0");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC1");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC2");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC3");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC4");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC5");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC6");
	snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC7");
	snd_soc_dapm_sync(dapm);

	for (i = 0; i < VA_MACRO_NUM_DECIMATORS; i++) {
		va_priv->va_hpf_work[i].va_priv = va_priv;
		va_priv->va_hpf_work[i].decimator = i;
		INIT_DELAYED_WORK(&va_priv->va_hpf_work[i].dwork,
			va_macro_tx_hpf_corner_freq_callback);
	}

	for (i = 0; i < VA_MACRO_NUM_DECIMATORS; i++) {
		va_priv->va_mute_dwork[i].va_priv = va_priv;
		va_priv->va_mute_dwork[i].decimator = i;
		INIT_DELAYED_WORK(&va_priv->va_mute_dwork[i].dwork,
			  va_macro_mute_update_callback);
	}
	va_priv->codec = codec;

	return 0;
}

static int va_macro_deinit(struct snd_soc_codec *codec)
{
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(codec, &va_dev, &va_priv, __func__))
		return -EINVAL;

	va_priv->codec = NULL;
	return 0;
}

static void va_macro_init_ops(struct macro_ops *ops,
			      char __iomem *va_io_base,
			      bool va_without_decimation)
{
	memset(ops, 0, sizeof(struct macro_ops));
	if (!va_without_decimation) {
		ops->dai_ptr = va_macro_dai;
		ops->num_dais = ARRAY_SIZE(va_macro_dai);
	} else {
		ops->dai_ptr = NULL;
		ops->num_dais = 0;
	}
	ops->init = va_macro_init;
	ops->exit = va_macro_deinit;
	ops->io_base = va_io_base;
	ops->mclk_fn = va_macro_mclk_ctrl;
	ops->event_handler = va_macro_event_handler;
}

static int va_macro_probe(struct platform_device *pdev)
{
	struct macro_ops ops;
	struct va_macro_priv *va_priv;
	u32 va_base_addr, sample_rate = 0;
	char __iomem *va_io_base;
	struct clk *va_core_clk;
	bool va_without_decimation = false;
	const char *micb_supply_str = "va-vdd-micb-supply";
	const char *micb_supply_str1 = "va-vdd-micb";
	const char *micb_voltage_str = "qcom,va-vdd-micb-voltage";
	const char *micb_current_str = "qcom,va-vdd-micb-current";
	int ret = 0;
	const char *dmic_sample_rate = "qcom,va-dmic-sample-rate";

	va_priv = devm_kzalloc(&pdev->dev, sizeof(struct va_macro_priv),
			    GFP_KERNEL);
	if (!va_priv)
		return -ENOMEM;

	va_priv->dev = &pdev->dev;
	ret = of_property_read_u32(pdev->dev.of_node, "reg",
				   &va_base_addr);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "reg");
		return ret;
	}
	va_without_decimation = of_property_read_bool(pdev->dev.parent->of_node,
					"qcom,va-without-decimation");

	va_priv->va_without_decimation = va_without_decimation;
	ret = of_property_read_u32(pdev->dev.of_node, dmic_sample_rate,
				   &sample_rate);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %d entry in dt\n",
			__func__, sample_rate);
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_2;
	} else {
		if (va_macro_validate_dmic_sample_rate(
		sample_rate, va_priv) == VA_MACRO_DMIC_SAMPLE_RATE_UNDEFINED)
			return -EINVAL;
	}

	va_io_base = devm_ioremap(&pdev->dev, va_base_addr,
				  VA_MAX_OFFSET);
	if (!va_io_base) {
		dev_err(&pdev->dev, "%s: ioremap failed\n", __func__);
		return -EINVAL;
	}
	va_priv->va_io_base = va_io_base;
	/* Register MCLK for va macro */
	va_core_clk = devm_clk_get(&pdev->dev, "va_core_clk");
	if (IS_ERR(va_core_clk)) {
		ret = PTR_ERR(va_core_clk);
		dev_err(&pdev->dev, "%s: clk get %s failed\n",
			__func__, "va_core_clk");
		return ret;
	}
	va_priv->va_core_clk = va_core_clk;

	if (of_parse_phandle(pdev->dev.of_node, micb_supply_str, 0)) {
		va_priv->micb_supply = devm_regulator_get(&pdev->dev,
						micb_supply_str1);
		if (IS_ERR(va_priv->micb_supply)) {
			ret = PTR_ERR(va_priv->micb_supply);
			dev_err(&pdev->dev,
				"%s:Failed to get micbias supply for VA Mic %d\n",
				__func__, ret);
			return ret;
		}
		ret = of_property_read_u32(pdev->dev.of_node,
					micb_voltage_str,
					&va_priv->micb_voltage);
		if (ret) {
			dev_err(&pdev->dev,
				"%s:Looking up %s property in node %s failed\n",
				__func__, micb_voltage_str,
				pdev->dev.of_node->full_name);
			return ret;
		}
		ret = of_property_read_u32(pdev->dev.of_node,
					micb_current_str,
					&va_priv->micb_current);
		if (ret) {
			dev_err(&pdev->dev,
				"%s:Looking up %s property in node %s failed\n",
				__func__, micb_current_str,
				pdev->dev.of_node->full_name);
			return ret;
		}
	}

	mutex_init(&va_priv->mclk_lock);
	dev_set_drvdata(&pdev->dev, va_priv);
	va_macro_init_ops(&ops, va_io_base, va_without_decimation);
	ret = bolero_register_macro(&pdev->dev, VA_MACRO, &ops);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: register macro failed\n", __func__);
		goto reg_macro_fail;
	}
	return ret;

reg_macro_fail:
	mutex_destroy(&va_priv->mclk_lock);
	return ret;
}

static int va_macro_remove(struct platform_device *pdev)
{
	struct va_macro_priv *va_priv;

	va_priv = dev_get_drvdata(&pdev->dev);

	if (!va_priv)
		return -EINVAL;

	bolero_unregister_macro(&pdev->dev, VA_MACRO);
	mutex_destroy(&va_priv->mclk_lock);
	return 0;
}


static const struct of_device_id va_macro_dt_match[] = {
	{.compatible = "qcom,va-macro"},
	{}
};

static struct platform_driver va_macro_driver = {
	.driver = {
		.name = "va_macro",
		.owner = THIS_MODULE,
		.of_match_table = va_macro_dt_match,
	},
	.probe = va_macro_probe,
	.remove = va_macro_remove,
};

module_platform_driver(va_macro_driver);

MODULE_DESCRIPTION("VA macro driver");
MODULE_LICENSE("GPL v2");
