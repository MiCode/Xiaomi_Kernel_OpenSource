/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/msm-dai-q6-v2.h>
#include <sound/pcm_params.h>
#include <mach/clk.h>

#define MSM_DAI_PRI_AUXPCM_DT_DEV_ID 1
#define MSM_DAI_SEC_AUXPCM_DT_DEV_ID 2

static const struct afe_clk_cfg lpass_clk_cfg_default = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_OSR_CLK_2_P048_MHZ,
	0,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_CLK1_VALID,
	0,
};
enum {
	STATUS_PORT_STARTED, /* track if AFE port has started */
	/* track AFE Tx port status for bi-directional transfers */
	STATUS_TX_PORT,
	/* track AFE Rx port status for bi-directional transfers */
	STATUS_RX_PORT,
	STATUS_MAX
};

enum {
	RATE_8KHZ,
	RATE_16KHZ,
	RATE_MAX_NUM_OF_AUX_PCM_RATES,
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
};

struct msm_dai_q6_auxpcm_dai_data {
	/* BITMAP to track Rx and Tx port usage count */
	DECLARE_BITMAP(auxpcm_port_status, STATUS_MAX);
	struct mutex rlock; /* auxpcm dev resource lock */
	u16 rx_pid; /* AUXPCM RX AFE port ID */
	u16 tx_pid; /* AUXPCM TX AFE port ID */
	struct afe_clk_cfg clk_cfg; /* hold LPASS clock configuration */
	struct msm_dai_q6_dai_data bdai_data; /* incoporate base DAI data */
};

/* MI2S format field for AFE_PORT_CMD_I2S_CONFIG command
 *  0: linear PCM
 *  1: non-linear PCM
 *  2: PCM data in IEC 60968 container
 *  3: compressed data in IEC 60958 container
 */
static const char *const mi2s_format[] = {
	"LPCM",
	"Compr",
	"LPCM-60958",
	"Compr-60958"
};

static const struct soc_enum mi2s_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(4, mi2s_format),
};

static int msm_dai_q6_auxpcm_hw_params(
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_auxpcm_dai_data *aux_dai_data =
			dev_get_drvdata(dai->dev);
	struct msm_dai_q6_dai_data *dai_data = &aux_dai_data->bdai_data;
	struct msm_dai_auxpcm_pdata *auxpcm_pdata =
			(struct msm_dai_auxpcm_pdata *) dai->dev->platform_data;
	int rc = 0;

	if (params_channels(params) != 1 || (params_rate(params) != 8000 &&
	    params_rate(params) != 16000)) {
		dev_err(dai->dev, "%s: invalid param chan %d rate %d\n",
			__func__, params_channels(params), params_rate(params));
		return -EINVAL;
	}

	mutex_lock(&aux_dai_data->rlock);

	if (test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status) ||
	    test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status)) {
		/* AUXPCM DAI in use */
		if (dai_data->rate != params_rate(params)) {
			dev_err(dai->dev, "%s: rate mismatch of running DAI\n",
			__func__);
			rc = -EINVAL;
		}
		mutex_unlock(&aux_dai_data->rlock);
		return rc;
	}

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	if (dai_data->rate == 8000) {
		dai_data->port_config.pcm.pcm_cfg_minor_version =
				AFE_API_VERSION_PCM_CONFIG;
		dai_data->port_config.pcm.aux_mode = auxpcm_pdata->mode_8k.mode;
		dai_data->port_config.pcm.sync_src = auxpcm_pdata->mode_8k.sync;
		dai_data->port_config.pcm.frame_setting =
					auxpcm_pdata->mode_8k.frame;
		dai_data->port_config.pcm.quantype =
					 auxpcm_pdata->mode_8k.quant;
		dai_data->port_config.pcm.ctrl_data_out_enable =
					 auxpcm_pdata->mode_8k.data;
		dai_data->port_config.pcm.sample_rate = dai_data->rate;
		dai_data->port_config.pcm.num_channels = dai_data->channels;
		dai_data->port_config.pcm.bit_width = 16;
		dai_data->port_config.pcm.slot_number_mapping[0] =
					 auxpcm_pdata->mode_8k.slot;
	} else {
		dai_data->port_config.pcm.pcm_cfg_minor_version =
				AFE_API_VERSION_PCM_CONFIG;
		dai_data->port_config.pcm.aux_mode =
					auxpcm_pdata->mode_16k.mode;
		dai_data->port_config.pcm.sync_src =
					auxpcm_pdata->mode_16k.sync;
		dai_data->port_config.pcm.frame_setting =
					auxpcm_pdata->mode_16k.frame;
		dai_data->port_config.pcm.quantype =
					auxpcm_pdata->mode_16k.quant;
		dai_data->port_config.pcm.ctrl_data_out_enable =
					auxpcm_pdata->mode_16k.data;
		dai_data->port_config.pcm.sample_rate = dai_data->rate;
		dai_data->port_config.pcm.num_channels = dai_data->channels;
		dai_data->port_config.pcm.bit_width = 16;
		dai_data->port_config.pcm.slot_number_mapping[0] =
					auxpcm_pdata->mode_16k.slot;
	}

	dev_dbg(dai->dev, "%s: aux_mode %x sync_src %x frame_setting %x\n",
		__func__, dai_data->port_config.pcm.aux_mode,
		dai_data->port_config.pcm.sync_src,
		dai_data->port_config.pcm.frame_setting);
	dev_dbg(dai->dev, "%s: qtype %x dout %x num_map %x\n",
		__func__, dai_data->port_config.pcm.quantype,
		dai_data->port_config.pcm.ctrl_data_out_enable,
		dai_data->port_config.pcm.slot_number_mapping[0]);

	mutex_unlock(&aux_dai_data->rlock);
	return rc;
}

