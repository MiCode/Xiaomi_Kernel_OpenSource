/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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
#include <sound/soc.h>
#include <linux/msm_ext_display.h>

#define MSM_EXT_DISP_PCM_RATES	SNDRV_PCM_RATE_48000
#define AUD_EXT_DISP_ACK_DISCONNECT (AUDIO_ACK_CONNECT ^ AUDIO_ACK_CONNECT)
#define AUD_EXT_DISP_ACK_CONNECT    (AUDIO_ACK_CONNECT)
#define AUD_EXT_DISP_ACK_ENABLE     (AUDIO_ACK_SET_ENABLE | AUDIO_ACK_ENABLE)

static const char *const ext_disp_audio_type_text[] = {"None", "HDMI", "DP"};
static const char *const ext_disp_audio_ack_text[] = {"Disconnect",  "Connect",
						      "Ack_Enable"};

static SOC_ENUM_SINGLE_EXT_DECL(ext_disp_audio_type, ext_disp_audio_type_text);
static SOC_ENUM_SINGLE_EXT_DECL(ext_disp_audio_ack_state,
				ext_disp_audio_ack_text);

struct msm_ext_disp_audio_codec_rx_data {
	struct platform_device *ext_disp_core_pdev;
	struct msm_ext_disp_audio_codec_ops ext_disp_ops;
	int cable_status;
};

static int msm_ext_disp_edid_ctl_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	struct msm_ext_disp_audio_edid_blk edid_blk;
	int rc;

	codec_data = snd_soc_codec_get_drvdata(codec);

	if (!codec_data) {
		dev_err(codec->dev, "%s: codec_data is NULL\n", __func__);
		return -EINVAL;
	}

	if (!codec_data->ext_disp_ops.get_audio_edid_blk) {
		dev_dbg(codec->dev, "%s: get_audio_edid_blk() is NULL\n",
			__func__);
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
		uinfo->count = 0;
		return 0;
	}

	rc = codec_data->ext_disp_ops.get_audio_edid_blk(
				codec_data->ext_disp_core_pdev, &edid_blk);
	if (rc >= 0) {
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
		uinfo->count = edid_blk.audio_data_blk_size +
			edid_blk.spk_alloc_data_blk_size;
	}

	dev_dbg(codec->dev, "%s: count: %d\n", __func__, uinfo->count);

	return rc;
}

static int msm_ext_disp_edid_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	struct msm_ext_disp_audio_edid_blk edid_blk;
	int rc;

	codec_data = snd_soc_codec_get_drvdata(codec);
	if (!codec_data || !codec_data->ext_disp_ops.get_audio_edid_blk) {
		dev_err(codec->dev, "%s: codec_data or get_audio_edid_blk() is NULL\n",
			__func__);
		return -EINVAL;
	}

	rc = codec_data->ext_disp_ops.get_audio_edid_blk(
			codec_data->ext_disp_core_pdev, &edid_blk);
	if (rc >= 0) {
		if (sizeof(ucontrol->value.bytes.data) <
			  (edid_blk.audio_data_blk_size +
			   edid_blk.spk_alloc_data_blk_size)) {
			dev_err(codec->dev,
				"%s: Not enough memory to copy EDID data\n",
				__func__);
			return -ENOMEM;
		}

		memcpy(ucontrol->value.bytes.data,
		       edid_blk.audio_data_blk,
		       edid_blk.audio_data_blk_size);
		memcpy((ucontrol->value.bytes.data +
		       edid_blk.audio_data_blk_size),
		       edid_blk.spk_alloc_data_blk,
		       edid_blk.spk_alloc_data_blk_size);

		dev_dbg(codec->dev, "%s: data_blk_size:%d, spk_alloc_data_blk_size:%d\n",
			__func__, edid_blk.audio_data_blk_size,
			edid_blk.spk_alloc_data_blk_size);
	}

	return rc;
}

