/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <video/msm_dba.h>

#define CHANNEL_STATUS_SIZE 24
#define MSM_DBA_AUDIO_N_SIZE 6144

/**
 * struct msm_hdmi_dba_codec_rx_data - the basic hdmi_dba_codec structure
 * @msm_dba_ops: operation supported by bridge chip.
 * @msm_dba_reg_info: Client information used with register API.
 * @msm_dba_audio_cfg: Structure for audio configuration.
 * @dba_data: bridge chip private data.
 * @hdmi_state: indicates HDMI state.
 * @edid_size: size of EDID info read from HDMI.
 * @pcm_status: indicates channel status.
 *
 * Contains required members used in hdmi dba codec driver.
 */

struct msm_hdmi_dba_codec_rx_data {
	struct msm_dba_ops dba_ops;
	struct msm_dba_reg_info dba_info;
	struct msm_dba_audio_cfg audio_cfg;
	void *dba_data;
	bool hdmi_state;
	u32 edid_size;
	char pcm_status[CHANNEL_STATUS_SIZE];
};

#ifdef CONFIG_MSM_DBA
/**
 * msm_hdmi_dsi_dba_cb() - callback from DBA bridge chip
 * @data: hdmi dba codec driver private structure.
 * @msm_dba_callback_event: event triggered from DBA.
 *
 * This function is to handle events from DBA bridge chip.
 *
 * Return: void.
 */
static void msm_hdmi_dsi_dba_cb(void *data, enum msm_dba_callback_event event)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data =
			(struct msm_hdmi_dba_codec_rx_data *) data;

	if (!codec_data) {
		pr_err_ratelimited("%s: Invalid data\n", __func__);
		return;
	}
	pr_debug("%s: event %d\n", __func__, event);
	switch (event) {
	case MSM_DBA_CB_HPD_CONNECT:
		codec_data->hdmi_state = true;
		break;

	case MSM_DBA_CB_HPD_DISCONNECT:
		codec_data->hdmi_state = false;
		break;

	case MSM_DBA_CB_AUDIO_FAILURE:
		pr_err_ratelimited("%s: audio fail callback event\n", __func__);
		break;

	default:
		pr_debug("%s: unhandled event:%d\n", __func__, event);
		break;
	}
}

/**
 * msm_hdmi_dba_codec_rx_init_dba() - Initialize call to DBA bridge chip
 * @codec_data: hdmi dba codec driver private structure.
 * @msm_dba_callback_event: event triggered from DBA.
 *
 * This function is to initialize as client to DBA bridge chip.
 *
 * Return: int.
 */
static int msm_hdmi_dba_codec_rx_init_dba(
			struct msm_hdmi_dba_codec_rx_data *codec_data)
{
	msm_dba_cb dba_cb = msm_hdmi_dsi_dba_cb;
	int ret = 0;
	u32 event_mask = (MSM_DBA_CB_HPD_CONNECT | MSM_DBA_CB_HPD_DISCONNECT |
			MSM_DBA_CB_AUDIO_FAILURE);

	if (!codec_data) {
		pr_err_ratelimited("%s: Invalid data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	strlcpy(codec_data->dba_info.client_name, "audio",
					MSM_DBA_CLIENT_NAME_LEN);

	codec_data->dba_info.instance_id = 0;
	codec_data->dba_info.cb = dba_cb;
	codec_data->dba_info.cb_data = codec_data;
	codec_data->dba_data = msm_dba_register_client(
					&codec_data->dba_info,
					&codec_data->dba_ops);
	if (!IS_ERR_OR_NULL(codec_data->dba_data)) {
		/* Enable callback */
		if (codec_data->dba_ops.interrupts_enable)
			codec_data->dba_ops.interrupts_enable(
						codec_data->dba_data, true,
						event_mask, 0);
	} else {
		pr_err_ratelimited("%s: error in registering audio client %d\n",
							__func__, ret);
		ret = -EINVAL;
	}
end:
	return ret;
}
#else
static int msm_hdmi_dba_codec_rx_init_dba(
			struct msm_hdmi_dba_codec_rx_data *codec_data)
{
	pr_debug("CONFIG_MSM_DBA not enabled\n");
	return -EINVAL;
}
#endif

/**
 * msm_hdmi_dba_format_put() - update format value
 * @kcontrol: kernel control data.
 * @ucontrol: user control elemental value.
 *
 * This function is used to set HDMI format.
 *
 * Return: int.
 */
static int msm_hdmi_dba_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data;
	int value;

	if (!kcontrol || !ucontrol) {
		pr_err_ratelimited("%s: invalid control\n", __func__);
		return -EINVAL;
	}
	codec_data = kcontrol->private_data;
	if (!codec_data) {
		pr_err_ratelimited("%s: codec_data is NULL\n", __func__);
		return -EINVAL;
	}
	value = ucontrol->value.integer.value[0];
	pr_debug("%s: value = %d\n", __func__, value);
	codec_data->audio_cfg.format = value;