static void msm_dai_q6_auxpcm_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int rc = 0;
	struct afe_clk_cfg *lpass_pcm_src_clk = NULL;
	struct msm_dai_q6_auxpcm_dai_data *aux_dai_data =
		dev_get_drvdata(dai->dev);

	mutex_lock(&aux_dai_data->rlock);

	if (!(test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status) ||
	      test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status))) {
		dev_dbg(dai->dev, "%s(): dai->id %d PCM ports already closed\n",
				__func__, dai->id);
		goto exit;
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status))
			clear_bit(STATUS_TX_PORT,
				  aux_dai_data->auxpcm_port_status);
		else {
			dev_dbg(dai->dev, "%s(): PCM_TX port already closed\n",
				__func__);
			goto exit;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status))
			clear_bit(STATUS_RX_PORT,
				  aux_dai_data->auxpcm_port_status);
		else {
			dev_dbg(dai->dev, "%s(): PCM_RX port already closed\n",
				__func__);
			goto exit;
		}
	}
	if (test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status) ||
	    test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status)) {
		dev_dbg(dai->dev, "%s(): cannot shutdown PCM ports\n",
			__func__);
		goto exit;
	}

	dev_dbg(dai->dev, "%s: dai->id = %d closing PCM AFE ports\n",
			__func__, dai->id);

	lpass_pcm_src_clk = (struct afe_clk_cfg *) &aux_dai_data->clk_cfg;

	rc = afe_close(aux_dai_data->rx_pid); /* can block */
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close PCM_RX  AFE port\n");

	rc = afe_close(aux_dai_data->tx_pid);
	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AUX PCM TX port\n");

	lpass_pcm_src_clk->clk_val1 = 0;
	afe_set_lpass_clock(aux_dai_data->rx_pid, lpass_pcm_src_clk);
	afe_set_lpass_clock(aux_dai_data->tx_pid, lpass_pcm_src_clk);

exit:
	mutex_unlock(&aux_dai_data->rlock);
	return;
}

static int msm_dai_q6_auxpcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_auxpcm_dai_data *aux_dai_data =
		dev_get_drvdata(dai->dev);
	struct msm_dai_q6_dai_data *dai_data = &aux_dai_data->bdai_data;
	struct msm_dai_auxpcm_pdata *auxpcm_pdata = NULL;
	int rc = 0;
	unsigned long pcm_clk_rate;
	struct afe_clk_cfg *lpass_pcm_src_clk = NULL;

	auxpcm_pdata = dai->dev->platform_data;
	lpass_pcm_src_clk = (struct afe_clk_cfg *) &aux_dai_data->clk_cfg;

	mutex_lock(&aux_dai_data->rlock);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (test_bit(STATUS_TX_PORT,
				aux_dai_data->auxpcm_port_status)) {
			dev_dbg(dai->dev, "%s(): PCM_TX port already ON\n",
				__func__);
			goto exit;
		} else
			set_bit(STATUS_TX_PORT,
				  aux_dai_data->auxpcm_port_status);
	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (test_bit(STATUS_RX_PORT,
				aux_dai_data->auxpcm_port_status)) {
			dev_dbg(dai->dev, "%s(): PCM_RX port already ON\n",
				__func__);
			goto exit;
		} else
			set_bit(STATUS_RX_PORT,
				  aux_dai_data->auxpcm_port_status);
	}
	if (test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status) &&
	    test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status)) {
		dev_dbg(dai->dev, "%s(): PCM ports already set\n", __func__);
		goto exit;
	}

	dev_dbg(dai->dev, "%s: dai->id:%d  opening afe ports\n",
			__func__, dai->id);

	rc = afe_q6_interface_prepare();
	if (IS_ERR_VALUE(rc)) {
		dev_err(dai->dev, "fail to open AFE APR\n");
		goto fail;
	}

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

	if (dai_data->rate == 8000) {
		pcm_clk_rate = auxpcm_pdata->mode_8k.pcm_clk_rate;
	} else if (dai_data->rate == 16000) {
		pcm_clk_rate = (auxpcm_pdata->mode_16k.pcm_clk_rate);
	} else {
		dev_err(dai->dev, "%s: Invalid AUX PCM rate %d\n", __func__,
			dai_data->rate);
		rc = -EINVAL;
		goto fail;
	}

	memcpy(lpass_pcm_src_clk, &lpass_clk_cfg_default,
			sizeof(struct afe_clk_cfg));
	lpass_pcm_src_clk->clk_val1 = pcm_clk_rate;

	rc = afe_set_lpass_clock(aux_dai_data->rx_pid, lpass_pcm_src_clk);
	if (rc < 0) {
		dev_err(dai->dev,
			"%s:afe_set_lpass_clock on RX pcm_src_clk failed\n",
			__func__);
		goto fail;
	}

	rc = afe_set_lpass_clock(aux_dai_data->tx_pid, lpass_pcm_src_clk);
	if (rc < 0) {
		dev_err(dai->dev,
			"%s:afe_set_lpass_clock on TX pcm_src_clk failed\n",
			__func__);
		goto fail;
	}

	afe_open(aux_dai_data->rx_pid, &dai_data->port_config, dai_data->rate);
	afe_open(aux_dai_data->tx_pid, &dai_data->port_config, dai_data->rate);
	goto exit;

fail:
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		clear_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status);
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		clear_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status);

exit:
	mutex_unlock(&aux_dai_data->rlock);
	return rc;
}

static int msm_dai_q6_auxpcm_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	int rc = 0;

	pr_debug("%s:port:%d  cmd:%d\n",
		__func__, dai->id, cmd);

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

static int msm_dai_q6_dai_auxpcm_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_auxpcm_dai_data *aux_dai_data;
	struct afe_clk_cfg *lpass_pcm_src_clk = NULL;
	int rc;

	aux_dai_data = dev_get_drvdata(dai->dev);

	dev_dbg(dai->dev, "%s(): dai->id %d closing afe\n",
		__func__, dai->id);

	if (test_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status) ||
	    test_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status)) {
		rc = afe_close(aux_dai_data->rx_pid); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AUXPCM RX AFE port\n");
		rc = afe_close(aux_dai_data->tx_pid);
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AUXPCM TX AFE port\n");
		clear_bit(STATUS_TX_PORT, aux_dai_data->auxpcm_port_status);
		clear_bit(STATUS_RX_PORT, aux_dai_data->auxpcm_port_status);
	}

	lpass_pcm_src_clk = (struct afe_clk_cfg *) &aux_dai_data->clk_cfg;
	lpass_pcm_src_clk->clk_val1 = 0;
	afe_set_lpass_clock(aux_dai_data->rx_pid, lpass_pcm_src_clk);
	afe_set_lpass_clock(aux_dai_data->tx_pid, lpass_pcm_src_clk);

	return 0;
}

