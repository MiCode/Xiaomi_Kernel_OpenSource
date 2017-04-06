/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include <linux/device.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mfd/msm-cdc-pinctrl.h>
#include <linux/printk.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/qdsp6v2/apr.h>
#include <linux/soundwire/swr-wcd.h>
#include <linux/qdsp6v2/audio_notifier.h>
#include <sound/apr_audio-v2.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/q6core.h>
#include <sound/tlv.h>
#include "msm_sdw.h"
#include "msm_sdw_registers.h"

#define MSM_SDW_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000)
#define MSM_SDW_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE)

#define MSM_SDW_STRING_LEN 80

#define INT_MCLK1_FREQ 9600000
#define SDW_NPL_FREQ 153600000

#define MSM_SDW_VERSION_1_0 0x0001
#define MSM_SDW_VERSION_ENTRY_SIZE 32

/*
 * 200 Milliseconds sufficient for DSP bring up in the modem
 * after Sub System Restart
 */
#define ADSP_STATE_READY_TIMEOUT_MS 200

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static struct snd_soc_dai_driver msm_sdw_dai[];
static bool skip_irq = true;

static int msm_sdw_config_ear_spkr_gain(struct snd_soc_codec *codec,
					int event, int gain_reg);
static int msm_sdw_config_compander(struct snd_soc_codec *, int, int);
static int msm_sdw_mclk_enable(struct msm_sdw_priv *msm_sdw,
			       int mclk_enable, bool dapm);
static int msm_int_enable_sdw_cdc_clk(struct msm_sdw_priv *msm_sdw,
				      int enable, bool dapm);

enum {
	VI_SENSE_1,
	VI_SENSE_2,
};

enum {
	AIF1_SDW_PB = 0,
	AIF1_SDW_VIFEED,
	NUM_CODEC_DAIS,
};

static const struct msm_sdw_reg_mask_val msm_sdw_spkr_default[] = {
	{MSM_SDW_COMPANDER7_CTL3, 0x80, 0x80},
	{MSM_SDW_COMPANDER8_CTL3, 0x80, 0x80},
	{MSM_SDW_COMPANDER7_CTL7, 0x01, 0x01},
	{MSM_SDW_COMPANDER8_CTL7, 0x01, 0x01},
	{MSM_SDW_BOOST0_BOOST_CTL, 0x7C, 0x50},
	{MSM_SDW_BOOST1_BOOST_CTL, 0x7C, 0x50},
};

static const struct msm_sdw_reg_mask_val msm_sdw_spkr_mode1[] = {
	{MSM_SDW_COMPANDER7_CTL3, 0x80, 0x00},
	{MSM_SDW_COMPANDER8_CTL3, 0x80, 0x00},
	{MSM_SDW_COMPANDER7_CTL7, 0x01, 0x00},
	{MSM_SDW_COMPANDER8_CTL7, 0x01, 0x00},
	{MSM_SDW_BOOST0_BOOST_CTL, 0x7C, 0x44},
	{MSM_SDW_BOOST1_BOOST_CTL, 0x7C, 0x44},
};

/**
 * msm_sdw_set_spkr_gain_offset - offset the speaker path
 * gain with the given offset value.
 *
 * @codec: codec instance
 * @offset: Indicates speaker path gain offset value.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int msm_sdw_set_spkr_gain_offset(struct snd_soc_codec *codec, int offset)
{
	struct msm_sdw_priv *priv;

	if (!codec) {
		pr_err("%s: NULL codec pointer!\n", __func__);
		return -EINVAL;
	}

	priv = snd_soc_codec_get_drvdata(codec);
	if (!priv)
		return -EINVAL;

	priv->spkr_gain_offset = offset;
	return 0;
}
EXPORT_SYMBOL(msm_sdw_set_spkr_gain_offset);

/**
 * msm_sdw_set_spkr_mode - Configures speaker compander and smartboost
 * settings based on speaker mode.
 *
 * @codec: codec instance
 * @mode: Indicates speaker configuration mode.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int msm_sdw_set_spkr_mode(struct snd_soc_codec *codec, int mode)
{
	struct msm_sdw_priv *priv;
	int i;
	const struct msm_sdw_reg_mask_val *regs;
	int size;

	if (!codec) {
		pr_err("%s: NULL codec pointer!\n", __func__);
		return -EINVAL;
	}

	priv = snd_soc_codec_get_drvdata(codec);
	if (!priv)
		return -EINVAL;

	switch (mode) {
	case SPKR_MODE_1:
		regs = msm_sdw_spkr_mode1;
		size = ARRAY_SIZE(msm_sdw_spkr_mode1);
		break;
	default:
		regs = msm_sdw_spkr_default;
		size = ARRAY_SIZE(msm_sdw_spkr_default);
		break;
	}

	priv->spkr_mode = mode;
	for (i = 0; i < size; i++)
		snd_soc_update_bits(codec, regs[i].reg,
				    regs[i].mask, regs[i].val);
	return 0;
}
EXPORT_SYMBOL(msm_sdw_set_spkr_mode);

static int msm_enable_sdw_npl_clk(struct msm_sdw_priv *msm_sdw, int enable)
{
	int ret = 0;

	dev_dbg(msm_sdw->dev, "%s: enable %d\n", __func__, enable);

	mutex_lock(&msm_sdw->sdw_npl_clk_mutex);
	if (enable) {
		if (msm_sdw->sdw_npl_clk_enabled == false) {
			msm_sdw->sdw_npl_clk.enable = 1;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_INT4_MI2S_RX,
				&msm_sdw->sdw_npl_clk);
			if (ret < 0) {
				dev_err(msm_sdw->dev,
					"%s: failed to enable SDW NPL CLK\n",
					__func__);
				mutex_unlock(&msm_sdw->sdw_npl_clk_mutex);
				return ret;
			}
			dev_dbg(msm_sdw->dev, "enabled sdw npl clk\n");
			msm_sdw->sdw_npl_clk_enabled = true;
		}
	} else {
		if (msm_sdw->sdw_npl_clk_enabled == true) {
			msm_sdw->sdw_npl_clk.enable = 0;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_INT4_MI2S_RX,
				&msm_sdw->sdw_npl_clk);
			if (ret < 0)
				dev_err(msm_sdw->dev,
					"%s: failed to disable SDW NPL CLK\n",
					__func__);
			msm_sdw->sdw_npl_clk_enabled = false;
		}
	}
	mutex_unlock(&msm_sdw->sdw_npl_clk_mutex);
	return ret;
}

static int msm_int_enable_sdw_cdc_clk(struct msm_sdw_priv *msm_sdw,
				      int enable, bool dapm)
{
	int ret = 0;

	mutex_lock(&msm_sdw->cdc_int_mclk1_mutex);
	dev_dbg(msm_sdw->dev, "%s: enable %d mclk1 ref counter %d\n",
		__func__, enable, msm_sdw->int_mclk1_rsc_ref);
	if (enable) {
		if (msm_sdw->int_mclk1_rsc_ref == 0) {
			cancel_delayed_work_sync(
					&msm_sdw->disable_int_mclk1_work);
			if (msm_sdw->int_mclk1_enabled == false) {
				msm_sdw->sdw_cdc_core_clk.enable = 1;
				ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_INT4_MI2S_RX,
					&msm_sdw->sdw_cdc_core_clk);
				if (ret < 0) {
					dev_err(msm_sdw->dev,
						"%s: failed to enable SDW MCLK\n",
						__func__);
					goto rtn;
				}
				dev_dbg(msm_sdw->dev,
					"enabled sdw codec core mclk\n");
				msm_sdw->int_mclk1_enabled = true;
			}
		}
		msm_sdw->int_mclk1_rsc_ref++;
	} else {
		cancel_delayed_work_sync(&msm_sdw->disable_int_mclk1_work);
		if (msm_sdw->int_mclk1_rsc_ref > 0) {
			msm_sdw->int_mclk1_rsc_ref--;
			dev_dbg(msm_sdw->dev,
				"%s: decrementing mclk_res_ref %d\n",
				 __func__, msm_sdw->int_mclk1_rsc_ref);
		}
		if (msm_sdw->int_mclk1_enabled == true &&
			msm_sdw->int_mclk1_rsc_ref == 0) {
			msm_sdw->sdw_cdc_core_clk.enable = 0;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_INT4_MI2S_RX,
				&msm_sdw->sdw_cdc_core_clk);
			if (ret < 0)
				dev_err(msm_sdw->dev,
					"%s: failed to disable SDW MCLK\n",
					__func__);
			msm_sdw->int_mclk1_enabled = false;
		}
	}
	mutex_unlock(&msm_sdw->cdc_int_mclk1_mutex);
rtn:
	return ret;
}
EXPORT_SYMBOL(msm_int_enable_sdw_cdc_clk);

static void msm_disable_int_mclk1(struct work_struct *work)
{
	struct msm_sdw_priv *msm_sdw = NULL;
	struct delayed_work *dwork;
	int ret = 0;

	dwork = to_delayed_work(work);
	msm_sdw = container_of(dwork, struct msm_sdw_priv,
			disable_int_mclk1_work);
	mutex_lock(&msm_sdw->cdc_int_mclk1_mutex);
	dev_dbg(msm_sdw->dev, "%s: mclk1_enabled %d mclk1_rsc_ref %d\n",
		__func__, msm_sdw->int_mclk1_enabled,
		msm_sdw->int_mclk1_rsc_ref);
	if (msm_sdw->int_mclk1_enabled == true
			&& msm_sdw->int_mclk1_rsc_ref == 0) {
		dev_dbg(msm_sdw->dev, "Disable the mclk1\n");
		msm_sdw->sdw_cdc_core_clk.enable = 0;
		ret = afe_set_lpass_clock_v2(
			AFE_PORT_ID_INT4_MI2S_RX,
			&msm_sdw->sdw_cdc_core_clk);
		if (ret < 0)
			dev_err(msm_sdw->dev,
				"%s failed to disable the MCLK1\n",
				__func__);
		msm_sdw->int_mclk1_enabled = false;
	}
	mutex_unlock(&msm_sdw->cdc_int_mclk1_mutex);
}

static int msm_int_mclk1_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct msm_sdw_priv *msm_sdw = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(msm_sdw->dev, "%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* enable the codec mclk config */
		msm_int_enable_sdw_cdc_clk(msm_sdw, 1, true);
		msm_sdw_mclk_enable(msm_sdw, 1, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* disable the codec mclk config */
		msm_sdw_mclk_enable(msm_sdw, 0, true);
		msm_int_enable_sdw_cdc_clk(msm_sdw, 0, true);
		break;
	default:
		dev_err(msm_sdw->dev,
			"%s: invalid DAPM event %d\n", __func__, event);
		ret = -EINVAL;
	}
	return ret;
}

