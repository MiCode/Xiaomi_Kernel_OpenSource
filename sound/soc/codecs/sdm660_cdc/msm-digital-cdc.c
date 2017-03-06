/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/qdsp6v2/apr.h>
#include <linux/workqueue.h>
#include <linux/regmap.h>
#include <sound/q6afe-v2.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "sdm660-cdc-registers.h"
#include "msm-digital-cdc.h"
#include "msm-cdc-common.h"
#include "../../msm/sdm660-common.h"

#define DRV_NAME "msm_digital_codec"
#define MCLK_RATE_9P6MHZ        9600000
#define MCLK_RATE_12P288MHZ     12288000
#define TX_MUX_CTL_CUT_OFF_FREQ_MASK	0x30
#define CF_MIN_3DB_4HZ			0x0
#define CF_MIN_3DB_75HZ			0x1
#define CF_MIN_3DB_150HZ		0x2

#define MSM_DIG_CDC_VERSION_ENTRY_SIZE 32

static unsigned long rx_digital_gain_reg[] = {
	MSM89XX_CDC_CORE_RX1_VOL_CTL_B2_CTL,
	MSM89XX_CDC_CORE_RX2_VOL_CTL_B2_CTL,
	MSM89XX_CDC_CORE_RX3_VOL_CTL_B2_CTL,
};

static unsigned long tx_digital_gain_reg[] = {
	MSM89XX_CDC_CORE_TX1_VOL_CTL_GAIN,
	MSM89XX_CDC_CORE_TX2_VOL_CTL_GAIN,
	MSM89XX_CDC_CORE_TX3_VOL_CTL_GAIN,
	MSM89XX_CDC_CORE_TX4_VOL_CTL_GAIN,
	MSM89XX_CDC_CORE_TX5_VOL_CTL_GAIN,
};

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);

struct snd_soc_codec *registered_digcodec;
struct hpf_work tx_hpf_work[NUM_DECIMATORS];

/* Codec supports 2 IIR filters */
enum {
	IIR1 = 0,
	IIR2,
	IIR_MAX,
};

static int msm_digcdc_clock_control(bool flag)
{
	int ret = -EINVAL;
	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(registered_digcodec->component.card);

	mutex_lock(&pdata->cdc_int_mclk0_mutex);
	if (flag) {
		if (atomic_read(&pdata->int_mclk0_enabled) == false) {
			pdata->digital_cdc_core_clk.enable = 1;
			ret = afe_set_lpass_clock_v2(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_core_clk);
			if (ret < 0) {
				pr_err("%s:failed to enable the MCLK\n",
				       __func__);
				mutex_unlock(&pdata->cdc_int_mclk0_mutex);
				return ret;
			}
			pr_debug("enabled digital codec core clk\n");
			atomic_set(&pdata->int_mclk0_enabled, true);
			schedule_delayed_work(&pdata->disable_int_mclk0_work,
					      50);
		}
	} else {
		dev_dbg(registered_digcodec->dev,
			"disable MCLK, workq to disable set already\n");
	}
	mutex_unlock(&pdata->cdc_int_mclk0_mutex);
	return 0;
}

static void enable_digital_callback(void *flag)
{
	msm_digcdc_clock_control(true);
}

static void disable_digital_callback(void *flag)
{
	pr_debug("disable mclk happens in workq\n");
}

static int msm_dig_cdc_put_dec_enum(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
			dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *w = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int dec_mux, decimator;
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	u16 tx_mux_ctl_reg;
	u8 adc_dmic_sel = 0x0;
	int ret = 0;
	char *dec_num;

	if (ucontrol->value.enumerated.item[0] > e->items) {
		dev_err(codec->dev, "%s: Invalid enum value: %d\n",
			__func__, ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}
	dec_mux = ucontrol->value.enumerated.item[0];

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name) {
		dev_err(codec->dev, "%s: failed to copy string\n",
			__func__);
		return -ENOMEM;
	}
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		dev_err(codec->dev, "%s: Invalid decimator = %s\n",
			__func__, w->name);
		ret =  -EINVAL;
		goto out;
	}

	dec_num = strpbrk(dec_name, "12345");
	if (dec_num == NULL) {
		dev_err(codec->dev, "%s: Invalid DEC selected\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtouint(dec_num, 10, &decimator);
	if (ret < 0) {
		dev_err(codec->dev, "%s: Invalid decimator = %s\n",
			__func__, dec_name);
		ret =  -EINVAL;
		goto out;
	}

	dev_dbg(w->dapm->dev, "%s(): widget = %s decimator = %u dec_mux = %u\n"
		, __func__, w->name, decimator, dec_mux);

	switch (decimator) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if ((dec_mux == 4) || (dec_mux == 5) ||
		    (dec_mux == 6) || (dec_mux == 7))
			adc_dmic_sel = 0x1;
		else
			adc_dmic_sel = 0x0;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid Decimator = %u\n",
			__func__, decimator);
		ret = -EINVAL;
		goto out;
	}

	tx_mux_ctl_reg =
		MSM89XX_CDC_CORE_TX1_MUX_CTL + 32 * (decimator - 1);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, adc_dmic_sel);

	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

out:
	kfree(widget_name);
	return ret;
}


static int msm_dig_cdc_codec_config_compander(struct snd_soc_codec *codec,
					      int interp_n, int event)
{
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: event %d shift %d, enabled %d\n",
		__func__, event, interp_n,
		dig_cdc->comp_enabled[interp_n]);

	/* compander is not enabled */
	if (!dig_cdc->comp_enabled[interp_n])
		return 0;

	switch (dig_cdc->comp_enabled[interp_n]) {
	case COMPANDER_1:
		if (SND_SOC_DAPM_EVENT_ON(event)) {
			/* Enable Compander Clock */
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B2_CTL, 0x0F, 0x09);
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_CLK_RX_B2_CTL, 0x01, 0x01);
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B1_CTL,
				1 << interp_n, 1 << interp_n);
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B3_CTL, 0xFF, 0x01);
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B2_CTL, 0xF0, 0x50);
			/* add sleep for compander to settle */
			usleep_range(1000, 1100);
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B3_CTL, 0xFF, 0x28);
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B2_CTL, 0xF0, 0xB0);

			/* Enable Compander GPIO */
			if (dig_cdc->codec_hph_comp_gpio)
				dig_cdc->codec_hph_comp_gpio(1, codec);
		} else if (SND_SOC_DAPM_EVENT_OFF(event)) {
			/* Disable Compander GPIO */
			if (dig_cdc->codec_hph_comp_gpio)
				dig_cdc->codec_hph_comp_gpio(0, codec);

			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B2_CTL, 0x0F, 0x05);
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B1_CTL,
				1 << interp_n, 0);
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_CLK_RX_B2_CTL, 0x01, 0x00);
		}
		break;
	default:
		dev_dbg(codec->dev, "%s: Invalid compander %d\n", __func__,
				dig_cdc->comp_enabled[interp_n]);
		break;
	};

	return 0;
}

/**
 * msm_dig_cdc_hph_comp_cb - registers callback to codec by machine driver.
 *
 * @codec_hph_comp_gpio: function pointer to set comp gpio at machine driver
 * @codec: codec pointer
 *
 */