static int msm_ext_disp_audio_type_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	enum msm_ext_disp_cable_state cable_state;
	enum msm_ext_disp_type disp_type;
	int rc;

	codec_data = snd_soc_codec_get_drvdata(codec);
	if (!codec_data ||
	    !codec_data->ext_disp_ops.get_audio_edid_blk ||
	    !codec_data->ext_disp_ops.get_intf_id) {
		dev_err(codec->dev, "%s: codec_data, get_audio_edid_blk() or get_intf_id is NULL\n",
			__func__);
		return -EINVAL;
	}

	cable_state = codec_data->ext_disp_ops.cable_status(
				   codec_data->ext_disp_core_pdev, 1);
	if (cable_state < 0) {
		dev_err(codec->dev, "%s: Error retrieving cable state from ext_disp, err:%d\n",
			__func__, cable_state);
		rc = cable_state;
		goto done;
	}

	codec_data->cable_status = cable_state;
	if (cable_state == EXT_DISPLAY_CABLE_DISCONNECT) {
		dev_err(codec->dev, "%s: Display cable disconnected\n",
			__func__);
		ucontrol->value.integer.value[0] = 0;
		rc = 0;
		goto done;
	}

	disp_type = codec_data->ext_disp_ops.get_intf_id(
						codec_data->ext_disp_core_pdev);
	if (disp_type >= 0) {
		switch (disp_type) {
		case EXT_DISPLAY_TYPE_DP:
			ucontrol->value.integer.value[0] = 2;
			rc = 0;
			break;
		case EXT_DISPLAY_TYPE_HDMI:
			ucontrol->value.integer.value[0] = 1;
			rc = 0;
			break;
		default:
			rc = -EINVAL;
			dev_err(codec->dev, "%s: Invalid disp_type:%d\n",
			       __func__, disp_type);
			goto done;
		}
		dev_dbg(codec->dev, "%s: Display type: %d\n",
			__func__, disp_type);
	} else {
		dev_err(codec->dev, "%s: Error retrieving disp_type from ext_disp, err:%d\n",
			__func__, disp_type);
		rc = disp_type;
	}

done:
	return rc;
}

static int msm_ext_disp_audio_ack_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	u32 ack_state = 0;
	int rc;

	codec_data = snd_soc_codec_get_drvdata(codec);
	if (!codec_data ||
	    !codec_data->ext_disp_ops.acknowledge) {
		dev_err(codec->dev,
			"%s: codec_data or ops acknowledge() is NULL\n",
			__func__);
		rc = -EINVAL;
		goto done;
	}

	switch (ucontrol->value.enumerated.item[0]) {
	case 0:
		ack_state = AUD_EXT_DISP_ACK_DISCONNECT;
		break;
	case 1:
		ack_state = AUD_EXT_DISP_ACK_CONNECT;
		break;
	case 2:
		ack_state = AUD_EXT_DISP_ACK_ENABLE;
		break;
	default:
		rc = -EINVAL;
		dev_err(codec->dev,
			"%s: invalid value %d for mixer ctl\n",
			__func__, ucontrol->value.enumerated.item[0]);
		goto done;
	}
	dev_dbg(codec->dev, "%s: control %d, ack set value 0x%x\n",
		__func__, ucontrol->value.enumerated.item[0], ack_state);

	rc = codec_data->ext_disp_ops.acknowledge(
			 codec_data->ext_disp_core_pdev, ack_state);
	if (rc < 0) {
		dev_err(codec->dev, "%s: error from acknowledge(), err:%d\n",
			__func__, rc);
	}

done:
	return rc;
}

static const struct snd_kcontrol_new msm_ext_disp_codec_rx_controls[] = {
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.iface  = SNDRV_CTL_ELEM_IFACE_PCM,
		.name   = "HDMI EDID",
		.info   = msm_ext_disp_edid_ctl_info,
		.get    = msm_ext_disp_edid_get,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.iface  = SNDRV_CTL_ELEM_IFACE_PCM,
		.name   = "Display Port EDID",
		.info   = msm_ext_disp_edid_ctl_info,
		.get    = msm_ext_disp_edid_get,
	},
	SOC_ENUM_EXT("External Display Type", ext_disp_audio_type,
		     msm_ext_disp_audio_type_get, NULL),
	SOC_ENUM_EXT("External Display Audio Ack", ext_disp_audio_ack_state,
		     NULL, msm_ext_disp_audio_ack_set),
};

static int msm_ext_disp_audio_codec_rx_dai_startup(
		struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret = 0;
	struct msm_ext_disp_audio_codec_rx_data *codec_data =
			dev_get_drvdata(dai->codec->dev);

	if (!codec_data || !codec_data->ext_disp_ops.cable_status) {
		dev_err(dai->dev, "%s() codec_data or cable_status is null\n",
			__func__);
		return -EINVAL;
	}

	codec_data->cable_status =
		codec_data->ext_disp_ops.cable_status(
		codec_data->ext_disp_core_pdev, 1);
	if (codec_data->cable_status < 0) {
		dev_err(dai->dev,
			"%s() ext disp core is not ready (ret val = %d)\n",
			__func__, codec_data->cable_status);
		ret = codec_data->cable_status;
	} else if (!codec_data->cable_status) {
		dev_err(dai->dev,
			"%s() ext disp cable is not connected (ret val = %d)\n",
			__func__, codec_data->cable_status);
		ret = -ENODEV;
	}

	return ret;
}

