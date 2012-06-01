/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/msm-dai-q6-v2.h>
#include <sound/pcm_params.h>
#include <mach/clk.h>

enum {
	STATUS_PORT_STARTED, /* track if AFE port has started */
	STATUS_MAX
};

struct msm_dai_q6_dai_data {
	DECLARE_BITMAP(status_mask, STATUS_MAX);
	u32 rate;
	u32 channels;
	union afe_port_config port_config;
};

static struct clk *pcm_clk;
static DEFINE_MUTEX(aux_pcm_mutex);
static int aux_pcm_count;
static struct msm_dai_auxpcm_pdata *auxpcm_plat_data;

static u8 num_of_bits_set(u8 sd_line_mask)
{
	u8 num_bits_set = 0;

	while (sd_line_mask) {
		num_bits_set++;
		sd_line_mask = sd_line_mask & (sd_line_mask - 1);
	}
	return num_bits_set;
}

static int msm_dai_q6_cdc_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	switch (dai_data->channels) {
	case 2:
		dai_data->port_config.i2s.mono_stereo = MSM_AFE_STEREO;
		break;
	case 1:
		dai_data->port_config.i2s.mono_stereo = MSM_AFE_MONO;
		break;
	default:
		return -EINVAL;
		break;
	}
	dai_data->rate = params_rate(params);
	dai_data->port_config.i2s.sample_rate = dai_data->rate;
	dai_data->port_config.i2s.i2s_cfg_minor_version =
						AFE_API_VERSION_I2S_CONFIG;
	dai_data->port_config.i2s.data_format =  AFE_LINEAR_PCM_DATA;
	dev_dbg(dai->dev, " channel %d sample rate %d entered\n",
	dai_data->channels, dai_data->rate);

	/* Q6 only supports 16 as now */
	dai_data->port_config.i2s.bit_width = 16;
	dai_data->port_config.i2s.channel_mode = 1;
	return 0;
}

static int msm_dai_q6_i2s_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	struct msm_i2s_data *i2s_pdata =
			(struct msm_i2s_data *) dai->dev->platform_data;

	dai_data->channels = params_channels(params);
	if (num_of_bits_set(i2s_pdata->sd_lines) == 1) {
		switch (dai_data->channels) {
		case 2:
			dai_data->port_config.i2s.mono_stereo = MSM_AFE_STEREO;
			break;
		case 1:
			dai_data->port_config.i2s.mono_stereo = MSM_AFE_MONO;
			break;
		default:
			pr_warn("greater than stereo has not been validated");
			break;
		}
	}
	dai_data->rate = params_rate(params);
	dai_data->port_config.i2s.sample_rate = dai_data->rate;
	dai_data->port_config.i2s.i2s_cfg_minor_version =
						AFE_API_VERSION_I2S_CONFIG;
	dai_data->port_config.i2s.data_format =  AFE_LINEAR_PCM_DATA;
	/* Q6 only supports 16 as now */
	dai_data->port_config.i2s.bit_width = 16;
	dai_data->port_config.i2s.channel_mode = 1;

	return 0;
}

static int msm_dai_q6_i2s_platform_data_validation(
					struct snd_soc_dai *dai)
{
	u8 num_of_sd_lines;
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	struct msm_i2s_data *i2s_pdata =
			(struct msm_i2s_data *)dai->dev->platform_data;
	struct snd_soc_dai_driver *dai_driver =
			(struct snd_soc_dai_driver *)dai->driver;

	num_of_sd_lines = num_of_bits_set(i2s_pdata->sd_lines);