void msm_dig_cdc_hph_comp_cb(
	int (*codec_hph_comp_gpio)(bool enable, struct snd_soc_codec *codec),
	struct snd_soc_codec *codec)
{
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: Enter\n", __func__);
	dig_cdc->codec_hph_comp_gpio = codec_hph_comp_gpio;
}
EXPORT_SYMBOL(msm_dig_cdc_hph_comp_cb);

static int msm_dig_cdc_codec_enable_interpolator(struct snd_soc_dapm_widget *w,
						 struct snd_kcontrol *kcontrol,
						 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct msm_dig *msm_dig_cdc = dev_get_drvdata(codec->dev);

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	if (w->shift >= MSM89XX_RX_MAX || w->shift < 0) {
		dev_err(codec->dev, "%s: wrong RX index: %d\n",
			__func__, w->shift);
		return -EINVAL;
	}
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msm_dig_cdc_codec_config_compander(codec, w->shift, event);
		/* apply the digital gain after the interpolator is enabled*/
		if ((w->shift) < ARRAY_SIZE(rx_digital_gain_reg))
			snd_soc_write(codec,
				  rx_digital_gain_reg[w->shift],
				  snd_soc_read(codec,
				  rx_digital_gain_reg[w->shift])
				  );
		break;
	case SND_SOC_DAPM_POST_PMD:
		msm_dig_cdc_codec_config_compander(codec, w->shift, event);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_RX_RESET_CTL,
			1 << w->shift, 1 << w->shift);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_RX_RESET_CTL,
			1 << w->shift, 0x0);
		/*
		 * disable the mute enabled during the PMD of this device
		 */
		if ((w->shift == 0) &&
			(msm_dig_cdc->mute_mask & HPHL_PA_DISABLE)) {
			pr_debug("disabling HPHL mute\n");
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B6_CTL, 0x01, 0x00);
			msm_dig_cdc->mute_mask &= ~(HPHL_PA_DISABLE);
		} else if ((w->shift == 1) &&
				(msm_dig_cdc->mute_mask & HPHR_PA_DISABLE)) {
			pr_debug("disabling HPHR mute\n");
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B6_CTL, 0x01, 0x00);
			msm_dig_cdc->mute_mask &= ~(HPHR_PA_DISABLE);
		} else if ((w->shift == 2) &&
				(msm_dig_cdc->mute_mask & SPKR_PA_DISABLE)) {
			pr_debug("disabling SPKR mute\n");
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX3_B6_CTL, 0x01, 0x00);
			msm_dig_cdc->mute_mask &= ~(SPKR_PA_DISABLE);
		}
	}
	return 0;
}

static int msm_dig_cdc_get_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		(snd_soc_read(codec,
			    (MSM89XX_CDC_CORE_IIR1_CTL + 64 * iir_idx)) &
		(1 << band_idx)) != 0;

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_dig_cdc_put_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	/* Mask first 5 bits, 6-8 are reserved */
	snd_soc_update_bits(codec,
		(MSM89XX_CDC_CORE_IIR1_CTL + 64 * iir_idx),
			    (1 << band_idx), (value << band_idx));

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
	  iir_idx, band_idx,
		((snd_soc_read(codec,
		(MSM89XX_CDC_CORE_IIR1_CTL + 64 * iir_idx)) &
	  (1 << band_idx)) != 0));

	return 0;
}

static uint32_t get_iir_band_coeff(struct snd_soc_codec *codec,
				   int iir_idx, int band_idx,
				   int coeff_idx)
{
	uint32_t value = 0;

	/* Address does not automatically update if reading */
	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_read(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx));

	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_read(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 8);

	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_read(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 16);

	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_read(codec, (MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL
		+ 64 * iir_idx)) & 0x3f) << 24);

	return value;

}

static void set_iir_band_coeff(struct snd_soc_codec *codec,
			       int iir_idx, int band_idx,
			       uint32_t value)
{
	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value & 0xFF));

	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 16) & 0xFF);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 24) & 0x3F);

}

static int msm_dig_cdc_get_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 0);
	ucontrol->value.integer.value[1] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 1);
	ucontrol->value.integer.value[2] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 2);
	ucontrol->value.integer.value[3] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 3);
	ucontrol->value.integer.value[4] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 4);

	dev_dbg(codec->dev, "%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[1],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[2],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[3],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[4]);
	return 0;
}

static int msm_dig_cdc_put_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	/* Mask top bit it is reserved */
	/* Updates addr automatically for each B2 write */
	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B1_CTL + 64 * iir_idx),
		(band_idx * BAND_MAX * sizeof(uint32_t)) & 0x7F);


	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[0]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[1]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[2]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[3]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[4]);

	dev_dbg(codec->dev, "%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 0),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 1),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 2),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 3),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 4));
	return 0;
}

static void tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct snd_soc_codec *codec;
	struct msm_dig *msm_dig_cdc;
	u16 tx_mux_ctl_reg;
	u8 hpf_cut_of_freq;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	codec = hpf_work->dig_cdc->codec;
	msm_dig_cdc = codec->control_data;
	hpf_cut_of_freq = hpf_work->tx_hpf_cut_of_freq;

	tx_mux_ctl_reg = MSM89XX_CDC_CORE_TX1_MUX_CTL +
			(hpf_work->decimator - 1) * 32;

	dev_dbg(codec->dev, "%s(): decimator %u hpf_cut_of_freq 0x%x\n",
		 __func__, hpf_work->decimator, (unsigned int)hpf_cut_of_freq);
	msm_dig_cdc->update_clkdiv(msm_dig_cdc->handle, 0x51);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30, hpf_cut_of_freq << 4);
}

static int msm_dig_cdc_codec_set_iir_gain(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int value = 0, reg;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (w->shift == 0)
			reg = MSM89XX_CDC_CORE_IIR1_GAIN_B1_CTL;
		else if (w->shift == 1)
			reg = MSM89XX_CDC_CORE_IIR2_GAIN_B1_CTL;
		else
			goto ret;
		value = snd_soc_read(codec, reg);
		snd_soc_write(codec, reg, value);
		break;
	default:
		pr_err("%s: event = %d not expected\n", __func__, event);
	}
ret:
	return 0;
}

static int msm_dig_cdc_compander_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);
	int comp_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int rx_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	dev_dbg(codec->dev, "%s: msm_dig_cdc->comp[%d]_enabled[%d] = %d\n",
			__func__, comp_idx, rx_idx,
			dig_cdc->comp_enabled[rx_idx]);

	ucontrol->value.integer.value[0] = dig_cdc->comp_enabled[rx_idx];

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_dig_cdc_compander_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);
	int comp_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int rx_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	if (dig_cdc->version >= DIANGU) {
		if (!value)
			dig_cdc->comp_enabled[rx_idx] = 0;
		else
			dig_cdc->comp_enabled[rx_idx] = comp_idx;
	}

	dev_dbg(codec->dev, "%s: msm_dig_cdc->comp[%d]_enabled[%d] = %d\n",
		__func__, comp_idx, rx_idx,
		dig_cdc->comp_enabled[rx_idx]);

	return 0;
}