static int msm_sdw_ahb_write_device(struct msm_sdw_priv *msm_sdw,
					u16 reg, u8 *value)
{
	u32 temp = (u32)(*value) & 0x000000FF;

	if (!msm_sdw->dev_up) {
		dev_err_ratelimited(msm_sdw->dev, "%s: q6 not ready\n",
				    __func__);
		return 0;
	}

	iowrite32(temp, msm_sdw->sdw_base + reg);
	return 0;
}

static int msm_sdw_ahb_read_device(struct msm_sdw_priv *msm_sdw,
					u16 reg, u8 *value)
{
	u32 temp;

	if (!msm_sdw->dev_up) {
		dev_err_ratelimited(msm_sdw->dev, "%s: q6 not ready\n",
				    __func__);
		return 0;
	}

	temp = ioread32(msm_sdw->sdw_base + reg);
	*value = (u8)temp;
	return 0;
}

static int __msm_sdw_reg_read(struct msm_sdw_priv *msm_sdw, unsigned short reg,
			int bytes, void *dest)
{
	int ret = -EINVAL, i;
	u8 temp = 0;

	dev_dbg(msm_sdw->dev, "%s reg = %x\n", __func__, reg);
	mutex_lock(&msm_sdw->cdc_int_mclk1_mutex);
	if (msm_sdw->int_mclk1_enabled == false) {
		msm_sdw->sdw_cdc_core_clk.enable = 1;
		ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_INT4_MI2S_RX,
					&msm_sdw->sdw_cdc_core_clk);
		if (ret < 0) {
			dev_err(msm_sdw->dev,
				"%s:failed to enable the INT_MCLK1\n",
				__func__);
			goto unlock_exit;
		}
		dev_dbg(msm_sdw->dev, "%s:enabled sdw codec core clk\n",
			__func__);
		for (i = 0; i < bytes; i++)  {
			ret = msm_sdw_ahb_read_device(
				msm_sdw, reg + (4 * i), &temp);
			((u8 *)dest)[i] = temp;
		}
		msm_sdw->int_mclk1_enabled = true;
		schedule_delayed_work(&msm_sdw->disable_int_mclk1_work, 50);
		goto unlock_exit;
	}
	for (i = 0; i < bytes; i++)  {
		ret = msm_sdw_ahb_read_device(
			msm_sdw, reg + (4 * i), &temp);
		((u8 *)dest)[i] = temp;
	}
unlock_exit:
	mutex_unlock(&msm_sdw->cdc_int_mclk1_mutex);
	if (ret < 0) {
		dev_err_ratelimited(msm_sdw->dev,
				    "%s: codec read failed for reg 0x%x\n",
				    __func__, reg);
		return ret;
	}
	dev_dbg(msm_sdw->dev, "Read 0x%02x from 0x%x\n", temp, reg);

	return 0;
}

static int __msm_sdw_reg_write(struct msm_sdw_priv *msm_sdw, unsigned short reg,
			       int bytes, void *src)
{
	int ret = -EINVAL, i;

	mutex_lock(&msm_sdw->cdc_int_mclk1_mutex);
	if (msm_sdw->int_mclk1_enabled == false) {
		msm_sdw->sdw_cdc_core_clk.enable = 1;
		ret = afe_set_lpass_clock_v2(AFE_PORT_ID_INT4_MI2S_RX,
					     &msm_sdw->sdw_cdc_core_clk);
		if (ret < 0) {
			dev_err(msm_sdw->dev,
				"%s: failed to enable the INT_MCLK1\n",
				__func__);
			ret = 0;
			goto unlock_exit;
		}
		dev_dbg(msm_sdw->dev, "%s: enabled INT_MCLK1\n", __func__);
		for (i = 0; i < bytes; i++)
			ret = msm_sdw_ahb_write_device(msm_sdw, reg + (4 * i),
						       &((u8 *)src)[i]);
		msm_sdw->int_mclk1_enabled = true;
		schedule_delayed_work(&msm_sdw->disable_int_mclk1_work, 50);
		goto unlock_exit;
	}
	for (i = 0; i < bytes; i++)
		ret = msm_sdw_ahb_write_device(msm_sdw, reg + (4 * i),
					       &((u8 *)src)[i]);
unlock_exit:
	mutex_unlock(&msm_sdw->cdc_int_mclk1_mutex);
	dev_dbg(msm_sdw->dev, "Write 0x%x val 0x%02x\n",
				reg, (u32)(*(u32 *)src));

	return ret;
}