	switch (num_of_sd_lines) {
	case 1:
		switch (i2s_pdata->sd_lines) {
		case MSM_MI2S_SD0:
			dai_data->port_config.i2s.channel_mode =
					AFE_PORT_I2S_SD0;
			break;
		case MSM_MI2S_SD1:
			dai_data->port_config.i2s.channel_mode =
					AFE_PORT_I2S_SD1;
			break;
		case MSM_MI2S_SD2:
			dai_data->port_config.i2s.channel_mode =
					AFE_PORT_I2S_SD2;
			break;
		case MSM_MI2S_SD3:
			dai_data->port_config.i2s.channel_mode =
					AFE_PORT_I2S_SD3;
			break;
		default:
			pr_err("%s: invalid SD line\n",
				   __func__);
			goto error_invalid_data;
		}
		break;
	case 2:
		switch (i2s_pdata->sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1:
			dai_data->port_config.i2s.channel_mode =
					AFE_PORT_I2S_QUAD01;
			break;
		case MSM_MI2S_SD2 | MSM_MI2S_SD3:
			dai_data->port_config.i2s.channel_mode =
					AFE_PORT_I2S_QUAD23;
			break;
		default:
			pr_err("%s: invalid SD line\n",
				   __func__);
			goto error_invalid_data;
		}
		break;
	case 3:
		switch (i2s_pdata->sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1 | MSM_MI2S_SD2:
			dai_data->port_config.i2s.channel_mode =
					AFE_PORT_I2S_6CHS;
			break;
		default:
			pr_err("%s: invalid SD lines\n",
				   __func__);
			goto error_invalid_data;
		}
		break;
	case 4:
		switch (i2s_pdata->sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1 | MSM_MI2S_SD2 | MSM_MI2S_SD3:
			dai_data->port_config.i2s.channel_mode =
					AFE_PORT_I2S_8CHS;
			break;
		default:
			pr_err("%s: invalid SD lines\n",
				   __func__);
			goto error_invalid_data;
		}
		break;
	default:
		pr_err("%s: invalid SD lines\n", __func__);
		goto error_invalid_data;
	}
	if (i2s_pdata->capability == MSM_MI2S_CAP_RX)
		dai_driver->playback.channels_max = num_of_sd_lines << 1;

	return 0;

error_invalid_data:
	return -EINVAL;
}

static int msm_dai_q6_cdc_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		dai_data->port_config.i2s.ws_src = 1; /* CPU is master */
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		dai_data->port_config.i2s.ws_src = 0; /* CPU is slave */
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static int msm_dai_q6_slim_bus_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	/* Q6 only supports 16 as now */
	dai_data->port_config.slim_sch.sb_cfg_minor_version =
				AFE_API_VERSION_SLIMBUS_CONFIG;
	dai_data->port_config.slim_sch.bit_width = 16;
	dai_data->port_config.slim_sch.data_format = 0;
	dai_data->port_config.slim_sch.num_channels = dai_data->channels;
	dai_data->port_config.slim_sch.sample_rate = dai_data->rate;

	dev_dbg(dai->dev, "%s:slimbus_dev_id[%hu] bit_wd[%hu] format[%hu]\n"
		"num_channel %hu  shared_ch_mapping[0]  %hu\n"
		"slave_port_mapping[1]  %hu slave_port_mapping[2]  %hu\n"
		"sample_rate %d\n", __func__,
		dai_data->port_config.slim_sch.slimbus_dev_id,
		dai_data->port_config.slim_sch.bit_width,
		dai_data->port_config.slim_sch.data_format,
		dai_data->port_config.slim_sch.num_channels,
		dai_data->port_config.slim_sch.shared_ch_mapping[0],
		dai_data->port_config.slim_sch.shared_ch_mapping[1],
		dai_data->port_config.slim_sch.shared_ch_mapping[2],
		dai_data->rate);

	return 0;
}