static const struct snd_kcontrol_new compander_kcontrols[] = {
	SOC_SINGLE_EXT("COMP0 RX1", COMPANDER_1, MSM89XX_RX1, 1, 0,
	msm_dig_cdc_compander_get, msm_dig_cdc_compander_set),

	SOC_SINGLE_EXT("COMP0 RX2", COMPANDER_1, MSM89XX_RX2, 1, 0,
	msm_dig_cdc_compander_get, msm_dig_cdc_compander_set),

};

static int msm_dig_cdc_set_interpolator_rate(struct snd_soc_dai *dai,
					     u8 rx_fs_rate_reg_val,
					     u32 sample_rate)
{
	snd_soc_update_bits(dai->codec,
			MSM89XX_CDC_CORE_RX1_B5_CTL, 0xF0, rx_fs_rate_reg_val);
	snd_soc_update_bits(dai->codec,
			MSM89XX_CDC_CORE_RX2_B5_CTL, 0xF0, rx_fs_rate_reg_val);
	return 0;
}

static int msm_dig_cdc_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	u8 tx_fs_rate, rx_fs_rate, rx_clk_fs_rate;
	int ret;

	dev_dbg(dai->codec->dev,
		"%s: dai_name = %s DAI-ID %x rate %d num_ch %d format %d\n",
		__func__, dai->name, dai->id, params_rate(params),
		params_channels(params), params_format(params));

	switch (params_rate(params)) {
	case 8000:
		tx_fs_rate = 0x00;
		rx_fs_rate = 0x00;
		rx_clk_fs_rate = 0x00;
		break;
	case 16000:
		tx_fs_rate = 0x20;
		rx_fs_rate = 0x20;
		rx_clk_fs_rate = 0x01;
		break;
	case 32000:
		tx_fs_rate = 0x40;
		rx_fs_rate = 0x40;
		rx_clk_fs_rate = 0x02;
		break;
	case 44100:
	case 48000:
		tx_fs_rate = 0x60;
		rx_fs_rate = 0x60;
		rx_clk_fs_rate = 0x03;
		break;
	case 96000:
		tx_fs_rate = 0x80;
		rx_fs_rate = 0x80;
		rx_clk_fs_rate = 0x04;
		break;
	case 192000:
		tx_fs_rate = 0xA0;
		rx_fs_rate = 0xA0;
		rx_clk_fs_rate = 0x05;
		break;
	default:
		dev_err(dai->codec->dev,
			"%s: Invalid sampling rate %d\n", __func__,
			params_rate(params));
		return -EINVAL;
	}

	snd_soc_update_bits(dai->codec,
			MSM89XX_CDC_CORE_CLK_RX_I2S_CTL, 0x0F, rx_clk_fs_rate);

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_CAPTURE:
		break;
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = msm_dig_cdc_set_interpolator_rate(dai, rx_fs_rate,
						  params_rate(params));
		if (ret < 0) {
			dev_err(dai->codec->dev,
				"%s: set decimator rate failed %d\n", __func__,
				ret);
			return ret;
		}
		break;
	default:
		dev_err(dai->codec->dev,
			"%s: Invalid stream type %d\n", __func__,
			substream->stream);
		return -EINVAL;
	}
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(dai->codec,
				MSM89XX_CDC_CORE_CLK_RX_I2S_CTL, 0x20, 0x20);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		snd_soc_update_bits(dai->codec,
				MSM89XX_CDC_CORE_CLK_RX_I2S_CTL, 0x20, 0x00);
		break;
	default:
		dev_err(dai->codec->dev, "%s: wrong format selected\n",
				__func__);
		return -EINVAL;
	}
	return 0;
}

static int msm_dig_cdc_codec_enable_dmic(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);
	u8  dmic_clk_en;
	u16 dmic_clk_reg;
	s32 *dmic_clk_cnt;
	unsigned int dmic;
	int ret;
	char *dmic_num = strpbrk(w->name, "1234");

	if (dmic_num == NULL) {
		dev_err(codec->dev, "%s: Invalid DMIC\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(dmic_num, 10, &dmic);
	if (ret < 0) {
		dev_err(codec->dev,
			"%s: Invalid DMIC line on the codec\n", __func__);
		return -EINVAL;
	}

	switch (dmic) {
	case 1:
	case 2:
		dmic_clk_en = 0x01;
		dmic_clk_cnt = &(dig_cdc->dmic_1_2_clk_cnt);
		dmic_clk_reg = MSM89XX_CDC_CORE_CLK_DMIC_B1_CTL;
		dev_dbg(codec->dev,
			"%s() event %d DMIC%d dmic_1_2_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);
		break;
	case 3:
	case 4:
		dmic_clk_en = 0x01;
		dmic_clk_cnt = &(dig_cdc->dmic_3_4_clk_cnt);
		dmic_clk_reg = MSM89XX_CDC_CORE_CLK_DMIC_B2_CTL;
		dev_dbg(codec->dev,
			"%s() event %d DMIC%d dmic_3_4_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);
		break;
	default:
		dev_err(codec->dev, "%s: Invalid DMIC Selection\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		(*dmic_clk_cnt)++;
		if (*dmic_clk_cnt == 1) {
			snd_soc_update_bits(codec, dmic_clk_reg,
					0x0E, 0x04);
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, dmic_clk_en);
		}
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_TX1_DMIC_CTL + (dmic - 1) * 0x20,
			0x07, 0x02);
		break;
	case SND_SOC_DAPM_POST_PMD:
		(*dmic_clk_cnt)--;
		if (*dmic_clk_cnt  == 0)
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, 0);
		break;
	}
	return 0;
}

static int msm_dig_cdc_codec_enable_dec(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct msm_asoc_mach_data *pdata = NULL;
	unsigned int decimator;
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);
	struct msm_dig *msm_dig_cdc = codec->control_data;
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	int ret = 0, i;
	u16 dec_reset_reg, tx_vol_ctl_reg, tx_mux_ctl_reg;
	u8 dec_hpf_cut_of_freq;
	int offset;
	char *dec_num;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		dev_err(codec->dev,
			"%s: Invalid decimator = %s\n", __func__, w->name);
		ret = -EINVAL;
		goto out;
	}

	dec_num = strpbrk(dec_name, "12345");
	if (dec_num == NULL) {
		dev_err(codec->dev, "%s: Invalid Decimator\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtouint(dec_num, 10, &decimator);
	if (ret < 0) {
		dev_err(codec->dev,
			"%s: Invalid decimator = %s\n", __func__, dec_name);
		ret = -EINVAL;
		goto out;
	}

	dev_dbg(codec->dev,
		"%s(): widget = %s dec_name = %s decimator = %u\n", __func__,
		w->name, dec_name, decimator);

	if (w->reg == MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL) {
		dec_reset_reg = MSM89XX_CDC_CORE_CLK_TX_RESET_B1_CTL;
		offset = 0;
	} else {
		dev_err(codec->dev, "%s: Error, incorrect dec\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	tx_vol_ctl_reg = MSM89XX_CDC_CORE_TX1_VOL_CTL_CFG +
			 32 * (decimator - 1);
	tx_mux_ctl_reg = MSM89XX_CDC_CORE_TX1_MUX_CTL +
			  32 * (decimator - 1);
	if (decimator == 5) {
		tx_vol_ctl_reg = MSM89XX_CDC_CORE_TX5_VOL_CTL_CFG;
		tx_mux_ctl_reg = MSM89XX_CDC_CORE_TX5_MUX_CTL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enableable TX digital mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		for (i = 0; i < NUM_DECIMATORS; i++) {
			if (decimator == i + 1)
				dig_cdc->dec_active[i] = true;
		}

		dec_hpf_cut_of_freq = snd_soc_read(codec, tx_mux_ctl_reg);

		dec_hpf_cut_of_freq = (dec_hpf_cut_of_freq & 0x30) >> 4;

		tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq =
			dec_hpf_cut_of_freq;

		if (dec_hpf_cut_of_freq != CF_MIN_3DB_150HZ) {

			/* set cut of freq to CF_MIN_3DB_150HZ (0x1); */
			snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
					    CF_MIN_3DB_150HZ << 4);
		}
		msm_dig_cdc->update_clkdiv(msm_dig_cdc->handle, 0x42);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* enable HPF */
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x00);

		if (tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq !=
				CF_MIN_3DB_150HZ) {

			schedule_delayed_work(&tx_hpf_work[decimator - 1].dwork,
					msecs_to_jiffies(300));
		}
		/* apply the digital gain after the decimator is enabled*/
		if ((w->shift) < ARRAY_SIZE(tx_digital_gain_reg))
			snd_soc_write(codec,
				  tx_digital_gain_reg[w->shift + offset],
				  snd_soc_read(codec,
				  tx_digital_gain_reg[w->shift + offset])
				  );
		if (pdata->lb_mode) {
			pr_debug("%s: loopback mode unmute the DEC\n",
							__func__);
			snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);
		}
				snd_soc_update_bits(codec, tx_vol_ctl_reg,
						0x01, 0x00);

		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		msleep(20);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		cancel_delayed_work_sync(&tx_hpf_work[decimator - 1].dwork);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift,
			1 << w->shift);
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift, 0x0);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
			(tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq) << 4);
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);
		for (i = 0; i < NUM_DECIMATORS; i++) {
			if (decimator == i + 1)
				dig_cdc->dec_active[i] = false;
		}
		break;
	}
