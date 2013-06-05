/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <sound/apr_audio.h>
#include <sound/q6afe.h>
#include <sound/msm-dai-q6.h>
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
	u32 bitwidth;
	union afe_port_config port_config;
};

struct msm_dai_q6_mi2s_dai_config {
	u16 pdata_mi2s_lines;
	struct msm_dai_q6_dai_data mi2s_dai_data;
};

struct msm_dai_q6_mi2s_dai_data {
	struct msm_dai_q6_mi2s_dai_config tx_dai;
	struct msm_dai_q6_mi2s_dai_config rx_dai;
	struct snd_pcm_hw_constraint_list rate_constraint;
	struct snd_pcm_hw_constraint_list bitwidth_constraint;
};

static struct clk *pcm_clk;
static struct clk *sec_pcm_clk;
static DEFINE_MUTEX(aux_pcm_mutex);
static int aux_pcm_count;
static struct msm_dai_auxpcm_pdata *auxpcm_plat_data;
static struct msm_dai_auxpcm_pdata *sec_auxpcm_plat_data;

static int msm_dai_q6_mi2s_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{

	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	int value = ucontrol->value.integer.value[0];
	dai_data->port_config.mi2s.format = value;
	pr_debug("%s: value = %d, channel = %d, line = %d\n",
		   __func__, value, dai_data->port_config.mi2s.channel,
		   dai_data->port_config.mi2s.line);
	return 0;
}

static int msm_dai_q6_mi2s_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{

	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	ucontrol->value.integer.value[0] = dai_data->port_config.mi2s.format ;
	return 0;
}


/* MI2S format field for AFE_PORT_CMD_I2S_CONFIG command
 *  0: linear PCM
 *  1: non-linear PCM
 *  2: PCM data in IEC 60968 container
 *  3: compressed data in IEC 60958 container
 */
static const char *mi2s_format[] = {
	"LPCM",
	"Compr",
	"LPCM-60958",
	"Compr-60958"};

static const struct soc_enum mi2s_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(4, mi2s_format),
};

static const struct snd_kcontrol_new mi2s_config_controls[] = {
	SOC_ENUM_EXT("MI2S RX Format", mi2s_config_enum[0],
				 msm_dai_q6_mi2s_format_get,
				 msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("SEC RX Format", mi2s_config_enum[0],
				 msm_dai_q6_mi2s_format_get,
				 msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("MI2S TX Format", mi2s_config_enum[0],
				 msm_dai_q6_mi2s_format_get,
				 msm_dai_q6_mi2s_format_put),
};

static u8 num_of_bits_set(u8 sd_line_mask)
{
	u8 num_bits_set = 0;

	while (sd_line_mask) {
		num_bits_set++;
		sd_line_mask = sd_line_mask & (sd_line_mask - 1);
	}
	return num_bits_set;
}

static int msm_dai_q6_mi2s_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
		dev_get_drvdata(dai->dev);

	dev_dbg(dai->dev, "%s: cnst list %p\n", __func__,
		mi2s_dai_data->rate_constraint.list);

	if (mi2s_dai_data->rate_constraint.list) {
		snd_pcm_hw_constraint_list(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE,
				&mi2s_dai_data->rate_constraint);
		snd_pcm_hw_constraint_list(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
				&mi2s_dai_data->bitwidth_constraint);
	}

	return 0;
}

static int msm_dai_q6_mi2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
		dev_get_drvdata(dai->dev);
	struct msm_dai_q6_mi2s_dai_config *mi2s_dai_config =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		&mi2s_dai_data->rx_dai : &mi2s_dai_data->tx_dai);
	struct msm_dai_q6_dai_data *dai_data = &mi2s_dai_config->mi2s_dai_data;

	dai_data->channels = params_channels(params);
	switch (dai_data->channels) {
	case 8:
	case 7:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_I2S_8CHS)
			goto error_invalid_data;
		dai_data->port_config.mi2s.line = AFE_I2S_8CHS;
		break;
	case 6:
	case 5:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_I2S_6CHS)
			goto error_invalid_data;
		dai_data->port_config.mi2s.line = AFE_I2S_6CHS;
		break;
	case 4:
	case 3:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_I2S_QUAD01)
			goto error_invalid_data;
		if (mi2s_dai_config->pdata_mi2s_lines == AFE_I2S_QUAD23)
			dai_data->port_config.mi2s.line =
				mi2s_dai_config->pdata_mi2s_lines;
		else
			dai_data->port_config.mi2s.line = AFE_I2S_QUAD01;
		break;
	case 2:
	case 1:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_I2S_SD0)
			goto error_invalid_data;
		switch (mi2s_dai_config->pdata_mi2s_lines) {
		case AFE_I2S_SD0:
		case AFE_I2S_SD1:
		case AFE_I2S_SD2:
		case AFE_I2S_SD3:
			dai_data->port_config.mi2s.line =
				mi2s_dai_config->pdata_mi2s_lines;
			break;
		case AFE_I2S_QUAD01:
		case AFE_I2S_6CHS:
		case AFE_I2S_8CHS:
			dai_data->port_config.mi2s.line = AFE_I2S_SD0;
			break;
		case AFE_I2S_QUAD23:
			dai_data->port_config.mi2s.line = AFE_I2S_SD2;
			break;
		}
		if (dai_data->channels == 2)
			dai_data->port_config.mi2s.channel = MSM_AFE_STEREO;
		else
			dai_data->port_config.mi2s.channel = MSM_AFE_MONO;
		break;
	default:
		goto error_invalid_data;
	}
	dai_data->rate = params_rate(params);
	dai_data->port_config.mi2s.bitwidth = 16;
	dai_data->bitwidth = 16;
	if (!mi2s_dai_data->rate_constraint.list) {
		mi2s_dai_data->rate_constraint.list = &dai_data->rate;
		mi2s_dai_data->bitwidth_constraint.list = &dai_data->bitwidth;
	}

	pr_debug("%s: dai_data->channels = %d, line = %d\n", __func__,
			dai_data->channels, dai_data->port_config.mi2s.line);
	return 0;
