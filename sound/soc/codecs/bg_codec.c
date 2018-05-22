/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <linux/qdsp6v2/apr.h>
#include <sound/info.h>
#include <soc/qcom/bg_glink.h>
#include <sound/q6core.h>
#include <soc/qcom/subsystem_notif.h>
#include <trace/events/power.h>
#include "bg_codec.h"
#include "pktzr.h"
#include "wcdcal-hwdep.h"

#define SAMPLE_RATE_48KHZ 48000
#define SAMPLE_RATE_16KHZ 16000

#define BG_RATES_MAX (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			    SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			    SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define BG_FORMATS_S16_S24_LE (SNDRV_PCM_FMTBIT_S16_LE | \
				  SNDRV_PCM_FMTBIT_S24_LE | \
				  SNDRV_PCM_FMTBIT_S24_3LE)
#define BG_BLOB_DATA_SIZE 3136
#define SPEAK_VREG_NAME "vdd-spkr"
/*
 *50 Milliseconds sufficient for DSP bring up in the modem
 * after Sub System Restart
 */
#define ADSP_STATE_READY_TIMEOUT_MS 50

enum {
	BG_AIF1_PB = 0,
	BG_AIF2_PB,
	BG_AIF3_PB,
	BG_AIF4_PB,
	BG_AIF1_CAP,
	BG_AIF2_CAP,
	BG_AIF3_CAP,
	BG_AIF4_CAP,
	NUM_CODEC_DAIS,
};


enum {
	PLAYBACK = 0,
	CAPTURE,
};

struct bg_hw_params {
	u32 active_session;
	u32 rx_sample_rate;
	u32 rx_bit_width;
	u32 rx_num_channels;
	u32 tx_sample_rate;
	u32 tx_bit_width;
	u32 tx_num_channels;
};

struct bg_dai_data {
	DECLARE_BITMAP(status_cdc_channel, NUM_CODEC_DAIS);
};

struct bg_cdc_priv {
	struct device *dev;
	struct snd_soc_codec *codec;
	struct platform_device *pdev_child;
	struct work_struct bg_cdc_add_child_devices_work;
	struct delayed_work bg_cdc_pktzr_init_work;
	struct delayed_work bg_cdc_cal_init_work;
	unsigned long status_mask;
	struct bg_hw_params hw_params;
	struct notifier_block bg_pm_nb;
	struct notifier_block bg_adsp_nb;
	struct notifier_block bg_ssr_nb;
	/* cal info for codec */
	struct fw_info *fw_data;
	struct firmware_cal *hwdep_spk_cal;
	struct firmware_cal *hwdep_mic_cal;
	/* Lock to protect init cal */
	struct mutex bg_cdc_lock;
	int src[NUM_CODEC_DAIS];
	bool hwd_started;
	bool bg_cal_updated;
	bool adsp_dev_up;
	bool bg_dev_up;
	bool bg_spk_connected;
	struct regulator *spkr_vreg;
	struct bg_dai_data dai_data;
	uint16_t num_sessions;
	uint16_t bg_cal_init_delay;
};

struct codec_ssn_rt_setup_t {
	/* active session_id */
	uint32_t active_session;
	/* To indicate if playback/record happens from/to BG or MSM */
	uint32_t route_to_bg;
};

struct graphite_basic_rsp_result {
	/* Valid Graphite error code or completion status */
	uint32_t status;
};

static void *adsp_state_notifier;
static void *bg_state_notifier;

static uint32_t get_active_session_id(int dai_id)

{
	uint32_t active_session;

	if ((dai_id >= NUM_CODEC_DAIS) || (dai_id < 0)) {
		pr_err("%s invalid dai id\n", __func__);
		return 0;
	}

	switch (dai_id) {
	case BG_AIF1_PB:
		active_session = 0x0001;
		break;
	case BG_AIF1_CAP:
		active_session = 0x00010000;
		break;
	case BG_AIF2_PB:
		active_session = 0x0001;
		break;
	case BG_AIF2_CAP:
		active_session = 0x00020000;
		break;
	case BG_AIF3_PB:
		active_session = 0x0002;
		break;
	case BG_AIF3_CAP:
		/* BG MIC 1 is at slot 3 of the TDM packet */
		active_session = 0x00020000;
		break;
	case BG_AIF4_PB:
		active_session = 0x0004;
		break;
	case BG_AIF4_CAP:
		/* BG MIC 0 is at slot 4 of the TDM packet */
		active_session = 0x00010000;
		break;
	default:
		active_session = 0;
	}
	pr_debug("active_session selected %x", active_session);
	return active_session;
}

static int bg_cdc_enable_regulator(struct regulator *spkr_vreg, bool enable)
{
	int ret = 0;

	if (enable) {
		ret = regulator_set_voltage(
			spkr_vreg, 1800000, 1800000);
		if (ret) {
			pr_err("VDD-speaker set voltage failed error=%d\n",
					 ret);
			goto err_vreg_regulator;
		} else {
			ret = regulator_enable(spkr_vreg);
			if (ret) {
				pr_err("VDD-speaker voltage failed %d\n", ret);
				goto err_vreg_regulator;
			}
		}
	} else {
		/* Set regulator to standby mode */
		ret = regulator_disable(spkr_vreg);
		if (ret < 0) {
			pr_err("Failed to set spkr_vreg mode %d\n", ret);
			goto err_vreg_regulator;
		}
		ret = regulator_set_optimum_mode(spkr_vreg, 0);
		if (ret < 0) {
			pr_err("Failed to set optimum  mode %d\n", ret);
			goto err_vreg_regulator;
		}
	}
	return 0;

err_vreg_regulator:
	return ret;
}

