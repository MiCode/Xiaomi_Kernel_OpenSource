// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
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

#define DRV_NAME "HDMI_codec"

#define MSM_EXT_DISP_PCM_RATES	SNDRV_PCM_RATE_48000
#define AUD_EXT_DISP_ACK_DISCONNECT (AUDIO_ACK_CONNECT ^ AUDIO_ACK_CONNECT)
#define AUD_EXT_DISP_ACK_CONNECT    (AUDIO_ACK_CONNECT)
#define AUD_EXT_DISP_ACK_ENABLE     (AUDIO_ACK_SET_ENABLE | AUDIO_ACK_ENABLE)

#define SOC_EXT_DISP_AUDIO_TYPE(index) \
	static SOC_ENUM_SINGLE_DECL(ext_disp_audio_type##index, SND_SOC_NOPM, \
				    index, ext_disp_audio_type_text)
#define SOC_EXT_DISP_AUDIO_ACK_STATE(index) \
	static SOC_ENUM_SINGLE_DECL(ext_disp_audio_ack_state##index, \
			    SND_SOC_NOPM, index, ext_disp_audio_ack_text)

#define SWITCH_DP_CODEC(codec_info, codec_data, dai_id, type) \
	codec_info.type = type; \
	codec_info.ctrl_id = codec_data->ctl[dai_id]; \
	codec_info.stream_id = codec_data->stream[dai_id]; \

enum {
        DP_CONTROLLER0 = 0,
        DP_CONTROLLER_MAX,
};

enum {
	DP_STREAM0 = 0,
	DP_STREAM1,
	HDMI,
	DP_STREAM_MAX,
};

/*
 * Dai id cannot be zero, if component has more than one dai and dai id
 * is used to differentiate between them
 */
enum {
	DP_DAI1 = 1,
	DP_DAI2,
	HDMI_DAI,
	HDMI_MS_DAI,
	DP_DAI_MAX,
};

static const char *const ext_disp_audio_type_text[] = {"None", "HDMI", "DP"};
static const char *const ext_disp_audio_ack_text[] = {"Disconnect",  "Connect",
						      "Ack_Enable"};

SOC_EXT_DISP_AUDIO_TYPE(1);
SOC_EXT_DISP_AUDIO_ACK_STATE(1);
SOC_EXT_DISP_AUDIO_TYPE(2);
SOC_EXT_DISP_AUDIO_ACK_STATE(2);
SOC_EXT_DISP_AUDIO_TYPE(3);
SOC_EXT_DISP_AUDIO_ACK_STATE(3);

struct msm_ext_disp_audio_codec_rx_data {
	struct platform_device *ext_disp_core_pdev;
	struct msm_ext_disp_audio_codec_ops ext_disp_ops;
	struct mutex dp_ops_lock;
	int cable_status[DP_DAI_MAX];
	int stream[DP_DAI_MAX];
	int ctl[DP_DAI_MAX];
};

static int msm_ext_disp_edid_ctl_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	struct msm_ext_disp_audio_edid_blk edid_blk;
	int rc = 0;
	struct msm_ext_disp_codec_id codec_info;
	int dai_id = kcontrol->private_value;
	int type;

	codec_data = snd_soc_component_get_drvdata(component);
	if (!codec_data) {
		dev_err(component->dev, "%s: codec_data is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(component->dev, "%s: DP ctl id %d Stream id %d\n", __func__,
		codec_data->ctl[dai_id], codec_data->stream[dai_id]);

	mutex_lock(&codec_data->dp_ops_lock);
	if (dai_id == HDMI_MS_DAI)
		type = EXT_DISPLAY_TYPE_HDMI;
	else
		type = EXT_DISPLAY_TYPE_DP;
	SWITCH_DP_CODEC(codec_info, codec_data, dai_id, type);
	rc = msm_ext_disp_select_audio_codec(codec_data->ext_disp_core_pdev,
						 &codec_info);
	if (!codec_data->ext_disp_ops.get_audio_edid_blk || rc) {
		dev_dbg(component->dev, "%s: get_audio_edid_blk() is NULL\n",
			__func__);
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
		uinfo->count = 0;
		mutex_unlock(&codec_data->dp_ops_lock);
		return 0;
	}

	rc = codec_data->ext_disp_ops.get_audio_edid_blk(
				codec_data->ext_disp_core_pdev, &edid_blk);
	mutex_unlock(&codec_data->dp_ops_lock);
	if (rc >= 0) {
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
		uinfo->count = edid_blk.audio_data_blk_size +
			edid_blk.spk_alloc_data_blk_size;
	}

	dev_dbg(component->dev, "%s: count: %d\n", __func__, uinfo->count);

	return rc;
}