error_invalid_data:
	pr_err("%s: dai_data->channels = %d, line = %d\n", __func__,
			 dai_data->channels, dai_data->port_config.mi2s.line);
	return -EINVAL;
}

static int msm_dai_q6_mi2s_get_lineconfig(u16 sd_lines, u16 *config_ptr,
	unsigned int *ch_cnt)
{
	u8 num_of_sd_lines;

	num_of_sd_lines = num_of_bits_set(sd_lines);

	switch (num_of_sd_lines) {
	case 0:
		pr_debug("%s: no line is assigned\n", __func__);
		break;
	case 1:
		switch (sd_lines) {
		case MSM_MI2S_SD0:
			*config_ptr = AFE_I2S_SD0;
			break;
		case MSM_MI2S_SD1:
			*config_ptr = AFE_I2S_SD1;
			break;
		case MSM_MI2S_SD2:
			*config_ptr = AFE_I2S_SD2;
			break;
		case MSM_MI2S_SD3:
			*config_ptr = AFE_I2S_SD3;
			break;
		default:
			pr_err("%s: invalid SD line\n",
				   __func__);
			goto error_invalid_data;
		}
		break;
	case 2:
		switch (sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1:
			*config_ptr = AFE_I2S_QUAD01;
			break;
		case MSM_MI2S_SD2 | MSM_MI2S_SD3:
			*config_ptr = AFE_I2S_QUAD23;
			break;
		default:
			pr_err("%s: invalid SD line\n",
				   __func__);
			goto error_invalid_data;
		}
		break;
	case 3:
		switch (sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1 | MSM_MI2S_SD2:
			*config_ptr = AFE_I2S_6CHS;
			break;
		default:
			pr_err("%s: invalid SD lines\n",
				   __func__);
			goto error_invalid_data;
		}
		break;
	case 4:
		switch (sd_lines) {
		case MSM_MI2S_SD0 | MSM_MI2S_SD1 | MSM_MI2S_SD2 | MSM_MI2S_SD3:
			*config_ptr = AFE_I2S_8CHS;
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

	*ch_cnt = num_of_sd_lines;

	return 0;

error_invalid_data:
	return -EINVAL;
}

static int msm_dai_q6_mi2s_platform_data_validation(
	struct platform_device *pdev, struct snd_soc_dai_driver *dai_driver)
{
	struct msm_dai_q6_mi2s_dai_data *dai_data = dev_get_drvdata(&pdev->dev);
	struct msm_mi2s_pdata *mi2s_pdata =
			(struct msm_mi2s_pdata *) pdev->dev.platform_data;
	u16 sdline_config;
	unsigned int ch_cnt;
	int rc = 0;

	if ((mi2s_pdata->rx_sd_lines & mi2s_pdata->tx_sd_lines) ||
	    (!mi2s_pdata->rx_sd_lines && !mi2s_pdata->tx_sd_lines)) {
		dev_err(&pdev->dev,
			"error sd line conflict or no line assigned\n");
		rc = -EINVAL;
		goto rtn;
	}

	rc = msm_dai_q6_mi2s_get_lineconfig(mi2s_pdata->rx_sd_lines,
					    &sdline_config, &ch_cnt);

	if (IS_ERR_VALUE(rc)) {
		dev_err(&pdev->dev, "invalid MI2S RX sd line config\n");
		goto rtn;
	}

	if (ch_cnt) {
		dai_data->rx_dai.mi2s_dai_data.port_config.mi2s.line =
			sdline_config;
		dai_data->rx_dai.pdata_mi2s_lines = sdline_config;
		dai_driver->playback.channels_min = 1;
		dai_driver->playback.channels_max = ch_cnt << 1;
	} else {
		dai_driver->playback.channels_min = 0;
		dai_driver->playback.channels_max = 0;
	}
	rc = msm_dai_q6_mi2s_get_lineconfig(mi2s_pdata->tx_sd_lines,
					    &sdline_config, &ch_cnt);

	if (IS_ERR_VALUE(rc)) {
		dev_err(&pdev->dev, "invalid MI2S TX sd line config\n");
		goto rtn;
	}

	if (ch_cnt) {
		dai_data->tx_dai.mi2s_dai_data.port_config.mi2s.line =
			sdline_config;
		dai_data->tx_dai.pdata_mi2s_lines = sdline_config;
		dai_driver->capture.channels_min = 1;
		dai_driver->capture.channels_max = ch_cnt << 1;
	} else {
		dai_driver->capture.channels_min = 0;
		dai_driver->capture.channels_max = 0;
	}

	dev_info(&pdev->dev, "%s: playback sdline %x capture sdline %x\n",
		 __func__, dai_data->rx_dai.pdata_mi2s_lines,
		 dai_data->tx_dai.pdata_mi2s_lines);
	dev_info(&pdev->dev, "%s: playback ch_max %d capture ch_mx %d\n",
		 __func__, dai_driver->playback.channels_max,
		 dai_driver->capture.channels_max);
rtn:
	return rc;
}

static int msm_dai_q6_mi2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
	dev_get_drvdata(dai->dev);

	if (test_bit(STATUS_PORT_STARTED,
		mi2s_dai_data->rx_dai.mi2s_dai_data.status_mask) ||
	    test_bit(STATUS_PORT_STARTED,
		mi2s_dai_data->tx_dai.mi2s_dai_data.status_mask)) {
		dev_err(dai->dev, "%s: err chg i2s mode while dai running",
			__func__);
		return -EPERM;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		mi2s_dai_data->rx_dai.mi2s_dai_data.port_config.mi2s.ws = 1;
		mi2s_dai_data->tx_dai.mi2s_dai_data.port_config.mi2s.ws = 1;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		mi2s_dai_data->rx_dai.mi2s_dai_data.port_config.mi2s.ws = 0;
		mi2s_dai_data->tx_dai.mi2s_dai_data.port_config.mi2s.ws = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int msm_dai_q6_mi2s_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
		dev_get_drvdata(dai->dev);
	struct msm_dai_q6_dai_data *dai_data =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		 &mi2s_dai_data->rx_dai.mi2s_dai_data :
		 &mi2s_dai_data->tx_dai.mi2s_dai_data);
	u16 port_id = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		       MI2S_RX : MI2S_TX);
	int rc = 0;

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		/* PORT START should be set if prepare called in active state */
		rc = afe_port_start(port_id, &dai_data->port_config,
				    dai_data->rate);

		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open AFE port %x\n",
				dai->id);
		else
			set_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);
	}

	return rc;
}