	return 0;
}

/**
 * msm_hdmi_dba_format_get() - get current format value
 * @kcontrol: kernel control data.
 * @ucontrol: user control elemental value.
 *
 * This function is used to get HDMI format.
 *
 * Return: int.
 */
static int msm_hdmi_dba_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data;

	if (!kcontrol || !ucontrol) {
		pr_err_ratelimited("%s: invalid control\n", __func__);
		return -EINVAL;
	}
	codec_data = kcontrol->private_data;
	if (!codec_data) {
		pr_err_ratelimited("%s: codec_data is NULL\n", __func__);
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] =
			codec_data->audio_cfg.format;

	return 0;
}

/**
 * msm_hdmi_dba_edid_ctl_info() - get EDID size
 * @kcontrol: kernel control data.
 * @uinfo: user control elemental info.
 *
 * This function is used to get EDID size.
 *
 * Return: int.
 */
static int msm_hdmi_dba_edid_ctl_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data;

	if (!kcontrol || !uinfo) {
		pr_err_ratelimited("%s: invalid control\n", __func__);
		return -EINVAL;
	}
	codec_data = kcontrol->private_data;
	if (!codec_data) {
		pr_err_ratelimited("%s: codec_data is NULL\n", __func__);
		return -EINVAL;
	}
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	if (codec_data->dba_ops.get_edid_size)
		codec_data->dba_ops.get_edid_size(codec_data->dba_data,
			&uinfo->count, 0);
	codec_data->edid_size = uinfo->count;

	return 0;
}

/**
 * msm_hdmi_dba_edid_get() - get EDID data
 * @kcontrol: kernel control data.
 * @ucontrol: user control elemental value.
 *
 * This function is used to call read EDID data.
 *
 * Return: int.
 */
static int msm_hdmi_dba_edid_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data;

	if (!kcontrol || !ucontrol) {
		pr_err_ratelimited("%s: invalid control\n", __func__);
		return -EINVAL;
	}
	codec_data = kcontrol->private_data;
	if (!codec_data) {
		pr_err_ratelimited("%s: codec_data is NULL\n", __func__);
		return -EINVAL;
	}
	if (!codec_data->hdmi_state) {
		pr_err_ratelimited("%s: hdmi is not connected yet\n", __func__);
		return -EINVAL;
	}
	if (codec_data->dba_ops.get_raw_edid)
		codec_data->dba_ops.get_raw_edid(codec_data->dba_data,
			codec_data->edid_size,
			ucontrol->value.bytes.data, 0);

	return 0;
}

/**
 * msm_hdmi_dba_cs_info() - get IEC958 size
 * @kcontrol: kernel control data.
 * @uinfo: user control elemental info.
 *
 * This function is used to get size of snd_aes_iec958.
 *
 * Return: int.
 */
static int msm_hdmi_dba_cs_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	if (!uinfo) {
		pr_err_ratelimited("%s: invalid info ptr\n", __func__);
		return -EINVAL;
	}
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof(struct snd_aes_iec958);

	return 0;
}

/**
 * msm_hdmi_dba_cs_get() - get pcm channel status
 * @kcontrol: kernel control data.
 * @ucontrol: user control elemental value.
 *
 * This function is used to get PCM channel status.
 *
 * Return: int.
 */
static int msm_hdmi_dba_cs_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data;

	if (!kcontrol || !ucontrol) {
		pr_err_ratelimited("%s: invalid control\n", __func__);
		return -EINVAL;
	}
	codec_data = kcontrol->private_data;
	if (!codec_data) {
		pr_err_ratelimited("%s: codec_data is NULL\n", __func__);
		return -EINVAL;
	}
	memcpy(ucontrol->value.iec958.status, codec_data->pcm_status,
				CHANNEL_STATUS_SIZE);

	return 0;
}

/**
 * msm_hdmi_dba_cs_get() - update pcm channel status
 * @kcontrol: kernel control data.
 * @ucontrol: user control elemental value.
 *
 * This function is used to set PCM channel status and
 * audio configuration info.
 *
 * Return: int.
 */