static int msm_ext_disp_audio_codec_rx_dai_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	u32 channel_allocation = 0;
	u32 level_shift  = 0; /* 0dB */
	bool down_mix = 0;
	u32 num_channels = params_channels(params);
	int rc = 0;
	struct msm_ext_disp_audio_setup_params audio_setup_params = {0};

	struct msm_ext_disp_audio_codec_rx_data *codec_data =
			dev_get_drvdata(dai->codec->dev);

	if (!codec_data || !codec_data->ext_disp_ops.audio_info_setup) {
		dev_err(dai->dev, "%s: codec_data or audio_info_setup is null\n",
			__func__);
		return -EINVAL;
	}

	if (codec_data->cable_status < 0) {
		dev_err_ratelimited(dai->dev,
			"%s() ext disp core is not ready (ret val = %d)\n",
			__func__, codec_data->cable_status);
		return codec_data->cable_status;
	} else if (!codec_data->cable_status) {
		dev_err_ratelimited(dai->dev,
			"%s() ext disp cable is not connected (ret val = %d)\n",
			__func__, codec_data->cable_status);
		return -ENODEV;
	}

	/*refer to HDMI spec CEA-861-E: Table 28 Audio InfoFrame Data Byte 4*/
	switch (num_channels) {
	case 2:
		channel_allocation  = 0;
		break;
	case 3:
		channel_allocation  = 0x02;/*default to FL/FR/FC*/
		audio_setup_params.sample_present = 0x3;
		break;
	case 4:
		channel_allocation  = 0x06;/*default to FL/FR/FC/RC*/
		audio_setup_params.sample_present = 0x7;
		break;
	case 5:
		channel_allocation  = 0x0A;/*default to FL/FR/FC/RR/RL*/
		audio_setup_params.sample_present = 0x7;
		break;
	case 6:
		channel_allocation  = 0x0B;
		audio_setup_params.sample_present = 0x7;
		break;
	case 7:
		channel_allocation  = 0x12;/*default to FL/FR/FC/RL/RR/RRC/RLC*/
		audio_setup_params.sample_present = 0xf;
		break;
	case 8:
		channel_allocation  = 0x13;
		audio_setup_params.sample_present = 0xf;
		break;
	default:
		dev_err(dai->dev, "invalid Channels = %u\n", num_channels);
		return -EINVAL;
	}

	dev_dbg(dai->dev,
		"%s() num_ch %u  samplerate %u channel_allocation = %u\n",
		__func__, num_channels, params_rate(params),
		channel_allocation);

	audio_setup_params.sample_rate_hz = params_rate(params);
	audio_setup_params.num_of_channels = num_channels;
	audio_setup_params.channel_allocation = channel_allocation;
	audio_setup_params.level_shift = level_shift;
	audio_setup_params.down_mix = down_mix;

	rc = codec_data->ext_disp_ops.audio_info_setup(
			codec_data->ext_disp_core_pdev, &audio_setup_params);
	if (rc < 0) {
		dev_err_ratelimited(dai->dev,
			"%s() ext disp core is not ready, rc: %d\n",
			__func__, rc);
	}

	return rc;
}

static void msm_ext_disp_audio_codec_rx_dai_shutdown(
		struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int rc;

	struct msm_ext_disp_audio_codec_rx_data *codec_data =
			dev_get_drvdata(dai->codec->dev);

	if (!codec_data || !codec_data->ext_disp_ops.teardown_done ||
	    !codec_data->ext_disp_ops.cable_status) {
		dev_err(dai->dev, "%s: codec data or teardown_done or cable_status is null\n",
			__func__);
		return;
	}

	rc = codec_data->ext_disp_ops.cable_status(
			codec_data->ext_disp_core_pdev, 0);
	if (rc < 0) {
		dev_err(dai->dev,
			"%s: ext disp core had problems releasing audio flag\n",
			__func__);
	}

	codec_data->ext_disp_ops.teardown_done(
		codec_data->ext_disp_core_pdev);
}