static struct snd_soc_dai_ops msm_dai_q6_auxpcm_ops = {
	.prepare	= msm_dai_q6_auxpcm_prepare,
	.trigger	= msm_dai_q6_auxpcm_trigger,
	.hw_params	= msm_dai_q6_auxpcm_hw_params,
	.shutdown	= msm_dai_q6_auxpcm_shutdown,
};

static struct snd_soc_dai_driver msm_dai_q6_aux_pcm_dai = {
	.playback = {
		.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.capture = {
		.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_auxpcm_ops,
	.remove = msm_dai_q6_dai_auxpcm_remove,
};

static int msm_dai_q6_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_port_start(dai->id, &dai_data->port_config,
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

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_SPECIAL:
		dai_data->port_config.i2s.bit_width = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		dai_data->port_config.i2s.bit_width = 24;
		break;
	default:
		return -EINVAL;
	}

	dai_data->rate = params_rate(params);
	dai_data->port_config.i2s.sample_rate = dai_data->rate;
	dai_data->port_config.i2s.i2s_cfg_minor_version =
						AFE_API_VERSION_I2S_CONFIG;
	dai_data->port_config.i2s.data_format =  AFE_LINEAR_PCM_DATA;
	dev_dbg(dai->dev, " channel %d sample rate %d entered\n",
	dai_data->channels, dai_data->rate);

	dai_data->port_config.i2s.channel_mode = 1;
	return 0;
}

static u8 num_of_bits_set(u8 sd_line_mask)
{
	u8 num_bits_set = 0;

	while (sd_line_mask) {
		num_bits_set++;
		sd_line_mask = sd_line_mask & (sd_line_mask - 1);
	}
	return num_bits_set;
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

static int msm_dai_q6_slim_bus_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_SPECIAL:
		dai_data->port_config.slim_sch.bit_width = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		dai_data->port_config.slim_sch.bit_width = 24;
		break;
	default:
		return -EINVAL;
	}

	dai_data->port_config.slim_sch.sb_cfg_minor_version =
				AFE_API_VERSION_SLIMBUS_CONFIG;
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

	pr_debug("%s: setting bt_fm parameters\n", __func__);

	dai_data->port_config.int_bt_fm.bt_fm_cfg_minor_version =
					AFE_API_VERSION_INTERNAL_BT_FM_CONFIG;
	dai_data->port_config.int_bt_fm.num_channels = dai_data->channels;
	dai_data->port_config.int_bt_fm.sample_rate = dai_data->rate;
	dai_data->port_config.int_bt_fm.bit_width = 16;

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

static int msm_dai_q6_psuedo_port_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	/* Q6 only supports 16 as now */
	dai_data->port_config.pseudo_port.pseud_port_cfg_minor_version =
				AFE_API_VERSION_PSEUDO_PORT_CONFIG;
	dai_data->port_config.pseudo_port.num_channels =
				params_channels(params);
	dai_data->port_config.pseudo_port.bit_width = 16;
	dai_data->port_config.pseudo_port.data_format = 0;
	dai_data->port_config.pseudo_port.timing_mode =
				AFE_PSEUDOPORT_TIMING_MODE_TIMER;
	dai_data->port_config.pseudo_port.sample_rate = params_rate(params);