out:
	kfree(widget_name);
	return ret;
}

static int msm_dig_cdc_event_notify(struct notifier_block *block,
				    unsigned long val,
				    void *data)
{
	enum dig_cdc_notify_event event = (enum dig_cdc_notify_event)val;
	struct snd_soc_codec *codec = registered_digcodec;
	struct msm_dig *msm_dig_cdc = codec->control_data;
	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);

	switch (event) {
	case DIG_CDC_EVENT_CLK_ON:
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_CLK_PDM_CTL, 0x03, 0x03);
		if (pdata->mclk_freq == MCLK_RATE_12P288MHZ ||
		    pdata->native_clk_set)
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_TOP_CTL, 0x01, 0x00);
		else if (pdata->mclk_freq == MCLK_RATE_9P6MHZ)
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_TOP_CTL, 0x01, 0x01);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_MCLK_CTL, 0x01, 0x01);
		break;
	case DIG_CDC_EVENT_CLK_OFF:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_PDM_CTL, 0x03, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_MCLK_CTL, 0x01, 0x00);
		break;
	case DIG_CDC_EVENT_RX1_MUTE_ON:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX1_B6_CTL, 0x01, 0x01);
		msm_dig_cdc->mute_mask |= HPHL_PA_DISABLE;
		break;
	case DIG_CDC_EVENT_RX1_MUTE_OFF:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX1_B6_CTL, 0x01, 0x00);
		msm_dig_cdc->mute_mask &= (~HPHL_PA_DISABLE);
		break;
	case DIG_CDC_EVENT_RX2_MUTE_ON:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX2_B6_CTL, 0x01, 0x01);
		msm_dig_cdc->mute_mask |= HPHR_PA_DISABLE;
		break;
	case DIG_CDC_EVENT_RX2_MUTE_OFF:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX2_B6_CTL, 0x01, 0x00);
		msm_dig_cdc->mute_mask &= (~HPHR_PA_DISABLE);
		break;
	case DIG_CDC_EVENT_RX3_MUTE_ON:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX3_B6_CTL, 0x01, 0x01);
		msm_dig_cdc->mute_mask |= SPKR_PA_DISABLE;
		break;
	case DIG_CDC_EVENT_RX3_MUTE_OFF:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX3_B6_CTL, 0x01, 0x00);
		msm_dig_cdc->mute_mask &= (~SPKR_PA_DISABLE);
		break;
	case DIG_CDC_EVENT_PRE_RX1_INT_ON:
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B3_CTL, 0x1C, 0x14);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B4_CTL, 0x18, 0x10);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B3_CTL, 0x80, 0x80);
		break;
	case DIG_CDC_EVENT_PRE_RX2_INT_ON:
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B3_CTL, 0x1C, 0x14);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B4_CTL, 0x18, 0x10);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B3_CTL, 0x80, 0x80);
		break;
	case DIG_CDC_EVENT_POST_RX1_INT_OFF:
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B3_CTL, 0x1C, 0x00);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B4_CTL, 0x18, 0xFF);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B3_CTL, 0x80, 0x00);
		break;
	case DIG_CDC_EVENT_POST_RX2_INT_OFF:
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B3_CTL, 0x1C, 0x00);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B4_CTL, 0x18, 0xFF);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B3_CTL, 0x80, 0x00);
		break;
	case DIG_CDC_EVENT_SSR_DOWN:
		regcache_cache_only(msm_dig_cdc->regmap, true);
		break;
	case DIG_CDC_EVENT_SSR_UP:
		regcache_cache_only(msm_dig_cdc->regmap, false);
		regcache_mark_dirty(msm_dig_cdc->regmap);
		regcache_sync(msm_dig_cdc->regmap);
		break;
	case DIG_CDC_EVENT_INVALID:
	default:
		break;
	}
	return 0;
}