static void msm_dai_q6_mi2s_shutdown(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
		dev_get_drvdata(dai->dev);
	struct msm_dai_q6_dai_data *dai_data =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		 &mi2s_dai_data->rx_dai.mi2s_dai_data :
		 &mi2s_dai_data->tx_dai.mi2s_dai_data);
	u16 port_id = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		MI2S_RX : MI2S_TX);
	int rc = 0;

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_close(port_id);
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}

	if (!test_bit(STATUS_PORT_STARTED,
			mi2s_dai_data->rx_dai.mi2s_dai_data.status_mask) &&
	    !test_bit(STATUS_PORT_STARTED,
			mi2s_dai_data->rx_dai.mi2s_dai_data.status_mask)) {
		mi2s_dai_data->rate_constraint.list = NULL;
		mi2s_dai_data->bitwidth_constraint.list = NULL;
	}

}

static int msm_dai_q6_cdc_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	switch (dai_data->channels) {
	case 2:
	case 4:
	case 6:
	case 8:
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

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	/* Q6 only supports 16 as now */
	dai_data->port_config.slim_sch.bit_width = 16;
	dai_data->port_config.slim_sch.data_format = 0;
	dai_data->port_config.slim_sch.num_channels = dai_data->channels;
	dai_data->port_config.slim_sch.reserved = 0;

	dev_dbg(dai->dev, "%s:slimbus_dev_id[%hu] bit_wd[%hu] format[%hu]\n"
		"num_channel %hu  slave_ch_mapping[0]  %hu\n"
		"slave_port_mapping[1]  %hu slave_port_mapping[2]  %hu\n"
		"slave_port_mapping[3]  %hu\n sample_rate %d\n", __func__,
		dai_data->port_config.slim_sch.slimbus_dev_id,
		dai_data->port_config.slim_sch.bit_width,
		dai_data->port_config.slim_sch.data_format,
		dai_data->port_config.slim_sch.num_channels,
		dai_data->port_config.slim_sch.slave_ch_mapping[0],
		dai_data->port_config.slim_sch.slave_ch_mapping[1],
		dai_data->port_config.slim_sch.slave_ch_mapping[2],
		dai_data->port_config.slim_sch.slave_ch_mapping[3],
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

	dai_data->rate = params_rate(params);
	switch (dai_data->rate) {
	case 8000:
		dai_data->port_config.pcm.mode = auxpcm_pdata->mode_8k.mode;
		dai_data->port_config.pcm.sync = auxpcm_pdata->mode_8k.sync;
		dai_data->port_config.pcm.frame = auxpcm_pdata->mode_8k.frame;
		dai_data->port_config.pcm.quant = auxpcm_pdata->mode_8k.quant;
		dai_data->port_config.pcm.slot = auxpcm_pdata->mode_8k.slot;
		dai_data->port_config.pcm.data = auxpcm_pdata->mode_8k.data;
		break;
	case 16000:
		dai_data->port_config.pcm.mode = auxpcm_pdata->mode_16k.mode;
		dai_data->port_config.pcm.sync = auxpcm_pdata->mode_16k.sync;
		dai_data->port_config.pcm.frame = auxpcm_pdata->mode_16k.frame;
		dai_data->port_config.pcm.quant = auxpcm_pdata->mode_16k.quant;
		dai_data->port_config.pcm.slot = auxpcm_pdata->mode_16k.slot;
		dai_data->port_config.pcm.data = auxpcm_pdata->mode_16k.data;
		break;
	default:
		dev_err(dai->dev, "AUX PCM supports only 8kHz and 16kHz sampling rate\n");
		return -EINVAL;
	}

	return 0;
}

static int msm_dai_q6_sec_auxpcm_hw_params(
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	struct msm_dai_auxpcm_pdata *auxpcm_pdata =
			(struct msm_dai_auxpcm_pdata *) dai->dev->platform_data;

	pr_debug("%s\n", __func__);
	if (params_channels(params) != 1) {
		dev_err(dai->dev, "SEC AUX PCM supports only mono stream\n");
		return -EINVAL;
	}
	dai_data->channels = params_channels(params);

	dai_data->rate = params_rate(params);
	switch (dai_data->rate) {
	case 8000:
		dai_data->port_config.pcm.mode = auxpcm_pdata->mode_8k.mode;
		dai_data->port_config.pcm.sync = auxpcm_pdata->mode_8k.sync;
		dai_data->port_config.pcm.frame = auxpcm_pdata->mode_8k.frame;
		dai_data->port_config.pcm.quant = auxpcm_pdata->mode_8k.quant;
		dai_data->port_config.pcm.slot = auxpcm_pdata->mode_8k.slot;
		dai_data->port_config.pcm.data = auxpcm_pdata->mode_8k.data;
		break;
	case 16000:
		dai_data->port_config.pcm.mode = auxpcm_pdata->mode_16k.mode;
		dai_data->port_config.pcm.sync = auxpcm_pdata->mode_16k.sync;
		dai_data->port_config.pcm.frame = auxpcm_pdata->mode_16k.frame;
		dai_data->port_config.pcm.quant = auxpcm_pdata->mode_16k.quant;
		dai_data->port_config.pcm.slot = auxpcm_pdata->mode_16k.slot;
		dai_data->port_config.pcm.data = auxpcm_pdata->mode_16k.data;
		break;
	default:
		dev_err(dai->dev, "AUX PCM supports only 8kHz and 16kHz sampling rate\n");
		return -EINVAL;
	}

	return 0;
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
	dai_data->port_config.rtproxy.frame_sz = params_period_bytes(params);
	dai_data->port_config.rtproxy.jitter =
				dai_data->port_config.rtproxy.frame_sz/2;
	dai_data->port_config.rtproxy.lw_mark = 0;
	dai_data->port_config.rtproxy.hw_mark = 0;
	dai_data->port_config.rtproxy.rsvd = 0;

	return 0;
}

static int msm_dai_q6_pseudo_hw_params(struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->rate = params_rate(params);
	dai_data->channels = params_channels(params) > 6 ?
				params_channels(params) : 6;

	dai_data->port_config.pseudo.bit_width = 16;
	dai_data->port_config.pseudo.num_channels =
			dai_data->channels;
	dai_data->port_config.pseudo.data_format = 0;
	dai_data->port_config.pseudo.timing_mode = 1;
	dai_data->port_config.pseudo.reserved = 16;
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
	case SECONDARY_I2S_TX:
		rc = msm_dai_q6_cdc_hw_params(params, dai, substream->stream);
		break;

	case SLIMBUS_0_RX:
	case SLIMBUS_1_RX:
	case SLIMBUS_3_RX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_RX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_RX:
	case SLIMBUS_4_TX:
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
	case PSEUDOPORT_01:
		rc = msm_dai_q6_pseudo_hw_params(params, dai);
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

	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	mutex_lock(&aux_pcm_mutex);
	dev_dbg(dai->dev, "%s dai->id = %d", __func__, dai->id);
	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		mutex_unlock(&aux_pcm_mutex);
		return;
	}
	clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
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

	clk_disable_unprepare(pcm_clk);
	rc = afe_close(PCM_RX); /* can block */
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close PCM_RX  AFE port\n");

	rc = afe_close(PCM_TX);
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM TX port\n");

	mutex_unlock(&aux_pcm_mutex);
}