static int msm_sdw_codec_enable_vi_feedback(struct snd_soc_dapm_widget *w,
					    struct snd_kcontrol *kcontrol,
					    int event)
{
	struct snd_soc_codec *codec = NULL;
	struct msm_sdw_priv *msm_sdw_p = NULL;
	int ret = 0;

	if (!w) {
		pr_err("%s invalid params\n", __func__);
		return -EINVAL;
	}
	codec = snd_soc_dapm_to_codec(w->dapm);
	msm_sdw_p = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: num_dai %d stream name %s\n",
		__func__, codec->component.num_dai, w->sname);

	dev_dbg(codec->dev, "%s(): w->name %s event %d w->shift %d\n",
		__func__, w->name, event, w->shift);
	if (w->shift != AIF1_SDW_VIFEED) {
		dev_err(codec->dev,
			"%s:Error in enabling the vi feedback path\n",
			__func__);
		ret = -EINVAL;
		goto out_vi;
	}
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (test_bit(VI_SENSE_1, &msm_sdw_p->status_mask)) {
			dev_dbg(codec->dev, "%s: spkr1 enabled\n", __func__);
			/* Enable V&I sensing */
			snd_soc_update_bits(codec,
				MSM_SDW_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x20);
			snd_soc_update_bits(codec,
				MSM_SDW_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				MSM_SDW_TX9_SPKR_PROT_PATH_CTL, 0x0F, 0x04);
			snd_soc_update_bits(codec,
				MSM_SDW_TX10_SPKR_PROT_PATH_CTL, 0x0F, 0x04);
			snd_soc_update_bits(codec,
				MSM_SDW_TX9_SPKR_PROT_PATH_CTL, 0x10, 0x10);
			snd_soc_update_bits(codec,
				MSM_SDW_TX10_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				MSM_SDW_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x00);
			snd_soc_update_bits(codec,
				MSM_SDW_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
		}
		if (test_bit(VI_SENSE_2, &msm_sdw_p->status_mask)) {
			dev_dbg(codec->dev, "%s: spkr2 enabled\n", __func__);
			/* Enable V&I sensing */
			snd_soc_update_bits(codec,
				MSM_SDW_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				MSM_SDW_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				MSM_SDW_TX11_SPKR_PROT_PATH_CTL, 0x0F,
				0x04);
			snd_soc_update_bits(codec,
				MSM_SDW_TX12_SPKR_PROT_PATH_CTL, 0x0F,
				0x04);
			snd_soc_update_bits(codec,
				MSM_SDW_TX11_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				MSM_SDW_TX12_SPKR_PROT_PATH_CTL, 0x10,
				0x10);
			snd_soc_update_bits(codec,
				MSM_SDW_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
			snd_soc_update_bits(codec,
				MSM_SDW_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x00);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (test_bit(VI_SENSE_1, &msm_sdw_p->status_mask)) {
			/* Disable V&I sensing */
			dev_dbg(codec->dev, "%s: spkr1 disabled\n", __func__);
			snd_soc_update_bits(codec,
				MSM_SDW_TX9_SPKR_PROT_PATH_CTL, 0x20, 0x20);
			snd_soc_update_bits(codec,
				MSM_SDW_TX10_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				MSM_SDW_TX9_SPKR_PROT_PATH_CTL, 0x10, 0x00);
			snd_soc_update_bits(codec,
				MSM_SDW_TX10_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
		}
		if (test_bit(VI_SENSE_2, &msm_sdw_p->status_mask)) {
			/* Disable V&I sensing */
			dev_dbg(codec->dev, "%s: spkr2 disabled\n", __func__);
			snd_soc_update_bits(codec,
				MSM_SDW_TX11_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				MSM_SDW_TX12_SPKR_PROT_PATH_CTL, 0x20,
				0x20);
			snd_soc_update_bits(codec,
				MSM_SDW_TX11_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
			snd_soc_update_bits(codec,
				MSM_SDW_TX12_SPKR_PROT_PATH_CTL, 0x10,
				0x00);
		}
		break;
	}
out_vi:
	return ret;
}

static int msm_sdwm_handle_irq(void *handle,
			       irqreturn_t (*swrm_irq_handler)(int irq,
							       void *data),
			       void *swrm_handle,
			       int action)
{
	struct msm_sdw_priv *msm_sdw;
	int ret = 0;

	if (!handle) {
		pr_err("%s: null handle received\n", __func__);
		return -EINVAL;
	}
	msm_sdw = (struct msm_sdw_priv *) handle;

	if (skip_irq)
		return ret;

	if (action) {
		ret = request_threaded_irq(msm_sdw->sdw_irq, NULL,
					   swrm_irq_handler,
					   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					   "swr_master_irq", swrm_handle);
		if (ret)
			dev_err(msm_sdw->dev, "%s: Failed to request irq %d\n",
				__func__, ret);
	} else
		free_irq(msm_sdw->sdw_irq, swrm_handle);

	return ret;
}

static void msm_sdw_codec_hd2_control(struct snd_soc_codec *codec,
				      u16 reg, int event)
{
	u16 hd2_scale_reg;
	u16 hd2_enable_reg = 0;

	if (reg == MSM_SDW_RX7_RX_PATH_CTL) {
		hd2_scale_reg = MSM_SDW_RX7_RX_PATH_SEC3;
		hd2_enable_reg = MSM_SDW_RX7_RX_PATH_CFG0;
	}
	if (reg == MSM_SDW_RX8_RX_PATH_CTL) {
		hd2_scale_reg = MSM_SDW_RX8_RX_PATH_SEC3;
		hd2_enable_reg = MSM_SDW_RX8_RX_PATH_CFG0;
	}

	if (hd2_enable_reg && SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_update_bits(codec, hd2_scale_reg, 0x3C, 0x10);
		snd_soc_update_bits(codec, hd2_scale_reg, 0x03, 0x01);
		snd_soc_update_bits(codec, hd2_enable_reg, 0x04, 0x04);
	}

	if (hd2_enable_reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, hd2_enable_reg, 0x04, 0x00);
		snd_soc_update_bits(codec, hd2_scale_reg, 0x03, 0x00);
		snd_soc_update_bits(codec, hd2_scale_reg, 0x3C, 0x00);
	}
}

static int msm_sdw_enable_swr(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct msm_sdw_priv *msm_sdw;
	int i, ch_cnt;

	msm_sdw = snd_soc_codec_get_drvdata(codec);

	if (!msm_sdw->nr)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!(strnstr(w->name, "RX4", sizeof("RX4 MIX"))) &&
		    !msm_sdw->rx_4_count)
			msm_sdw->rx_4_count++;
		if (!(strnstr(w->name, "RX5", sizeof("RX5 MIX"))) &&
		    !msm_sdw->rx_5_count)
			msm_sdw->rx_5_count++;
		ch_cnt = msm_sdw->rx_4_count + msm_sdw->rx_5_count;

		for (i = 0; i < msm_sdw->nr; i++) {
			swrm_wcd_notify(msm_sdw->sdw_ctrl_data[i].sdw_pdev,
					SWR_DEVICE_UP, NULL);
			swrm_wcd_notify(msm_sdw->sdw_ctrl_data[i].sdw_pdev,
					SWR_SET_NUM_RX_CH, &ch_cnt);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!(strnstr(w->name, "RX4", sizeof("RX4 MIX"))) &&
		    msm_sdw->rx_4_count)
			msm_sdw->rx_4_count--;
		if (!(strnstr(w->name, "RX5", sizeof("RX5 MIX"))) &&
		    msm_sdw->rx_5_count)
			msm_sdw->rx_5_count--;
		ch_cnt = msm_sdw->rx_4_count + msm_sdw->rx_5_count;

		for (i = 0; i < msm_sdw->nr; i++)
			swrm_wcd_notify(msm_sdw->sdw_ctrl_data[i].sdw_pdev,
					SWR_SET_NUM_RX_CH, &ch_cnt);
		break;
	}
	dev_dbg(msm_sdw->dev, "%s: current swr ch cnt: %d\n",
		__func__, msm_sdw->rx_4_count + msm_sdw->rx_5_count);

	return 0;
}