static ssize_t msm_dig_codec_version_read(struct snd_info_entry *entry,
					  void *file_private_data,
					  struct file *file,
					  char __user *buf, size_t count,
					  loff_t pos)
{
	struct msm_dig_priv *msm_dig;
	char buffer[MSM_DIG_CDC_VERSION_ENTRY_SIZE];
	int len = 0;

	msm_dig = (struct msm_dig_priv *) entry->private_data;
	if (!msm_dig) {
		pr_err("%s: msm_dig priv is null\n", __func__);
		return -EINVAL;
	}

	switch (msm_dig->version) {
	case DRAX_CDC:
	    len = snprintf(buffer, sizeof(buffer), "SDM660-CDC_1_0\n");
	    break;
	default:
	    len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops msm_dig_codec_info_ops = {
	.read = msm_dig_codec_version_read,
};

/*
 * msm_dig_codec_info_create_codec_entry - creates msm_dig module
 * @codec_root: The parent directory
 * @codec: Codec instance
 *
 * Creates msm_dig module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int msm_dig_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					  struct snd_soc_codec *codec)
{
	struct snd_info_entry *version_entry;
	struct msm_dig_priv *msm_dig;
	struct snd_soc_card *card;

	if (!codec_root || !codec)
		return -EINVAL;

	msm_dig = snd_soc_codec_get_drvdata(codec);
	card = codec->component.card;
	msm_dig->entry = snd_register_module_info(codec_root->module,
						  "msm_digital_codec",
						  codec_root);
	if (!msm_dig->entry) {
		dev_dbg(codec->dev, "%s: failed to create msm_digital entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   msm_dig->entry);
	if (!version_entry) {
		dev_dbg(codec->dev, "%s: failed to create msm_digital version entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry->private_data = msm_dig;
	version_entry->size = MSM_DIG_CDC_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &msm_dig_codec_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		return -ENOMEM;
	}
	msm_dig->version_entry = version_entry;
	return 0;
}
EXPORT_SYMBOL(msm_dig_codec_info_create_codec_entry);

static int msm_dig_cdc_soc_probe(struct snd_soc_codec *codec)
{
	struct msm_dig_priv *dig_cdc = NULL;
	struct msm_dig *msm_dig_cdc = dev_get_drvdata(codec->dev);
	int i, ret;

	dig_cdc = devm_kzalloc(codec->dev, sizeof(struct msm_dig_priv),
			      GFP_KERNEL);
	if (!dig_cdc)
		return -ENOMEM;
	snd_soc_codec_set_drvdata(codec, dig_cdc);
	dig_cdc->codec = codec;
	codec->control_data = msm_dig_cdc;

	snd_soc_add_codec_controls(codec, compander_kcontrols,
			ARRAY_SIZE(compander_kcontrols));

	for (i = 0; i < NUM_DECIMATORS; i++) {
		tx_hpf_work[i].dig_cdc = dig_cdc;
		tx_hpf_work[i].decimator = i + 1;
		INIT_DELAYED_WORK(&tx_hpf_work[i].dwork,
			tx_hpf_corner_freq_callback);
	}

	for (i = 0; i < MSM89XX_RX_MAX; i++)
		dig_cdc->comp_enabled[i] = COMPANDER_NONE;

	/* Register event notifier */
	msm_dig_cdc->nblock.notifier_call = msm_dig_cdc_event_notify;
	if (msm_dig_cdc->register_notifier) {
		ret = msm_dig_cdc->register_notifier(msm_dig_cdc->handle,
						     &msm_dig_cdc->nblock,
						     true);
		if (ret) {
			pr_err("%s: Failed to register notifier %d\n",
				__func__, ret);
			return ret;
		}
	}
	/* Assign to DRAX_CDC for initial version */
	dig_cdc->version = DRAX_CDC;
	registered_digcodec = codec;
	return 0;
}

static int msm_dig_cdc_soc_remove(struct snd_soc_codec *codec)
{
	struct msm_dig *msm_dig_cdc = dev_get_drvdata(codec->dev);

	if (msm_dig_cdc->register_notifier)
		msm_dig_cdc->register_notifier(msm_dig_cdc->handle,
					       &msm_dig_cdc->nblock,
					       false);
	iounmap(msm_dig_cdc->dig_base);
	return 0;
}

static const struct snd_soc_dapm_route audio_dig_map[] = {
	{"RX_I2S_CLK", NULL, "CDC_CONN"},
	{"I2S RX1", NULL, "RX_I2S_CLK"},
	{"I2S RX2", NULL, "RX_I2S_CLK"},
	{"I2S RX3", NULL, "RX_I2S_CLK"},

	{"I2S TX1", NULL, "TX_I2S_CLK"},
	{"I2S TX2", NULL, "TX_I2S_CLK"},
	{"I2S TX3", NULL, "TX_I2S_CLK"},
	{"I2S TX4", NULL, "TX_I2S_CLK"},
	{"I2S TX5", NULL, "TX_I2S_CLK"},
	{"I2S TX6", NULL, "TX_I2S_CLK"},

	{"I2S TX1", NULL, "DEC1 MUX"},
	{"I2S TX2", NULL, "DEC2 MUX"},
	{"I2S TX3", NULL, "I2S TX2 INP1"},
	{"I2S TX4", NULL, "I2S TX2 INP2"},
	{"I2S TX5", NULL, "DEC3 MUX"},
	{"I2S TX6", NULL, "I2S TX3 INP2"},

	{"I2S TX2 INP1", "RX_MIX1", "RX1 MIX2"},
	{"I2S TX2 INP1", "DEC3", "DEC3 MUX"},
	{"I2S TX2 INP2", "RX_MIX2", "RX2 MIX2"},
	{"I2S TX2 INP2", "RX_MIX3", "RX3 MIX1"},
	{"I2S TX2 INP2", "DEC4", "DEC4 MUX"},
	{"I2S TX3 INP2", "DEC4", "DEC4 MUX"},
	{"I2S TX3 INP2", "DEC5", "DEC5 MUX"},

	{"PDM_OUT_RX1", NULL, "RX1 CHAIN"},
	{"PDM_OUT_RX2", NULL, "RX2 CHAIN"},
	{"PDM_OUT_RX3", NULL, "RX3 CHAIN"},

	{"RX1 CHAIN", NULL, "RX1 MIX2"},
	{"RX2 CHAIN", NULL, "RX2 MIX2"},
	{"RX3 CHAIN", NULL, "RX3 MIX1"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP3"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP1"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP2"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP1"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP2"},
	{"RX1 MIX2", NULL, "RX1 MIX1"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP1"},
	{"RX2 MIX2", NULL, "RX2 MIX1"},
	{"RX2 MIX2", NULL, "RX2 MIX2 INP1"},

	{"RX1 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX1 MIX1 INP1", "IIR2", "IIR2"},
	{"RX1 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX1 MIX1 INP2", "IIR2", "IIR2"},
	{"RX1 MIX1 INP3", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP3", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP3", "RX3", "I2S RX3"},

	{"RX2 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX2 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX2 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX2 MIX1 INP1", "IIR1", "IIR1"},
	{"RX2 MIX1 INP1", "IIR2", "IIR2"},
	{"RX2 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX2 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX2 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX2 MIX1 INP2", "IIR1", "IIR1"},
	{"RX2 MIX1 INP2", "IIR2", "IIR2"},

	{"RX3 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX3 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX3 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX3 MIX1 INP1", "IIR1", "IIR1"},
	{"RX3 MIX1 INP1", "IIR2", "IIR2"},
	{"RX3 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX3 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX3 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX3 MIX1 INP2", "IIR1", "IIR1"},
	{"RX3 MIX1 INP2", "IIR2", "IIR2"},

	{"RX1 MIX2 INP1", "IIR1", "IIR1"},
	{"RX2 MIX2 INP1", "IIR1", "IIR1"},
	{"RX1 MIX2 INP1", "IIR2", "IIR2"},
	{"RX2 MIX2 INP1", "IIR2", "IIR2"},

		/* Decimator Inputs */
	{"DEC1 MUX", "DMIC1", "DMIC1"},
	{"DEC1 MUX", "DMIC2", "DMIC2"},
	{"DEC1 MUX", "DMIC3", "DMIC3"},
	{"DEC1 MUX", "DMIC4", "DMIC4"},
	{"DEC1 MUX", "ADC1", "ADC1_IN"},
	{"DEC1 MUX", "ADC2", "ADC2_IN"},
	{"DEC1 MUX", "ADC3", "ADC3_IN"},
	{"DEC1 MUX", NULL, "CDC_CONN"},

	{"DEC2 MUX", "DMIC1", "DMIC1"},
	{"DEC2 MUX", "DMIC2", "DMIC2"},
	{"DEC2 MUX", "DMIC3", "DMIC3"},
	{"DEC2 MUX", "DMIC4", "DMIC4"},
	{"DEC2 MUX", "ADC1", "ADC1_IN"},
	{"DEC2 MUX", "ADC2", "ADC2_IN"},
	{"DEC2 MUX", "ADC3", "ADC3_IN"},
	{"DEC2 MUX", NULL, "CDC_CONN"},

	{"DEC3 MUX", "DMIC1", "DMIC1"},
	{"DEC3 MUX", "DMIC2", "DMIC2"},
	{"DEC3 MUX", "DMIC3", "DMIC3"},
	{"DEC3 MUX", "DMIC4", "DMIC4"},
	{"DEC3 MUX", "ADC1", "ADC1_IN"},
	{"DEC3 MUX", "ADC2", "ADC2_IN"},
	{"DEC3 MUX", "ADC3", "ADC3_IN"},
	{"DEC3 MUX", NULL, "CDC_CONN"},

	{"DEC4 MUX", "DMIC1", "DMIC1"},
	{"DEC4 MUX", "DMIC2", "DMIC2"},
	{"DEC4 MUX", "DMIC3", "DMIC3"},
	{"DEC4 MUX", "DMIC4", "DMIC4"},
	{"DEC4 MUX", "ADC1", "ADC1_IN"},
	{"DEC4 MUX", "ADC2", "ADC2_IN"},
	{"DEC4 MUX", "ADC3", "ADC3_IN"},
	{"DEC4 MUX", NULL, "CDC_CONN"},

	{"DEC5 MUX", "DMIC1", "DMIC1"},
	{"DEC5 MUX", "DMIC2", "DMIC2"},
	{"DEC5 MUX", "DMIC3", "DMIC3"},
	{"DEC5 MUX", "DMIC4", "DMIC4"},
	{"DEC5 MUX", "ADC1", "ADC1_IN"},
	{"DEC5 MUX", "ADC2", "ADC2_IN"},
	{"DEC5 MUX", "ADC3", "ADC3_IN"},
	{"DEC5 MUX", NULL, "CDC_CONN"},

	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"IIR1 INP1 MUX", "DEC3", "DEC3 MUX"},
	{"IIR1 INP1 MUX", "DEC4", "DEC4 MUX"},
	{"IIR2", NULL, "IIR2 INP1 MUX"},
	{"IIR2 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR2 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"IIR1 INP1 MUX", "DEC3", "DEC3 MUX"},
	{"IIR1 INP1 MUX", "DEC4", "DEC4 MUX"},
};


static const char * const i2s_tx2_inp1_text[] = {
	"ZERO", "RX_MIX1", "DEC3"
};

static const char * const i2s_tx2_inp2_text[] = {
	"ZERO", "RX_MIX2", "RX_MIX3", "DEC4"
};

static const char * const i2s_tx3_inp2_text[] = {
	"DEC4", "DEC5"
};

static const char * const rx_mix1_text[] = {
	"ZERO", "IIR1", "IIR2", "RX1", "RX2", "RX3"
};

static const char * const rx_mix2_text[] = {
	"ZERO", "IIR1", "IIR2"
};

static const char * const dec_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "DMIC1", "DMIC2", "DMIC3", "DMIC4"
};

static const char * const iir_inp1_text[] = {
	"ZERO", "DEC1", "DEC2", "RX1", "RX2", "RX3", "DEC3", "DEC4"
};

/* I2S TX MUXes */
static const struct soc_enum i2s_tx2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_I2S_SD1_CTL,
		2, 3, i2s_tx2_inp1_text);

static const struct soc_enum i2s_tx2_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_I2S_SD1_CTL,
		0, 4, i2s_tx2_inp2_text);