static int msm_dai_q6_bt_fm_hw_params(struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	dev_dbg(dai->dev, "channels %d sample rate %d entered\n",
		dai_data->channels, dai_data->rate);

	memset(&dai_data->port_config, 0, sizeof(dai_data->port_config));

	return 0;
}
static int msm_dai_q6_auxpcm_hw_params(
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	struct msm_dai_auxpcm_pdata *auxpcm_pdata =
			(struct msm_dai_auxpcm_pdata *) dai->dev->platform_data;

	if (params_channels(params) != 1) {
		dev_err(dai->dev, "AUX PCM supports only mono stream\n");
		return -EINVAL;
	}
	dai_data->channels = params_channels(params);

	if (params_rate(params) != 8000) {
		dev_err(dai->dev, "AUX PCM supports only 8KHz sampling rate\n");
		return -EINVAL;
	}
	dai_data->rate = params_rate(params);

	dai_data->port_config.pcm.pcm_cfg_minor_version =
				AFE_API_VERSION_PCM_CONFIG;
	dai_data->port_config.pcm.aux_mode = auxpcm_pdata->mode;
	dai_data->port_config.pcm.sync_src = auxpcm_pdata->sync;
	dai_data->port_config.pcm.frame_setting = auxpcm_pdata->frame;
	dai_data->port_config.pcm.quantype = auxpcm_pdata->quant;
	dai_data->port_config.pcm.ctrl_data_out_enable = auxpcm_pdata->data;
	dai_data->port_config.pcm.sample_rate = dai_data->rate;
	dai_data->port_config.pcm.num_channels = dai_data->channels;
	dai_data->port_config.pcm.bit_width = 16;
	dai_data->port_config.pcm.slot_number_mapping[0] = auxpcm_pdata->slot;

	return 0;
}

static int msm_dai_q6_afe_rtproxy_hw_params(struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->rate = params_rate(params);
	dai_data->port_config.rtproxy.num_channels = params_channels(params);
	dai_data->port_config.rtproxy.sample_rate = params_rate(params);

	pr_debug("channel %d entered,dai_id: %d,rate: %d\n",
	dai_data->port_config.rtproxy.num_channels, dai->id, dai_data->rate);

	dai_data->port_config.rtproxy.rt_proxy_cfg_minor_version =
				AFE_API_VERSION_RT_PROXY_CONFIG;
	dai_data->port_config.rtproxy.bit_width = 16; /* Q6 only supports 16 */
	dai_data->port_config.rtproxy.interleaved = 1;
	dai_data->port_config.rtproxy.frame_size = params_period_bytes(params);
	dai_data->port_config.rtproxy.jitter_allowance =
				dai_data->port_config.rtproxy.frame_size/2;
	dai_data->port_config.rtproxy.low_water_mark = 0;
	dai_data->port_config.rtproxy.high_water_mark = 0;

	return 0;
}

/* Current implementation assumes hw_param is called once
 * This may not be the case but what to do when ADM and AFE
 * port are already opened and parameter changes
 */
static int msm_dai_q6_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	int rc = 0;

	switch (dai->id) {
	case PRIMARY_I2S_TX:
	case PRIMARY_I2S_RX:
	case SECONDARY_I2S_RX:
		rc = msm_dai_q6_cdc_hw_params(params, dai, substream->stream);
		break;
	case MI2S_RX:
		rc = msm_dai_q6_i2s_hw_params(params, dai, substream->stream);
		break;
	case SLIMBUS_0_RX:
	case SLIMBUS_1_RX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
		rc = msm_dai_q6_slim_bus_hw_params(params, dai,
				substream->stream);
		break;
	case INT_BT_SCO_RX:
	case INT_BT_SCO_TX:
	case INT_FM_RX:
	case INT_FM_TX:
		rc = msm_dai_q6_bt_fm_hw_params(params, dai, substream->stream);
		break;
	case RT_PROXY_DAI_001_TX:
	case RT_PROXY_DAI_001_RX:
	case RT_PROXY_DAI_002_TX:
	case RT_PROXY_DAI_002_RX:
		rc = msm_dai_q6_afe_rtproxy_hw_params(params, dai);
		break;
	case VOICE_PLAYBACK_TX:
	case VOICE_RECORD_RX:
	case VOICE_RECORD_TX:
		rc = 0;
		break;
	default:
		dev_err(dai->dev, "invalid AFE port ID\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

static void msm_dai_q6_auxpcm_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int rc = 0;

	mutex_lock(&aux_pcm_mutex);

	if (aux_pcm_count == 0) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count is 0. Just"
				" return\n", __func__, dai->id);
		mutex_unlock(&aux_pcm_mutex);
		return;
	}

	aux_pcm_count--;

	if (aux_pcm_count > 0) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count = %d\n",
			__func__, dai->id, aux_pcm_count);
		mutex_unlock(&aux_pcm_mutex);
		return;
	} else if (aux_pcm_count < 0) {
		dev_err(dai->dev, "%s(): ERROR: dai->id %d"
			" aux_pcm_count = %d < 0\n",
			__func__, dai->id, aux_pcm_count);
		aux_pcm_count = 0;
		mutex_unlock(&aux_pcm_mutex);
		return;
	}

	pr_debug("%s: dai->id = %d aux_pcm_count = %d\n", __func__,
			dai->id, aux_pcm_count);

	rc = afe_close(PCM_RX); /* can block */
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close PCM_RX  AFE port\n");

	rc = afe_close(PCM_TX);
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM TX port\n");

	mutex_unlock(&aux_pcm_mutex);
}