static int msm_sdw_codec_enable_interpolator(struct snd_soc_dapm_widget *w,
					     struct snd_kcontrol *kcontrol,
					     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct msm_sdw_priv *msm_sdw = snd_soc_codec_get_drvdata(codec);
	u16 gain_reg;
	u16 reg;
	int val;
	int offset_val = 0;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	if (!(strcmp(w->name, "RX INT4 INTERP"))) {
		reg = MSM_SDW_RX7_RX_PATH_CTL;
		gain_reg = MSM_SDW_RX7_RX_VOL_CTL;
	} else if (!(strcmp(w->name, "RX INT5 INTERP"))) {
		reg = MSM_SDW_RX8_RX_PATH_CTL;
		gain_reg = MSM_SDW_RX8_RX_VOL_CTL;
	} else {
		dev_err(codec->dev, "%s: Interpolator reg not found\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, reg, 0x10, 0x10);
		msm_sdw_codec_hd2_control(codec, reg, event);
		snd_soc_update_bits(codec, reg, 1 << 0x5, 1 << 0x5);
		break;
	case SND_SOC_DAPM_POST_PMU:
		msm_sdw_config_compander(codec, w->shift, event);
		/* apply gain after int clk is enabled */
		if ((msm_sdw->spkr_gain_offset == RX_GAIN_OFFSET_M1P5_DB) &&
		    (msm_sdw->comp_enabled[COMP1] ||
		     msm_sdw->comp_enabled[COMP2]) &&
		    (gain_reg == MSM_SDW_RX7_RX_VOL_CTL ||
		     gain_reg == MSM_SDW_RX8_RX_VOL_CTL)) {
			snd_soc_update_bits(codec, MSM_SDW_RX7_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    MSM_SDW_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			snd_soc_update_bits(codec, MSM_SDW_RX8_RX_PATH_SEC1,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    MSM_SDW_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x01);
			offset_val = -2;
		}
		val = snd_soc_read(codec, gain_reg);
		val += offset_val;
		snd_soc_write(codec, gain_reg, val);
		msm_sdw_config_ear_spkr_gain(codec, event, gain_reg);
		snd_soc_update_bits(codec, reg, 0x10, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, reg, 1 << 0x5, 0 << 0x5);
		snd_soc_update_bits(codec, reg,	0x40, 0x40);
		snd_soc_update_bits(codec, reg,	0x40, 0x00);
		msm_sdw_codec_hd2_control(codec, reg, event);
		msm_sdw_config_compander(codec, w->shift, event);
		if ((msm_sdw->spkr_gain_offset == RX_GAIN_OFFSET_M1P5_DB) &&
		    (msm_sdw->comp_enabled[COMP1] ||
		     msm_sdw->comp_enabled[COMP2]) &&
		    (gain_reg == MSM_SDW_RX7_RX_VOL_CTL ||
		     gain_reg == MSM_SDW_RX8_RX_VOL_CTL)) {
			snd_soc_update_bits(codec, MSM_SDW_RX7_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    MSM_SDW_RX7_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			snd_soc_update_bits(codec, MSM_SDW_RX8_RX_PATH_SEC1,
					    0x01, 0x00);
			snd_soc_update_bits(codec,
					    MSM_SDW_RX8_RX_PATH_MIX_SEC0,
					    0x01, 0x00);
			offset_val = 2;
			val = snd_soc_read(codec, gain_reg);
			val += offset_val;
			snd_soc_write(codec, gain_reg, val);
		}
		msm_sdw_config_ear_spkr_gain(codec, event, gain_reg);
		break;
	};

	return 0;
}

static int msm_sdw_config_ear_spkr_gain(struct snd_soc_codec *codec,
					int event, int gain_reg)
{
	int comp_gain_offset, val;
	struct msm_sdw_priv *msm_sdw = snd_soc_codec_get_drvdata(codec);

	switch (msm_sdw->spkr_mode) {
	/* Compander gain in SPKR_MODE1 case is 12 dB */
	case SPKR_MODE_1:
		comp_gain_offset = -12;
		break;
	/* Default case compander gain is 15 dB */
	default:
		comp_gain_offset = -15;
		break;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* Apply ear spkr gain only if compander is enabled */
		if (msm_sdw->comp_enabled[COMP1] &&
		    (gain_reg == MSM_SDW_RX7_RX_VOL_CTL) &&
		    (msm_sdw->ear_spkr_gain != 0)) {
			/* For example, val is -8(-12+5-1) for 4dB of gain */
			val = comp_gain_offset + msm_sdw->ear_spkr_gain - 1;
			snd_soc_write(codec, gain_reg, val);

			dev_dbg(codec->dev, "%s: RX4 Volume %d dB\n",
				__func__, val);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * Reset RX4 volume to 0 dB if compander is enabled and
		 * ear_spkr_gain is non-zero.
		 */
		if (msm_sdw->comp_enabled[COMP1] &&
		    (gain_reg == MSM_SDW_RX7_RX_VOL_CTL) &&
		    (msm_sdw->ear_spkr_gain != 0)) {
			snd_soc_write(codec, gain_reg, 0x0);

			dev_dbg(codec->dev, "%s: Reset RX4 Volume to 0 dB\n",
				__func__);
		}
		break;
	}

	return 0;
}

static int msm_sdw_codec_spk_boost_event(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 boost_path_ctl, boost_path_cfg1;
	u16 reg;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	if (!strcmp(w->name, "RX INT4 CHAIN")) {
		boost_path_ctl = MSM_SDW_BOOST0_BOOST_PATH_CTL;
		boost_path_cfg1 = MSM_SDW_RX7_RX_PATH_CFG1;
		reg = MSM_SDW_RX7_RX_PATH_CTL;
	} else if (!strcmp(w->name, "RX INT5 CHAIN")) {
		boost_path_ctl = MSM_SDW_BOOST1_BOOST_PATH_CTL;
		boost_path_cfg1 = MSM_SDW_RX8_RX_PATH_CFG1;
		reg = MSM_SDW_RX8_RX_PATH_CTL;
	} else {
		dev_err(codec->dev, "%s: boost reg not found\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, boost_path_ctl, 0x10, 0x10);
		snd_soc_update_bits(codec, boost_path_cfg1, 0x01, 0x01);
		snd_soc_update_bits(codec, reg, 0x10, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, boost_path_cfg1, 0x01, 0x00);
		snd_soc_update_bits(codec, boost_path_ctl, 0x10, 0x00);
		break;
	};

	return 0;
}

static int msm_sdw_config_compander(struct snd_soc_codec *codec, int comp,
				    int event)
{
	struct msm_sdw_priv *msm_sdw = snd_soc_codec_get_drvdata(codec);
	u16 comp_ctl0_reg, rx_path_cfg0_reg;

	if (comp < COMP1 || comp >= COMP_MAX)
		return 0;

	dev_dbg(codec->dev, "%s: event %d compander %d, enabled %d\n",
		__func__, event, comp + 1, msm_sdw->comp_enabled[comp]);

	if (!msm_sdw->comp_enabled[comp])
		return 0;

	comp_ctl0_reg = MSM_SDW_COMPANDER7_CTL0 + (comp * 0x20);
	rx_path_cfg0_reg = MSM_SDW_RX7_RX_PATH_CFG0 + (comp * 0x1E0);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Enable Compander Clock */
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x01, 0x01);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, rx_path_cfg0_reg, 0x02, 0x02);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x04, 0x04);
		snd_soc_update_bits(codec, rx_path_cfg0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x01, 0x00);
		snd_soc_update_bits(codec, comp_ctl0_reg, 0x04, 0x00);
	}

	return 0;
}

static int msm_sdw_get_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	struct msm_sdw_priv *msm_sdw = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = msm_sdw->comp_enabled[comp];
	return 0;
}

static int msm_sdw_set_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_sdw_priv *msm_sdw = snd_soc_codec_get_drvdata(codec);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: Compander %d enable current %d, new %d\n",
		__func__, comp + 1, msm_sdw->comp_enabled[comp], value);
	msm_sdw->comp_enabled[comp] = value;

	return 0;
}

static int msm_sdw_ear_spkr_pa_gain_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_sdw_priv *msm_sdw = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = msm_sdw->ear_spkr_gain;

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_sdw_ear_spkr_pa_gain_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_sdw_priv *msm_sdw = snd_soc_codec_get_drvdata(codec);

	msm_sdw->ear_spkr_gain =  ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: gain = %d\n", __func__,
		msm_sdw->ear_spkr_gain);

	return 0;
}

static int msm_sdw_vi_feed_mixer_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct msm_sdw_priv *msm_sdw_p = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = msm_sdw_p->vi_feed_value;

	return 0;
}

static int msm_sdw_vi_feed_mixer_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
					dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(widget->dapm);
	struct msm_sdw_priv *msm_sdw_p = snd_soc_codec_get_drvdata(codec);
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 port_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: enable: %d, port_id:%d, dai_id: %d\n",
		__func__, enable, port_id, dai_id);

	msm_sdw_p->vi_feed_value = ucontrol->value.integer.value[0];

	mutex_lock(&msm_sdw_p->codec_mutex);
	if (enable) {
		if (port_id == MSM_SDW_TX0 && !test_bit(VI_SENSE_1,
						&msm_sdw_p->status_mask))
			set_bit(VI_SENSE_1, &msm_sdw_p->status_mask);
		if (port_id == MSM_SDW_TX1 && !test_bit(VI_SENSE_2,
						&msm_sdw_p->status_mask))
			set_bit(VI_SENSE_2, &msm_sdw_p->status_mask);
	} else {
		if (port_id == MSM_SDW_TX0 && test_bit(VI_SENSE_1,
					&msm_sdw_p->status_mask))
			clear_bit(VI_SENSE_1, &msm_sdw_p->status_mask);
		if (port_id == MSM_SDW_TX1 && test_bit(VI_SENSE_2,
					&msm_sdw_p->status_mask))
			clear_bit(VI_SENSE_2, &msm_sdw_p->status_mask);
	}
	mutex_unlock(&msm_sdw_p->codec_mutex);
	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, NULL);

	return 0;
}

static int msm_sdw_mclk_enable(struct msm_sdw_priv *msm_sdw,
			       int mclk_enable, bool dapm)
{
	dev_dbg(msm_sdw->dev, "%s: mclk_enable = %u, dapm = %d clk_users= %d\n",
		__func__, mclk_enable, dapm, msm_sdw->sdw_mclk_users);
	if (mclk_enable) {
		msm_sdw->sdw_mclk_users++;
		if (msm_sdw->sdw_mclk_users == 1) {
			regmap_update_bits(msm_sdw->regmap,
					MSM_SDW_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x01, 0x01);
			regmap_update_bits(msm_sdw->regmap,
				MSM_SDW_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x01);
			/* 9.6MHz MCLK, set value 0x00 if other frequency */
			regmap_update_bits(msm_sdw->regmap,
				MSM_SDW_TOP_FREQ_MCLK, 0x01, 0x01);
		}
	} else {
		msm_sdw->sdw_mclk_users--;
		if (msm_sdw->sdw_mclk_users == 0) {
			regmap_update_bits(msm_sdw->regmap,
					MSM_SDW_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x01, 0x00);
			regmap_update_bits(msm_sdw->regmap,
					MSM_SDW_CLK_RST_CTRL_MCLK_CONTROL,
					0x01, 0x00);
		}
	}
	return 0;
}
EXPORT_SYMBOL(msm_sdw_mclk_enable);