static const struct soc_enum i2s_tx3_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_I2S_SD1_CTL,
		4, 2, i2s_tx3_inp2_text);

/* RX1 MIX1 */
static const struct soc_enum rx_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX1_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX1_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX1_B2_CTL,
		0, 6, rx_mix1_text);

/* RX1 MIX2 */
static const struct soc_enum rx_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX1_B3_CTL,
		0, 3, rx_mix2_text);

/* RX2 MIX1 */
static const struct soc_enum rx2_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX2_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX2_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX2_B1_CTL,
		0, 6, rx_mix1_text);

/* RX2 MIX2 */
static const struct soc_enum rx2_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX2_B3_CTL,
		0, 3, rx_mix2_text);

/* RX3 MIX1 */
static const struct soc_enum rx3_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX3_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX3_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX3_B1_CTL,
		0, 6, rx_mix1_text);

/* DEC */
static const struct soc_enum dec1_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_B1_CTL,
		0, 8, dec_mux_text);

static const struct soc_enum dec2_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_B1_CTL,
		3, 8, dec_mux_text);

static const struct soc_enum dec3_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_B2_CTL,
		0, 8, dec_mux_text);

static const struct soc_enum dec4_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_B2_CTL,
		3, 8, dec_mux_text);

static const struct soc_enum decsva_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_B3_CTL,
		0, 8, dec_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_EQ1_B1_CTL,
		0, 8, iir_inp1_text);

static const struct soc_enum iir2_inp1_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_EQ2_B1_CTL,
		0, 8, iir_inp1_text);

/*cut of frequency for high pass filter*/
static const char * const cf_text[] = {
	"MIN_3DB_4Hz", "MIN_3DB_75Hz", "MIN_3DB_150Hz"
};

static const struct soc_enum cf_rxmix1_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_RX1_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix2_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_RX2_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix3_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_RX3_B4_CTL, 0, 3, cf_text);

static const struct snd_kcontrol_new rx3_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP1 Mux", rx3_mix1_inp1_chain_enum);

#define MSM89XX_DEC_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_dapm_get_enum_double, \
	.put = msm_dig_cdc_put_dec_enum, \
	.private_value = (unsigned long)&xenum }

static const struct snd_kcontrol_new dec1_mux =
	MSM89XX_DEC_ENUM("DEC1 MUX Mux", dec1_mux_enum);

static const struct snd_kcontrol_new dec2_mux =
	MSM89XX_DEC_ENUM("DEC2 MUX Mux", dec2_mux_enum);

static const struct snd_kcontrol_new dec3_mux =
	MSM89XX_DEC_ENUM("DEC3 MUX Mux", dec3_mux_enum);

static const struct snd_kcontrol_new dec4_mux =
	MSM89XX_DEC_ENUM("DEC4 MUX Mux", dec4_mux_enum);

static const struct snd_kcontrol_new decsva_mux =
	MSM89XX_DEC_ENUM("DEC5 MUX Mux", decsva_mux_enum);

static const struct snd_kcontrol_new i2s_tx2_inp1_mux =
	SOC_DAPM_ENUM("I2S TX2 INP1 Mux", i2s_tx2_inp1_chain_enum);