	dev_dbg(dai->dev, "%s: bit_wd[%hu] num_channels [%hu] format[%hu]\n"
		"timing Mode %hu sample_rate %d\n", __func__,
		dai_data->port_config.pseudo_port.bit_width,
		dai_data->port_config.pseudo_port.num_channels,
		dai_data->port_config.pseudo_port.data_format,
		dai_data->port_config.pseudo_port.timing_mode,
		dai_data->port_config.pseudo_port.sample_rate);

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
	case SLIMBUS_2_RX:
	case SLIMBUS_3_RX:
	case SLIMBUS_4_RX:
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_TX:
	case SLIMBUS_5_TX:
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
	case VOICE2_PLAYBACK_TX:
	case VOICE_RECORD_RX:
	case VOICE_RECORD_TX:
		rc = msm_dai_q6_psuedo_port_hw_params(params,
						dai, substream->stream);
		break;
	default:
		dev_err(dai->dev, "invalid AFE port ID\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

static void msm_dai_q6_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		pr_debug("%s, stop pseudo port:%d\n", __func__,  dai->id);
		rc = afe_close(dai->id); /* can block */

		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		pr_debug("%s: dai_data->status_mask = %ld\n", __func__,
			*dai_data->status_mask);
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
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

	dev_dbg(dai->dev, "enter %s, id = %d\n", __func__, dai->id);
	switch (dai->id) {
	case SLIMBUS_0_RX:
	case SLIMBUS_1_RX:
	case SLIMBUS_2_RX:
	case SLIMBUS_3_RX:
	case SLIMBUS_4_RX:
		/*
		 * channel number to be between 128 and 255.
		 * For RX port use channel numbers
		 * from 138 to 144 for pre-Taiko
		 * from 144 to 159 for Taiko
		 */
		if (!rx_slot)
			return -EINVAL;
		for (i = 0; i < rx_num; i++) {
			dai_data->port_config.slim_sch.shared_ch_mapping[i] =
			    rx_slot[i];
			pr_debug("%s: find number of channels[%d] ch[%d]\n",
			       __func__, i, rx_slot[i]);
		}
		dai_data->port_config.slim_sch.num_channels = rx_num;
		pr_debug("%s:SLIMBUS_%d_RX cnt[%d] ch[%d %d]\n", __func__,
			(dai->id - SLIMBUS_0_RX) / 2, rx_num,
			dai_data->port_config.slim_sch.shared_ch_mapping[0],
			dai_data->port_config.slim_sch.shared_ch_mapping[1]);

		break;
	case SLIMBUS_0_TX:
	case SLIMBUS_1_TX:
	case SLIMBUS_2_TX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_TX:
	case SLIMBUS_5_TX:
		/*
		 * channel number to be between 128 and 255.
		 * For TX port use channel numbers
		 * from 128 to 137 for pre-Taiko
		 * from 128 to 143 for Taiko
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
		pr_debug("%s:SLIMBUS_%d_TX cnt[%d] ch[%d %d]\n", __func__,
			(dai->id - SLIMBUS_0_TX) / 2, tx_num,
			dai_data->port_config.slim_sch.shared_ch_mapping[0],
			dai_data->port_config.slim_sch.shared_ch_mapping[1]);
		break;
	default:
		dev_err(dai->dev, "invalid cpu_dai id %d\n", dai->id);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static struct snd_soc_dai_ops msm_dai_q6_ops = {
	.prepare	= msm_dai_q6_prepare,
	.hw_params	= msm_dai_q6_hw_params,
	.shutdown	= msm_dai_q6_shutdown,
	.set_fmt	= msm_dai_q6_set_fmt,
	.set_channel_map = msm_dai_q6_set_channel_map,
};

static int msm_dai_q6_dai_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc = 0;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data), GFP_KERNEL);

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
		pr_debug("%s, stop pseudo port:%d\n", __func__,  dai->id);
		rc = afe_close(dai->id); /* can block */

		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
	kfree(dai_data);
	snd_soc_unregister_dai(dai->dev);

	return 0;
}

static struct snd_soc_dai_driver msm_dai_q6_afe_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
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
		.channels_max = 8,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_1_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
		SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
		SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 192000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_1_tx_dai = {
	.capture = {
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

static struct snd_soc_dai_driver msm_dai_q6_voice_playback_tx_dai = {
	.playback = {
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

static int __devinit msm_auxpcm_dev_probe(struct platform_device *pdev)
{
	struct msm_dai_q6_auxpcm_dai_data *dai_data;
	struct msm_dai_auxpcm_pdata *auxpcm_pdata;
	uint32_t val_array[RATE_MAX_NUM_OF_AUX_PCM_RATES];
	const char *intf_name;
	int rc = 0;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_auxpcm_dai_data),
			   GFP_KERNEL);
	if (!dai_data) {
		dev_err(&pdev->dev,
			"Failed to allocate memory for auxpcm DAI data\n");
		return -ENOMEM;
	}

	auxpcm_pdata = kzalloc(sizeof(struct msm_dai_auxpcm_pdata),
				GFP_KERNEL);

	if (!auxpcm_pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory for platform data\n");
		goto fail_pdata_nomem;
	}

	dev_dbg(&pdev->dev, "%s: dev %p, dai_data %p, auxpcm_pdata %p\n",
		__func__, &pdev->dev, dai_data, auxpcm_pdata);

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-mode",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-mode missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.mode = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.mode = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-sync",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-sync missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.sync = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.sync = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-frame",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);

	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-frame missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.frame = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.frame = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-quant",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-quant missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.quant = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.quant = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-slot",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-slot missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.slot = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.slot = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-data",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-cpudai-auxpcm-data missing in DT node\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.data = (u16)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.data = (u16)val_array[RATE_16KHZ];

	rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-cpudai-auxpcm-pcm-clk-rate",
			val_array, RATE_MAX_NUM_OF_AUX_PCM_RATES);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: qcom,msm-cpudai-auxpcm-pcm-clk-rate missing in DT\n",
			__func__);
		goto fail_invalid_dt;
	}
	auxpcm_pdata->mode_8k.pcm_clk_rate = (int)val_array[RATE_8KHZ];
	auxpcm_pdata->mode_16k.pcm_clk_rate = (int)val_array[RATE_16KHZ];

	rc = of_property_read_string(pdev->dev.of_node,
			"qcom,msm-auxpcm-interface", &intf_name);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: qcom,msm-auxpcm-interface missing in DT node\n",
			__func__);
		goto fail_nodev_intf;
	}

	if (!strncmp(intf_name, "primary", sizeof("primary"))) {
		dai_data->rx_pid = AFE_PORT_ID_PRIMARY_PCM_RX;
		dai_data->tx_pid = AFE_PORT_ID_PRIMARY_PCM_TX;
		pdev->id = MSM_DAI_PRI_AUXPCM_DT_DEV_ID;
	} else if (!strncmp(intf_name, "secondary", sizeof("secondary"))) {
		dai_data->rx_pid = AFE_PORT_ID_SECONDARY_PCM_RX;
		dai_data->tx_pid = AFE_PORT_ID_SECONDARY_PCM_TX;
		pdev->id = MSM_DAI_SEC_AUXPCM_DT_DEV_ID;
	} else {
		dev_err(&pdev->dev, "%s: invalid DT intf name %s\n",
			__func__, intf_name);
		goto fail_invalid_intf;
	}

	mutex_init(&dai_data->rlock);
	dev_set_name(&pdev->dev, "%s.%d", "msm-dai-q6-auxpcm", pdev->id);
	dev_dbg(&pdev->dev, "dev name %s\n", dev_name(&pdev->dev));

	dev_set_drvdata(&pdev->dev, dai_data);
	pdev->dev.platform_data = (void *) auxpcm_pdata;

	rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_aux_pcm_dai);
	if (rc) {
		dev_err(&pdev->dev, "%s: auxpcm dai reg failed, rc=%d\n",
				__func__, rc);
		goto fail_reg_dai;
	}

	return rc;

fail_reg_dai:
fail_invalid_intf:
fail_nodev_intf:
fail_invalid_dt:
	kfree(auxpcm_pdata);
fail_pdata_nomem:
	kfree(dai_data);
	return rc;
}

static int __devexit msm_auxpcm_dev_remove(struct platform_device *pdev)
{
	struct msm_dai_q6_auxpcm_dai_data *dai_data;

	dai_data = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_dai(&pdev->dev);

	mutex_destroy(&dai_data->rlock);
	kfree(dai_data);
	kfree(pdev->dev.platform_data);

	return 0;
}

static struct of_device_id msm_auxpcm_dev_dt_match[] = {
	{ .compatible = "qcom,msm-auxpcm-dev", },
	{}
};


static struct platform_driver msm_auxpcm_dev_driver = {
	.probe  = msm_auxpcm_dev_probe,
	.remove = __devexit_p(msm_auxpcm_dev_remove),
	.driver = {
		.name = "msm-auxpcm-dev",
		.owner = THIS_MODULE,
		.of_match_table = msm_auxpcm_dev_dt_match,
	},
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_96000 |
		SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		.channels_min = 1,
		.channels_max = 8,
		.rate_min = 8000,
		.rate_max = 192000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_96000 |
		SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 8,
		.rate_min = 8000,
		.rate_max = 192000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static int msm_dai_q6_mi2s_format_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	int value = ucontrol->value.integer.value[0];
	dai_data->port_config.i2s.data_format = value;
	pr_debug("%s: value = %d, channel = %d, line = %d\n",
		 __func__, value, dai_data->port_config.i2s.mono_stereo,
		 dai_data->port_config.i2s.channel_mode);
	return 0;
}