static void msm_dai_q6_sec_auxpcm_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int rc = 0;

	pr_debug("%s\n", __func__);
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

	clk_disable_unprepare(sec_pcm_clk);
	rc = afe_close(SECONDARY_PCM_RX); /* can block */
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close PCM_RX  AFE port\n");

	rc = afe_close(SECONDARY_PCM_TX);
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
	struct msm_dai_auxpcm_pdata *auxpcm_pdata =
			(struct msm_dai_auxpcm_pdata *) dai->dev->platform_data;
	unsigned long pcm_clk_rate;

	mutex_lock(&aux_pcm_mutex);
	set_bit(STATUS_PORT_STARTED,
			dai_data->status_mask);
	dev_dbg(dai->dev, "%s dai->id = %d", __func__, dai->id);
	aux_pcm_count++;
	if (aux_pcm_count >= 2) {
		dev_dbg(dai->dev, "%s(): dai->id %d aux_pcm_count = %d >= 2\n",
			__func__, dai->id, aux_pcm_count);
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
	 * settings and opening of afe port is a strict requirement.
	 * afe_port_start is called to make sure to make sure the port
	 * is open before deasserting the clock line. This is
	 * required because pcm register is not written before
	 * clock deassert. Hence the hw does not get updated with
	 * new setting if the below clock assert/deasset and afe_port_start
	 * sequence is not followed.
	 */

	clk_reset(pcm_clk, CLK_RESET_ASSERT);

	afe_port_start(PCM_RX, &dai_data->port_config, dai_data->rate);

	afe_port_start(PCM_TX, &dai_data->port_config, dai_data->rate);
	if (dai_data->rate == 8000) {
		pcm_clk_rate = auxpcm_pdata->mode_8k.pcm_clk_rate;
	} else if (dai_data->rate == 16000) {
		pcm_clk_rate = auxpcm_pdata->mode_16k.pcm_clk_rate;
	} else {
		dev_err(dai->dev, "%s: Invalid AUX PCM rate %d\n", __func__,
			  dai_data->rate);
		return -EINVAL;
	}

	rc = clk_set_rate(pcm_clk, pcm_clk_rate);
	if (rc < 0) {
		pr_err("%s: clk_set_rate failed\n", __func__);
		return rc;
	}

	clk_prepare_enable(pcm_clk);
	clk_reset(pcm_clk, CLK_RESET_DEASSERT);

	mutex_unlock(&aux_pcm_mutex);

	return rc;
}