static int bg_cdc_cal(struct bg_cdc_priv *bg_cdc)
{
	u8 *init_params = NULL, *init_head = NULL;
	struct pktzr_cmd_rsp rsp;
	u32 mic_blob_size = sizeof(app_mic_init_params);
	u32 spk_blob_size = sizeof(smart_pa_init_params);
	int ret = 0;

	init_params = kzalloc(BG_BLOB_DATA_SIZE, GFP_KERNEL);
	if (!init_params) {
		ret = -ENOMEM;
		goto err2;
	}
	init_head = init_params;

	bg_cdc->hwdep_mic_cal = wcdcal_get_fw_cal(bg_cdc->fw_data,
						  BG_CODEC_MIC_CAL);

	if (bg_cdc->hwdep_mic_cal &&
		(mic_blob_size < bg_cdc->hwdep_mic_cal->size))
		bg_cdc->hwdep_mic_cal = NULL;

	bg_cdc->hwdep_spk_cal = wcdcal_get_fw_cal(bg_cdc->fw_data,
						 BG_CODEC_SPEAKER_CAL);
	if (bg_cdc->hwdep_spk_cal &&
		(spk_blob_size < bg_cdc->hwdep_spk_cal->size))
		bg_cdc->hwdep_spk_cal = NULL;

	if (bg_cdc->hwdep_mic_cal) {
		pr_debug("%s:mic cal size %d\n", __func__,
				bg_cdc->hwdep_mic_cal->size);
		memcpy(init_params, &bg_cdc->hwdep_mic_cal->size,
				sizeof(bg_cdc->hwdep_mic_cal->size));
		init_params += sizeof(bg_cdc->hwdep_mic_cal->size);
		memcpy(init_params, bg_cdc->hwdep_mic_cal->data,
				bg_cdc->hwdep_mic_cal->size);
		init_params += bg_cdc->hwdep_mic_cal->size;
	} else {
		pr_debug("%s:default mic cal size %d\n", __func__,
				mic_blob_size);
		memcpy(init_params, &mic_blob_size,
			sizeof(mic_blob_size));
		init_params += sizeof(mic_blob_size);
		memcpy(init_params, app_mic_init_params,
		sizeof(app_mic_init_params));
		init_params += sizeof(app_mic_init_params);
	}
	if (bg_cdc->bg_spk_connected) {
		if (bg_cdc->hwdep_spk_cal) {
			pr_debug("%s: spk cal size %d\n", __func__,
					bg_cdc->hwdep_spk_cal->size);
			memcpy(init_params, &bg_cdc->hwdep_spk_cal->size,
					sizeof(bg_cdc->hwdep_spk_cal->size));
			init_params += sizeof(bg_cdc->hwdep_spk_cal->size);
			memcpy(init_params, bg_cdc->hwdep_spk_cal->data,
					bg_cdc->hwdep_spk_cal->size);
		} else {
			pr_debug("%s: default spk cal size %d\n", __func__,
					spk_blob_size);
			memcpy(init_params, &spk_blob_size,
				sizeof(spk_blob_size));
			init_params += sizeof(spk_blob_size);
			memcpy(init_params, smart_pa_init_params,
				sizeof(smart_pa_init_params));
		}
	} else
		pr_debug("%s: spk not connected ignoring spk cal\n", __func__);
	rsp.buf_size = sizeof(struct graphite_basic_rsp_result);
	rsp.buf = kzalloc(rsp.buf_size, GFP_KERNEL);
	if (!rsp.buf) {
		ret = -ENOMEM;
		goto err1;
	}
	/* Send command to BG to init session */
	ret = pktzr_cmd_init_params(init_head, BG_BLOB_DATA_SIZE, &rsp);
	if (ret < 0) {
		pr_err("pktzr cmd set params failed\n");
		goto err;
	}
	bg_cdc->bg_cal_updated = true;
err:
	kfree(rsp.buf);
err1:
	kfree(init_head);
err2:
	return ret;
}

static void bg_cdc_cal_init(struct work_struct *work)
{
	struct bg_cdc_priv *bg_cdc;
	struct delayed_work *dwork;
	struct bg_hw_params hw_params;
	struct pktzr_cmd_rsp rsp;
	int ret = 0;

	dwork = to_delayed_work(work);
	bg_cdc = container_of(dwork, struct bg_cdc_priv,
				bg_cdc_cal_init_work);
	mutex_lock(&bg_cdc->bg_cdc_lock);
	if (!bg_cdc->bg_cal_updated) {
		ret = bg_cdc_enable_regulator(bg_cdc->spkr_vreg, true);
		if (ret < 0) {
			pr_err("%s: enable_regulator failed %d\n", __func__,
				ret);
				goto err;
		}
		/* Send open command */
		rsp.buf_size = sizeof(struct graphite_basic_rsp_result);
		rsp.buf = kzalloc(rsp.buf_size, GFP_KERNEL);
		if (!rsp.buf)
			goto err2;
		memcpy(&hw_params, &bg_cdc->hw_params, sizeof(hw_params));
		/* Send command to BG to start session */
		ret = pktzr_cmd_open(&hw_params, sizeof(hw_params), &rsp);
		if (ret < 0) {
			pr_err("%s: pktzr cmd open failed\n", __func__);
			goto err1;
		}
		ret = bg_cdc_cal(bg_cdc);
		if (ret < 0) {
			pr_err("%s: calibiration failed\n", __func__);
			goto err1;
		}
		kfree(rsp.buf);
	}
	if (!(snd_card_is_online_state
		(bg_cdc->codec->component.card->snd_card)) &&
		(bg_cdc->adsp_dev_up)) {
		snd_soc_card_change_online_state(bg_cdc->codec->component.card,
						 1);
	}
	mutex_unlock(&bg_cdc->bg_cdc_lock);
	return;
err1:
	kfree(rsp.buf);
err2:
	bg_cdc_enable_regulator(bg_cdc->spkr_vreg, false);
err:
	if (!(snd_card_is_online_state
		(bg_cdc->codec->component.card->snd_card)) &&
		(bg_cdc->adsp_dev_up)) {
		snd_soc_card_change_online_state(bg_cdc->codec->component.card,
						 1);
	}
	mutex_unlock(&bg_cdc->bg_cdc_lock);
	return;
}