static int msm_dai_q6_mi2s_format_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct msm_dai_q6_dai_data *dai_data = kcontrol->private_data;
	ucontrol->value.integer.value[0] =
		dai_data->port_config.i2s.data_format;
	return 0;
}

static const struct snd_kcontrol_new mi2s_config_controls[] = {
	SOC_ENUM_EXT("PRI MI2S RX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("SEC MI2S RX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("TERT MI2S RX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("QUAT MI2S RX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("PRI MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("SEC MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("TERT MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
	SOC_ENUM_EXT("QUAT MI2S TX Format", mi2s_config_enum[0],
		     msm_dai_q6_mi2s_format_get,
		     msm_dai_q6_mi2s_format_put),
};

static int msm_dai_q6_dai_mi2s_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_mi2s_dai_data *mi2s_dai_data =
			dev_get_drvdata(dai->dev);
	struct snd_kcontrol *kcontrol = NULL;
	int rc = 0;
	const struct snd_kcontrol_new *ctrl = NULL;

	if (mi2s_dai_data->rx_dai.mi2s_dai_data.port_config.i2s.channel_mode) {
		if (!strncmp(dai->name, "msm-dai-q6-mi2s.0", 17))
			ctrl = &mi2s_config_controls[0];
		if (!strncmp(dai->name, "msm-dai-q6-mi2s.1", 17))
			ctrl = &mi2s_config_controls[1];
		if (!strncmp(dai->name, "msm-dai-q6-mi2s.2", 17))
			ctrl = &mi2s_config_controls[2];
		if (!strncmp(dai->name, "msm-dai-q6-mi2s.3", 17))
			ctrl = &mi2s_config_controls[3];
	}

	if (ctrl) {
		kcontrol = snd_ctl_new1(ctrl,
					&mi2s_dai_data->rx_dai.mi2s_dai_data);
		rc = snd_ctl_add(dai->card->snd_card, kcontrol);

		if (IS_ERR_VALUE(rc)) {
			dev_err(dai->dev, "%s: err add RX fmt ctl DAI = %s\n",
				__func__, dai->name);
			goto rtn;
		}
	}

	ctrl = NULL;
	if (mi2s_dai_data->tx_dai.mi2s_dai_data.port_config.i2s.channel_mode) {
		if (!strncmp(dai->name, "msm-dai-q6-mi2s.0", 17))
			ctrl = &mi2s_config_controls[4];
		if (!strncmp(dai->name, "msm-dai-q6-mi2s.1", 17))
			ctrl = &mi2s_config_controls[5];
		if (!strncmp(dai->name, "msm-dai-q6-mi2s.2", 17))
			ctrl = &mi2s_config_controls[6];
		if (!strncmp(dai->name, "msm-dai-q6-mi2s.3", 17))
			ctrl = &mi2s_config_controls[7];
	}

	if (ctrl) {
		rc = snd_ctl_add(dai->card->snd_card,
				snd_ctl_new1(ctrl,
				&mi2s_dai_data->tx_dai.mi2s_dai_data));

		if (IS_ERR_VALUE(rc)) {
			if (kcontrol)
				snd_ctl_remove(dai->card->snd_card, kcontrol);
			dev_err(dai->dev, "%s: err add TX fmt ctl DAI = %s\n",
				__func__, dai->name);
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

static int msm_dai_q6_mi2s_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{

	return 0;
}


static int msm_mi2s_get_port_id(u32 mi2s_id, int stream, u16 *port_id)
{
	int ret = 0;

	switch (stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		switch (mi2s_id) {
		case MSM_PRIM_MI2S:
			*port_id = AFE_PORT_ID_PRIMARY_MI2S_RX;
			break;
		case MSM_SEC_MI2S:
			*port_id = AFE_PORT_ID_SECONDARY_MI2S_RX;
			break;
		case MSM_TERT_MI2S:
			*port_id = AFE_PORT_ID_TERTIARY_MI2S_RX;
			break;
		case MSM_QUAT_MI2S:
			*port_id = AFE_PORT_ID_QUATERNARY_MI2S_RX;
			break;
		break;
		default:
			ret = -1;
		break;
		}
	break;
	case SNDRV_PCM_STREAM_CAPTURE:
		switch (mi2s_id) {
		case MSM_PRIM_MI2S:
			*port_id = AFE_PORT_ID_PRIMARY_MI2S_TX;
			break;
		case MSM_SEC_MI2S:
			*port_id = AFE_PORT_ID_SECONDARY_MI2S_TX;
			break;
		case MSM_TERT_MI2S:
			*port_id = AFE_PORT_ID_TERTIARY_MI2S_TX;
			break;
		case MSM_QUAT_MI2S:
			*port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
			break;
		default:
			ret = -1;
		break;
		}
	break;
	default:
		ret = -1;
	break;
	}
	pr_debug("%s: port_id = %#x\n", __func__, *port_id);
	return ret;
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
	u16 port_id = 0;
	int rc = 0;

	if (msm_mi2s_get_port_id(dai->id, substream->stream,
				 &port_id) != 0) {
		dev_err(dai->dev, "%s: Invalid Port ID %#x\n",
				__func__, port_id);
		return -EINVAL;
	}

	dev_dbg(dai->dev, "%s: dai id %d, afe port id = %x\n"
		"dai_data->channels = %u sample_rate = %u\n", __func__,
		dai->id, port_id, dai_data->channels, dai_data->rate);

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		/* PORT START should be set if prepare called
		 * in active state.
		 */
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
	struct afe_param_id_i2s_cfg *i2s = &dai_data->port_config.i2s;


	dai_data->channels = params_channels(params);
	switch (dai_data->channels) {
	case 8:
	case 7:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_PORT_I2S_8CHS)
			goto error_invalid_data;
		dai_data->port_config.i2s.channel_mode = AFE_PORT_I2S_8CHS;
		break;
	case 6:
	case 5:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_PORT_I2S_6CHS)
			goto error_invalid_data;
		dai_data->port_config.i2s.channel_mode = AFE_PORT_I2S_6CHS;
		break;
	case 4:
	case 3:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_PORT_I2S_QUAD01)
			goto error_invalid_data;
		if (mi2s_dai_config->pdata_mi2s_lines == AFE_PORT_I2S_QUAD23)
			dai_data->port_config.i2s.channel_mode =
				mi2s_dai_config->pdata_mi2s_lines;
		else
			dai_data->port_config.i2s.channel_mode =
					AFE_PORT_I2S_QUAD01;
		break;
	case 2:
	case 1:
		if (mi2s_dai_config->pdata_mi2s_lines < AFE_PORT_I2S_SD0)
			goto error_invalid_data;
		switch (mi2s_dai_config->pdata_mi2s_lines) {
		case AFE_PORT_I2S_SD0:
		case AFE_PORT_I2S_SD1:
		case AFE_PORT_I2S_SD2:
		case AFE_PORT_I2S_SD3:
			dai_data->port_config.i2s.channel_mode =
				mi2s_dai_config->pdata_mi2s_lines;
			break;
		case AFE_PORT_I2S_QUAD01:
		case AFE_PORT_I2S_6CHS:
		case AFE_PORT_I2S_8CHS:
			dai_data->port_config.i2s.channel_mode =
						AFE_PORT_I2S_SD0;
			break;
		case AFE_PORT_I2S_QUAD23:
			dai_data->port_config.i2s.channel_mode =
						AFE_PORT_I2S_SD2;
			break;
		}
		if (dai_data->channels == 2)
			dai_data->port_config.i2s.mono_stereo =
						MSM_AFE_CH_STEREO;
		else
			dai_data->port_config.i2s.mono_stereo = MSM_AFE_MONO;
		break;
	default:
		goto error_invalid_data;
	}
	dai_data->rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_SPECIAL:
		dai_data->port_config.i2s.bit_width = 16;
		dai_data->bitwidth = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		dai_data->port_config.i2s.bit_width = 24;
		dai_data->bitwidth = 24;
		break;
	default:
		return -EINVAL;
	}

	dai_data->port_config.i2s.i2s_cfg_minor_version =
			AFE_API_VERSION_I2S_CONFIG;
	dai_data->port_config.i2s.sample_rate = dai_data->rate;
	if (test_bit(STATUS_PORT_STARTED,
	    mi2s_dai_data->rx_dai.mi2s_dai_data.status_mask) ||
	    test_bit(STATUS_PORT_STARTED,
	    mi2s_dai_data->tx_dai.mi2s_dai_data.status_mask)) {
		if ((mi2s_dai_data->tx_dai.mi2s_dai_data.rate !=
		    mi2s_dai_data->rx_dai.mi2s_dai_data.rate) ||
		   (mi2s_dai_data->rx_dai.mi2s_dai_data.bitwidth !=
		    mi2s_dai_data->tx_dai.mi2s_dai_data.bitwidth)) {
			dev_err(dai->dev, "%s: Error mismatch in HW params\n"
				"Tx sample_rate = %u bit_width = %hu\n"
				"Rx sample_rate = %u bit_width = %hu\n"
				, __func__,
				mi2s_dai_data->tx_dai.mi2s_dai_data.rate,
				mi2s_dai_data->tx_dai.mi2s_dai_data.bitwidth,
				mi2s_dai_data->rx_dai.mi2s_dai_data.rate,
				mi2s_dai_data->rx_dai.mi2s_dai_data.bitwidth);
			return -EINVAL;
		}
	}
	dev_dbg(dai->dev, "%s: dai id %d dai_data->channels = %d\n"
		"sample_rate = %u i2s_cfg_minor_version = %#x\n"
		"bit_width = %hu  channel_mode = %#x mono_stereo = %#x\n"
		"ws_src = %#x sample_rate = %u data_format = %#x\n"
		"reserved = %u\n", __func__, dai->id, dai_data->channels,
		dai_data->rate, i2s->i2s_cfg_minor_version, i2s->bit_width,
		i2s->channel_mode, i2s->mono_stereo, i2s->ws_src,
		i2s->sample_rate, i2s->data_format, i2s->reserved);

	return 0;

error_invalid_data:
	pr_debug("%s: dai_data->channels = %d channel_mode = %d\n", __func__,
		 dai_data->channels, dai_data->port_config.i2s.channel_mode);
	return -EINVAL;
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
		mi2s_dai_data->rx_dai.mi2s_dai_data.port_config.i2s.ws_src = 1;
		mi2s_dai_data->tx_dai.mi2s_dai_data.port_config.i2s.ws_src = 1;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		mi2s_dai_data->rx_dai.mi2s_dai_data.port_config.i2s.ws_src = 0;
		mi2s_dai_data->tx_dai.mi2s_dai_data.port_config.i2s.ws_src = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
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
	 u16 port_id = 0;
	int rc = 0;

	if (msm_mi2s_get_port_id(dai->id, substream->stream,
				 &port_id) != 0) {
		dev_err(dai->dev, "%s: Invalid Port ID %#x\n",
				__func__, port_id);
	}

	dev_dbg(dai->dev, "%s: closing afe port id = %x\n",
			__func__, port_id);

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_close(port_id);
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
}

static struct snd_soc_dai_ops msm_dai_q6_mi2s_ops = {
	.startup	= msm_dai_q6_mi2s_startup,
	.prepare	= msm_dai_q6_mi2s_prepare,
	.hw_params	= msm_dai_q6_mi2s_hw_params,
	.set_fmt	= msm_dai_q6_mi2s_set_fmt,
	.shutdown	= msm_dai_q6_mi2s_shutdown,
};

/* Channel min and max are initialized base on platform data */
static struct snd_soc_dai_driver msm_dai_q6_mi2s_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min =     8000,
		.rate_max =     48000,
	},
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		.rate_min =     8000,
		.rate_max =     48000,
	},
	.ops = &msm_dai_q6_mi2s_ops,
	.probe = msm_dai_q6_dai_mi2s_probe,
	.remove = msm_dai_q6_dai_mi2s_remove,
};


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
			*config_ptr = AFE_PORT_I2S_SD0;
			break;
		case MSM_MI2S_SD1:
			*config_ptr = AFE_PORT_I2S_SD1;
			break;
		case MSM_MI2S_SD2:
			*config_ptr = AFE_PORT_I2S_SD2;
			break;
		case MSM_MI2S_SD3:
			*config_ptr = AFE_PORT_I2S_SD3;
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
			*config_ptr = AFE_PORT_I2S_QUAD01;
			break;
		case MSM_MI2S_SD2 | MSM_MI2S_SD3:
			*config_ptr = AFE_PORT_I2S_QUAD23;
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
			*config_ptr = AFE_PORT_I2S_6CHS;
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
			*config_ptr = AFE_PORT_I2S_8CHS;
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
	unsigned int ch_cnt;
	int rc = 0;
	u16 sd_line;

	if (mi2s_pdata == NULL) {
		pr_err("%s: mi2s_pdata NULL", __func__);
		return -EINVAL;
	}

	rc = msm_dai_q6_mi2s_get_lineconfig(mi2s_pdata->rx_sd_lines,
					    &sd_line, &ch_cnt);

	if (IS_ERR_VALUE(rc)) {
		dev_err(&pdev->dev, "invalid MI2S RX sd line config\n");
		goto rtn;
	}

	if (ch_cnt) {
		dai_data->rx_dai.mi2s_dai_data.port_config.i2s.channel_mode =
		sd_line;
		dai_data->rx_dai.pdata_mi2s_lines = sd_line;
		dai_driver->playback.channels_min = 1;
		dai_driver->playback.channels_max = ch_cnt << 1;
	} else {
		dai_driver->playback.channels_min = 0;
		dai_driver->playback.channels_max = 0;
	}
	rc = msm_dai_q6_mi2s_get_lineconfig(mi2s_pdata->tx_sd_lines,
					    &sd_line, &ch_cnt);

	if (IS_ERR_VALUE(rc)) {
		dev_err(&pdev->dev, "invalid MI2S TX sd line config\n");
		goto rtn;
	}

	if (ch_cnt) {
		dai_data->tx_dai.mi2s_dai_data.port_config.i2s.channel_mode =
		sd_line;
		dai_data->tx_dai.pdata_mi2s_lines = sd_line;
		dai_driver->capture.channels_min = 1;
		dai_driver->capture.channels_max = ch_cnt << 1;
	} else {
		dai_driver->capture.channels_min = 0;
		dai_driver->capture.channels_max = 0;
	}

	dev_dbg(&pdev->dev, "%s: playback sdline %x capture sdline %x\n",
		__func__, dai_data->rx_dai.pdata_mi2s_lines,
		dai_data->tx_dai.pdata_mi2s_lines);
	dev_dbg(&pdev->dev, "%s: playback ch_max %d capture ch_mx %d\n",
		__func__, dai_driver->playback.channels_max,
		dai_driver->capture.channels_max);