static int msm_dai_q6_sec_auxpcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;
	struct msm_dai_auxpcm_pdata *auxpcm_pdata =
			(struct msm_dai_auxpcm_pdata *) dai->dev->platform_data;
	unsigned long pcm_clk_rate;

	pr_info("%s\n", __func__);

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
	 * settings and opening of afe port is a strict requirement.
	 * afe_port_start is called to make sure to make sure the port
	 * is open before deasserting the clock line. This is
	 * required because pcm register is not written before
	 * clock deassert. Hence the hw does not get updated with
	 * new setting if the below clock assert/deasset and afe_port_start
	 * sequence is not followed.
	 */

	clk_reset(sec_pcm_clk, CLK_RESET_ASSERT);

	afe_port_start(SECONDARY_PCM_RX, &dai_data->port_config,
		       dai_data->rate);

	afe_port_start(SECONDARY_PCM_TX, &dai_data->port_config,
		       dai_data->rate);
	if (dai_data->rate == 8000) {
		pcm_clk_rate = auxpcm_pdata->mode_8k.pcm_clk_rate;
	} else if (dai_data->rate == 16000) {
		pcm_clk_rate = auxpcm_pdata->mode_16k.pcm_clk_rate;
	} else {
		dev_err(dai->dev, "%s: Invalid AUX PCM rate %d\n", __func__,
			  dai_data->rate);
		return -EINVAL;
	}

	rc = clk_set_rate(sec_pcm_clk, pcm_clk_rate);
	if (rc < 0) {
		pr_err("%s: clk_set_rate failed\n", __func__);
		return rc;
	}

	clk_prepare_enable(sec_pcm_clk);
	clk_reset(sec_pcm_clk, CLK_RESET_DEASSERT);

	mutex_unlock(&aux_pcm_mutex);

	return rc;
}