static int msm_ext_disp_edid_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	struct msm_ext_disp_audio_edid_blk edid_blk;
	struct msm_ext_disp_codec_id codec_info;
	int rc = 0;
	int dai_id = kcontrol->private_value;
	int type;

	codec_data = snd_soc_component_get_drvdata(component);
	if (!codec_data) {
		dev_err(component->dev, "%s: codec_data is NULL\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(component->dev, "%s: DP ctl id %d Stream id %d\n", __func__,
		codec_data->ctl[dai_id], codec_data->stream[dai_id]);

	mutex_lock(&codec_data->dp_ops_lock);
	if (dai_id == HDMI_MS_DAI)
		type = EXT_DISPLAY_TYPE_HDMI;
	else
		type = EXT_DISPLAY_TYPE_DP;
	SWITCH_DP_CODEC(codec_info, codec_data, dai_id, type);
	rc = msm_ext_disp_select_audio_codec(codec_data->ext_disp_core_pdev,
						 &codec_info);
	if (!codec_data->ext_disp_ops.get_audio_edid_blk || rc) {
		dev_err(component->dev, "%s: codec_data or get_audio_edid_blk() is NULL\n",
			__func__);
		mutex_unlock(&codec_data->dp_ops_lock);
		return -EINVAL;
	}
	rc = codec_data->ext_disp_ops.get_audio_edid_blk(
			codec_data->ext_disp_core_pdev, &edid_blk);
	mutex_unlock(&codec_data->dp_ops_lock);
	if (rc >= 0) {
		if (sizeof(ucontrol->value.bytes.data) <
			  (edid_blk.audio_data_blk_size +
			   edid_blk.spk_alloc_data_blk_size)) {
			dev_err(component->dev,
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

		dev_dbg(component->dev, "%s: data_blk_size:%d, spk_alloc_data_blk_size:%d\n",
			__func__, edid_blk.audio_data_blk_size,
			edid_blk.spk_alloc_data_blk_size);
	}

	return rc;
}

static int msm_ext_disp_audio_type_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	enum msm_ext_disp_cable_state cable_state;
	enum msm_ext_disp_type disp_type;
	struct msm_ext_disp_codec_id codec_info;
	int rc = 0;
	int dai_id = ((struct soc_enum *) kcontrol->private_value)->shift_l;
	int type;

	codec_data = snd_soc_component_get_drvdata(component);
	if (!codec_data) {
		dev_err(component->dev, "%s: codec_data is NULL\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(component->dev, "%s: DP ctl id %d Stream id %d\n", __func__,
		codec_data->ctl[dai_id], codec_data->stream[dai_id]);

	mutex_lock(&codec_data->dp_ops_lock);
	if (dai_id == HDMI_MS_DAI)
		type = EXT_DISPLAY_TYPE_HDMI;
	else
		type = EXT_DISPLAY_TYPE_DP;
	SWITCH_DP_CODEC(codec_info, codec_data, dai_id, type);
	rc = msm_ext_disp_select_audio_codec(codec_data->ext_disp_core_pdev,
						 &codec_info);

	if (!codec_data->ext_disp_ops.cable_status ||
	    !codec_data->ext_disp_ops.get_intf_id || rc) {
		dev_err_ratelimited(component->dev, "%s: cable_status() or get_intf_id is NULL\n",
			__func__);
		rc = -EINVAL;
		goto cable_err;
	}

	cable_state = codec_data->ext_disp_ops.cable_status(
				codec_data->ext_disp_core_pdev, 1);
	if (cable_state < 0) {
		dev_err(component->dev, "%s: Error retrieving cable state from ext_disp, err:%d\n",
			__func__, cable_state);
		rc = cable_state;
		goto cable_err;
	}

	codec_data->cable_status[dai_id] = cable_state;
	if (cable_state == EXT_DISPLAY_CABLE_DISCONNECT) {
		dev_err(component->dev, "%s: Display cable disconnected\n",
			__func__);
		ucontrol->value.integer.value[0] = 0;
		rc = 0;
		goto cable_err;
	}

	disp_type = codec_data->ext_disp_ops.get_intf_id(
						codec_data->ext_disp_core_pdev);
	mutex_unlock(&codec_data->dp_ops_lock);
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
			dev_err(component->dev, "%s: Invalid disp_type:%d\n",
			       __func__, disp_type);
			goto done;
		}
		dev_dbg(component->dev, "%s: Display type: %d\n",
			__func__, disp_type);
	} else {
		dev_err(component->dev, "%s: Error retrieving disp_type from ext_disp, err:%d\n",
			__func__, disp_type);
		rc = disp_type;
	}
	return rc;

cable_err:
	mutex_unlock(&codec_data->dp_ops_lock);
done:
	return rc;
}

