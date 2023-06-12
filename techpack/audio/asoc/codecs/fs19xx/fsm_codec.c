/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2018-10-22 File created.
 */

#include "fsm_public.h"
#if defined(CONFIG_FSM_CODEC)
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/miscdevice.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/version.h>


/* Supported rates and data formats */
#define FSM_RATES   SNDRV_PCM_RATE_8000_96000
#define FSM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE \
					 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

static const unsigned int fsm_rates[] = { 8000, 16000, 32000, 44100, 48000, 96000 };
static const struct snd_pcm_hw_constraint_list fsm_constraints = {
	.list = fsm_rates,
	.count = ARRAY_SIZE(fsm_rates),
};

static int fsm_get_scene_index(uint16_t scene)
{
	int index = 0;

	while (scene) {
		scene = (scene >> 1);
		if (scene == 0) {
			break;
		}
		index++;
	}

	return index;
}

int fsm_scene_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	fsm_config_t *cfg = fsm_get_config();
	int scene_index;

	if (!cfg) {
		ucontrol->value.integer.value[0] = -1;
		return 0;
	}
	scene_index = fsm_get_scene_index(cfg->next_scene);
	pr_info("scene: %04X, BIT(%d)", cfg->next_scene, scene_index);
	ucontrol->value.integer.value[0] = scene_index;

	return 0;
}

int fsm_scene_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int next_scene = ucontrol->value.integer.value[0];

	pr_info("next_scene: %d", next_scene);
	fsm_set_scene(next_scene);

	return 0;
}

int fsm_volume_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	fsm_config_t *cfg = fsm_get_config();
	int volume;

	volume = ((cfg != NULL) ? cfg->volume : FSM_VOLUME_MAX);
	ucontrol->value.integer.value[0] = volume;
	pr_info("volume: %ld", ucontrol->value.integer.value[0]);

	return 0;
}

int fsm_volume_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int volume = ucontrol->value.integer.value[0];

	pr_info("volume: %d", volume);
	fsm_set_volume(volume);

	return 0;
}

int fsm_stop_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	fsm_config_t *cfg = fsm_get_config();

	ucontrol->value.integer.value[0] = ((cfg != NULL) ? cfg->force_mute : 1);
	pr_info("stop: %ld", ucontrol->value.integer.value[0]);

	return 0;
}

int fsm_stop_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	fsm_config_t *cfg = fsm_get_config();
	int stop = ucontrol->value.integer.value[0];

	if (!cfg) {
		return -1;
	}
	pr_info("stop: %x", stop);
	if (stop) {
		cfg->force_mute = true;
		fsm_speaker_off();
	}
	else {
		cfg->force_mute = false;
		fsm_speaker_onn();
	}

	return 0;
}

int fsm_rotation_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	fsm_config_t *cfg = fsm_get_config();

	ucontrol->value.integer.value[0] = ((cfg != NULL) ? cfg->cur_angle : 0);
	pr_info("angle: %ld", ucontrol->value.integer.value[0]);

	return 0;
}

int fsm_rotation_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int angle = ucontrol->value.integer.value[0];

	pr_info("angle: %d", angle);
	fsm_stereo_rotation(angle);

	return 0;
}

static const struct snd_kcontrol_new fsm_snd_controls[] =
{
	SOC_SINGLE_EXT("FSM_Scene", SND_SOC_NOPM, 0, FSM_SCENE_MAX, 0,
			fsm_scene_get, fsm_scene_put),
	SOC_SINGLE_EXT("FSM_Volume", SND_SOC_NOPM, 0, FSM_VOLUME_MAX, 0,
			fsm_volume_get, fsm_volume_put),
	SOC_SINGLE_EXT("FSM_Stop", SND_SOC_NOPM, 0, 1, 0,
			fsm_stop_get, fsm_stop_put),
	SOC_SINGLE_EXT("FSM_Rotation", SND_SOC_NOPM, 0, 360, 0,
			fsm_rotation_get, fsm_rotation_put),
};

static struct snd_soc_dapm_widget fsm_dapm_widgets_common[] =
{
	/* Stream widgets */
	SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
#ifdef CONFIG_FSM_NONDSP
	SND_SOC_DAPM_AIF_OUT("AIF OUT", "AIF Capture", 0, SND_SOC_NOPM, 0, 0),
#endif
	SND_SOC_DAPM_OUTPUT("OUTL"),
	SND_SOC_DAPM_INPUT("AEC Loopback"),
};