rtn:
	return rc;
}

static __devinit int msm_dai_q6_mi2s_dev_probe(struct platform_device *pdev)
{
	struct msm_dai_q6_mi2s_dai_data *dai_data;
	const char *q6_mi2s_dev_id = "qcom,msm-dai-q6-mi2s-dev-id";
	u32 tx_line = 0;
	u32  rx_line = 0;
	u32 mi2s_intf = 0;
	struct msm_mi2s_pdata *mi2s_pdata;
	int rc;
	struct snd_soc_dai_driver *mi2s_dai;

	rc = of_property_read_u32(pdev->dev.of_node, q6_mi2s_dev_id,
				  &mi2s_intf);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: missing %x in dt node\n", __func__, mi2s_intf);
		goto rtn;
	}

	dev_dbg(&pdev->dev, "dev name %s dev id %x\n", dev_name(&pdev->dev),
		mi2s_intf);

	if (mi2s_intf < MSM_PRIM_MI2S || mi2s_intf > MSM_QUAT_MI2S) {
		dev_err(&pdev->dev,
			"%s: Invalid MI2S ID %u from Device Tree\n",
			__func__, mi2s_intf);
		rc = -ENXIO;
		goto rtn;
	}

	dev_set_name(&pdev->dev, "%s.%d", "msm-dai-q6-mi2s", mi2s_intf);
	pdev->id = mi2s_intf;

	mi2s_pdata = kzalloc(sizeof(struct msm_mi2s_pdata), GFP_KERNEL);
	if (!mi2s_pdata) {
		dev_err(&pdev->dev, "fail to allocate mi2s_pdata data\n");
		rc = -ENOMEM;
		goto rtn;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,msm-mi2s-rx-lines",
				  &rx_line);
	if (rc) {
		dev_err(&pdev->dev, "%s: Rx line from DT file %s\n", __func__,
			"qcom,msm-mi2s-rx-lines");
		goto free_pdata;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,msm-mi2s-tx-lines",
				  &tx_line);
	if (rc) {
		dev_err(&pdev->dev, "%s: Tx line from DT file %s\n", __func__,
			"qcom,msm-mi2s-tx-lines");
		goto free_pdata;
	}
	dev_dbg(&pdev->dev, "dev name %s Rx line %x , Tx ine %x\n",
		dev_name(&pdev->dev), rx_line, tx_line);
	mi2s_pdata->rx_sd_lines = rx_line;
	mi2s_pdata->tx_sd_lines = tx_line;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_mi2s_dai_data),
			   GFP_KERNEL);
	if (!dai_data) {
		dev_err(&pdev->dev, "fail to allocate dai data\n");
		rc = -ENOMEM;
		goto free_pdata;
	} else
		dev_set_drvdata(&pdev->dev, dai_data);

	pdev->dev.platform_data = mi2s_pdata;

	mi2s_dai = kzalloc(sizeof(struct snd_soc_dai_driver), GFP_KERNEL);
	if (!mi2s_dai) {
		dev_err(&pdev->dev, "fail to allocate for mi2s_dai\n");
		rc = -ENOMEM;
		goto free_dai_data;
	}

	memcpy(mi2s_dai, &msm_dai_q6_mi2s_dai,
	       sizeof(struct snd_soc_dai_driver));
	rc = msm_dai_q6_mi2s_platform_data_validation(pdev, mi2s_dai);
	if (IS_ERR_VALUE(rc))
		goto free_dai;

	rc = snd_soc_register_dai(&pdev->dev, mi2s_dai);
	if (IS_ERR_VALUE(rc))
		goto err_register;
	return 0;