static int msm_ext_disp_audio_ack_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	u32 ack_state = 0;
	struct msm_ext_disp_codec_id codec_info;
	int rc = 0;
	int dai_id = ((struct soc_enum *) kcontrol->private_value)->shift_l;
	int type;

	codec_data = snd_soc_component_get_drvdata(component);
	if (!codec_data) {
		dev_err(component->dev,
			"%s: codec_data is NULL\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(component->dev, "%s: DP ctl id %d Stream id %d\n", __func__,
		codec_data->ctl[dai_id], codec_data->stream[dai_id]);

	mutex_lock(&codec_data->dp_ops_lock);
	if (dai_id == HDMI_MS_DAI)
		type = EXT_DISPLAY_TYPE_HDMI;
	else
		type = EXT_DISPLAY_TYPE_DP;
	SWITCH_DP_CODEC(codec_info, codec_data, dai_id, type);
	rc = msm_ext_disp_select_audio_codec(codec_data->ext_disp_core_pdev,
						 &codec_info);

	if (!codec_data->ext_disp_ops.acknowledge || rc) {
		dev_err(component->dev,
			"%s: codec_data ops acknowledge() is NULL\n",
			__func__);
		rc = -EINVAL;
		goto err;
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
		dev_err(component->dev,
			"%s: invalid value %d for mixer ctl\n",
			__func__, ucontrol->value.enumerated.item[0]);
		goto err;
	}
	dev_dbg(component->dev, "%s: control %d, ack set value 0x%x\n",
		__func__, ucontrol->value.enumerated.item[0], ack_state);

	rc = codec_data->ext_disp_ops.acknowledge(
			 codec_data->ext_disp_core_pdev, ack_state);
	mutex_unlock(&codec_data->dp_ops_lock);
	if (rc < 0) {
		dev_err(component->dev, "%s: error from acknowledge(), err:%d\n",
			__func__, rc);
	}
	return rc;

err:
	mutex_unlock(&codec_data->dp_ops_lock);
	return rc;
}

static int msm_ext_disp_audio_device_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	int rc = 0;
	int dai_id = ((struct soc_multi_mixer_control *)
				kcontrol->private_value)->shift;

	if (dai_id < 0 || dai_id > DP_DAI2) {
		dev_err(component->dev,
			"%s: invalid dai id: %d\n", __func__, dai_id);
		rc = -EINVAL;
		goto done;
	}

	codec_data = snd_soc_component_get_drvdata(component);
	if (!codec_data) {
		dev_err(component->dev,
			"%s: codec_data or ops acknowledge() is NULL\n",
			__func__);
		rc = -EINVAL;
		goto done;
	}
	ucontrol->value.integer.value[0] = codec_data->ctl[dai_id];
	ucontrol->value.integer.value[1] = codec_data->stream[dai_id];

done:
	return rc;
}