static int msm_sdw_swrm_read(void *handle, int reg)
{
	struct msm_sdw_priv *msm_sdw;
	unsigned short sdw_rd_addr_base;
	unsigned short sdw_rd_data_base;
	int val, ret;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	msm_sdw = (struct msm_sdw_priv *)handle;

	dev_dbg(msm_sdw->dev, "%s: Reading soundwire register, 0x%x\n",
		__func__, reg);
	sdw_rd_addr_base = MSM_SDW_AHB_BRIDGE_RD_ADDR_0;
	sdw_rd_data_base = MSM_SDW_AHB_BRIDGE_RD_DATA_0;

	/*
	 * Add sleep as SWR slave access read takes time.
	 * Allow for RD_DONE to complete for previous register if any.
	 */
	usleep_range(100, 105);

	/* read_lock */
	mutex_lock(&msm_sdw->sdw_read_lock);
	ret = regmap_bulk_write(msm_sdw->regmap, sdw_rd_addr_base,
				(u8 *)&reg, 4);
	if (ret < 0) {
		dev_err(msm_sdw->dev, "%s: RD Addr Failure\n", __func__);
		goto err;
	}
	/* Check for RD value */
	ret = regmap_bulk_read(msm_sdw->regmap, sdw_rd_data_base,
			       (u8 *)&val, 4);
	if (ret < 0) {
		dev_err(msm_sdw->dev, "%s: RD Data Failure\n", __func__);
		goto err;
	}
	ret = val;
err:
	/* read_unlock */
	mutex_unlock(&msm_sdw->sdw_read_lock);
	return ret;
}

static int msm_sdw_bulk_write(struct msm_sdw_priv *msm_sdw,
				struct msm_sdw_reg_val *bulk_reg,
				size_t len)
{
	int i, ret = 0;
	unsigned short sdw_wr_addr_base;
	unsigned short sdw_wr_data_base;

	sdw_wr_addr_base = MSM_SDW_AHB_BRIDGE_WR_ADDR_0;
	sdw_wr_data_base = MSM_SDW_AHB_BRIDGE_WR_DATA_0;

	/*
	 * Add sleep as SWR slave write takes time.
	 * Allow for any previous pending write to complete.
	 */
	usleep_range(50, 55);
	for (i = 0; i < len; i += 2) {
		/* First Write the Data to register */
		ret = regmap_bulk_write(msm_sdw->regmap,
			sdw_wr_data_base, bulk_reg[i].buf, 4);
		if (ret < 0) {
			dev_err(msm_sdw->dev, "%s: WR Data Failure\n",
				__func__);
			break;
		}
		/* Next Write Address */
		ret = regmap_bulk_write(msm_sdw->regmap,
			sdw_wr_addr_base, bulk_reg[i+1].buf, 4);
		if (ret < 0) {
			dev_err(msm_sdw->dev,
				"%s: WR Addr Failure: 0x%x\n",
				__func__, (u32)(bulk_reg[i+1].buf[0]));
			break;
		}
	}
	return ret;
}

static int msm_sdw_swrm_bulk_write(void *handle, u32 *reg, u32 *val, size_t len)
{
	struct msm_sdw_priv *msm_sdw;
	struct msm_sdw_reg_val *bulk_reg;
	unsigned short sdw_wr_addr_base;
	unsigned short sdw_wr_data_base;
	int i, j, ret;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}

	msm_sdw = (struct msm_sdw_priv *)handle;
	if (len <= 0) {
		dev_err(msm_sdw->dev,
			"%s: Invalid size: %zu\n", __func__, len);
		return -EINVAL;
	}

	sdw_wr_addr_base = MSM_SDW_AHB_BRIDGE_WR_ADDR_0;
	sdw_wr_data_base = MSM_SDW_AHB_BRIDGE_WR_DATA_0;

	bulk_reg = kzalloc((2 * len * sizeof(struct msm_sdw_reg_val)),
			   GFP_KERNEL);
	if (!bulk_reg)
		return -ENOMEM;

	for (i = 0, j = 0; i < (len * 2); i += 2, j++) {
		bulk_reg[i].reg = sdw_wr_data_base;
		bulk_reg[i].buf = (u8 *)(&val[j]);
		bulk_reg[i].bytes = 4;
		bulk_reg[i+1].reg = sdw_wr_addr_base;
		bulk_reg[i+1].buf = (u8 *)(&reg[j]);
		bulk_reg[i+1].bytes = 4;
	}
	mutex_lock(&msm_sdw->sdw_write_lock);

	ret = msm_sdw_bulk_write(msm_sdw, bulk_reg, (len * 2));
	if (ret)
		dev_err(msm_sdw->dev, "%s: swrm bulk write failed, ret: %d\n",
			__func__, ret);

	mutex_unlock(&msm_sdw->sdw_write_lock);
	kfree(bulk_reg);

	return ret;
}

static int msm_sdw_swrm_write(void *handle, int reg, int val)
{
	struct msm_sdw_priv *msm_sdw;
	unsigned short sdw_wr_addr_base;
	unsigned short sdw_wr_data_base;
	struct msm_sdw_reg_val bulk_reg[2];
	int ret;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	msm_sdw = (struct msm_sdw_priv *)handle;

	sdw_wr_addr_base = MSM_SDW_AHB_BRIDGE_WR_ADDR_0;
	sdw_wr_data_base = MSM_SDW_AHB_BRIDGE_WR_DATA_0;

	/* First Write the Data to register */
	bulk_reg[0].reg = sdw_wr_data_base;
	bulk_reg[0].buf = (u8 *)(&val);
	bulk_reg[0].bytes = 4;
	bulk_reg[1].reg = sdw_wr_addr_base;
	bulk_reg[1].buf = (u8 *)(&reg);
	bulk_reg[1].bytes = 4;

	mutex_lock(&msm_sdw->sdw_write_lock);

	ret = msm_sdw_bulk_write(msm_sdw, bulk_reg, 2);
	if (ret < 0)
		dev_err(msm_sdw->dev, "%s: WR Data Failure\n", __func__);

	mutex_unlock(&msm_sdw->sdw_write_lock);
	return ret;
}

static int msm_sdw_swrm_clock(void *handle, bool enable)
{
	struct msm_sdw_priv *msm_sdw = (struct msm_sdw_priv *) handle;

	mutex_lock(&msm_sdw->sdw_clk_lock);

	dev_dbg(msm_sdw->dev, "%s: swrm clock %s\n",
		__func__, (enable ? "enable" : "disable"));
	if (enable) {
		msm_sdw->sdw_clk_users++;
		if (msm_sdw->sdw_clk_users == 1) {
			msm_int_enable_sdw_cdc_clk(msm_sdw, 1, true);
			msm_sdw_mclk_enable(msm_sdw, 1, true);
			regmap_update_bits(msm_sdw->regmap,
				MSM_SDW_CLK_RST_CTRL_SWR_CONTROL, 0x01, 0x01);
			msm_enable_sdw_npl_clk(msm_sdw, true);
			msm_cdc_pinctrl_select_active_state(
							msm_sdw->sdw_gpio_p);
		}
	} else {
		msm_sdw->sdw_clk_users--;
		if (msm_sdw->sdw_clk_users == 0) {
			regmap_update_bits(msm_sdw->regmap,
				MSM_SDW_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x00);
			msm_sdw_mclk_enable(msm_sdw, 0, true);
			msm_int_enable_sdw_cdc_clk(msm_sdw, 0, true);
			msm_enable_sdw_npl_clk(msm_sdw, false);
			msm_cdc_pinctrl_select_sleep_state(msm_sdw->sdw_gpio_p);
		}
	}
	dev_dbg(msm_sdw->dev, "%s: swrm clock users %d\n",
		__func__, msm_sdw->sdw_clk_users);
	mutex_unlock(&msm_sdw->sdw_clk_lock);
	return 0;
}

static int msm_sdw_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	dev_dbg(dai->codec->dev, "%s(): substream = %s  stream = %d\n",
		__func__,
		substream->name, substream->stream);
	return 0;
}

static int msm_sdw_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	u8 clk_fs_rate, fs_rate;

	dev_dbg(dai->codec->dev,
		"%s: dai_name = %s DAI-ID %x rate %d num_ch %d format %d\n",
		__func__, dai->name, dai->id, params_rate(params),
		params_channels(params), params_format(params));

	switch (params_rate(params)) {
	case 8000:
		clk_fs_rate = 0x00;
		fs_rate = 0x00;
		break;
	case 16000:
		clk_fs_rate = 0x01;
		fs_rate = 0x01;
		break;
	case 32000:
		clk_fs_rate = 0x02;
		fs_rate = 0x03;
		break;
	case 48000:
		clk_fs_rate = 0x03;
		fs_rate = 0x04;
		break;
	case 96000:
		clk_fs_rate = 0x04;
		fs_rate = 0x05;
		break;
	case 192000:
		clk_fs_rate = 0x05;
		fs_rate = 0x06;
		break;
	default:
		dev_err(dai->codec->dev,
			"%s: Invalid sampling rate %d\n", __func__,
			params_rate(params));
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		snd_soc_update_bits(dai->codec,
				MSM_SDW_TOP_TX_I2S_CTL, 0x1C,
				(clk_fs_rate << 2));
	} else {
		snd_soc_update_bits(dai->codec,
				MSM_SDW_TOP_RX_I2S_CTL, 0x1C,
				(clk_fs_rate << 2));
		snd_soc_update_bits(dai->codec,
				MSM_SDW_RX7_RX_PATH_CTL, 0x0F,
				fs_rate);
		snd_soc_update_bits(dai->codec,
				MSM_SDW_RX8_RX_PATH_CTL, 0x0F,
				fs_rate);
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			snd_soc_update_bits(dai->codec,
					MSM_SDW_TOP_TX_I2S_CTL, 0x20, 0x20);
		else
			snd_soc_update_bits(dai->codec,
					MSM_SDW_TOP_RX_I2S_CTL, 0x20, 0x20);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			snd_soc_update_bits(dai->codec,
					MSM_SDW_TOP_TX_I2S_CTL, 0x20, 0x00);
		else
			snd_soc_update_bits(dai->codec,
					MSM_SDW_TOP_RX_I2S_CTL, 0x20, 0x00);
		break;
	default:
		dev_err(dai->codec->dev, "%s: wrong format selected\n",
				__func__);
		return -EINVAL;
	}

	return 0;
}