err_register:
	dev_err(&pdev->dev, "fail to msm_dai_q6_mi2s_dev_probe\n");
free_dai:
	kfree(mi2s_dai);
free_dai_data:
	kfree(dai_data);
free_pdata:
	kfree(mi2s_pdata);
rtn:
	return rc;
}

static __devexit int msm_dai_q6_mi2s_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static int msm_dai_q6_dev_probe(struct platform_device *pdev)
{
	int rc, id;
	const char *q6_dev_id = "qcom,msm-dai-q6-dev-id";

	rc = of_property_read_u32(pdev->dev.of_node, q6_dev_id, &id);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, q6_dev_id);
		return rc;
	}

	pdev->id = id;
	dev_set_name(&pdev->dev, "%s.%d", "msm-dai-q6-dev", id);

	pr_debug("%s: dev name %s, id:%d\n", __func__,
		 dev_name(&pdev->dev), pdev->id);

	switch (id) {
	case SLIMBUS_0_RX:
	case SLIMBUS_2_RX:
		rc = snd_soc_register_dai(&pdev->dev,
					  &msm_dai_q6_slimbus_rx_dai);
		break;
	case SLIMBUS_0_TX:
	case SLIMBUS_2_TX:
	case SLIMBUS_5_TX:
		rc = snd_soc_register_dai(&pdev->dev,
					  &msm_dai_q6_slimbus_tx_dai);
		break;
	case SLIMBUS_1_RX:
	case SLIMBUS_3_RX:
	case SLIMBUS_4_RX:
		rc = snd_soc_register_dai(&pdev->dev,
					  &msm_dai_q6_slimbus_1_rx_dai);
		break;
	case SLIMBUS_1_TX:
	case SLIMBUS_3_TX:
	case SLIMBUS_4_TX:
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
	case VOICE2_PLAYBACK_TX:
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

static int msm_dai_q6_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_dai_q6_dev_dt_match[] = {
	{ .compatible = "qcom,msm-dai-q6-dev", },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_dai_q6_dev_dt_match);

static struct platform_driver msm_dai_q6_dev = {
	.probe  = msm_dai_q6_dev_probe,
	.remove = msm_dai_q6_dev_remove,
	.driver = {
		.name = "msm-dai-q6-dev",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_q6_dev_dt_match,
	},
};

static int msm_dai_q6_probe(struct platform_device *pdev)
{
	int rc;
	pr_debug("%s: dev name %s, id:%d\n", __func__,
		 dev_name(&pdev->dev), pdev->id);
	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
	} else
		dev_dbg(&pdev->dev, "%s: added child node\n", __func__);

	return rc;
}