static int msm_ext_disp_audio_device_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	int rc = 0;
	int dai_id = ((struct soc_multi_mixer_control *)
				kcontrol->private_value)->shift;

	if (dai_id < 0 || dai_id > DP_DAI2) {
		dev_err(component->dev,
			"%s: invalid dai id: %d\n", __func__, dai_id);
		rc = -EINVAL;
		goto done;
	}

	codec_data = snd_soc_component_get_drvdata(component);
	if (!codec_data) {
		dev_err(component->dev,
			"%s: codec_data or ops acknowledge() is NULL\n",
			__func__);
		rc = -EINVAL;
		goto done;
	}

	if ((ucontrol->value.integer.value[0] > (DP_CONTROLLER_MAX - 1)) ||
		(ucontrol->value.integer.value[1] > (DP_STREAM_MAX - 1)) ||
		(ucontrol->value.integer.value[0] < 0) ||
		(ucontrol->value.integer.value[1] < 0)) {
		dev_err(component->dev,
			"%s: DP audio control index invalid\n",
			__func__);
		rc = -EINVAL;
		goto done;
	}

	mutex_lock(&codec_data->dp_ops_lock);
	codec_data->ctl[dai_id] = ucontrol->value.integer.value[0];
	codec_data->stream[dai_id] = ucontrol->value.integer.value[1];
	mutex_unlock(&codec_data->dp_ops_lock);

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
		.private_value = HDMI_DAI,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.iface  = SNDRV_CTL_ELEM_IFACE_PCM,
		.name   = "HDMI MS EDID",
		.info   = msm_ext_disp_edid_ctl_info,
		.get    = msm_ext_disp_edid_get,
		.private_value = HDMI_MS_DAI,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.iface  = SNDRV_CTL_ELEM_IFACE_PCM,
		.name   = "Display Port EDID",
		.info   = msm_ext_disp_edid_ctl_info,
		.get    = msm_ext_disp_edid_get,
		.private_value = DP_DAI1,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.iface  = SNDRV_CTL_ELEM_IFACE_PCM,
		.name   = "Display Port1 EDID",
		.info   = msm_ext_disp_edid_ctl_info,
		.get    = msm_ext_disp_edid_get,
		.private_value = DP_DAI2,
	},
	SOC_ENUM_EXT("External Display Type",
		     ext_disp_audio_type1,
		     msm_ext_disp_audio_type_get, NULL),
	SOC_ENUM_EXT("External Display1 Type",
		     ext_disp_audio_type2,
		     msm_ext_disp_audio_type_get, NULL),
	SOC_ENUM_EXT("External HDMI Type",
		     ext_disp_audio_type3,
		     msm_ext_disp_audio_type_get, NULL),
	SOC_ENUM_EXT("External Display Audio Ack",
		     ext_disp_audio_ack_state1,
		     NULL, msm_ext_disp_audio_ack_set),
	SOC_ENUM_EXT("External Display1 Audio Ack",
		     ext_disp_audio_ack_state2,
		     NULL, msm_ext_disp_audio_ack_set),
	SOC_ENUM_EXT("External HDMI Audio Ack",
		     ext_disp_audio_ack_state3,
		     NULL, msm_ext_disp_audio_ack_set),

	SOC_SINGLE_MULTI_EXT("External Display Audio Device",
			SND_SOC_NOPM, DP_DAI1, DP_STREAM_MAX - 1, 0, 2,
			msm_ext_disp_audio_device_get,
			msm_ext_disp_audio_device_set),
	SOC_SINGLE_MULTI_EXT("External Display1 Audio Device",
			SND_SOC_NOPM, DP_DAI2, DP_STREAM_MAX - 1, 0, 2,
			msm_ext_disp_audio_device_get,
			msm_ext_disp_audio_device_set),
	SOC_SINGLE_MULTI_EXT("External HDMI Device",
			SND_SOC_NOPM, HDMI_MS_DAI, DP_STREAM_MAX - 1, 0, 2,
			msm_ext_disp_audio_device_get,
			msm_ext_disp_audio_device_set),

};