static void msm_sdw_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	dev_dbg(dai->codec->dev,
		"%s(): substream = %s  stream = %d\n", __func__,
		substream->name, substream->stream);
}

static ssize_t msm_sdw_codec_version_read(struct snd_info_entry *entry,
					  void *file_private_data,
					  struct file *file,
					  char __user *buf, size_t count,
					  loff_t pos)
{
	struct msm_sdw_priv *msm_sdw;
	char buffer[MSM_SDW_VERSION_ENTRY_SIZE];
	int len = 0;

	msm_sdw = (struct msm_sdw_priv *) entry->private_data;
	if (!msm_sdw) {
		pr_err("%s: msm_sdw priv is null\n", __func__);
		return -EINVAL;
	}

	switch (msm_sdw->version) {
	case MSM_SDW_VERSION_1_0:
	    len = snprintf(buffer, sizeof(buffer), "SDW-CDC_1_0\n");
	    break;
	default:
	    len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops msm_sdw_codec_info_ops = {
	.read = msm_sdw_codec_version_read,
};

/*
 * msm_sdw_codec_info_create_codec_entry - creates msm_sdw module
 * @codec_root: The parent directory
 * @codec: Codec instance
 *
 * Creates msm_sdw module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int msm_sdw_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					  struct snd_soc_codec *codec)
{
	struct snd_info_entry *version_entry;
	struct msm_sdw_priv *msm_sdw;
	struct snd_soc_card *card;

	if (!codec_root || !codec)
		return -EINVAL;

	msm_sdw = snd_soc_codec_get_drvdata(codec);
	card = codec->component.card;
	msm_sdw->entry = snd_register_module_info(codec_root->module,
						  "152c1000.msm-sdw-codec",
						  codec_root);
	if (!msm_sdw->entry) {
		dev_err(codec->dev, "%s: failed to create msm_sdw entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   msm_sdw->entry);
	if (!version_entry) {
		dev_err(codec->dev, "%s: failed to create msm_sdw version entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry->private_data = msm_sdw;
	version_entry->size = MSM_SDW_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &msm_sdw_codec_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		return -ENOMEM;
	}
	msm_sdw->version_entry = version_entry;

	return 0;
}
EXPORT_SYMBOL(msm_sdw_codec_info_create_codec_entry);

static struct snd_soc_dai_ops msm_sdw_dai_ops = {
	.startup = msm_sdw_startup,
	.shutdown = msm_sdw_shutdown,
	.hw_params = msm_sdw_hw_params,
};

static struct snd_soc_dai_driver msm_sdw_dai[] = {
	{
		.name = "msm_sdw_i2s_rx1",
		.id = AIF1_SDW_PB,
		.playback = {
			.stream_name = "AIF1_SDW Playback",
			.rates = MSM_SDW_RATES,
			.formats = MSM_SDW_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &msm_sdw_dai_ops,
	},
	{
		.name = "msm_sdw_vifeedback",
		.id = AIF1_SDW_VIFEED,
		.capture = {
			.stream_name = "VIfeed_SDW",
			.rates = MSM_SDW_RATES,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 2,
			.channels_max = 4,
		},
		.ops = &msm_sdw_dai_ops,
	},
};

static const char * const rx_mix1_text[] = {
	"ZERO", "RX4", "RX5"
};

static const char * const msm_sdw_ear_spkr_pa_gain_text[] = {
	"G_DEFAULT", "G_0_DB", "G_1_DB", "G_2_DB", "G_3_DB",
	"G_4_DB", "G_5_DB", "G_6_DB"
};

static SOC_ENUM_SINGLE_EXT_DECL(msm_sdw_ear_spkr_pa_gain_enum,
				msm_sdw_ear_spkr_pa_gain_text);
/* RX4 MIX1 */
static const struct soc_enum rx4_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM_SDW_TOP_RX7_PATH_INPUT0_MUX,
		0, 3, rx_mix1_text);

static const struct soc_enum rx4_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM_SDW_TOP_RX7_PATH_INPUT1_MUX,
		0, 3, rx_mix1_text);

/* RX5 MIX1 */
static const struct soc_enum rx5_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM_SDW_TOP_RX8_PATH_INPUT0_MUX,
		0, 3, rx_mix1_text);

static const struct soc_enum rx5_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM_SDW_TOP_RX8_PATH_INPUT1_MUX,
		0, 3, rx_mix1_text);

static const struct snd_kcontrol_new rx4_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX4 MIX1 INP1 Mux", rx4_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx4_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX4 MIX1 INP2 Mux", rx4_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx5_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX5 MIX1 INP1 Mux", rx5_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx5_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX5 MIX1 INP2 Mux", rx5_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new aif1_vi_mixer[] = {
	SOC_SINGLE_EXT("SPKR_VI_1", SND_SOC_NOPM, MSM_SDW_TX0, 1, 0,
			msm_sdw_vi_feed_mixer_get, msm_sdw_vi_feed_mixer_put),
	SOC_SINGLE_EXT("SPKR_VI_2", SND_SOC_NOPM, MSM_SDW_TX1, 1, 0,
			msm_sdw_vi_feed_mixer_get, msm_sdw_vi_feed_mixer_put),
};