static int _bg_codec_hw_params(struct bg_cdc_priv *bg_cdc)
{
	struct bg_hw_params hw_params;
	struct pktzr_cmd_rsp rsp;
	int ret = 0;

	cancel_delayed_work_sync(&bg_cdc->bg_cdc_cal_init_work);
	mutex_lock(&bg_cdc->bg_cdc_lock);
	if (!bg_cdc->bg_dev_up) {
		pr_err("%s: Bg ssr in progress\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	if (!bg_cdc->bg_cal_updated) {
		ret = bg_cdc_enable_regulator(bg_cdc->spkr_vreg, true);
		if (ret < 0) {
			pr_err("%s: enable_regulator failed %d\n", __func__,
			      ret);
			goto err;
		}
		ret =  bg_cdc_cal(bg_cdc);
		if (ret < 0) {
			pr_err("%s:failed to send cal data", __func__);
			goto err1;
		}
	} else {
		pr_debug("%s:cal data already sent to BG", __func__);
	}

	rsp.buf_size = sizeof(struct graphite_basic_rsp_result);
	rsp.buf = kzalloc(rsp.buf_size, GFP_KERNEL);
	if (!rsp.buf) {
		ret = -ENOMEM;
		goto err;
	}
	memcpy(&hw_params, &bg_cdc->hw_params, sizeof(hw_params));
	/* Send command to BG to set_params */
	ret = pktzr_cmd_set_params(&hw_params, sizeof(hw_params), &rsp);
	mutex_unlock(&bg_cdc->bg_cdc_lock);
	if (ret < 0)
		pr_err("pktzr cmd set params failed with error %d\n", ret);

	kfree(rsp.buf);
	return ret;
err1:
	bg_cdc_enable_regulator(bg_cdc->spkr_vreg, false);
err:
	mutex_unlock(&bg_cdc->bg_cdc_lock);
	return ret;
}

static int _bg_codec_start(struct bg_cdc_priv *bg_cdc, int dai_id)
{
	struct pktzr_cmd_rsp rsp;
	struct codec_ssn_rt_setup_t codec_start;
	int ret = 0;

	rsp.buf = NULL;
	codec_start.active_session = get_active_session_id(dai_id);
	if (codec_start.active_session == 0) {
		pr_err("%s:Invalid dai id %d", __func__, dai_id);
		return -EINVAL;
	}

	mutex_lock(&bg_cdc->bg_cdc_lock);
	if ((bg_cdc->num_sessions == 0) &&
	    (bg_cdc->bg_dev_up)) {
		/* set regulator to normal mode */
		ret = regulator_set_optimum_mode(bg_cdc->spkr_vreg, 100000);
		if (ret < 0) {
			pr_err("Fail to set spkr_vreg mode%d\n", ret);
			goto err;
		}
	}
	bg_cdc->num_sessions++;
	if (!bg_cdc->bg_dev_up) {
		pr_err("%s: Bg ssr in progress\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	if (test_bit(dai_id, bg_cdc->dai_data.status_cdc_channel)) {
		pr_debug("%s: dai_id %d ports already opened\n",
				__func__, dai_id);
		ret =  -EINVAL;
		goto err;
	}
	codec_start.route_to_bg = bg_cdc->src[dai_id];
	pr_debug("%s active_session %x route_to_bg %d\n",
		__func__, codec_start.active_session, codec_start.route_to_bg);
	rsp.buf_size = sizeof(struct graphite_basic_rsp_result);
	rsp.buf = kzalloc(rsp.buf_size, GFP_KERNEL);
	if (!rsp.buf) {
		ret = -ENOMEM;
		goto err;
	}
	ret = pktzr_cmd_start(&codec_start, sizeof(codec_start), &rsp);
	if (ret < 0)
		pr_err("pktzr cmd start failed %d\n", ret);

	set_bit(dai_id, bg_cdc->dai_data.status_cdc_channel);
	kfree(rsp.buf);
err:
	mutex_unlock(&bg_cdc->bg_cdc_lock);
	return ret;
}

static int _bg_codec_stop(struct bg_cdc_priv *bg_cdc, int dai_id)
{
	struct pktzr_cmd_rsp rsp;
	struct codec_ssn_rt_setup_t codec_start;
	int ret = 0;

	rsp.buf = NULL;
	codec_start.active_session = get_active_session_id(dai_id);
	if (codec_start.active_session == 0) {
		pr_err("%s:Invalid dai id %d", __func__, dai_id);
		return -EINVAL;
	}
	mutex_lock(&bg_cdc->bg_cdc_lock);
	if (bg_cdc->num_sessions > 0)
		bg_cdc->num_sessions--;
	if (!bg_cdc->bg_dev_up) {
		pr_err("%s: Bg ssr in progress\n", __func__);
		if (test_bit(dai_id, bg_cdc->dai_data.status_cdc_channel))
			clear_bit(dai_id, bg_cdc->dai_data.status_cdc_channel);
		ret = -EINVAL;
		goto err;
	}
	if (!(test_bit(dai_id, bg_cdc->dai_data.status_cdc_channel))) {
		pr_debug("%s: dai_id %d ports already closed\n",
				__func__, dai_id);
		ret = -EINVAL;
		goto err;
	}
	codec_start.route_to_bg = bg_cdc->src[dai_id];
	pr_debug("%s active_session %x route_to_bg %d\n",
		__func__, codec_start.active_session, codec_start.route_to_bg);
	rsp.buf_size = sizeof(struct graphite_basic_rsp_result);
	rsp.buf = kzalloc(rsp.buf_size, GFP_KERNEL);
	if (!rsp.buf) {
		ret = -ENOMEM;
		goto err;
	}
	ret = pktzr_cmd_stop(&codec_start, sizeof(codec_start), &rsp);
	if (ret < 0)
		pr_err("pktzr cmd stop failed with error %d\n", ret);

	if ((bg_cdc->num_sessions == 0) && (bg_cdc->bg_dev_up)) {
		/* Reset the regulator mode if this is the last session */
		ret = regulator_set_optimum_mode(bg_cdc->spkr_vreg, 0);
		if (ret < 0)
			pr_err("Failed to set spkr_vreg mode%d\n", ret);
	}
	clear_bit(dai_id, bg_cdc->dai_data.status_cdc_channel);
	mutex_unlock(&bg_cdc->bg_cdc_lock);
	kfree(rsp.buf);
	return ret;
err:
	mutex_unlock(&bg_cdc->bg_cdc_lock);
	return ret;
}



static int bg_get_src(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(codec);
	int dai_id = ((struct soc_multi_mixer_control *)
				kcontrol->private_value)->reg;

	if ((bg_cdc == NULL) || (dai_id >= NUM_CODEC_DAIS) ||
		(dai_id < 0)) {
		pr_err("%s invalid input\n", __func__);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = bg_cdc->src[dai_id];
	dev_dbg(codec->dev, "%s: dai_id: %d src: %d\n", __func__,
			dai_id, bg_cdc->src[dai_id]);

	return 0;
}

static int bg_put_src(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(codec);
	int dai_id = ((struct soc_multi_mixer_control *)
				kcontrol->private_value)->reg;

	if ((bg_cdc == NULL) || (dai_id >= NUM_CODEC_DAIS) ||
		(dai_id < 0)) {
		pr_err("%s invalid input\n", __func__);
		return -EINVAL;
	}

	bg_cdc->src[dai_id] = ucontrol->value.integer.value[0];
	dev_dbg(codec->dev, "%s: dai_id: %d src: %d\n", __func__,
			dai_id, bg_cdc->src[dai_id]);

	return 0;
}

static int bg_get_hwd_state(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(codec);
	int dai_id = ((struct soc_multi_mixer_control *)
				kcontrol->private_value)->reg;

	if ((bg_cdc == NULL) || (dai_id >= NUM_CODEC_DAIS) ||
		(dai_id < 0)) {
		pr_err("%s invalid input\n", __func__);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = bg_cdc->hwd_started;
	dev_dbg(codec->dev, "%s: dai_id: %d hwd_enable: %d\n", __func__,
			dai_id, bg_cdc->hwd_started);

	return 0;
}

static int bg_put_hwd_state(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(codec);
	int dai_id = ((struct soc_multi_mixer_control *)
				kcontrol->private_value)->reg;
	uint32_t active_session_id = 0;
	int ret = 0;

	if ((bg_cdc == NULL) || (dai_id >= NUM_CODEC_DAIS) ||
		(dai_id < 0)) {
		pr_err("%s bgcdc is null or invalid dai id\n", __func__);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: dai_id: %d hwd_enable %ld\n", __func__,
			dai_id, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] && (!bg_cdc->hwd_started)) {
		/* enable bg hwd */
		bg_cdc->hw_params.tx_sample_rate = SAMPLE_RATE_16KHZ;
		bg_cdc->hw_params.tx_bit_width = 16;
		bg_cdc->hw_params.tx_num_channels = 1;
		active_session_id = get_active_session_id(dai_id);
		if (active_session_id == 0) {
			pr_err("%s:Invalid dai id %d", __func__, dai_id);
			return -EINVAL;
		}
		bg_cdc->hw_params.active_session = active_session_id;
		/* Send command to BG for HW params */
		ret = _bg_codec_hw_params(bg_cdc);
		if (ret < 0) {
			pr_err("_bg_codec_hw_params fail for dai %d", dai_id);
			return ret;
		}
		/* Send command to BG to start session */
		ret = _bg_codec_start(bg_cdc, dai_id);
		if (ret < 0) {
			pr_err("_bg_codec_start fail for dai %d", dai_id);
			return ret;
		}
		bg_cdc->hwd_started = true;
	} else if (bg_cdc->hwd_started &&
			(ucontrol->value.integer.value[0] == 0)) {
		/*hwd was on, this is a command to stop it*/
		bg_cdc->hwd_started = false;
		ret = _bg_codec_stop(bg_cdc, dai_id);
		if (ret < 0) {
			pr_err("bg_codec_stop failed for dai %d\n", dai_id);
			return ret;
		}
	}

	return ret;
}

static const struct snd_kcontrol_new bg_snd_controls[] = {
	SOC_SINGLE_EXT("RX_0 SRC", BG_AIF1_PB, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("RX_1 SRC", BG_AIF2_PB, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("RX_2 SRC", BG_AIF3_PB, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("RX_3 SRC", BG_AIF4_PB, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("TX_0 DST", BG_AIF1_CAP, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("TX_1 DST", BG_AIF2_CAP, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("TX_2 DST", BG_AIF3_CAP, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("TX_3 DST", BG_AIF4_CAP, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("TX_2 HWD", BG_AIF3_CAP, 0, 1, 0,
	bg_get_hwd_state, bg_put_hwd_state),
	SOC_SINGLE_EXT("TX_3 HWD", BG_AIF4_CAP, 0, 1, 0,
	bg_get_hwd_state, bg_put_hwd_state),
};

static int bg_cdc_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s: substream = %s  stream = %d\n" , __func__,
			substream->name, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		set_bit(PLAYBACK, &bg_cdc->status_mask);
	else
		set_bit(CAPTURE, &bg_cdc->status_mask);

	return 0;
}

static void bg_cdc_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s: substream = %s  stream = %d\n" , __func__,
			substream->name, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		clear_bit(PLAYBACK, &bg_cdc->status_mask);
	else
		clear_bit(PLAYBACK, &bg_cdc->status_mask);

}

static int bg_cdc_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(dai->codec);
	int ret = 0;

	pr_debug("%s: dai_name = %s DAI-ID %x rate %d width %d num_ch %d\n",
		 __func__, dai->name, dai->id, params_rate(params),
		 params_width(params), params_channels(params));
	mutex_lock(&bg_cdc->bg_cdc_lock);
	if (!bg_cdc->bg_dev_up) {
		pr_err("%s: Bg ssr in progress\n", __func__);
		mutex_unlock(&bg_cdc->bg_cdc_lock);
		return -EINVAL;
	}
	mutex_unlock(&bg_cdc->bg_cdc_lock);

	bg_cdc->hw_params.active_session = get_active_session_id(dai->id);
	if (bg_cdc->hw_params.active_session == 0) {
		pr_err("%s:Invalid dai id %d", __func__, dai->id);
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (!bg_cdc->bg_spk_connected) {
			pr_err("%s:speaker not connected\n", __func__);
			return -EINVAL;
		}
		bg_cdc->hw_params.rx_sample_rate = params_rate(params);
		bg_cdc->hw_params.rx_bit_width = params_width(params);
		bg_cdc->hw_params.rx_num_channels = params_channels(params);
	} else {
		bg_cdc->hw_params.tx_sample_rate = params_rate(params);
		bg_cdc->hw_params.tx_bit_width = params_width(params);
		bg_cdc->hw_params.tx_num_channels = params_channels(params);
	}

	/* Send command to BG for HW params */

	ret = _bg_codec_hw_params(bg_cdc);
	if (ret < 0)
		pr_err("_bg_codec_hw_params failed for dai %d", dai->id);
	return ret;
}

static int bg_cdc_hw_free(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	int ret = 0;
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s: dai_name = %s DAI-ID %x\n", __func__, dai->name, dai->id);

	ret = _bg_codec_stop(bg_cdc, dai->id);
	if (ret < 0)
		pr_err("bg_codec_stop failed for dai %d\n", dai->id);

	return ret;
}

static int bg_cdc_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret = 0;
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(dai->codec);

	/* check if RX, TX sampling freq is same if not return error. */
	if (test_bit(PLAYBACK, &bg_cdc->status_mask) &&
	    test_bit(CAPTURE, &bg_cdc->status_mask)) {
		if (((bg_cdc->hw_params.rx_sample_rate !=
		    bg_cdc->hw_params.tx_sample_rate) ||
		    (bg_cdc->hw_params.rx_bit_width !=
		    bg_cdc->hw_params.tx_bit_width)) &&
			!bg_cdc->hwd_started) {
			pr_err("%s diff rx and tx configuration %d:%d:%d:%d\n",
				__func__, bg_cdc->hw_params.rx_sample_rate,
				bg_cdc->hw_params.tx_sample_rate,
				bg_cdc->hw_params.rx_bit_width,
				bg_cdc->hw_params.rx_bit_width);
			return -EINVAL;
		}
	} else if (test_bit(CAPTURE, &bg_cdc->status_mask) &&
					bg_cdc->hwd_started){
		pr_err("Cannot enable recording if hwd is in progress");
		return -EINVAL;
	}

	/* Send command to BG to start session */
	ret = _bg_codec_start(bg_cdc, dai->id);
	if (ret < 0)
		pr_err("_bg_codec_start failed for dai %d", dai->id);
	return ret;
}

static int bg_cdc_set_channel_map(struct snd_soc_dai *dai,
				  unsigned int tx_num, unsigned int *tx_slot,
				  unsigned int rx_num, unsigned int *rx_slot)
{
	pr_debug("in func %s", __func__);
	return 0;
}


static int bg_cdc_get_channel_map(struct snd_soc_dai *dai,
				  unsigned int *tx_num, unsigned int *tx_slot,
				  unsigned int *rx_num, unsigned int *rx_slot)
{
	pr_debug("in func %s", __func__);
	return 0;
}

static struct snd_soc_dai_ops bg_cdc_dai_ops = {
	.startup = bg_cdc_startup,
	.shutdown = bg_cdc_shutdown,
	.hw_params = bg_cdc_hw_params,
	.hw_free = bg_cdc_hw_free,
	.prepare = bg_cdc_prepare,
	.set_channel_map = bg_cdc_set_channel_map,
	.get_channel_map = bg_cdc_get_channel_map,
};

static struct snd_soc_dai_driver bg_cdc_dai[] = {
	{
		.name = "bg_cdc_rx1",
		.id = BG_AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_rx2",
		.id = BG_AIF2_PB,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_rx3",
		.id = BG_AIF3_PB,
		.playback = {
			.stream_name = "AIF3 Playback",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_rx4",
		.id = BG_AIF4_PB,
		.playback = {
			.stream_name = "AIF4 Playback",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_tx1",
		.id = BG_AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_tx2",
		.id = BG_AIF2_CAP,
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_tx3",
		.id = BG_AIF3_CAP,
		.capture = {
			.stream_name = "AIF3 Capture",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_tx4",
		.id = BG_AIF4_CAP,
		.capture = {
			.stream_name = "AIF4 Capture",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &bg_cdc_dai_ops,
	},
};

static int data_cmd_rsp(void *buf, uint32_t len, void *priv_data,
			bool *is_basic_rsp)
{
	struct graphite_basic_rsp_result *resp;

	pr_debug("in data_cmd_rsp");

	if (buf != NULL) {
		resp = buf;
		pr_err("%s: status = %d\n", __func__, resp->status);
	}

	return 0;
}

static void bg_cdc_pktzr_init(struct work_struct *work)
{
	int ret;
	struct bg_cdc_priv *bg_cdc;
	struct delayed_work *dwork;
	int num_of_intents = 1;
	uint32_t size = 4096;
	struct bg_glink_ch_cfg ch_info[1] = {
		{"CODEC_CHANNEL", num_of_intents, &size}
	};

	pr_debug("%s\n", __func__);
	dwork = to_delayed_work(work);
	bg_cdc = container_of(dwork, struct bg_cdc_priv,
			     bg_cdc_pktzr_init_work);

	ret = pktzr_init(bg_cdc->pdev_child, ch_info, 1, data_cmd_rsp);
	if (ret < 0) {
		dev_err(bg_cdc->dev, "%s: failed in pktzr_init\n", __func__);
		return;
	}
	schedule_delayed_work(&bg_cdc->bg_cdc_cal_init_work,
			      msecs_to_jiffies(bg_cdc->bg_cal_init_delay));
	bg_cdc->bg_cal_init_delay = 0;
}

static int bg_cdc_bg_device_up(struct bg_cdc_priv *bg_cdc)
{
	schedule_delayed_work(&bg_cdc->bg_cdc_pktzr_init_work,
			msecs_to_jiffies(0));
	return 0;
}

static int bg_cdc_bg_device_down(struct bg_cdc_priv *bg_cdc)
{
	cancel_delayed_work_sync(&bg_cdc->bg_cdc_pktzr_init_work);
	cancel_delayed_work_sync(&bg_cdc->bg_cdc_cal_init_work);
	mutex_lock(&bg_cdc->bg_cdc_lock);
	pktzr_deinit();
	if (bg_cdc->bg_cal_updated) {
		bg_cdc_enable_regulator(bg_cdc->spkr_vreg, false);
		bg_cdc->bg_cal_updated = false;
	}
	snd_soc_card_change_online_state(bg_cdc->codec->component.card, 0);
	mutex_unlock(&bg_cdc->bg_cdc_lock);
	return 0;
}

static int bg_cdc_adsp_device_up(struct bg_cdc_priv *bg_cdc)
{
	bool timedout;
	unsigned long timeout;

	mutex_lock(&bg_cdc->bg_cdc_lock);
	if (!q6core_is_adsp_ready()) {
		timeout = jiffies +
			  msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);
		while (!(timedout = time_after(jiffies, timeout))) {
			if (!q6core_is_adsp_ready()) {
				dev_err(bg_cdc->dev, "ADSP isn't ready\n");
			} else {
				dev_err(bg_cdc->dev, "ADSP is ready\n");
				break;
			}
		}
	} else
		dev_err(bg_cdc->dev, "%s:ADSP is ready\n", __func__);

	if (bg_cdc->bg_dev_up)
		snd_soc_card_change_online_state(bg_cdc->codec->component.card,
						 1);
	mutex_unlock(&bg_cdc->bg_cdc_lock);
	return 0;
}

static int bg_cdc_adsp_device_down(struct bg_cdc_priv *bg_cdc)
{
	snd_soc_card_change_online_state(bg_cdc->codec->component.card, 0);
	return 0;
}

static int bg_state_callback(struct notifier_block *nb, unsigned long value,
				void *priv)
{
	struct bg_cdc_priv *bg_cdc =
			container_of(nb, struct bg_cdc_priv, bg_ssr_nb);

	if (value == SUBSYS_BEFORE_SHUTDOWN) {
		bg_cdc_bg_device_down(bg_cdc);
		mutex_lock(&bg_cdc->bg_cdc_lock);
		bg_cdc->bg_dev_up = false;
		mutex_unlock(&bg_cdc->bg_cdc_lock);
	} else if (value == SUBSYS_AFTER_POWERUP) {
		bg_cdc_bg_device_up(bg_cdc);
		mutex_lock(&bg_cdc->bg_cdc_lock);
		bg_cdc->bg_dev_up = true;
		mutex_unlock(&bg_cdc->bg_cdc_lock);
	}
	return NOTIFY_OK;
}

static int adsp_state_callback(struct notifier_block *nb, unsigned long value,
				void *priv)
{
	struct bg_cdc_priv *bg_cdc =
			container_of(nb, struct bg_cdc_priv, bg_adsp_nb);

	if (value == SUBSYS_BEFORE_SHUTDOWN) {
		bg_cdc_adsp_device_down(bg_cdc);
		bg_cdc->adsp_dev_up = false;
	} else if (value == SUBSYS_AFTER_POWERUP) {
		bg_cdc_adsp_device_up(bg_cdc);
		bg_cdc->adsp_dev_up = true;
	}
	return NOTIFY_OK;
}

static int bg_cdc_pm_suspend(struct bg_cdc_priv *bg_cdc)
{
	/* Do not remove the regulator vote if a session is active */
	if (bg_cdc->num_sessions > 0) {
		pr_debug("audio session in progress don't devote\n");
		return 0;
	}
	cancel_delayed_work_sync(&bg_cdc->bg_cdc_pktzr_init_work);
	cancel_delayed_work_sync(&bg_cdc->bg_cdc_cal_init_work);
	mutex_lock(&bg_cdc->bg_cdc_lock);
	if (bg_cdc->bg_cal_updated) {
		bg_cdc_enable_regulator(bg_cdc->spkr_vreg, false);
		bg_cdc->bg_cal_updated = false;
	}
	mutex_unlock(&bg_cdc->bg_cdc_lock);
	return 0;
}

static int bg_cdc_pm_resume(struct bg_cdc_priv *bg_cdc)
{
	schedule_delayed_work(&bg_cdc->bg_cdc_cal_init_work,
				msecs_to_jiffies(bg_cdc->bg_cal_init_delay));
	return 0;
}

static int bg_pm_event(struct notifier_block *nb,
			unsigned long event, void *ptr)
{
	struct bg_cdc_priv *bg_cdc =
			container_of(nb, struct bg_cdc_priv, bg_pm_nb);
	switch (event) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		return bg_cdc_pm_resume(bg_cdc);
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		return bg_cdc_pm_suspend(bg_cdc);
	default:
		return NOTIFY_DONE;
	}
}

static int bg_cdc_codec_probe(struct snd_soc_codec *codec)
{
	struct bg_cdc_priv *bg_cdc = dev_get_drvdata(codec->dev);
	const char *subsys_name = NULL;
	int ret;

	schedule_delayed_work(&bg_cdc->bg_cdc_pktzr_init_work,
				msecs_to_jiffies(400));

	bg_cdc->fw_data = devm_kzalloc(codec->dev,
				      sizeof(*(bg_cdc->fw_data)), GFP_KERNEL);

	bg_cdc->bg_cal_updated = false;
	bg_cdc->adsp_dev_up = true;
	bg_cdc->bg_dev_up = true;

	set_bit(BG_CODEC_MIC_CAL, bg_cdc->fw_data->cal_bit);
	set_bit(BG_CODEC_SPEAKER_CAL, bg_cdc->fw_data->cal_bit);

	ret = wcd_cal_create_hwdep(bg_cdc->fw_data,
				   WCD9XXX_CODEC_HWDEP_NODE, codec);
	if (ret < 0) {
		dev_err(codec->dev, "%s hwdep failed %d\n", __func__, ret);
		devm_kfree(codec->dev, bg_cdc->fw_data);
	}
	ret = of_property_read_string(codec->dev->of_node,
				     "qcom,subsys-name",
				     &subsys_name);
	bg_cdc->bg_adsp_nb.notifier_call = adsp_state_callback;
	if (ret) {
		dev_dbg(codec->dev, "missing subsys-name entry in dt node\n");
		adsp_state_notifier = subsys_notif_register_notifier("adsp",
							&bg_cdc->bg_adsp_nb);
	} else {
		adsp_state_notifier = subsys_notif_register_notifier(
							subsys_name,
							&bg_cdc->bg_adsp_nb);
	}
	if (!adsp_state_notifier)
		dev_err(codec->dev, "Failed to register adsp notifier\n");
	bg_cdc->bg_ssr_nb.notifier_call = bg_state_callback;
	bg_state_notifier = subsys_notif_register_notifier("bg-wear",
							&bg_cdc->bg_ssr_nb);
	if (!bg_state_notifier)
		dev_err(codec->dev, "Failed to register bg notifier\n");

	bg_cdc->bg_pm_nb.notifier_call = bg_pm_event;
	register_pm_notifier(&bg_cdc->bg_pm_nb);
	bg_cdc->codec = codec;
	return 0;
}

static int bg_cdc_codec_remove(struct snd_soc_codec *codec)
{
	struct bg_cdc_priv *bg_cdc = dev_get_drvdata(codec->dev);
	pr_debug("In func %s\n", __func__);
	pktzr_deinit();

	cancel_delayed_work_sync(&bg_cdc->bg_cdc_pktzr_init_work);
	cancel_delayed_work_sync(&bg_cdc->bg_cdc_cal_init_work);
	if (adsp_state_notifier)
		subsys_notif_unregister_notifier(adsp_state_notifier,
						 &bg_cdc->bg_adsp_nb);
	if (bg_state_notifier)
		subsys_notif_unregister_notifier(bg_state_notifier,
						 &bg_cdc->bg_ssr_nb);
	unregister_pm_notifier(&bg_cdc->bg_pm_nb);
	kfree(bg_cdc->fw_data);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_bg_cdc = {
	.probe = bg_cdc_codec_probe,
	.remove = bg_cdc_codec_remove,
	.controls = bg_snd_controls,
	.num_controls = ARRAY_SIZE(bg_snd_controls),
};

static void bg_cdc_add_child_devices(struct work_struct *work)
{
	struct platform_device *pdev = NULL;
	struct device_node *node;
	char plat_dev_name[50] = "bg-cdc";
	struct bg_cdc_priv *bg_cdc;
	int ret;

	pr_debug("%s\n", __func__);
	bg_cdc = container_of(work, struct bg_cdc_priv,
			     bg_cdc_add_child_devices_work);
	if (!bg_cdc) {
		pr_err("%s: Memory for BG codec does not exist\n",
			__func__);
		return;
	}
	for_each_available_child_of_node(bg_cdc->dev->of_node, node) {
		pr_debug("hnode->name = %s\n", node->name);
		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(bg_cdc->dev, "%s: pdev memory alloc failed\n",
					__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = bg_cdc->dev;
		pdev->dev.of_node = node;

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
					"%s: Cannot add platform device\n",
					__func__);
			goto fail_pdev_add;
		}
		bg_cdc->pdev_child = pdev;
	}
fail_pdev_add:
	if (pdev)
		platform_device_put(pdev);
err:
	return;
}

static int bg_cdc_probe(struct platform_device *pdev)
{
	struct bg_cdc_priv *bg_cdc;
	int ret = 0;
	int adsp_state;

	adsp_state = apr_get_subsys_state();
	if (adsp_state != APR_SUBSYS_LOADED) {
		pr_err("%s:Adsp is not loaded yet %d\n",
			__func__, adsp_state);
		return -EPROBE_DEFER;
	}

	bg_cdc = kzalloc(sizeof(struct bg_cdc_priv),
			    GFP_KERNEL);
	if (!bg_cdc)
		return -ENOMEM;

	bg_cdc->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, bg_cdc);
	/* 10 sec delay was expected to get HAL up and send cal data */
	bg_cdc->bg_cal_init_delay = 10000;

	if (of_get_property(
		pdev->dev.of_node,
		SPEAK_VREG_NAME "-supply", NULL)) {
		bg_cdc->spkr_vreg = regulator_get(&pdev->dev, SPEAK_VREG_NAME);
		if (IS_ERR(bg_cdc->spkr_vreg)) {
			ret = PTR_ERR(bg_cdc->spkr_vreg);
			pr_err("VDD-speaker get failed error=%d\n", ret);
			goto err_cdc_reg;
		}
		dev_dbg(&pdev->dev, "%s: got regulator handle\n", __func__);
	} else {
		ret = -EINVAL;
		goto err_cdc_reg;
	}
	bg_cdc->bg_spk_connected = of_property_read_bool(pdev->dev.of_node,
						"qcom,bg-speaker-connected");
	if (!bg_cdc->bg_spk_connected)
		dev_info(&pdev->dev, "%s: speaker not connected to target %d\n",
			__func__, bg_cdc->bg_spk_connected);

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_bg_cdc,
					bg_cdc_dai, ARRAY_SIZE(bg_cdc_dai));
	if (ret) {
		dev_err(&pdev->dev, "%s: Codec registration failed, ret = %d\n",
			__func__, ret);
		regulator_put(bg_cdc->spkr_vreg);
		goto err_cdc_reg;
	}

	INIT_WORK(&bg_cdc->bg_cdc_add_child_devices_work,
		  bg_cdc_add_child_devices);
	INIT_DELAYED_WORK(&bg_cdc->bg_cdc_pktzr_init_work,
		  bg_cdc_pktzr_init);
	INIT_DELAYED_WORK(&bg_cdc->bg_cdc_cal_init_work,
		  bg_cdc_cal_init);
	schedule_work(&bg_cdc->bg_cdc_add_child_devices_work);
	mutex_init(&bg_cdc->bg_cdc_lock);

	dev_dbg(&pdev->dev, "%s: BG driver probe done\n", __func__);
	return ret;

err_cdc_reg:

	kfree(bg_cdc);
	return ret;
}

static int bg_cdc_remove(struct platform_device *pdev)
{
	struct bg_cdc_priv *bg_cdc;

	bg_cdc = platform_get_drvdata(pdev);

	snd_soc_unregister_codec(&pdev->dev);
	mutex_destroy(&bg_cdc->bg_cdc_lock);
	regulator_put(bg_cdc->spkr_vreg);
	kfree(bg_cdc);
	return 0;
}


#define MODULE_NAME "bg_codec"

static const struct of_device_id audio_codec_of_match[] = {
	{ .compatible = "qcom,bg-codec", },
	{},
};

static struct platform_driver bg_codec_driver = {
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = audio_codec_of_match,
	},
	.probe = bg_cdc_probe,
	.remove = bg_cdc_remove,
};
module_platform_driver(bg_codec_driver);

MODULE_DESCRIPTION("BG Codec driver Loader module");
MODULE_LICENSE("GPL v2");