static const struct snd_soc_dapm_route fsm_dapm_routes_common[] =
{
	{ "OUTL", NULL, "AIF IN" },
	{ "AIF OUT", NULL, "AEC Loopback" },
};

static struct snd_soc_dapm_context *snd_soc_fsm_get_dapm(
		struct snd_soc_codec *codec)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
	return &codec->dapm;
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
	return snd_soc_codec_get_dapm(codec);
#else
	return snd_soc_component_get_dapm(codec);
#endif
}

static int fsm_add_widgets(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_fsm_get_dapm(codec);

	snd_soc_add_codec_controls(codec, fsm_snd_controls,
				ARRAY_SIZE(fsm_snd_controls));

	if (1) return 0;

	snd_soc_dapm_new_controls(dapm, fsm_dapm_widgets_common,
				ARRAY_SIZE(fsm_dapm_widgets_common));
	snd_soc_dapm_add_routes(dapm, fsm_dapm_routes_common,
				ARRAY_SIZE(fsm_dapm_routes_common));
	return 0;
}

//#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
//static struct snd_soc_codec *snd_soc_kcontrol_codec(
//			struct snd_kcontrol *kcontrol)
//{
//	return snd_kcontrol_chip(kcontrol);
//}
//#endif

static int fsm_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	int ret;

	if (!substream->runtime) {
		return 0;
	}

	ret = snd_pcm_hw_constraint_mask64(substream->runtime, \
			SNDRV_PCM_HW_PARAM_FORMAT, FSM_FORMATS);
	if (ret < 0) {
		pr_err("set pcm param format fail:%d", ret);
		return ret;
	}

	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&fsm_constraints);

	return ret;
}


static void fsm_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
}

static int fsm_set_sysclk(struct snd_soc_dai *codec_dai,
			int clk_id, unsigned int freq, int dir)
{
	pr_info("freq:%d", freq);
	return 0;
}

static int fsm_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	//int format = 0;
	int ret = 0;

	pr_debug("fmt: %X", fmt);
	/*switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			break;
		case SND_SOC_DAIFMT_CBM_CFM:
		default:
			// only supports Slave mode
			pr_err("invalid DAI master/slave interface");
			ret = -EINVAL;
			break;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			format = 3;
			break;
		default:
			pr_err("invalid dai format: %x", fmt);
			ret = -EINVAL;
			break;
	}*/
	pr_info("format:%d, ret:%d", fmt, ret);

	return ret;
}

static int fsm_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	int chn_size;
	int format;
	int srate;
	int bclk;

	format = params_format(params);
	pr_debug("format:%X", format);
	switch (format)
	{
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		break;
	}

	srate = params_rate(params);
	chn_size = snd_pcm_format_physical_width(format);
	bclk = srate * chn_size * 2;
	fsm_set_i2s_clocks(srate, bclk);
	pr_info("srate:%d, chn:%d, chn_size:%d, bclk:%d", srate,
			params_channels(params), chn_size, bclk);

	return 0;
}

static int fsm_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{

	if (stream != SNDRV_PCM_STREAM_PLAYBACK) {
		pr_info("captrue stream");
		return 0;
	}

	if (mute) {
		fsm_speaker_off();
	}
	else {
		fsm_speaker_onn();
		fsm_afe_mod_ctrl(true);
	}

	return 0;
}

#ifdef FSM_UNUSED_CODE
static int fsm_digital_mute(struct snd_soc_dai *dai, int mute)
{
	return fsm_mute_stream(dai, mute, SNDRV_PCM_STREAM_PLAYBACK);
}

static int fsm_trigger(struct snd_pcm_substream *substream,
			int cmd, struct snd_soc_dai *dai)
{
	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
			break;
		case SNDRV_PCM_TRIGGER_STOP:
			break;
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			break;
		default:
			return -EINVAL;
	}
	return 0;
}
#endif

static const struct snd_soc_dai_ops fsm_aif_dai_ops = {
	.startup      = fsm_startup,
	.set_fmt      = fsm_set_fmt,
	.set_sysclk   = fsm_set_sysclk,
	.hw_params    = fsm_hw_params,
	.mute_stream  = fsm_mute_stream,
#ifdef FSM_UNUSED_CODE
	.digital_mute = fsm_digital_mute,
	.trigger      = fsm_trigger,
#endif
	.shutdown     = fsm_shutdown,
};

static struct snd_soc_dai_driver fsm_aif_dai[] = {
	{
		.name = "fs19xx-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = FSM_RATES,
			.formats = FSM_FORMATS,
		},
#ifdef CONFIG_FSM_NONDSP
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = FSM_RATES,
			.formats = FSM_FORMATS,
		},