static int msm_ext_disp_audio_codec_rx_dai_startup(
		struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret = 0, rc = 0;
	struct msm_ext_disp_codec_id codec_info;
	struct msm_ext_disp_audio_codec_rx_data *codec_data =
			dev_get_drvdata(dai->component->dev);
	int type;

	if (!codec_data) {
		dev_err(dai->dev, "%s() codec_data is null\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(dai->component->dev, "%s: DP ctl id %d Stream id %d\n",
		__func__,
		codec_data->ctl[dai->id], codec_data->stream[dai->id]);

	mutex_lock(&codec_data->dp_ops_lock);
	if (dai->id == HDMI_MS_DAI)
		type = EXT_DISPLAY_TYPE_HDMI;
	else
		type = EXT_DISPLAY_TYPE_DP;
	SWITCH_DP_CODEC(codec_info, codec_data, dai->id, type);
	rc = msm_ext_disp_select_audio_codec(codec_data->ext_disp_core_pdev,
						 &codec_info);

	if (!codec_data->ext_disp_ops.cable_status || rc) {
		dev_err(dai->dev, "%s() cable_status is null\n",
			__func__);
		mutex_unlock(&codec_data->dp_ops_lock);
		return -EINVAL;
	}

	codec_data->cable_status[dai->id] =
		codec_data->ext_disp_ops.cable_status(
		codec_data->ext_disp_core_pdev, 1);
	mutex_unlock(&codec_data->dp_ops_lock);
	if (codec_data->cable_status[dai->id] < 0) {
		dev_err(dai->dev,
			"%s() ext disp core is not ready (ret val = %d)\n",
			__func__, codec_data->cable_status[dai->id]);
		ret = codec_data->cable_status[dai->id];
	} else if (!codec_data->cable_status[dai->id]) {
		dev_err(dai->dev,
			"%s() ext disp cable is not connected (ret val = %d)\n",
			__func__, codec_data->cable_status[dai->id]);
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
	struct msm_ext_disp_codec_id codec_info;
	int rc = 0;
	struct msm_ext_disp_audio_setup_params audio_setup_params = {0};
	int type;
	struct msm_ext_disp_audio_codec_rx_data *codec_data =
			dev_get_drvdata(dai->component->dev);

	if (!codec_data) {
		dev_err(dai->dev, "%s() codec_data is null\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(dai->component->dev, "%s: DP ctl id %d Stream id %d\n",
		__func__,
		codec_data->ctl[dai->id], codec_data->stream[dai->id]);

	mutex_lock(&codec_data->dp_ops_lock);
	if (dai->id == HDMI_MS_DAI)
		type = EXT_DISPLAY_TYPE_HDMI;
	else
		type = EXT_DISPLAY_TYPE_DP;
	SWITCH_DP_CODEC(codec_info, codec_data, dai->id, type);
	rc = msm_ext_disp_select_audio_codec(codec_data->ext_disp_core_pdev,
						 &codec_info);

	if (!codec_data->ext_disp_ops.audio_info_setup || rc) {
		dev_err(dai->dev, "%s: audio_info_setup is null\n",
			__func__);
		mutex_unlock(&codec_data->dp_ops_lock);
		return -EINVAL;
	}
	mutex_unlock(&codec_data->dp_ops_lock);

	if (codec_data->cable_status[dai->id] < 0) {
		dev_err_ratelimited(dai->dev,
			"%s() ext disp core is not ready (ret val = %d)\n",
			__func__, codec_data->cable_status[dai->id]);
		return codec_data->cable_status[dai->id];
	} else if (!codec_data->cable_status[dai->id]) {
		dev_err_ratelimited(dai->dev,
			"%s() ext disp cable is not connected (ret val = %d)\n",
			__func__, codec_data->cable_status[dai->id]);
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

	mutex_lock(&codec_data->dp_ops_lock);
	SWITCH_DP_CODEC(codec_info, codec_data, dai->id, type);
	rc = msm_ext_disp_select_audio_codec(codec_data->ext_disp_core_pdev,
						 &codec_info);
	if (rc)
		goto end;
	rc = codec_data->ext_disp_ops.audio_info_setup(
			codec_data->ext_disp_core_pdev, &audio_setup_params);
end:
	mutex_unlock(&codec_data->dp_ops_lock);
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
	int rc = 0;
	struct msm_ext_disp_codec_id codec_info;

	struct msm_ext_disp_audio_codec_rx_data *codec_data =
			dev_get_drvdata(dai->component->dev);
	int type;

	if (!codec_data) {
		dev_err(dai->dev, "%s() codec_data is null\n",
			__func__);
		return;
	}

	dev_dbg(dai->component->dev, "%s: DP ctl id %d Stream id %d\n",
		__func__,
		codec_data->ctl[dai->id], codec_data->stream[dai->id]);

	mutex_lock(&codec_data->dp_ops_lock);
	if (dai->id == HDMI_MS_DAI)
		type = EXT_DISPLAY_TYPE_HDMI;
	else
		type = EXT_DISPLAY_TYPE_DP;
	SWITCH_DP_CODEC(codec_info, codec_data, dai->id, type);
	rc = msm_ext_disp_select_audio_codec(codec_data->ext_disp_core_pdev,
						 &codec_info);

	if (!codec_data->ext_disp_ops.teardown_done ||
	    !codec_data->ext_disp_ops.cable_status || rc) {
		dev_err(dai->dev, "%s: teardown_done or cable_status is null\n",
			__func__);
		mutex_unlock(&codec_data->dp_ops_lock);
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
	mutex_unlock(&codec_data->dp_ops_lock);
}

static int msm_ext_disp_audio_codec_rx_probe(
		struct snd_soc_component *component)
{
	struct msm_ext_disp_audio_codec_rx_data *codec_data;
	struct device_node *of_node_parent = NULL;

	codec_data = kzalloc(sizeof(struct msm_ext_disp_audio_codec_rx_data),
		GFP_KERNEL);

	if (!codec_data) {
		dev_err(component->dev, "%s(): fail to allocate dai data\n",
				__func__);
		return -ENOMEM;
	}

	of_node_parent = of_get_parent(component->dev->of_node);
	if (!of_node_parent) {
		dev_err(component->dev, "%s(): Parent device tree node not found\n",
				__func__);
		kfree(codec_data);
		return -ENODEV;
	}

	codec_data->ext_disp_core_pdev = of_find_device_by_node(of_node_parent);
	if (!codec_data->ext_disp_core_pdev) {
		dev_err(component->dev, "%s(): can't get parent pdev\n",
			__func__);
		kfree(codec_data);
		return -ENODEV;
	}

	if (msm_ext_disp_register_audio_codec(codec_data->ext_disp_core_pdev,
				&codec_data->ext_disp_ops)) {
		dev_err(component->dev, "%s(): can't register with ext disp core",
				__func__);
		kfree(codec_data);
		return -ENODEV;
	}

	mutex_init(&codec_data->dp_ops_lock);
	dev_set_drvdata(component->dev, codec_data);

	dev_dbg(component->dev, "%s(): registered %s with ext disp core\n",
		__func__, component->name);

	return 0;
}

static void msm_ext_disp_audio_codec_rx_remove(
		struct snd_soc_component *component)
{
	struct msm_ext_disp_audio_codec_rx_data *codec_data;

	codec_data = dev_get_drvdata(component->dev);
	mutex_destroy(&codec_data->dp_ops_lock);
	kfree(codec_data);

	return;
}

static struct snd_soc_dai_ops msm_ext_disp_audio_codec_rx_dai_ops = {
	.startup   = msm_ext_disp_audio_codec_rx_dai_startup,
	.hw_params = msm_ext_disp_audio_codec_rx_dai_hw_params,
	.shutdown  = msm_ext_disp_audio_codec_rx_dai_shutdown
};

static struct snd_soc_dai_driver msm_ext_disp_audio_codec_rx_dais[] = {
	{
		.name = "msm_hdmi_audio_codec_rx_dai",
		.id = HDMI_DAI,
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
		.name = "msm_hdmi_ms_audio_codec_rx_dai",
		.id = HDMI_MS_DAI,
		.playback = {
			.stream_name = "HDMI MS Playback",
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
		.id = DP_DAI1,
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
	{
		.name = "msm_dp_audio_codec_rx1_dai",
		.id = DP_DAI2,
		.playback = {
			.stream_name = "Display Port1 Playback",
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

static const struct snd_soc_component_driver msm_ext_disp_codec_rx_driver = {
	.name = DRV_NAME,
	.probe = msm_ext_disp_audio_codec_rx_probe,
	.remove =  msm_ext_disp_audio_codec_rx_remove,
	.controls = msm_ext_disp_codec_rx_controls,
	.num_controls = ARRAY_SIZE(msm_ext_disp_codec_rx_controls),
};

static int msm_ext_disp_audio_codec_rx_plat_probe(
		struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s(): dev name %s\n", __func__,
		dev_name(&pdev->dev));

	return snd_soc_register_component(&pdev->dev,
		&msm_ext_disp_codec_rx_driver,
		msm_ext_disp_audio_codec_rx_dais,
		ARRAY_SIZE(msm_ext_disp_audio_codec_rx_dais));
}

static int msm_ext_disp_audio_codec_rx_plat_remove(
		struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
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
	int rc = 0;

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
