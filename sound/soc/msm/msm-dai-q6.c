/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/mfd/wcd9310/core.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio.h>
#include <sound/q6afe.h>
#include <sound/msm-dai-q6.h>
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
		dai_data->port_config.mi2s.channel = MSM_AFE_STEREO;
		break;
	case 1:
		dai_data->port_config.mi2s.channel = MSM_AFE_MONO;
		break;
	default:
		return -EINVAL;
		break;
	}
	dai_data->rate = params_rate(params);

	dev_dbg(dai->dev, " channel %d sample rate %d entered\n",
	dai_data->channels, dai_data->rate);

	/* Q6 only supports 16 as now */
	dai_data->port_config.mi2s.bitwidth = 16;
	dai_data->port_config.mi2s.line = 1;
	return 0;
}

static int msm_dai_q6_mi2s_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	struct msm_mi2s_data *mi2s_pdata =
			(struct msm_mi2s_data *) dai->dev->platform_data;

	dai_data->channels = params_channels(params);
	if (num_of_bits_set(mi2s_pdata->sd_lines) == 1) {
		switch (dai_data->channels) {
		case 2:
			dai_data->port_config.mi2s.channel = MSM_AFE_STEREO;
			break;
		case 1:
			dai_data->port_config.mi2s.channel = MSM_AFE_MONO;
			break;
		default:
			pr_warn("greater than stereo has not been validated");
			break;
		}
	}
	/* Q6 only supports 16 as now */
	dai_data->port_config.mi2s.bitwidth = 16;

	return 0;
}

static int msm_dai_q6_mi2s_platform_data_validation(
					struct snd_soc_dai *dai)
{
	u8 num_of_sd_lines;
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	struct msm_mi2s_data *mi2s_pdata =
			(struct msm_mi2s_data *)dai->dev->platform_data;
	struct snd_soc_dai_driver *dai_driver =
			(struct snd_soc_dai_driver *)dai->driver;

	num_of_sd_lines = num_of_bits_set(mi2s_pdata->sd_lines);