#endif
		.ops = &fsm_aif_dai_ops,
		.symmetric_rates = 1,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
		//.symmetric_channels = 1,
		//.symmetric_samplebits = 1,
#endif
	},
};

static int fsm_codec_probe(struct snd_soc_codec *codec)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev;
	int ret;

	if (codec == NULL) {
		return -EINVAL;
	}
	pr_info("dev_name: %s", dev_name(codec->dev));
	fsm_dev = snd_soc_codec_get_drvdata(codec);
	if (cfg == NULL || fsm_dev == NULL) {
		return -EINVAL;
	}
	if (fsm_dev->id == 0) {
		fsm_set_pdev(codec->dev);
		ret = fsm_firmware_init(cfg->fw_name);
		if (ret) {
			// firmware init fail, it will retry in speaker on
			pr_err("firmware init fail:%d, retry later", ret);
		}
	}
	fsm_afe_init_controls(codec);
	ret = fsm_add_widgets(codec);
	pr_info("codec probe done");

	FSM_FUNC_EXIT(ret);
	return ret;
}

static remove_ret_type fsm_codec_remove(struct snd_soc_codec *codec)
{
	fsm_dev_t *fsm_dev;

	if (codec == NULL) {
		return remove_ret_val;
	}
	fsm_dev = snd_soc_codec_get_drvdata(codec);
	if (fsm_dev == NULL) {
		return remove_ret_val;
	}
	if (fsm_dev->id == 0) {
		fsm_firmware_deinit();
	}
	return remove_ret_val;
}

static struct snd_soc_codec_driver soc_codec_dev_fsm = {
	.probe  = fsm_codec_probe,
	.remove = fsm_codec_remove,
};

/**
 * fmt_single_name() @ sound/soc/soc-core.c
 */
#define NAME_SIZE (32)
static char *fmt_single_name(struct device *dev, int *id)
{
	char *found, name[NAME_SIZE];
	int id1, id2;

	if (dev_name(dev) == NULL)
		return NULL;

	strlcpy(name, dev_name(dev), NAME_SIZE);

	/* are we a "%s.%d" name (platform and SPI components) */
	found = strstr(name, dev->driver->name);
	if (found) {
		/* get ID */
		if (sscanf(&found[strlen(dev->driver->name)], ".%d", id) == 1) {

			/* discard ID from name if ID == -1 */
			if (*id == -1)
				found[strlen(dev->driver->name)] = '\0';
		}

	} else {
		/* I2C component devices are named "bus-addr"  */
		if (sscanf(name, "%x-%x", &id1, &id2) == 2) {
			char tmp[NAME_SIZE];

			/* create unique ID number from I2C addr and bus */
			*id = ((id1 & 0xffff) << 16) + id2;

			/* sanitize component name for DAI link creation */
			snprintf(tmp, NAME_SIZE, "%s.%s", dev->driver->name, name);
			strlcpy(name, tmp, NAME_SIZE);
		} else
			*id = 0;
	}

	return kstrdup(name, GFP_KERNEL);
}

int fsm_codec_register(struct device *dev, int id)
{
	fsm_config_t *cfg = fsm_get_config();
	int size = ARRAY_SIZE(fsm_aif_dai);
	int dai_id;
	int ret;

	if (!cfg || cfg->codec_inited >= size) {
		// not support codec or codec inited
		return MODULE_INITED;
	}
	pr_info("id:%d, size:%d", id, size);
	if (id < 0 || id >= size) {
		pr_err("invalid id: %d", id);
		return -EINVAL;
	}
	ret = snd_soc_register_codec(dev, &soc_codec_dev_fsm,
			&fsm_aif_dai[id], 1);
	if (ret < 0) {
		dev_err(dev, "failed to register CODEC DAI: %d", ret);
		return ret;
	}
	cfg->codec_inited++;
	cfg->codec_name[id] = fmt_single_name(dev, &dai_id);
	cfg->codec_dai_name[id] = kstrdup(fsm_aif_dai[id].name, GFP_KERNEL);
	pr_info("codec:%s, dai:%s", cfg->codec_name[id], cfg->codec_dai_name[id]);

	return ret;
}

void fsm_codec_unregister(struct device *dev)
{
	fsm_config_t *cfg = fsm_get_config();

	pr_info("enter");
	if (!cfg || !cfg->codec_inited) {
		return;
	}
	snd_soc_unregister_codec(dev);
	cfg->codec_inited--;
}
#endif