static const struct snd_kcontrol_new i2s_tx2_inp2_mux =
	SOC_DAPM_ENUM("I2S TX2 INP2 Mux", i2s_tx2_inp2_chain_enum);

static const struct snd_kcontrol_new i2s_tx3_inp2_mux =
	SOC_DAPM_ENUM("I2S TX3 INP2 Mux", i2s_tx3_inp2_chain_enum);

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

static const struct snd_kcontrol_new iir2_inp1_mux =
	SOC_DAPM_ENUM("IIR2 INP1 Mux", iir2_inp1_mux_enum);

static const struct snd_kcontrol_new rx_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP1 Mux", rx_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP2 Mux", rx_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP3 Mux", rx_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP1 Mux", rx2_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP2 Mux", rx2_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP3 Mux", rx2_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP2 Mux", rx3_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP3 Mux", rx3_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx1_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX2 INP1 Mux", rx_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX2 INP1 Mux", rx2_mix2_inp1_chain_enum);

static const struct snd_soc_dapm_widget msm_dig_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("I2S RX1", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("I2S RX2", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("I2S RX3", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT("I2S TX1", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX2", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX3", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX4", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX5", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX6", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MIXER_E("RX1 MIX2", MSM89XX_CDC_CORE_CLK_RX_B1_CTL,
			     MSM89XX_RX1, 0, NULL, 0,
			     msm_dig_cdc_codec_enable_interpolator,
			     SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2 MIX2", MSM89XX_CDC_CORE_CLK_RX_B1_CTL,
			     MSM89XX_RX2, 0, NULL, 0,
			     msm_dig_cdc_codec_enable_interpolator,
			     SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX3 MIX1", MSM89XX_CDC_CORE_CLK_RX_B1_CTL,
			     MSM89XX_RX3, 0, NULL, 0,
			     msm_dig_cdc_codec_enable_interpolator,
			     SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("RX1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("RX1 CHAIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 CHAIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX3 CHAIN", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("RX1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX2 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX3 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX1 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx1_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix2_inp1_mux),

	SND_SOC_DAPM_SUPPLY_S("CDC_CONN", -2, MSM89XX_CDC_CORE_CLK_OTHR_CTL,
		2, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("DEC1 MUX",
		MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, msm_dig_cdc_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC2 MUX",
		MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, msm_dig_cdc_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC3 MUX",
		MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL, 2, 0,
		&dec3_mux, msm_dig_cdc_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC4 MUX",
		MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL, 3, 0,
		&dec4_mux, msm_dig_cdc_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC5 MUX",
		MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL, 4, 0,
		&decsva_mux, msm_dig_cdc_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	/* Sidetone */
	SND_SOC_DAPM_MUX("IIR1 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp1_mux),
	SND_SOC_DAPM_PGA_E("IIR1", MSM89XX_CDC_CORE_CLK_SD_CTL, 0, 0, NULL, 0,
		msm_dig_cdc_codec_set_iir_gain, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX("IIR2 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir2_inp1_mux),
	SND_SOC_DAPM_PGA_E("IIR2", MSM89XX_CDC_CORE_CLK_SD_CTL, 1, 0, NULL, 0,
		msm_dig_cdc_codec_set_iir_gain, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY("RX_I2S_CLK",
		MSM89XX_CDC_CORE_CLK_RX_I2S_CTL, 4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("TX_I2S_CLK",
		MSM89XX_CDC_CORE_CLK_TX_I2S_CTL, 4, 0, NULL, 0),


	SND_SOC_DAPM_MUX("I2S TX2 INP1", SND_SOC_NOPM, 0, 0,
			&i2s_tx2_inp1_mux),
	SND_SOC_DAPM_MUX("I2S TX2 INP2", SND_SOC_NOPM, 0, 0,
			&i2s_tx2_inp2_mux),
	SND_SOC_DAPM_MUX("I2S TX3 INP2", SND_SOC_NOPM, 0, 0,
			&i2s_tx3_inp2_mux),

	/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		msm_dig_cdc_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		msm_dig_cdc_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		msm_dig_cdc_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		msm_dig_cdc_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("ADC1_IN"),
	SND_SOC_DAPM_INPUT("ADC2_IN"),
	SND_SOC_DAPM_INPUT("ADC3_IN"),
	SND_SOC_DAPM_OUTPUT("PDM_OUT_RX1"),
	SND_SOC_DAPM_OUTPUT("PDM_OUT_RX2"),
	SND_SOC_DAPM_OUTPUT("PDM_OUT_RX3"),
};

static const struct soc_enum cf_dec1_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_TX1_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec2_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_TX2_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec3_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_TX3_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec4_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_TX4_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_decsva_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_TX5_MUX_CTL, 4, 3, cf_text);

static const struct snd_kcontrol_new msm_dig_snd_controls[] = {
	SOC_SINGLE_SX_TLV("DEC1 Volume",
		MSM89XX_CDC_CORE_TX1_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC2 Volume",
		  MSM89XX_CDC_CORE_TX2_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC3 Volume",
		  MSM89XX_CDC_CORE_TX3_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC4 Volume",
		  MSM89XX_CDC_CORE_TX4_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC5 Volume",
		  MSM89XX_CDC_CORE_TX5_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("IIR1 INP1 Volume",
			  MSM89XX_CDC_CORE_IIR1_GAIN_B1_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP2 Volume",
			  MSM89XX_CDC_CORE_IIR1_GAIN_B2_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP3 Volume",
			  MSM89XX_CDC_CORE_IIR1_GAIN_B3_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP4 Volume",
			  MSM89XX_CDC_CORE_IIR1_GAIN_B4_CTL,
			0,  -84,	40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR2 INP1 Volume",
			  MSM89XX_CDC_CORE_IIR2_GAIN_B1_CTL,
			0,  -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("RX1 Digital Volume",
		MSM89XX_CDC_CORE_RX1_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX2 Digital Volume",
		MSM89XX_CDC_CORE_RX2_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX3 Digital Volume",
		MSM89XX_CDC_CORE_RX3_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),

	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),

	SOC_SINGLE_EXT("IIR2 Enable Band1", IIR2, BAND1, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band2", IIR2, BAND2, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band3", IIR2, BAND3, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band4", IIR2, BAND4, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band5", IIR2, BAND5, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR2 Band1", IIR2, BAND1, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band2", IIR2, BAND2, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band3", IIR2, BAND3, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band4", IIR2, BAND4, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band5", IIR2, BAND5, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),

	SOC_SINGLE("RX1 HPF Switch",
		MSM89XX_CDC_CORE_RX1_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX2 HPF Switch",
		MSM89XX_CDC_CORE_RX2_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX3 HPF Switch",
		MSM89XX_CDC_CORE_RX3_B5_CTL, 2, 1, 0),

	SOC_ENUM("RX1 HPF cut off", cf_rxmix1_enum),
	SOC_ENUM("RX2 HPF cut off", cf_rxmix2_enum),
	SOC_ENUM("RX3 HPF cut off", cf_rxmix3_enum),

	SOC_ENUM("TX1 HPF cut off", cf_dec1_enum),
	SOC_ENUM("TX2 HPF cut off", cf_dec2_enum),
	SOC_ENUM("TX3 HPF cut off", cf_dec3_enum),
	SOC_ENUM("TX4 HPF cut off", cf_dec4_enum),
	SOC_ENUM("TX5 HPF cut off", cf_decsva_enum),
	SOC_SINGLE("TX1 HPF Switch",
		MSM89XX_CDC_CORE_TX1_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX2 HPF Switch",
		MSM89XX_CDC_CORE_TX2_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX3 HPF Switch",
		MSM89XX_CDC_CORE_TX3_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX4 HPF Switch",
		MSM89XX_CDC_CORE_TX4_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX5 HPF Switch",
		MSM89XX_CDC_CORE_TX5_MUX_CTL, 3, 1, 0),
};

static int msm_dig_cdc_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = NULL;
	u16 tx_vol_ctl_reg = 0;
	u8 decimator = 0, i;
	struct msm_dig_priv *dig_cdc;

	pr_debug("%s: Digital Mute val = %d\n", __func__, mute);

	if (!dai || !dai->codec) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}
	codec = dai->codec;
	dig_cdc = snd_soc_codec_get_drvdata(codec);

	if (dai->id == AIF1_PB) {
		dev_dbg(codec->dev, "%s: Not capture use case skip\n",
			__func__);
		return 0;
	}

	mute = (mute) ? 1 : 0;
	if (!mute) {
		/*
		 * 15 ms is an emperical value for the mute time
		 * that was arrived by checking the pop level
		 * to be inaudible
		 */
		usleep_range(15000, 15010);
	}

	if (dai->id == AIF3_SVA) {
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_TX5_VOL_CTL_CFG, 0x01, mute);
		goto ret;
	}
	for (i = 0; i < (NUM_DECIMATORS - 1); i++) {
		if (dig_cdc->dec_active[i])
			decimator = i + 1;
		if (decimator && decimator < NUM_DECIMATORS) {
			/* mute/unmute decimators corresponding to Tx DAI's */
			tx_vol_ctl_reg =
			MSM89XX_CDC_CORE_TX1_VOL_CTL_CFG +
					32 * (decimator - 1);
			snd_soc_update_bits(codec, tx_vol_ctl_reg,
					    0x01, mute);
		}
		decimator = 0;
	}
ret:
	return 0;
}

static struct snd_soc_dai_ops msm_dig_dai_ops = {
	.hw_params = msm_dig_cdc_hw_params,
	.digital_mute = msm_dig_cdc_digital_mute,
};


static struct snd_soc_dai_driver msm_codec_dais[] = {
	{
		.name = "msm_dig_cdc_dai_rx1",
		.id = AIF1_PB,
		.playback = { /* Support maximum range */
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		 .ops = &msm_dig_dai_ops,
	},
	{
		.name = "msm_dig_cdc_dai_tx1",
		.id = AIF1_CAP,
		.capture = { /* Support maximum range */
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		 .ops = &msm_dig_dai_ops,
	},
	{
		.name = "msm_dig_cdc_dai_tx2",
		.id = AIF3_SVA,
		.capture = { /* Support maximum range */
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		 .ops = &msm_dig_dai_ops,
	},
	{
		.name = "msm_dig_cdc_dai_vifeed",
		.id = AIF2_VIFEED,
		.capture = { /* Support maximum range */
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		 .ops = &msm_dig_dai_ops,
	},
};

static struct regmap *msm_digital_get_regmap(struct device *dev)
{
	struct msm_dig *msm_dig_cdc = dev_get_drvdata(dev);

	return msm_dig_cdc->regmap;
}

static struct snd_soc_codec_driver soc_msm_dig_codec = {
	.probe  = msm_dig_cdc_soc_probe,
	.remove = msm_dig_cdc_soc_remove,
	.controls = msm_dig_snd_controls,
	.num_controls = ARRAY_SIZE(msm_dig_snd_controls),
	.dapm_widgets = msm_dig_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(msm_dig_dapm_widgets),
	.dapm_routes = audio_dig_map,
	.num_dapm_routes = ARRAY_SIZE(audio_dig_map),
	.get_regmap = msm_digital_get_regmap,
};

const struct regmap_config msm_digital_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 8,
	.lock = enable_digital_callback,
	.unlock = disable_digital_callback,
	.cache_type = REGCACHE_FLAT,
	.reg_defaults = msm89xx_cdc_core_defaults,
	.num_reg_defaults = MSM89XX_CDC_CORE_MAX_REGISTER,
	.readable_reg = msm89xx_cdc_core_readable_reg,
	.volatile_reg = msm89xx_cdc_core_volatile_reg,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.max_register = MSM89XX_CDC_CORE_MAX_REGISTER,
};

static int msm_dig_cdc_probe(struct platform_device *pdev)
{
	int ret;
	u32 dig_cdc_addr;
	struct msm_dig *msm_dig_cdc;
	struct dig_ctrl_platform_data *pdata;

	msm_dig_cdc = devm_kzalloc(&pdev->dev, sizeof(struct msm_dig),
			      GFP_KERNEL);
	if (!msm_dig_cdc)
		return -ENOMEM;
	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "%s: pdata from parent is NULL\n",
			__func__);
		ret = -EINVAL;
		goto rtn;
	}
	dev_set_drvdata(&pdev->dev, msm_dig_cdc);

	ret = of_property_read_u32(pdev->dev.of_node, "reg",
					&dig_cdc_addr);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "reg");
		return ret;
	}

	msm_dig_cdc->dig_base = ioremap(dig_cdc_addr,
					MSM89XX_CDC_CORE_MAX_REGISTER);
	if (msm_dig_cdc->dig_base == NULL) {
		dev_err(&pdev->dev, "%s ioremap failed\n", __func__);
		return -ENOMEM;
	}
	msm_dig_cdc->regmap =
		devm_regmap_init_mmio_clk(&pdev->dev, NULL,
			msm_dig_cdc->dig_base, &msm_digital_regmap_config);

	msm_dig_cdc->update_clkdiv = pdata->update_clkdiv;
	msm_dig_cdc->get_cdc_version = pdata->get_cdc_version;
	msm_dig_cdc->handle = pdata->handle;
	msm_dig_cdc->register_notifier = pdata->register_notifier;

	snd_soc_register_codec(&pdev->dev, &soc_msm_dig_codec,
				msm_codec_dais, ARRAY_SIZE(msm_codec_dais));
	dev_dbg(&pdev->dev, "%s: registered DIG CODEC 0x%x\n",
			__func__, dig_cdc_addr);
rtn:
	return ret;
}

static int msm_dig_cdc_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_dig_cdc_of_match[] = {
	{.compatible = "qcom,msm-digital-codec"},
	{},
};

static struct platform_driver msm_digcodec_driver = {
	.driver                 = {
		.owner          = THIS_MODULE,
		.name           = DRV_NAME,
		.of_match_table = msm_dig_cdc_of_match,
	},
	.probe                  = msm_dig_cdc_probe,
	.remove                 = msm_dig_cdc_remove,
};
module_platform_driver(msm_digcodec_driver);

MODULE_DESCRIPTION("MSM Audio Digital codec driver");
MODULE_LICENSE("GPL v2");