	switch (num_of_sd_lines) {
	case 1:
		switch (mi2s_pdata->sd_lines) {
		case MSM_MI2S_SD0:
			dai_data->port_config.mi2s.line = AFE_I2S_SD0;
			break;
		case MSM_MI2S_SD1:
			dai_data->port_config.mi2s.line = AFE_I2S_SD1;
			break;
		case MSM_MI2S_SD2:
			dai_data->port_config.mi2s.line = AFE_I2S_SD2;
			break;
		case MSM_MI2S_SD3:
			dai_data->port_config.mi2s.line = AFE_I2S_SD3;
			break;
		default:
			pr_err("%s: invalid SD line\n",
				   __func__);
			goto error_invalid_data;
		}
		break;
	case 2:
		switch (mi2s_pdata->sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1:
			dai_data->port_config.mi2s.line = AFE_I2S_QUAD01;
			break;
		case MSM_MI2S_SD2 | MSM_MI2S_SD3:
			dai_data->port_config.mi2s.line = AFE_I2S_QUAD23;
			break;
		default:
			pr_err("%s: invalid SD line\n",
				   __func__);
			goto error_invalid_data;
		}
		break;
	case 3:
		switch (mi2s_pdata->sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1 | MSM_MI2S_SD2:
			dai_data->port_config.mi2s.line = AFE_I2S_6CHS;
			break;
		default:
			pr_err("%s: invalid SD lines\n",
				   __func__);
			goto error_invalid_data;
		}
		break;
	case 4:
		switch (mi2s_pdata->sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1 | MSM_MI2S_SD2 | MSM_MI2S_SD3:
			dai_data->port_config.mi2s.line = AFE_I2S_8CHS;
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
	if (mi2s_pdata->capability == MSM_MI2S_CAP_RX)
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
		dai_data->port_config.mi2s.ws = 1; /* CPU is master */
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		dai_data->port_config.mi2s.ws = 0; /* CPU is slave */
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
	u8 pgd_la, inf_la;
	u16 *slave_port_mapping;

	memset(dai_data->port_config.slimbus.slave_port_mapping, 0,
		sizeof(dai_data->port_config.slimbus.slave_port_mapping));

	dai_data->channels = params_channels(params);

	slave_port_mapping = dai_data->port_config.slimbus.slave_port_mapping;

	switch (dai_data->channels) {
	case 4:
		if (dai->id == SLIMBUS_0_TX) {
			slave_port_mapping[0] = 7;
			slave_port_mapping[1] = 8;
			slave_port_mapping[2] = 9;
			slave_port_mapping[3] = 10;
		} else {
			return -EINVAL;
		}
		break;
	case 3:
		if (dai->id == SLIMBUS_0_TX) {
			slave_port_mapping[0] = 7;
			slave_port_mapping[1] = 8;
			slave_port_mapping[2] = 9;
		} else {
			return -EINVAL;
		}
		break;
	case 2:
		if (dai->id == SLIMBUS_0_RX) {
			slave_port_mapping[0] = 1;
			slave_port_mapping[1] = 2;
		} else {
			slave_port_mapping[0] = 7;
			slave_port_mapping[1] = 8;
		}
		break;
	case 1:
		if (dai->id == SLIMBUS_0_RX)
			slave_port_mapping[0] = 1;
		else
			slave_port_mapping[0] = 7;
		break;
	default:
		return -EINVAL;
		break;
	}
	dai_data->rate = params_rate(params);
	tabla_get_logical_addresses(&pgd_la, &inf_la);

	dai_data->port_config.slimbus.slimbus_dev_id =  AFE_SLIMBUS_DEVICE_1;
	dai_data->port_config.slimbus.slave_dev_pgd_la = pgd_la;
	dai_data->port_config.slimbus.slave_dev_intfdev_la = inf_la;
	/* Q6 only supports 16 as now */
	dai_data->port_config.slimbus.bit_width = 16;
	dai_data->port_config.slimbus.data_format = 0;
	dai_data->port_config.slimbus.num_channels = dai_data->channels;
	dai_data->port_config.slimbus.reserved = 0;

	dev_dbg(dai->dev, "slimbus_dev_id  %hu  slave_dev_pgd_la 0x%hx\n"
		"slave_dev_intfdev_la 0x%hx   bit_width %hu   data_format %hu\n"
		"num_channel %hu  slave_port_mapping[0]  %hu\n"
		"slave_port_mapping[1]  %hu slave_port_mapping[2]  %hu\n"
		"sample_rate %d\n",
		dai_data->port_config.slimbus.slimbus_dev_id,
		dai_data->port_config.slimbus.slave_dev_pgd_la,
		dai_data->port_config.slimbus.slave_dev_intfdev_la,
		dai_data->port_config.slimbus.bit_width,
		dai_data->port_config.slimbus.data_format,
		dai_data->port_config.slimbus.num_channels,
		dai_data->port_config.slimbus.slave_port_mapping[0],
		dai_data->port_config.slimbus.slave_port_mapping[1],
		dai_data->port_config.slimbus.slave_port_mapping[2],
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
	dai_data->port_config.pcm.mode = auxpcm_pdata->mode;
	dai_data->port_config.pcm.sync = auxpcm_pdata->sync;
	dai_data->port_config.pcm.frame = auxpcm_pdata->frame;
	dai_data->port_config.pcm.quant = auxpcm_pdata->quant;
	dai_data->port_config.pcm.slot = auxpcm_pdata->slot;
	dai_data->port_config.pcm.data = auxpcm_pdata->data;

	return 0;
}

static int get_frame_size(u16 rate, u16 ch)
{
	if (rate == 8000) {
		if (ch == 1)
			return 128 * 2;
		else
			return 128 * 2 * 2;
	} else if (rate == 16000) {
		if (ch == 1)
			return 128 * 2 * 2;
		else
			return 128 * 2 * 4;
	} else if (rate == 48000) {
		if (ch == 1)
			return 128 * 2 * 6;
		else
			return 128 * 2 * 12;
	} else
		return 128 * 2 * 12;
}

static int msm_dai_q6_afe_rtproxy_hw_params(struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->rate = params_rate(params);
	dai_data->port_config.rtproxy.num_ch =
			params_channels(params);

	pr_debug("channel %d entered,dai_id: %d,rate: %d\n",
	dai_data->port_config.rtproxy.num_ch, dai->id, dai_data->rate);

	dai_data->port_config.rtproxy.bitwidth = 16; /* Q6 only supports 16 */
	dai_data->port_config.rtproxy.interleaved = 1;
	dai_data->port_config.rtproxy.frame_sz = get_frame_size(dai_data->rate,
					dai_data->port_config.rtproxy.num_ch);
	dai_data->port_config.rtproxy.jitter =
				dai_data->port_config.rtproxy.frame_sz/2;
	dai_data->port_config.rtproxy.lw_mark = 0;
	dai_data->port_config.rtproxy.hw_mark = 0;
	dai_data->port_config.rtproxy.rsvd = 0;

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
		rc = msm_dai_q6_mi2s_hw_params(params, dai, substream->stream);
		break;
	case SLIMBUS_0_RX:
	case SLIMBUS_0_TX:
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
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	pr_debug("%s: dai->id = %d", __func__, dai->id);

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		clk_disable(pcm_clk);
		rc = afe_close(dai->id); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		pr_debug("%s: dai_data->status_mask = %ld\n", __func__,
			*dai_data->status_mask);
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);

		rc = afe_close(PCM_TX);
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AUX PCM TX port\n");
	}
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

	struct msm_dai_auxpcm_pdata *auxpcm_pdata =
			(struct msm_dai_auxpcm_pdata *) dai->dev->platform_data;

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_q6_interface_prepare();
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open AFE APR\n");

		rc = afe_q6_interface_prepare();
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open AFE APR\n");

		pr_debug("%s:dai->id:%d dai_data->status_mask = %ld\n",
			__func__, dai->id, *dai_data->status_mask);

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

		clk_reset(pcm_clk, CLK_RESET_ASSERT);

		afe_open(dai->id, &dai_data->port_config,
			dai_data->rate);
		set_bit(STATUS_PORT_STARTED,
			dai_data->status_mask);

		afe_open(PCM_TX, &dai_data->port_config, dai_data->rate);

		rc = clk_set_rate(pcm_clk, auxpcm_pdata->pcm_clk_rate);
		if (rc < 0) {
			pr_err("%s: clk_set_rate failed\n", __func__);
			return rc;
		}

		clk_enable(pcm_clk);
		clk_reset(pcm_clk, CLK_RESET_DEASSERT);

	}
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
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	pr_debug("%s:port:%d  cmd:%d dai_data->status_mask = %ld",
		__func__, dai->id, cmd, *dai_data->status_mask);