static int msm_hdmi_dba_cs_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data;
	unsigned char *status;

	if (!kcontrol || !ucontrol) {
		pr_err_ratelimited("%s: invalid control\n", __func__);
		return -EINVAL;
	}
	codec_data = kcontrol->private_data;
	if (!codec_data) {
		pr_err_ratelimited("%s: codec_data is NULL\n", __func__);
		return -EINVAL;
	}
	status = codec_data->pcm_status;

	memcpy(status, ucontrol->value.iec958.status, CHANNEL_STATUS_SIZE);
	if (!codec_data->hdmi_state) {
		pr_err_ratelimited("%s: hdmi is not connected yet\n", __func__);
		return -EINVAL;
	}
	codec_data->audio_cfg.format = (status[0] & 0x02);
	codec_data->audio_cfg.sampling_rate = (status[3] & 0x0F);
	codec_data->audio_cfg.interface = MSM_DBA_AUDIO_I2S_INTERFACE;
	codec_data->audio_cfg.i2s_fmt = MSM_DBA_AUDIO_I2S_FMT_STANDARD;
	codec_data->audio_cfg.word_endianness =
		MSM_DBA_AUDIO_WORD_BIG_ENDIAN;
	codec_data->audio_cfg.channel_status_source =
		MSM_DBA_AUDIO_CS_SOURCE_REGISTERS;
	codec_data->audio_cfg.mode = MSM_DBA_AUDIO_MODE_AUTOMATIC;
	codec_data->audio_cfg.n = MSM_DBA_AUDIO_N_SIZE;

	return 0;
}

/*
 * HDMI format field for AFE_PORT_MULTI_CHAN_HDMI_AUDIO_IF_CONFIG command
 *  0: linear PCM
 *  1: non-linear PCM
 */
static const char * const hdmi_format[] = {
	"LPCM",
	"Compr"
};

static const struct soc_enum hdmi_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, hdmi_format),
};

static struct snd_kcontrol_new msm_hdmi_dba_rx_controls[] = {
	{
		.access =	(SNDRV_CTL_ELEM_ACCESS_READWRITE |
				SNDRV_CTL_ELEM_ACCESS_INACTIVE),
		.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
		.name =		SNDRV_CTL_NAME_IEC958("", PLAYBACK, PCM_STREAM),
		.info =		msm_hdmi_dba_cs_info,
		.get =		msm_hdmi_dba_cs_get,
		.put =		msm_hdmi_dba_cs_put,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.iface	= SNDRV_CTL_ELEM_IFACE_PCM,
		.name	= "HDMI EDID",
		.info	= msm_hdmi_dba_edid_ctl_info,
		.get	= msm_hdmi_dba_edid_get,
	},
	SOC_ENUM_EXT("HDMI RX Format", hdmi_config_enum[0],
			msm_hdmi_dba_format_get,
			msm_hdmi_dba_format_put),
};


static int msm_hdmi_dba_codec_rx_dai_startup(
		struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	return 0;
}

static int msm_hdmi_dba_codec_rx_dai_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data =
					dev_get_drvdata(dai->codec->dev);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_SPECIAL:
		codec_data->audio_cfg.channel_status_word_length =
			MSM_DBA_AUDIO_WORD_16BIT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		codec_data->audio_cfg.channel_status_word_length =
			MSM_DBA_AUDIO_WORD_24BIT;
		break;
	default:
		pr_err_ratelimited("%s: format %d\n",
			__func__, params_format(params));
		return -EINVAL;
	}
	/* configure channel status information */
	if (codec_data->dba_ops.configure_audio)
		codec_data->dba_ops.configure_audio(codec_data->dba_data,
				&codec_data->audio_cfg, 0);
	/* start audio */
	if (codec_data->dba_ops.audio_on)
		codec_data->dba_ops.audio_on(codec_data->dba_data,
				true, 0);

	return 0;
}

static void msm_hdmi_dba_codec_rx_dai_shutdown(
		struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data =
					dev_get_drvdata(dai->codec->dev);

	/* Stop audio */
	if (codec_data->dba_ops.audio_on)
		codec_data->dba_ops.audio_on(codec_data->dba_data,
			false, 0);
}

static int msm_hdmi_dba_codec_rx_dai_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int msm_hdmi_dba_codec_rx_dai_resume(struct snd_soc_codec *codec)
{
	return 0;
}

static struct snd_soc_dai_ops msm_hdmi_dba_codec_rx_dai_ops = {
	.startup	= msm_hdmi_dba_codec_rx_dai_startup,
	.hw_params	= msm_hdmi_dba_codec_rx_dai_hw_params,
	.shutdown	= msm_hdmi_dba_codec_rx_dai_shutdown
};