static int msm_dai_q6_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id msm_dai_q6_dt_match[] = {
	{ .compatible = "qcom,msm-dai-q6", },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_dai_q6_dt_match);
static struct platform_driver msm_dai_q6 = {
	.probe  = msm_dai_q6_probe,
	.remove = msm_dai_q6_remove,
	.driver = {
		.name = "msm-dai-q6",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_q6_dt_match,
	},
};

static int msm_dai_mi2s_q6_probe(struct platform_device *pdev)
{
	int rc;
	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
	} else
		dev_dbg(&pdev->dev, "%s: added child node\n", __func__);
	return rc;
}

static int msm_dai_mi2s_q6_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id msm_dai_mi2s_dt_match[] = {
	{ .compatible = "qcom,msm-dai-mi2s", },
	{ }
};

MODULE_DEVICE_TABLE(of, msm_dai_mi2s_dt_match);

static struct platform_driver msm_dai_mi2s_q6 = {
	.probe  = msm_dai_mi2s_q6_probe,
	.remove = msm_dai_mi2s_q6_remove,
	.driver = {
		.name = "msm-dai-mi2s",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_mi2s_dt_match,
	},
};

static const struct of_device_id msm_dai_q6_mi2s_dev_dt_match[] = {
	{ .compatible = "qcom,msm-dai-q6-mi2s", },
	{ }
};

MODULE_DEVICE_TABLE(of, msm_dai_q6_mi2s_dev_dt_match);

static struct platform_driver msm_dai_q6_mi2s_driver = {
	.probe  = msm_dai_q6_mi2s_dev_probe,
	.remove  = __devexit_p(msm_dai_q6_mi2s_dev_remove),
	.driver = {
		.name = "msm-dai-q6-mi2s",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_q6_mi2s_dev_dt_match,
	},
};

static int __init msm_dai_q6_init(void)
{
	int rc;

	rc = platform_driver_register(&msm_auxpcm_dev_driver);
	if (rc) {
		pr_err("%s: fail to register auxpcm dev driver", __func__);
		goto fail;
	}

	rc = platform_driver_register(&msm_dai_q6);
	if (rc) {
		pr_err("%s: fail to register dai q6 driver", __func__);
		goto dai_q6_fail;
	}

	rc = platform_driver_register(&msm_dai_q6_dev);
	if (rc) {
		pr_err("%s: fail to register dai q6 dev driver", __func__);
		goto dai_q6_dev_fail;
	}

	rc = platform_driver_register(&msm_dai_q6_mi2s_driver);
	if (rc) {
		pr_err("%s: fail to register dai MI2S dev drv\n", __func__);
		goto dai_q6_mi2s_drv_fail;
	}

	rc = platform_driver_register(&msm_dai_mi2s_q6);
	if (rc) {
		pr_err("%s: fail to register dai MI2S\n", __func__);
		goto dai_mi2s_q6_fail;
	}
	return rc;

dai_mi2s_q6_fail:
	platform_driver_unregister(&msm_dai_q6_mi2s_driver);
dai_q6_mi2s_drv_fail:
	platform_driver_unregister(&msm_dai_q6_dev);
dai_q6_dev_fail:
	platform_driver_unregister(&msm_dai_q6);
dai_q6_fail:
	platform_driver_unregister(&msm_auxpcm_dev_driver);
fail:
	return rc;
}
module_init(msm_dai_q6_init);

static void __exit msm_dai_q6_exit(void)
{
	platform_driver_unregister(&msm_dai_q6_dev);
	platform_driver_unregister(&msm_dai_q6);
	platform_driver_unregister(&msm_auxpcm_dev_driver);
}
module_exit(msm_dai_q6_exit);

/* Module information */
MODULE_DESCRIPTION("MSM DSP DAI driver");
MODULE_LICENSE("GPL v2");