	switch (cmd) {

	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* afe_open will be called from prepare */
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {

			clk_disable(pcm_clk);
			afe_port_stop_nowait(dai->id);
			clear_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);

			afe_port_stop_nowait(PCM_TX);
		}
		break;

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

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data),
		GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	/*
	 * The clk name for AUX PCM operation is passed as platform
	 * data to the cpu driver, since cpu drive is unaware of any
	 * boarc specific configuration.
	 */
	pcm_clk = clk_get(NULL, auxpcm_pdata->clk);
	if (IS_ERR(pcm_clk)) {
		pr_err("%s: could not get pcm_clk\n", __func__);
		return PTR_ERR(pcm_clk);
		kfree(dai_data);
	}

	return rc;
}

static int msm_dai_q6_dai_auxpcm_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc;

	dai_data = dev_get_drvdata(dai->dev);

	/* If AFE port is still up, close it */
	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_close(dai->id); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);

		rc = afe_close(PCM_TX);
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AUX PCM TX port\n");
	}


	kfree(dai_data);
	snd_soc_unregister_dai(dai->dev);

	return 0;
}
static int msm_dai_q6_dai_mi2s_probe(struct snd_soc_dai *dai)
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

	rc = msm_dai_q6_mi2s_platform_data_validation(dai);
	if (rc != 0) {
		pr_err("%s: The msm_dai_q6_mi2s_platform_data_validation failed\n",
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

	dev_dbg(dai->dev, "enter %s, id = %d\n", __func__, dai->id);
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

static struct snd_soc_dai_ops msm_dai_q6_ops = {
	.prepare	= msm_dai_q6_prepare,
	.trigger	= msm_dai_q6_trigger,
	.hw_params	= msm_dai_q6_hw_params,
	.shutdown	= msm_dai_q6_shutdown,
	.set_fmt	= msm_dai_q6_set_fmt,
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
	.probe = msm_dai_q6_dai_probe,
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
};

static struct snd_soc_dai_driver msm_dai_q6_mi2s_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_mi2s_probe,
	.remove = msm_dai_q6_dai_probe,
};

/* To do: change to register DAIs as batch */
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
	case PCM_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_aux_pcm_rx_dai);
		break;
	case PCM_TX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_aux_pcm_tx_dai);
		break;
	case MI2S_RX:
		rc = snd_soc_register_dai(&pdev->dev,
					&msm_dai_q6_mi2s_rx_dai);
		break;
	case SLIMBUS_0_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_rx_dai);
		break;
	case SLIMBUS_0_TX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_tx_dai);
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