static int msm_dai_q6_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		switch (dai->id) {
		case VOICE_PLAYBACK_TX:
		case VOICE_RECORD_TX:
		case VOICE_RECORD_RX:
			rc = afe_start_pseudo_port(dai->id);
			break;
		default:
			rc = afe_port_start(dai->id, &dai_data->port_config,
					    dai_data->rate);
		}

		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open AFE port %x\n",
				dai->id);
		else
			set_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);
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
	if (!pcm_clk) {

		pcm_clk = clk_get(dai->dev, auxpcm_pdata->clk);

		if (IS_ERR(pcm_clk)) {
			pr_err("%s: could not get pcm_clk\n", __func__);
			pcm_clk = NULL;
			return -ENODEV;
		}
	}

	mutex_unlock(&aux_pcm_mutex);

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data), GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	pr_debug("%s : probe done for dai->id %d\n", __func__, dai->id);
	return rc;
}

static int msm_dai_q6_dai_sec_auxpcm_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc = 0;
	struct msm_dai_auxpcm_pdata *auxpcm_pdata =
			(struct msm_dai_auxpcm_pdata *) dai->dev->platform_data;

	pr_info("%s\n", __func__);

	mutex_lock(&aux_pcm_mutex);

	if (!sec_auxpcm_plat_data)
		sec_auxpcm_plat_data = auxpcm_pdata;
	else if (sec_auxpcm_plat_data != auxpcm_pdata) {
		dev_err(dai->dev, "AUX PCM RX and TX devices does not have"
				" same platform data sec_auxpcm_plat_data\n");
		return -EINVAL;
	}

	/*
	 * The clk name for AUX PCM operation is passed as platform
	 * data to the cpu driver, since cpu drive is unaware of any
	 * boarc specific configuration.
	 */
	if (!sec_pcm_clk) {

		sec_pcm_clk = clk_get(dai->dev, auxpcm_pdata->clk);
		if (IS_ERR(sec_pcm_clk)) {
			pr_err("%s: could not get sec_pcm_clk\n", __func__);
			sec_pcm_clk = NULL;
			return -ENODEV;
		}
	}

	mutex_unlock(&aux_pcm_mutex);

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data), GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	pr_debug("%s : probe done for dai->id %d\n", __func__, dai->id);
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

static int msm_dai_q6_dai_sec_auxpcm_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc;

	pr_debug("%s\n", __func__);
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

	rc = afe_close(SECONDARY_PCM_RX); /* can block */
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM RX AFE port\n");

	rc = afe_close(SECONDARY_PCM_TX);
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM TX AFE port\n");

done:
	kfree(dai_data);
	snd_soc_unregister_dai(dai->dev);

	mutex_unlock(&aux_pcm_mutex);

	return 0;
}

static int msm_dai_q6_dai_mi2s_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
		dev_get_drvdata(dai->dev);
	struct snd_kcontrol *kcontrol = NULL;
	int rc = 0;

	if (mi2s_dai_data->rx_dai.mi2s_dai_data.port_config.mi2s.line) {
		kcontrol = snd_ctl_new1(&mi2s_config_controls[0],
					&mi2s_dai_data->rx_dai.mi2s_dai_data);
		rc = snd_ctl_add(dai->card->snd_card, kcontrol);

		if (IS_ERR_VALUE(rc)) {
			dev_err(dai->dev, "%s: err add RX fmt ctl\n", __func__);
			goto rtn;
		}
	}

	if (mi2s_dai_data->tx_dai.mi2s_dai_data.port_config.mi2s.line) {
		rc = snd_ctl_add(dai->card->snd_card,
				snd_ctl_new1(&mi2s_config_controls[2],
				&mi2s_dai_data->tx_dai.mi2s_dai_data));

		if (IS_ERR_VALUE(rc)) {
			if (kcontrol)
				snd_ctl_remove(dai->card->snd_card, kcontrol);
			dev_err(dai->dev, "%s: err add TX fmt ctl\n", __func__);
		}
	}

rtn:
	return rc;
}