static int msm_hdmi_dba_codec_rx_probe(struct snd_soc_codec *codec)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data;
	struct snd_kcontrol_new kcontrols[ARRAY_SIZE(msm_hdmi_dba_rx_controls)];
	int rc = 0, i = 0;

	codec_data = dev_get_drvdata(codec->dev);
	for (i = 0; i < ARRAY_SIZE(msm_hdmi_dba_rx_controls); i++) {
		kcontrols[i] = msm_hdmi_dba_rx_controls[i];
		rc = snd_ctl_add(codec->component.card->snd_card,
			snd_ctl_new1(&kcontrols[i], codec_data));
		if (IS_ERR_VALUE(rc)) {
			dev_err(codec->dev, "%s: err in adding ctrls = %d\n",
			__func__, rc);
			goto err;
		}
	}
	snd_soc_add_codec_controls(codec, kcontrols,
					ARRAY_SIZE(msm_hdmi_dba_rx_controls));

	/* Register to HDMI bridge */
	msm_hdmi_dba_codec_rx_init_dba(codec_data);
	return 0;
err:
	kfree(codec_data);
	codec_data = NULL;
	return rc;
}

static int msm_hdmi_dba_codec_rx_remove(struct snd_soc_codec *codec)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data;

	codec_data = dev_get_drvdata(codec->dev);
	kfree(codec_data);
	codec_data = NULL;

	return 0;
}

static struct snd_soc_dai_driver msm_hdmi_dba_codec_rx_dais[] = {
	{
		.name = "msm_hdmi_dba_codec_rx_dai",
		.playback = {
			.stream_name = "HDMI DBA Playback",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE,
			.rate_min =     8000,
			.rate_max =     48000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &msm_hdmi_dba_codec_rx_dai_ops,
	},
};

static struct snd_soc_codec_driver msm_hdmi_dba_codec_rx_soc_driver = {
	.probe = msm_hdmi_dba_codec_rx_probe,
	.remove =  msm_hdmi_dba_codec_rx_remove,
	.suspend = msm_hdmi_dba_codec_rx_dai_suspend,
	.resume = msm_hdmi_dba_codec_rx_dai_resume
};

static int msm_hdmi_dba_codec_rx_plat_probe(
		struct platform_device *pdev)
{
	int ret = 0;
	const char *dba_bridge_chip = "qcom,dba-bridge-chip";
	const char *type = NULL;
	struct msm_hdmi_dba_codec_rx_data *codec_data;

	dev_dbg(&pdev->dev, "%s(): dev name %s\n", __func__,
		dev_name(&pdev->dev));
	codec_data = kzalloc(sizeof(struct msm_hdmi_dba_codec_rx_data),
			GFP_KERNEL);

	if (!codec_data) {
		dev_err(&pdev->dev, "fail to allocate codec data\n");
		return -ENOMEM;
	}
	dev_set_drvdata(&pdev->dev, codec_data);
	ret = of_property_read_string(pdev->dev.of_node,
		dba_bridge_chip, &type);
	if (ret) {
		dev_err(&pdev->dev, "%s: missing %s in dt node\n",
			__func__, dba_bridge_chip);
		goto err;
	}
	strlcpy(codec_data->dba_info.chip_name, type,
				MSM_DBA_CHIP_NAME_MAX_LEN);

	return snd_soc_register_codec(&pdev->dev,
		&msm_hdmi_dba_codec_rx_soc_driver,
		msm_hdmi_dba_codec_rx_dais,
		ARRAY_SIZE(msm_hdmi_dba_codec_rx_dais));
err:
	kfree(codec_data);
	codec_data = NULL;
	return ret;
}

static int msm_hdmi_dba_codec_rx_plat_remove(
		struct platform_device *pdev)
{
	struct msm_hdmi_dba_codec_rx_data *codec_data =
					platform_get_drvdata(pdev);

	snd_soc_unregister_codec(&pdev->dev);
	kfree(codec_data);

	return 0;
}

static const struct of_device_id msm_hdmi_dba_codec_rx_dt_match[] = {
	{ .compatible = "qcom,msm-hdmi-dba-codec-rx", },
	{}
};
MODULE_DEVICE_TABLE(of, msm_hdmi_dba_codec_dt_match);

static struct platform_driver msm_hdmi_dba_codec_rx_driver = {
	.driver = {
		.name = "msm-hdmi-dba-codec-rx",
		.owner = THIS_MODULE,
		.of_match_table = msm_hdmi_dba_codec_rx_dt_match,
	},
	.probe = msm_hdmi_dba_codec_rx_plat_probe,
	.remove = msm_hdmi_dba_codec_rx_plat_remove,
};

static int __init msm_hdmi_dba_codec_rx_init(void)
{
	return platform_driver_register(&msm_hdmi_dba_codec_rx_driver);
}
module_init(msm_hdmi_dba_codec_rx_init);

static void __exit msm_hdmi_dba_codec_rx_exit(void)
{
	platform_driver_unregister(&msm_hdmi_dba_codec_rx_driver);
}
module_exit(msm_hdmi_dba_codec_rx_exit);

MODULE_DESCRIPTION("MSM HDMI DBA CODEC driver");
MODULE_LICENSE("GPL v2");