static const struct snd_soc_dapm_widget msm_sdw_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("I2S RX4", "AIF1_SDW Playback", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("I2S RX5", "AIF1_SDW Playback", 0,
		SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT_E("AIF1_SDW VI", "VIfeed_SDW", 0, SND_SOC_NOPM,
		AIF1_SDW_VIFEED, 0, msm_sdw_codec_enable_vi_feedback,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("AIF1_VI_SDW Mixer", SND_SOC_NOPM, AIF1_SDW_VIFEED,
		0, aif1_vi_mixer, ARRAY_SIZE(aif1_vi_mixer)),

	SND_SOC_DAPM_MUX_E("RX4 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx4_mix1_inp1_mux, msm_sdw_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX4 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx4_mix1_inp2_mux, msm_sdw_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX5 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx5_mix1_inp1_mux, msm_sdw_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("RX5 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx5_mix1_inp2_mux, msm_sdw_enable_swr,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("RX4 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX5 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER_E("RX INT4 INTERP", SND_SOC_NOPM,
		COMP1, 0, NULL, 0, msm_sdw_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX INT5 INTERP", SND_SOC_NOPM,
		COMP2, 0, NULL, 0, msm_sdw_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("RX INT4 CHAIN", SND_SOC_NOPM, 0, 0,
		NULL, 0, msm_sdw_codec_spk_boost_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX INT5 CHAIN", SND_SOC_NOPM, 0, 0,
		NULL, 0, msm_sdw_codec_spk_boost_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("VIINPUT_SDW"),

	SND_SOC_DAPM_OUTPUT("SPK1 OUT"),
	SND_SOC_DAPM_OUTPUT("SPK2 OUT"),

	SND_SOC_DAPM_SUPPLY_S("SDW_CONN", -1, MSM_SDW_TOP_I2S_CLK,
		0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY_S("INT_MCLK1", -2, SND_SOC_NOPM, 0, 0,
	msm_int_mclk1_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("SDW_RX_I2S_CLK",
		MSM_SDW_TOP_RX_I2S_CTL, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("SDW_TX_I2S_CLK",
		MSM_SDW_TOP_TX_I2S_CTL, 0, 0, NULL, 0),
};

static const struct snd_kcontrol_new msm_sdw_snd_controls[] = {
	SOC_ENUM_EXT("EAR SPKR PA Gain", msm_sdw_ear_spkr_pa_gain_enum,
		     msm_sdw_ear_spkr_pa_gain_get,
		     msm_sdw_ear_spkr_pa_gain_put),
	SOC_SINGLE_SX_TLV("RX4 Digital Volume", MSM_SDW_RX7_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX5 Digital Volume", MSM_SDW_RX8_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_EXT("COMP1 Switch", SND_SOC_NOPM, COMP1, 1, 0,
		msm_sdw_get_compander, msm_sdw_set_compander),
	SOC_SINGLE_EXT("COMP2 Switch", SND_SOC_NOPM, COMP2, 1, 0,
		msm_sdw_get_compander, msm_sdw_set_compander),
};

static const struct snd_soc_dapm_route audio_map[] = {

	{"AIF1_SDW VI", NULL, "SDW_TX_I2S_CLK"},
	{"SDW_TX_I2S_CLK", NULL, "INT_MCLK1"},
	{"SDW_TX_I2S_CLK", NULL, "SDW_CONN"},

	/* VI Feedback */
	{"AIF1_VI_SDW Mixer", "SPKR_VI_1", "VIINPUT_SDW"},
	{"AIF1_VI_SDW Mixer", "SPKR_VI_2", "VIINPUT_SDW"},
	{"AIF1_SDW VI", NULL, "AIF1_VI_SDW Mixer"},

	{"SDW_RX_I2S_CLK", NULL, "INT_MCLK1"},
	{"SDW_RX_I2S_CLK", NULL, "SDW_CONN"},
	{"I2S RX4", NULL, "SDW_RX_I2S_CLK"},
	{"I2S RX5", NULL, "SDW_RX_I2S_CLK"},

	{"RX4 MIX1 INP1", "RX4", "I2S RX4"},
	{"RX4 MIX1 INP1", "RX5", "I2S RX5"},
	{"RX4 MIX1 INP2", "RX4", "I2S RX4"},
	{"RX4 MIX1 INP2", "RX5", "I2S RX5"},
	{"RX5 MIX1 INP1", "RX4", "I2S RX4"},
	{"RX5 MIX1 INP1", "RX5", "I2S RX5"},
	{"RX5 MIX1 INP2", "RX4", "I2S RX4"},
	{"RX5 MIX1 INP2", "RX5", "I2S RX5"},

	{"RX4 MIX1", NULL, "RX4 MIX1 INP1"},
	{"RX4 MIX1", NULL, "RX4 MIX1 INP2"},
	{"RX5 MIX1", NULL, "RX5 MIX1 INP1"},
	{"RX5 MIX1", NULL, "RX5 MIX1 INP2"},

	{"RX INT4 INTERP", NULL, "RX4 MIX1"},
	{"RX INT4 CHAIN", NULL, "RX INT4 INTERP"},
	{"SPK1 OUT", NULL, "RX INT4 CHAIN"},

	{"RX INT5 INTERP", NULL, "RX5 MIX1"},
	{"RX INT5 CHAIN", NULL, "RX INT5 INTERP"},
	{"SPK2 OUT", NULL, "RX INT5 CHAIN"},
};

static const struct msm_sdw_reg_mask_val msm_sdw_reg_init[] = {
	{MSM_SDW_BOOST0_BOOST_CFG1, 0x3F, 0x12},
	{MSM_SDW_BOOST0_BOOST_CFG2, 0x1C, 0x08},
	{MSM_SDW_COMPANDER7_CTL7, 0x1E, 0x18},
	{MSM_SDW_BOOST1_BOOST_CFG1, 0x3F, 0x12},
	{MSM_SDW_BOOST1_BOOST_CFG2, 0x1C, 0x08},
	{MSM_SDW_COMPANDER8_CTL7, 0x1E, 0x18},
	{MSM_SDW_BOOST0_BOOST_CTL, 0x70, 0x50},
	{MSM_SDW_BOOST1_BOOST_CTL, 0x70, 0x50},
	{MSM_SDW_RX7_RX_PATH_CFG1, 0x08, 0x08},
	{MSM_SDW_RX8_RX_PATH_CFG1, 0x08, 0x08},
	{MSM_SDW_TOP_TOP_CFG1, 0x02, 0x02},
	{MSM_SDW_TOP_TOP_CFG1, 0x01, 0x01},
	{MSM_SDW_TX9_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{MSM_SDW_TX10_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{MSM_SDW_TX11_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{MSM_SDW_TX12_SPKR_PROT_PATH_CFG0, 0x01, 0x01},
	{MSM_SDW_COMPANDER7_CTL3, 0x80, 0x80},
	{MSM_SDW_COMPANDER8_CTL3, 0x80, 0x80},
	{MSM_SDW_COMPANDER7_CTL7, 0x01, 0x01},
	{MSM_SDW_COMPANDER8_CTL7, 0x01, 0x01},
	{MSM_SDW_RX7_RX_PATH_CFG0, 0x01, 0x01},
	{MSM_SDW_RX8_RX_PATH_CFG0, 0x01, 0x01},
	{MSM_SDW_RX7_RX_PATH_MIX_CFG, 0x01, 0x01},
	{MSM_SDW_RX8_RX_PATH_MIX_CFG, 0x01, 0x01},
};

static void msm_sdw_init_reg(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(msm_sdw_reg_init); i++)
		snd_soc_update_bits(codec,
				msm_sdw_reg_init[i].reg,
				msm_sdw_reg_init[i].mask,
				msm_sdw_reg_init[i].val);
}

static int msm_sdw_notifier_service_cb(struct notifier_block *nb,
				       unsigned long opcode, void *ptr)
{
	int i;
	struct msm_sdw_priv *msm_sdw = container_of(nb,
						    struct msm_sdw_priv,
						    service_nb);
	bool adsp_ready = false;
	unsigned long timeout;
	static bool initial_boot = true;

	pr_debug("%s: Service opcode 0x%lx\n", __func__, opcode);

	mutex_lock(&msm_sdw->codec_mutex);
	switch (opcode) {
	case AUDIO_NOTIFIER_SERVICE_DOWN:
		if (initial_boot)
			break;
		msm_sdw->int_mclk1_enabled = false;
		msm_sdw->dev_up = false;
		for (i = 0; i < msm_sdw->nr; i++)
			swrm_wcd_notify(msm_sdw->sdw_ctrl_data[i].sdw_pdev,
					SWR_DEVICE_DOWN, NULL);
		break;
	case AUDIO_NOTIFIER_SERVICE_UP:
		if (initial_boot)
			initial_boot = false;
		if (!q6core_is_adsp_ready()) {
			dev_dbg(msm_sdw->dev, "ADSP isn't ready\n");
			timeout = jiffies +
				  msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);
			while (!time_after(jiffies, timeout)) {
				if (!q6core_is_adsp_ready()) {
					dev_dbg(msm_sdw->dev,
						"ADSP isn't ready\n");
				} else {
					dev_dbg(msm_sdw->dev,
						"ADSP is ready\n");
					adsp_ready = true;
					goto powerup;
				}
			}
		} else {
			adsp_ready = true;
			dev_dbg(msm_sdw->dev, "%s: DSP is ready\n", __func__);
		}
powerup:
		if (adsp_ready) {
			msm_sdw->dev_up = true;
			msm_sdw_init_reg(msm_sdw->codec);
			regcache_mark_dirty(msm_sdw->regmap);
			regcache_sync(msm_sdw->regmap);
			msm_sdw_set_spkr_mode(msm_sdw->codec,
					      msm_sdw->spkr_mode);
		}
		break;
	default:
		break;
	}
	mutex_unlock(&msm_sdw->codec_mutex);
	return NOTIFY_OK;
}

static int msm_sdw_codec_probe(struct snd_soc_codec *codec)
{
	struct msm_sdw_priv *msm_sdw;
	int i, ret;

	msm_sdw = snd_soc_codec_get_drvdata(codec);
	if (!msm_sdw) {
		pr_err("%s:SDW priv data null\n", __func__);
		return -EINVAL;
	}
	msm_sdw->codec = codec;
	for (i = 0; i < COMP_MAX; i++)
		msm_sdw->comp_enabled[i] = 0;

	msm_sdw->spkr_gain_offset = RX_GAIN_OFFSET_0_DB;
	msm_sdw_init_reg(codec);
	msm_sdw->version = MSM_SDW_VERSION_1_0;

	msm_sdw->service_nb.notifier_call = msm_sdw_notifier_service_cb;
	ret = audio_notifier_register("msm_sdw",
				AUDIO_NOTIFIER_ADSP_DOMAIN,
				&msm_sdw->service_nb);
	if (ret < 0)
		dev_err(msm_sdw->dev,
			"%s: Audio notifier register failed ret = %d\n",
			__func__, ret);
	return 0;
}

static int msm_sdw_codec_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static struct regmap *msm_sdw_get_regmap(struct device *dev)
{
	struct msm_sdw_priv *msm_sdw = dev_get_drvdata(dev);

	return msm_sdw->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_msm_sdw = {
	.probe = msm_sdw_codec_probe,
	.remove = msm_sdw_codec_remove,
	.controls = msm_sdw_snd_controls,
	.num_controls = ARRAY_SIZE(msm_sdw_snd_controls),
	.dapm_widgets = msm_sdw_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(msm_sdw_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
	.get_regmap = msm_sdw_get_regmap,
};

static void msm_sdw_add_child_devices(struct work_struct *work)
{
	struct msm_sdw_priv *msm_sdw;
	struct platform_device *pdev;
	struct device_node *node;
	struct msm_sdw_ctrl_data *sdw_ctrl_data = NULL, *temp;
	int ret, ctrl_num = 0;
	struct wcd_sdw_ctrl_platform_data *platdata;
	char plat_dev_name[MSM_SDW_STRING_LEN];

	msm_sdw = container_of(work, struct msm_sdw_priv,
			     msm_sdw_add_child_devices_work);
	if (!msm_sdw) {
		pr_err("%s: Memory for msm_sdw does not exist\n",
			__func__);
		return;
	}
	if (!msm_sdw->dev->of_node) {
		dev_err(msm_sdw->dev,
			"%s: DT node for msm_sdw does not exist\n", __func__);
		return;
	}

	platdata = &msm_sdw->sdw_plat_data;

	for_each_available_child_of_node(msm_sdw->dev->of_node, node) {
		if (!strcmp(node->name, "swr_master"))
			strlcpy(plat_dev_name, "msm_sdw_swr_ctrl",
				(MSM_SDW_STRING_LEN - 1));
		else if (strnstr(node->name, "msm_cdc_pinctrl",
				 strlen("msm_cdc_pinctrl")) != NULL)
			strlcpy(plat_dev_name, node->name,
				(MSM_SDW_STRING_LEN - 1));
		else
			continue;

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(msm_sdw->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = msm_sdw->dev;
		pdev->dev.of_node = node;

		if (!strcmp(node->name, "swr_master")) {
			ret = platform_device_add_data(pdev, platdata,
						       sizeof(*platdata));
			if (ret) {
				dev_err(&pdev->dev,
					"%s: cannot add plat data ctrl:%d\n",
					__func__, ctrl_num);
				goto fail_pdev_add;
			}
		}

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			goto fail_pdev_add;
		}

		if (!strcmp(node->name, "swr_master")) {
			temp = krealloc(sdw_ctrl_data,
					(ctrl_num + 1) * sizeof(
					struct msm_sdw_ctrl_data),
					GFP_KERNEL);
			if (!temp) {
				dev_err(&pdev->dev, "out of memory\n");
				ret = -ENOMEM;
				goto err;
			}
			sdw_ctrl_data = temp;
			sdw_ctrl_data[ctrl_num].sdw_pdev = pdev;
			ctrl_num++;
			dev_dbg(&pdev->dev,
				"%s: Added soundwire ctrl device(s)\n",
				__func__);
			msm_sdw->nr = ctrl_num;
			msm_sdw->sdw_ctrl_data = sdw_ctrl_data;
		}
	}

	return;
fail_pdev_add:
	platform_device_put(pdev);
err:
	return;
}

static int msm_sdw_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_sdw_priv *msm_sdw;
	int adsp_state;

	adsp_state = apr_get_subsys_state();
	if (adsp_state != APR_SUBSYS_LOADED) {
		dev_err(&pdev->dev, "Adsp is not loaded yet %d\n",
				adsp_state);
		return -EPROBE_DEFER;
	}

	msm_sdw = devm_kzalloc(&pdev->dev, sizeof(struct msm_sdw_priv),
			    GFP_KERNEL);
	if (!msm_sdw)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, msm_sdw);
	msm_sdw->dev_up = true;

	msm_sdw->dev = &pdev->dev;
	INIT_WORK(&msm_sdw->msm_sdw_add_child_devices_work,
		  msm_sdw_add_child_devices);
	msm_sdw->sdw_plat_data.handle = (void *) msm_sdw;
	msm_sdw->sdw_plat_data.read = msm_sdw_swrm_read;
	msm_sdw->sdw_plat_data.write = msm_sdw_swrm_write;
	msm_sdw->sdw_plat_data.bulk_write = msm_sdw_swrm_bulk_write;
	msm_sdw->sdw_plat_data.clk = msm_sdw_swrm_clock;
	msm_sdw->sdw_plat_data.handle_irq = msm_sdwm_handle_irq;
	ret = of_property_read_u32(pdev->dev.of_node, "reg",
				   &msm_sdw->sdw_base_addr);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "reg");
		goto err_sdw_cdc;
	}

	msm_sdw->sdw_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,cdc-sdw-gpios", 0);
	msm_sdw->sdw_base = ioremap(msm_sdw->sdw_base_addr,
				    MSM_SDW_MAX_REGISTER);
	msm_sdw->read_dev = __msm_sdw_reg_read;
	msm_sdw->write_dev = __msm_sdw_reg_write;

	msm_sdw->regmap = msm_sdw_regmap_init(msm_sdw->dev,
					      &msm_sdw_regmap_config);
	msm_sdw->sdw_irq = platform_get_irq_byname(pdev, "swr_master_irq");
	if (msm_sdw->sdw_irq < 0) {
		dev_err(msm_sdw->dev, "%s() error getting irq handle: %d\n",
				__func__, msm_sdw->sdw_irq);
		ret = -ENODEV;
		goto err_sdw_cdc;
	}
	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_msm_sdw,
				     msm_sdw_dai, ARRAY_SIZE(msm_sdw_dai));
	if (ret) {
		dev_err(&pdev->dev, "%s: Codec registration failed, ret = %d\n",
			__func__, ret);
		goto err_sdw_cdc;
	}
	/* initialize the int_mclk1 */
	msm_sdw->sdw_cdc_core_clk.clk_set_minor_version =
			AFE_API_VERSION_I2S_CONFIG;
	msm_sdw->sdw_cdc_core_clk.clk_id =
			Q6AFE_LPASS_CLK_ID_INT_MCLK_1;
	msm_sdw->sdw_cdc_core_clk.clk_freq_in_hz =
			INT_MCLK1_FREQ;
	msm_sdw->sdw_cdc_core_clk.clk_attri =
			Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO;
	msm_sdw->sdw_cdc_core_clk.clk_root =
			Q6AFE_LPASS_CLK_ROOT_DEFAULT;
	msm_sdw->sdw_cdc_core_clk.enable = 0;

	/* initialize the sdw_npl_clk */
	msm_sdw->sdw_npl_clk.clk_set_minor_version =
			AFE_API_VERSION_I2S_CONFIG;
	msm_sdw->sdw_npl_clk.clk_id =
			AFE_CLOCK_SET_CLOCK_ID_SWR_NPL_CLK;
	msm_sdw->sdw_npl_clk.clk_freq_in_hz = SDW_NPL_FREQ;
	msm_sdw->sdw_npl_clk.clk_attri =
			Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO;
	msm_sdw->sdw_npl_clk.clk_root =
			Q6AFE_LPASS_CLK_ROOT_DEFAULT;
	msm_sdw->sdw_npl_clk.enable = 0;

	INIT_DELAYED_WORK(&msm_sdw->disable_int_mclk1_work,
			  msm_disable_int_mclk1);
	mutex_init(&msm_sdw->cdc_int_mclk1_mutex);
	mutex_init(&msm_sdw->sdw_npl_clk_mutex);
	mutex_init(&msm_sdw->io_lock);
	mutex_init(&msm_sdw->sdw_read_lock);
	mutex_init(&msm_sdw->sdw_write_lock);
	mutex_init(&msm_sdw->sdw_clk_lock);
	mutex_init(&msm_sdw->codec_mutex);
	schedule_work(&msm_sdw->msm_sdw_add_child_devices_work);

	dev_dbg(&pdev->dev, "%s: msm_sdw driver probe done\n", __func__);
	return ret;

err_sdw_cdc:
	devm_kfree(&pdev->dev, msm_sdw);
	return ret;
}

static int msm_sdw_remove(struct platform_device *pdev)
{
	struct msm_sdw_priv *msm_sdw;

	msm_sdw = dev_get_drvdata(&pdev->dev);

	mutex_destroy(&msm_sdw->io_lock);
	mutex_destroy(&msm_sdw->sdw_read_lock);
	mutex_destroy(&msm_sdw->sdw_write_lock);
	mutex_destroy(&msm_sdw->sdw_clk_lock);
	mutex_destroy(&msm_sdw->codec_mutex);
	mutex_destroy(&msm_sdw->cdc_int_mclk1_mutex);
	devm_kfree(&pdev->dev, msm_sdw);
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_sdw_codec_dt_match[] = {
	{ .compatible = "qcom,msm-sdw-codec", },
	{}
};

static struct platform_driver msm_sdw_codec_driver = {
	.probe = msm_sdw_probe,
	.remove = msm_sdw_remove,
	.driver = {
		.name = "msm_sdw_codec",
		.owner = THIS_MODULE,
		.of_match_table = msm_sdw_codec_dt_match,
	},
};
module_platform_driver(msm_sdw_codec_driver);

MODULE_DESCRIPTION("MSM Soundwire Codec driver");
MODULE_LICENSE("GPL v2");