static int msm_dai_q6_dai_mi2s_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
		dev_get_drvdata(dai->dev);
	int rc;

	/* If AFE port is still up, close it */
	if (test_bit(STATUS_PORT_STARTED,
			mi2s_dai_data->rx_dai.mi2s_dai_data.status_mask)) {
		rc = afe_close(MI2S_RX); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close MI2S_RX port\n");
		clear_bit(STATUS_PORT_STARTED,
			  mi2s_dai_data->rx_dai.mi2s_dai_data.status_mask);
	}
	if (test_bit(STATUS_PORT_STARTED,
			mi2s_dai_data->tx_dai.mi2s_dai_data.status_mask)) {
		rc = afe_close(MI2S_TX); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close MI2S_TX port\n");
		clear_bit(STATUS_PORT_STARTED,
			  mi2s_dai_data->tx_dai.mi2s_dai_data.status_mask);
	}
	kfree(mi2s_dai_data);
	snd_soc_unregister_dai(dai->dev);

	return 0;
}

static int msm_dai_q6_dai_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc = 0;
	const struct snd_kcontrol_new *kcontrol;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data),
		GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);
	if (dai->id == SECONDARY_I2S_RX) {
		kcontrol = &mi2s_config_controls[1];
		rc = snd_ctl_add(dai->card->snd_card,
				 snd_ctl_new1(kcontrol, dai_data));
	}

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
	case SECONDARY_I2S_RX:
	case SECONDARY_I2S_TX:
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

	dev_dbg(dai->dev, "%s: dai_id = %d\n", __func__, dai->id);
	switch (dai->id) {
	case SLIMBUS_0_RX:
	case SLIMBUS_1_RX:
	case SLIMBUS_2_RX:
	case SLIMBUS_3_RX:
	case SLIMBUS_4_RX:
		/* channel number to be between 128 and 255. For RX port
		 * use channel numbers from 138 to 144, for TX port
		 * use channel numbers from 128 to 137
		 * For ports between MDM-APQ use channel numbers from 145
		 */
		if (!rx_slot)
			return -EINVAL;
		for (i = 0; i < rx_num; i++) {
			dai_data->port_config.slim_sch.slave_ch_mapping[i] =
							rx_slot[i];
			pr_debug("%s: find number of channels[%d] ch[%d]\n",
							__func__, i,
							rx_slot[i]);
		}
		dai_data->port_config.slim_sch.num_channels = rx_num;
		pr_debug("%s:SLIMBUS_%d_RX cnt[%d] ch[%d %d]\n", __func__,
				(dai->id - SLIMBUS_0_RX) / 2,
		rx_num, dai_data->port_config.slim_sch.slave_ch_mapping[0],
		dai_data->port_config.slim_sch.slave_ch_mapping[1]);

		break;
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_TX:
		/* channel number to be between 128 and 255. For RX port
		 * use channel numbers from 138 to 144, for TX port
		 * use channel numbers from 128 to 137
		 * For ports between MDM-APQ use channel numbers from 145
		 */
		if (!tx_slot)
			return -EINVAL;
		for (i = 0; i < tx_num; i++) {
			dai_data->port_config.slim_sch.slave_ch_mapping[i] =
							tx_slot[i];
			pr_debug("%s: find number of channels[%d] ch[%d]\n",
						__func__, i, tx_slot[i]);
		}
		dai_data->port_config.slim_sch.num_channels = tx_num;
		pr_debug("%s:SLIMBUS_%d_TX cnt[%d] ch[%d %d]\n", __func__,
			(dai->id - SLIMBUS_0_TX) / 2,
		tx_num, dai_data->port_config.slim_sch.slave_ch_mapping[0],
		dai_data->port_config.slim_sch.slave_ch_mapping[1]);
		break;
	default:
		dev_err(dai->dev, "invalid cpu_dai set_fmt\n");
		rc = -EINVAL;
		break;
	}
	return rc;
}

static struct snd_soc_dai_ops msm_dai_q6_mi2s_ops = {
	.startup	= msm_dai_q6_mi2s_startup,
	.prepare	= msm_dai_q6_mi2s_prepare,
	.hw_params	= msm_dai_q6_mi2s_hw_params,
	.shutdown	= msm_dai_q6_mi2s_shutdown,
	.set_fmt	= msm_dai_q6_mi2s_set_fmt,
};