static void msm_dai_q6_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		switch (dai->id) {
		case VOICE_PLAYBACK_TX:
		case VOICE_RECORD_TX:
		case VOICE_RECORD_RX:
			pr_debug("%s, stop pseudo port:%d\n",
						__func__,  dai->id);
			rc = afe_stop_pseudo_port(dai->id);
			break;
		default:
			rc = afe_close(dai->id); /* can block */
			break;
		}
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		pr_debug("%s: dai_data->status_mask = %ld\n", __func__,
			*dai_data->status_mask);
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
}

static int msm_dai_q6_auxpcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	mutex_lock(&aux_pcm_mutex);

	if (aux_pcm_count == 2) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count is 2. Just"
			" return.\n", __func__, dai->id);
		mutex_unlock(&aux_pcm_mutex);
		return 0;
	} else if (aux_pcm_count > 2) {
		dev_err(dai->dev, "%s(): ERROR: dai->id %d"
			" aux_pcm_count = %d > 2\n",
			__func__, dai->id, aux_pcm_count);
		mutex_unlock(&aux_pcm_mutex);
		return 0;
	}

	aux_pcm_count++;
	if (aux_pcm_count == 2)  {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count = %d after "
			" increment\n", __func__, dai->id, aux_pcm_count);
		mutex_unlock(&aux_pcm_mutex);
		return 0;
	}

	pr_debug("%s:dai->id:%d  aux_pcm_count = %d. opening afe\n",
			__func__, dai->id, aux_pcm_count);

	rc = afe_q6_interface_prepare();
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to open AFE APR\n");

	/*
	 * For AUX PCM Interface the below sequence of clk
	 * settings and afe_open is a strict requirement.
	 *
	 * Also using afe_open instead of afe_port_start_nowait
	 * to make sure the port is open before deasserting the
	 * clock line. This is required because pcm register is
	 * not written before clock deassert. Hence the hw does
	 * not get updated with new setting if the below clock
	 * assert/deasset and afe_open sequence is not followed.
	 */

	afe_open(PCM_RX, &dai_data->port_config, dai_data->rate);

	afe_open(PCM_TX, &dai_data->port_config, dai_data->rate);

	mutex_unlock(&aux_pcm_mutex);

	return rc;
}

static int msm_dai_q6_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		/* PORT START should be set if prepare called in active state */
		rc = afe_q6_interface_prepare();
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open AFE APR\n");
	}
	return rc;
}

static int msm_dai_q6_auxpcm_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	int rc = 0;

	pr_debug("%s:port:%d  cmd:%d  aux_pcm_count= %d",
		__func__, dai->id, cmd, aux_pcm_count);

	switch (cmd) {

	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* afe_open will be called from prepare */
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return 0;

	default:
		rc = -EINVAL;
	}

	return rc;

}