static int msm_ext_disp_audio_codec_rx_probe(struct snd_soc_codec *codec)
{
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	struct device_node *of_node_parent = NULL;

	codec_data = kzalloc(sizeof(struct msm_ext_disp_audio_codec_rx_data),
		GFP_KERNEL);

	if (!codec_data) {
		dev_err(codec->dev, "%s(): fail to allocate dai data\n",
				__func__);
		return -ENOMEM;
	}

	of_node_parent = of_get_parent(codec->dev->of_node);
	if (!of_node_parent) {
		dev_err(codec->dev, "%s(): Parent device tree node not found\n",
				__func__);
		kfree(codec_data);
		return -ENODEV;
	}

	codec_data->ext_disp_core_pdev = of_find_device_by_node(of_node_parent);
	if (!codec_data->ext_disp_core_pdev) {
		dev_err(codec->dev, "%s(): can't get parent pdev\n", __func__);
		kfree(codec_data);
		return -ENODEV;
	}

	if (msm_ext_disp_register_audio_codec(codec_data->ext_disp_core_pdev,
				&codec_data->ext_disp_ops)) {
		dev_err(codec->dev, "%s(): can't register with ext disp core",
				__func__);
		kfree(codec_data);
		return -ENODEV;
	}

	dev_set_drvdata(codec->dev, codec_data);

	dev_dbg(codec->dev, "%s(): registered %s with ext disp core\n",
		__func__, codec->component.name);

	return 0;
}

static int msm_ext_disp_audio_codec_rx_remove(struct snd_soc_codec *codec)
{
	struct msm_ext_disp_audio_codec_rx_data *codec_data;

	codec_data = dev_get_drvdata(codec->dev);
	kfree(codec_data);

	return 0;
}

static struct snd_soc_dai_ops msm_ext_disp_audio_codec_rx_dai_ops = {
	.startup   = msm_ext_disp_audio_codec_rx_dai_startup,
	.hw_params = msm_ext_disp_audio_codec_rx_dai_hw_params,
	.shutdown  = msm_ext_disp_audio_codec_rx_dai_shutdown
};

static struct snd_soc_dai_driver msm_ext_disp_audio_codec_rx_dais[] = {
	{
		.name = "msm_hdmi_audio_codec_rx_dai",
		.playback = {
			.stream_name = "HDMI Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 48000,
			.rate_max = 48000,
			.rates = MSM_EXT_DISP_PCM_RATES,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &msm_ext_disp_audio_codec_rx_dai_ops,
	},
	{
		.name = "msm_dp_audio_codec_rx_dai",
		.playback = {
			.stream_name = "Display Port Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 48000,
			.rate_max = 192000,
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE |
				   SNDRV_PCM_FMTBIT_S24_3LE,
		},
		.ops = &msm_ext_disp_audio_codec_rx_dai_ops,
	},
};

static struct snd_soc_codec_driver msm_ext_disp_audio_codec_rx_soc_driver = {
	.probe = msm_ext_disp_audio_codec_rx_probe,
	.remove =  msm_ext_disp_audio_codec_rx_remove,
	.component_driver = {
		.controls = msm_ext_disp_codec_rx_controls,
		.num_controls = ARRAY_SIZE(msm_ext_disp_codec_rx_controls),
	},
};

static int msm_ext_disp_audio_codec_rx_plat_probe(
		struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s(): dev name %s\n", __func__,
		dev_name(&pdev->dev));

	return snd_soc_register_codec(&pdev->dev,
		&msm_ext_disp_audio_codec_rx_soc_driver,
		msm_ext_disp_audio_codec_rx_dais,
		ARRAY_SIZE(msm_ext_disp_audio_codec_rx_dais));
}

static int msm_ext_disp_audio_codec_rx_plat_remove(
		struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}
static const struct of_device_id msm_ext_disp_audio_codec_rx_dt_match[] = {
	{ .compatible = "qcom,msm-ext-disp-audio-codec-rx", },
	{}
};
MODULE_DEVICE_TABLE(of, msm_ext_disp_audio_codec_rx_dt_match);

static struct platform_driver msm_ext_disp_audio_codec_rx_driver = {
	.driver = {
		.name = "msm-ext-disp-audio-codec-rx",
		.owner = THIS_MODULE,
		.of_match_table = msm_ext_disp_audio_codec_rx_dt_match,
		.suppress_bind_attrs = true,
	},
	.probe = msm_ext_disp_audio_codec_rx_plat_probe,
	.remove = msm_ext_disp_audio_codec_rx_plat_remove,
};

static int __init msm_ext_disp_audio_codec_rx_init(void)
{
	int rc;

	rc = platform_driver_register(&msm_ext_disp_audio_codec_rx_driver);
	if (rc) {
		pr_err("%s: failed to register ext disp codec driver err:%d\n",
		       __func__, rc);
	}

	return rc;
}
module_init(msm_ext_disp_audio_codec_rx_init);

static void __exit msm_ext_disp_audio_codec_rx_exit(void)
{
	platform_driver_unregister(&msm_ext_disp_audio_codec_rx_driver);
}
module_exit(msm_ext_disp_audio_codec_rx_exit);

MODULE_DESCRIPTION("MSM External Display Audio CODEC Driver");
MODULE_LICENSE("GPL v2");