static struct snd_soc_dai_ops msm_dai_q6_ops = {
	.prepare	= msm_dai_q6_prepare,
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

static struct snd_soc_dai_ops msm_dai_q6_sec_auxpcm_ops = {
	.prepare	= msm_dai_q6_sec_auxpcm_prepare,
	.trigger	= msm_dai_q6_auxpcm_trigger,
	.hw_params	= msm_dai_q6_sec_auxpcm_hw_params,
	.shutdown	= msm_dai_q6_sec_auxpcm_shutdown,
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
		.channels_max = 4,
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
	.capture = {
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
	.capture = {
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
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_auxpcm_ops,
	.probe = msm_dai_q6_dai_auxpcm_probe,
	.remove = msm_dai_q6_dai_auxpcm_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_aux_pcm_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_auxpcm_ops,
	.probe = msm_dai_q6_dai_auxpcm_probe,
	.remove = msm_dai_q6_dai_auxpcm_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_sec_aux_pcm_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_sec_auxpcm_ops,
	.probe = msm_dai_q6_dai_sec_auxpcm_probe,
	.remove = msm_dai_q6_dai_sec_auxpcm_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_sec_aux_pcm_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_sec_auxpcm_ops,
	.probe = msm_dai_q6_dai_sec_auxpcm_probe,
	.remove = msm_dai_q6_dai_sec_auxpcm_remove,
};

/* Channel min and max are initialized base on platform data */
static struct snd_soc_dai_driver msm_dai_q6_mi2s_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min =     8000,
		.rate_max =     48000,
	},
	.ops = &msm_dai_q6_mi2s_ops,
	.probe = msm_dai_q6_dai_mi2s_probe,
	.remove = msm_dai_q6_dai_mi2s_remove,
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

static struct snd_soc_dai_driver msm_dai_q6_slimbus_2_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_96000 |
		SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =	192000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_2_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_96000 |
		SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 8,
		.rate_min =     8000,
		.rate_max =	192000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_3_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
		SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};
static struct snd_soc_dai_driver msm_dai_q6_pseudo_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
		SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 6,
		.rate_min = 8000,
		.rate_max = 48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
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
	case SECONDARY_I2S_TX:
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

	case SECONDARY_PCM_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_sec_aux_pcm_rx_dai);
		break;
	case SECONDARY_PCM_TX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_sec_aux_pcm_tx_dai);
		break;
	case SLIMBUS_0_RX:
	case SLIMBUS_4_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_rx_dai);
		break;
	case SLIMBUS_0_TX:
	case SLIMBUS_4_TX:
	case SLIMBUS_3_TX:
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
	case SLIMBUS_2_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_2_rx_dai);
		break;
	case SLIMBUS_2_TX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_2_tx_dai);
		break;
	case SLIMBUS_3_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_3_rx_dai);
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
	case PSEUDOPORT_01:
		rc = snd_soc_register_dai(&pdev->dev,
					&msm_dai_q6_pseudo_dai);
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

static __devinit int msm_dai_q6_mi2s_dev_probe(struct platform_device *pdev)
{
	struct msm_dai_q6_mi2s_dai_data *dai_data;
	int rc = 0;

	dev_dbg(&pdev->dev, "%s: pdev %p dev %p\n", __func__, pdev, &pdev->dev);

	dai_data = kzalloc(sizeof(struct msm_dai_q6_mi2s_dai_data),
		GFP_KERNEL);

	if (!dai_data) {
		dev_err(&pdev->dev, "fail to allocate dai data\n");
		rc = -ENOMEM;
		goto rtn;
	} else
		dev_set_drvdata(&pdev->dev, dai_data);

	rc = msm_dai_q6_mi2s_platform_data_validation(pdev,
						      &msm_dai_q6_mi2s_dai);
	if (IS_ERR_VALUE(rc))
		goto err_pdata;

	dai_data->rate_constraint.count = 1;
	dai_data->bitwidth_constraint.count = 1;
	rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_mi2s_dai);

	if (IS_ERR_VALUE(rc))
		goto err_pdata;

	return 0;

err_pdata:

	dev_err(&pdev->dev, "fail to msm_dai_q6_mi2s_dev_probe\n");
	kfree(dai_data);
rtn:
	return rc;
}

static __devexit int msm_dai_q6_mi2s_dev_remove(struct platform_device *pdev)
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

static struct platform_driver msm_dai_q6_mi2s_driver = {
	.probe  = msm_dai_q6_mi2s_dev_probe,
	.remove = msm_dai_q6_mi2s_dev_remove,
	.driver = {
		.name = "msm-dai-q6-mi2s",
		.owner = THIS_MODULE,
	},
};

static int __init msm_dai_q6_init(void)
{
	int rc1, rc2;

	rc1 = platform_driver_register(&msm_dai_q6_mi2s_driver);

	if (IS_ERR_VALUE(rc1))
		pr_err("%s: fail to register mi2s dai driver\n", __func__);

	rc2 = platform_driver_register(&msm_dai_q6_driver);

	if (IS_ERR_VALUE(rc2))
		pr_err("%s: fail to register mi2s dai driver\n", __func__);

	return (IS_ERR_VALUE(rc1) && IS_ERR_VALUE(rc2)) ? -1 : 0;
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