static int msm_dai_q6_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	/* Start/stop port without waiting for Q6 AFE response. Need to have
	 * native q6 AFE driver propagates AFE response in order to handle
	 * port start/stop command error properly if error does arise.
	 */
	pr_debug("%s:port:%d  cmd:%d dai_data->status_mask = %ld",
		__func__, dai->id, cmd, *dai_data->status_mask);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
			switch (dai->id) {
			case VOICE_PLAYBACK_TX:
			case VOICE_RECORD_TX:
			case VOICE_RECORD_RX:
				afe_pseudo_port_start_nowait(dai->id);
				break;
			default:
				afe_port_start_nowait(dai->id,
					&dai_data->port_config, dai_data->rate);
				break;
			}
			set_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
			switch (dai->id) {
			case VOICE_PLAYBACK_TX:
			case VOICE_RECORD_TX:
			case VOICE_RECORD_RX:
				afe_pseudo_port_stop_nowait(dai->id);
				break;
			default:
				afe_port_stop_nowait(dai->id);
				break;
			}
			clear_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);
		}
		break;

	default:
		rc = -EINVAL;
	}

	return rc;
}
static int msm_dai_q6_dai_auxpcm_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc = 0;

	struct msm_dai_auxpcm_pdata *auxpcm_pdata =
			(struct msm_dai_auxpcm_pdata *) dai->dev->platform_data;

	mutex_lock(&aux_pcm_mutex);

	if (!auxpcm_plat_data)
		auxpcm_plat_data = auxpcm_pdata;
	else if (auxpcm_plat_data != auxpcm_pdata) {

		dev_err(dai->dev, "AUX PCM RX and TX devices does not have"
				" same platform data\n");
		return -EINVAL;
	}

	/*
	 * The clk name for AUX PCM operation is passed as platform
	 * data to the cpu driver, since cpu drive is unaware of any
	 * boarc specific configuration.
	 */
	if (!pcm_clk)
		pcm_clk = clk_get(dai->dev, auxpcm_pdata->clk);

	mutex_unlock(&aux_pcm_mutex);

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data), GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	pr_err("%s : probe done for dai->id %d\n", __func__, dai->id);
	return rc;
}

static int msm_dai_q6_dai_auxpcm_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc;

	dai_data = dev_get_drvdata(dai->dev);

	mutex_lock(&aux_pcm_mutex);

	if (aux_pcm_count == 0) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count is 0. clean"
				" up and return\n", __func__, dai->id);
		goto done;
	}

	aux_pcm_count--;

	if (aux_pcm_count > 0) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count = %d\n",
			__func__, dai->id, aux_pcm_count);
		goto done;
	} else if (aux_pcm_count < 0) {
		dev_err(dai->dev, "%s(): ERROR: dai->id %d"
			" aux_pcm_count = %d < 0\n",
			__func__, dai->id, aux_pcm_count);
		goto done;
	}

	dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count = %d."
			"closing afe\n",
		__func__, dai->id, aux_pcm_count);

	rc = afe_close(PCM_RX); /* can block */
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM RX AFE port\n");

	rc = afe_close(PCM_TX);
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM TX AFE port\n");

done:
	kfree(dai_data);
	snd_soc_unregister_dai(dai->dev);

	mutex_unlock(&aux_pcm_mutex);

	return 0;
}
static int msm_dai_q6_dai_i2s_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc = 0;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data),
		GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
		goto rtn;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	rc = msm_dai_q6_i2s_platform_data_validation(dai);
	if (rc != 0) {
		pr_err("%s: The msm_dai_q6_i2s_platform_data_validation failed\n",
			    __func__);
		kfree(dai_data);
	}
rtn:
	return rc;
}

static int msm_dai_q6_dai_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc = 0;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data),
		GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	return rc;
}

static int msm_dai_q6_dai_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc;

	dai_data = dev_get_drvdata(dai->dev);

	/* If AFE port is still up, close it */
	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		switch (dai->id) {
		case VOICE_PLAYBACK_TX:
		case VOICE_RECORD_TX:
		case VOICE_RECORD_RX:
			pr_debug("%s, stop pseudo port:%d\n",
						__func__,  dai->id);
			rc = afe_stop_pseudo_port(dai->id);
			break;
		default:
			rc = afe_close(dai->id); /* can block */
		}
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
	kfree(dai_data);
	snd_soc_unregister_dai(dai->dev);

	return 0;
}

static int msm_dai_q6_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	int rc = 0;

	dev_dbg(dai->dev, "enter %s, id = %d fmt[%d]\n", __func__,
							dai->id, fmt);
	switch (dai->id) {
	case PRIMARY_I2S_TX:
	case PRIMARY_I2S_RX:
	case MI2S_RX:
	case SECONDARY_I2S_RX:
		rc = msm_dai_q6_cdc_set_fmt(dai, fmt);
		break;
	default:
		dev_err(dai->dev, "invalid cpu_dai set_fmt\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int msm_dai_q6_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)

{
	int rc = 0;
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	unsigned int i = 0;

	dev_dbg(dai->dev, "enter %s, id = %d\n", __func__,
							dai->id);
	switch (dai->id) {
	case SLIMBUS_0_RX:
	case SLIMBUS_1_RX:
		/* channel number to be between 128 and 255. For RX port
		 * use channel numbers from 138 to 144, for TX port
		 * use channel numbers from 128 to 137
		 * For ports between MDM-APQ use channel numbers from 145
		 */
		if (!rx_slot)
			return -EINVAL;
		for (i = 0; i < rx_num; i++) {
			dai_data->port_config.slim_sch.shared_ch_mapping[i] =
							rx_slot[i];
			pr_debug("%s: find number of channels[%d] ch[%d]\n",
							__func__, i,
							rx_slot[i]);
		}
		dai_data->port_config.slim_sch.num_channels = rx_num;
		pr_debug("%s:SLIMBUS_0_RX cnt[%d] ch[%d %d]\n", __func__,
		rx_num, dai_data->port_config.slim_sch.shared_ch_mapping[0],
		dai_data->port_config.slim_sch.shared_ch_mapping[1]);

		break;
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
		/* channel number to be between 128 and 255. For RX port
		 * use channel numbers from 138 to 144, for TX port
		 * use channel numbers from 128 to 137
		 * For ports between MDM-APQ use channel numbers from 145
		 */
		if (!tx_slot)
			return -EINVAL;
		for (i = 0; i < tx_num; i++) {
			dai_data->port_config.slim_sch.shared_ch_mapping[i] =
							tx_slot[i];
			pr_debug("%s: find number of channels[%d] ch[%d]\n",
						__func__, i, tx_slot[i]);
		}
		dai_data->port_config.slim_sch.num_channels = tx_num;
		pr_debug("%s:SLIMBUS_0_TX cnt[%d] ch[%d %d]\n", __func__,
		tx_num, dai_data->port_config.slim_sch.shared_ch_mapping[0],
		dai_data->port_config.slim_sch.shared_ch_mapping[1]);
		break;
	default:
		dev_err(dai->dev, "invalid cpu_dai set_fmt\n");
		rc = -EINVAL;
		break;
	}
	return rc;
}

static struct snd_soc_dai_ops msm_dai_q6_ops = {
	.prepare	= msm_dai_q6_prepare,
	.trigger	= msm_dai_q6_trigger,
	.hw_params	= msm_dai_q6_hw_params,
	.shutdown	= msm_dai_q6_shutdown,
	.set_fmt	= msm_dai_q6_set_fmt,
	.set_channel_map = msm_dai_q6_set_channel_map,
};

static struct snd_soc_dai_ops msm_dai_q6_auxpcm_ops = {
	.prepare	= msm_dai_q6_auxpcm_prepare,
	.trigger	= msm_dai_q6_auxpcm_trigger,
	.hw_params	= msm_dai_q6_auxpcm_hw_params,
	.shutdown	= msm_dai_q6_auxpcm_shutdown,
};

static struct snd_soc_dai_driver msm_dai_q6_i2s_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 4,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_i2s_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_i2s_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_afe_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_afe_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_voice_playback_tx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_max =     48000,
		.rate_min =     8000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_incall_record_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =     48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_bt_sco_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_bt_sco_tx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_fm_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 2,
		.channels_max = 2,
		.rate_max = 48000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_fm_tx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 2,
		.channels_max = 2,
		.rate_max = 48000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_aux_pcm_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 8000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_auxpcm_ops,
	.probe = msm_dai_q6_dai_auxpcm_probe,
	.remove = msm_dai_q6_dai_auxpcm_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_aux_pcm_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 8000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_auxpcm_ops,
	.probe = msm_dai_q6_dai_auxpcm_probe,
	.remove = msm_dai_q6_dai_auxpcm_remove,
};


static struct snd_soc_dai_driver msm_dai_q6_slimbus_1_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_min = 8000,
		.rate_max = 16000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_1_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_min = 8000,
		.rate_max = 16000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static __devinit int msm_dai_q6_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	dev_dbg(&pdev->dev, "dev name %s\n", dev_name(&pdev->dev));

	switch (pdev->id) {
	case PRIMARY_I2S_RX:
	case SECONDARY_I2S_RX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_i2s_rx_dai);
		break;
	case PRIMARY_I2S_TX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_i2s_tx_dai);
		break;
	case AFE_PORT_ID_PRIMARY_PCM_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_aux_pcm_rx_dai);
		break;
	case AFE_PORT_ID_PRIMARY_PCM_TX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_aux_pcm_tx_dai);
		break;
	case MI2S_RX:
		rc = snd_soc_register_dai(&pdev->dev,
					&msm_dai_q6_i2s_rx_dai);
		break;
	case SLIMBUS_0_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_rx_dai);
		break;
	case SLIMBUS_0_TX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_tx_dai);
		break;

	case SLIMBUS_1_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_1_rx_dai);
		break;
	case SLIMBUS_1_TX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_1_tx_dai);
		break;
	case INT_BT_SCO_RX:
		rc = snd_soc_register_dai(&pdev->dev,
					&msm_dai_q6_bt_sco_rx_dai);
		break;
	case INT_BT_SCO_TX:
		rc = snd_soc_register_dai(&pdev->dev,
					&msm_dai_q6_bt_sco_tx_dai);
		break;
	case INT_FM_RX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_fm_rx_dai);
		break;
	case INT_FM_TX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_fm_tx_dai);
		break;
	case RT_PROXY_DAI_001_RX:
	case RT_PROXY_DAI_002_RX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_afe_rx_dai);
		break;
	case RT_PROXY_DAI_001_TX:
	case RT_PROXY_DAI_002_TX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_afe_tx_dai);
		break;
	case VOICE_PLAYBACK_TX:
		rc = snd_soc_register_dai(&pdev->dev,
					&msm_dai_q6_voice_playback_tx_dai);
		break;
	case VOICE_RECORD_RX:
	case VOICE_RECORD_TX:
		rc = snd_soc_register_dai(&pdev->dev,
						&msm_dai_q6_incall_record_dai);
		break;
	default:
		rc = -ENODEV;
		break;
	}
	return rc;
}

static __devexit int msm_dai_q6_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver msm_dai_q6_driver = {
	.probe  = msm_dai_q6_dev_probe,
	.remove = msm_dai_q6_dev_remove,
	.driver = {
		.name = "msm-dai-q6",
		.owner = THIS_MODULE,
	},
};

static int __init msm_dai_q6_init(void)
{
	return platform_driver_register(&msm_dai_q6_driver);
}
module_init(msm_dai_q6_init);

static void __exit msm_dai_q6_exit(void)
{
	platform_driver_unregister(&msm_dai_q6_driver);
}
module_exit(msm_dai_q6_exit);

/* Module information */
MODULE_DESCRIPTION("MSM DSP DAI driver");
MODULE_LICENSE("GPL v2");
