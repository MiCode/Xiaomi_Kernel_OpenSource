/*
 * tegra_cs42l73.c - Tegra machine ASoC driver for boards using CS42L73 codec.
 *
 * Author: Vijay Mali <vmali@nvidia.com>
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <asm/mach-types.h>

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/a2220.h>
#include <linux/edp.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#include <mach/tegra_asoc_pdata.h>
#include <mach/gpio-tegra.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../codecs/cs42l73.h"

#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
#include "tegra30_ahub.h"
#include "tegra30_i2s.h"
#include "tegra30_dam.h"
#endif
#define DRV_NAME "tegra-snd-cs42l73"

#define GPIO_SPKR_EN    BIT(0)
#define GPIO_HP_MUTE    BIT(1)
#define GPIO_INT_MIC_EN BIT(2)
#define GPIO_EXT_MIC_EN BIT(3)
#define GPIO_HP_DET     BIT(4)

#define DAI_LINK_HIFI		0
#define DAI_LINK_SPDIF		1
#define DAI_LINK_BTSCO		2
#define DAI_LINK_VOICE_CALL	3
#define DAI_LINK_BT_VOICE_CALL	4
#define NUM_DAI_LINKS	5

const char *tegra_cs42l73_i2s_dai_name[TEGRA30_NR_I2S_IFC] = {
	"tegra30-i2s.0",
	"tegra30-i2s.1",
	"tegra30-i2s.2",
	"tegra30-i2s.3",
	"tegra30-i2s.4",
};

extern int g_is_call_mode;


struct tegra_cs42l73 {
	struct tegra_asoc_utils_data util_data;
	struct tegra_asoc_platform_data *pdata;
	int gpio_requested;
	bool init_done;
	int is_call_mode;
	int is_device_bt;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	struct codec_config codec_info[NUM_I2S_DEVICES];
#endif
	struct regulator *dmic_reg;
	struct regulator *dmic_1v8_reg;
	struct regulator *hmic_reg;
	struct regulator *spkr_reg;
	struct edp_client *spk_edp_client;
	enum snd_soc_bias_level bias_level;
	struct snd_soc_card *pcard;
#ifdef CONFIG_SWITCH
	int jack_status;
#endif
	volatile int clock_enabled;
};

static int tegra_call_mode_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int tegra_call_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_cs42l73 *machine = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = machine->is_call_mode;

	return 0;
}

static int tegra_call_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_cs42l73 *machine = snd_kcontrol_chip(kcontrol);
	int is_call_mode_new = ucontrol->value.integer.value[0];
	int codec_index;
	unsigned int i;
	int uses_voice_codec;

	if (machine->is_call_mode == is_call_mode_new)
		return 0;

	if (machine->is_device_bt) {
		codec_index = BT_SCO;
		uses_voice_codec = 0;
	} else {
		codec_index = VOICE_CODEC;
		uses_voice_codec = 1;
	}

	if (is_call_mode_new) {
		if (machine->codec_info[codec_index].rate == 0 ||
			machine->codec_info[codec_index].channels == 0)
				return -EINVAL;

		for (i = 0; i < machine->pcard->num_links; i++)
			machine->pcard->dai_link[i].ignore_suspend = 1;

		tegra30_make_voice_call_connections(
			&machine->codec_info[codec_index],
			&machine->codec_info[BASEBAND], uses_voice_codec);
	} else {
		tegra30_break_voice_call_connections(
			&machine->codec_info[codec_index],
			&machine->codec_info[BASEBAND], uses_voice_codec);

		for (i = 0; i < machine->pcard->num_links; i++)
			machine->pcard->dai_link[i].ignore_suspend = 0;
	}

	machine->is_call_mode = is_call_mode_new;
	g_is_call_mode = machine->is_call_mode;

	return 1;
}

struct snd_kcontrol_new tegra_call_mode_control = {
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Call Mode Switch",
	.private_value = 0xffff,
	.info = tegra_call_mode_info,
	.get = tegra_call_mode_get,
	.put = tegra_call_mode_put
};

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static int tegra_cs42l73_set_dam_cif(int dam_ifc, int srate,
			int channels, int bit_size, int src_on, int src_srate,
			int src_channels, int src_bit_size)
{
	tegra30_dam_set_gain(dam_ifc, TEGRA30_DAM_CHIN1, 0x1000);
	tegra30_dam_set_samplerate(dam_ifc, TEGRA30_DAM_CHOUT,
				srate);
	tegra30_dam_set_samplerate(dam_ifc, TEGRA30_DAM_CHIN1,
				srate);
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
	tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHIN1,
		channels, bit_size, channels,
				32);
	tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHOUT,
		channels, bit_size, channels,
				32);
#else
	tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHIN1,
		channels, bit_size, channels,
				bit_size);
	tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHOUT,
		channels, bit_size, channels,
				bit_size);
#endif

	tegra30_dam_set_gain(dam_ifc, TEGRA30_DAM_CHIN0_SRC, 0x1000);
	if (src_on) {
		tegra30_dam_set_samplerate(dam_ifc, TEGRA30_DAM_CHIN0_SRC,
			src_srate);
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
		tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHIN0_SRC,
			src_channels, src_bit_size, 1, 32);
#else
		tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHIN0_SRC,
			src_channels, src_bit_size, 1, 16);
#endif
	}

	return 0;
}
#endif


static int tegra_cs42l73_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
#endif
	int srate, mclk, sample_size, i2s_daifmt, i2s_master;
	int err;
	int rate;

	i2s_master = pdata->i2s_param[HIFI_CODEC].is_i2s_master;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_size = 16;
		break;
	default:
		return -EINVAL;
	}
	srate = params_rate(params);

	switch (srate) {
	case 8000:
		mclk = 6144000;
		break;
	case 16000:
	case 32000:
		mclk = 12288000;
		break;
	case 11025:
		mclk = 512 * srate;
		break;
	default:
		mclk = 256 * srate;
		break;
	}

	i2s_daifmt = SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= i2s_master ? SND_SOC_DAIFMT_CBS_CFS :
				SND_SOC_DAIFMT_CBM_CFM;

	switch (pdata->i2s_param[HIFI_CODEC].i2s_mode) {
	case TEGRA_DAIFMT_I2S:
		i2s_daifmt |= SND_SOC_DAIFMT_I2S;
		break;
	case TEGRA_DAIFMT_DSP_A:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
		break;
	case TEGRA_DAIFMT_DSP_B:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_B;
		break;
	case TEGRA_DAIFMT_LEFT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
		break;
	case TEGRA_DAIFMT_RIGHT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
		break;
	default:
		dev_err(card->dev, "Can't configure i2s format\n");
		return -EINVAL;
	}

	a2220_port_path_change(pdata->i2s_param[HIFI_CODEC].is_i2s_master ?
						   A100_msg_PortC_D_PASS :
						   A100_msg_PortD_C_PASS);

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		if (!(machine->util_data.set_mclk % mclk)) {
			mclk = machine->util_data.set_mclk;
		} else {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

	rate = clk_get_rate(machine->util_data.clk_cdev1);

	err = snd_soc_dai_set_fmt(codec_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "codec_dai fmt not set\n");
		return err;
	}

	err = snd_soc_dai_set_fmt(cpu_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai fmt not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(codec_dai, 0, rate, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return err;
	}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) &&
		i2s->is_dam_used)
		tegra_cs42l73_set_dam_cif(i2s->dam_ifc, srate,
		params_channels(params), sample_size, 0, 0, 0, 0);
#endif

	return 0;
}

#if 1
static int tegra_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	int srate, mclk, min_mclk;
	int err;

	srate = params_rate(params);
	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		mclk = 11289600;
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		mclk = 12288000;
		break;
	default:
		return -EINVAL;
	}
	min_mclk = 128 * srate;

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		if (!(machine->util_data.set_mclk % min_mclk))
			mclk = machine->util_data.set_mclk;
		else {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

	return 0;
}
#endif

static int tegra_bt_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(rtd->cpu_dai);
#endif
	struct snd_soc_card *card = rtd->card;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int err, srate, mclk, min_mclk, sample_size;
	int i2s_daifmt;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_size = 16;
		break;
	default:
		return -EINVAL;
	}

	srate = params_rate(params);
	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		mclk = 11289600;
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		mclk = 12288000;
		break;
	default:
		return -EINVAL;
	}
	min_mclk = 64 * srate;

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		if (!(machine->util_data.set_mclk % min_mclk))
			mclk = machine->util_data.set_mclk;
		else {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

	i2s_daifmt = SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= pdata->i2s_param[BT_SCO].is_i2s_master ?
			SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;

	switch (pdata->i2s_param[BT_SCO].i2s_mode) {
	case TEGRA_DAIFMT_I2S:
		i2s_daifmt |= SND_SOC_DAIFMT_I2S;
		break;
	case TEGRA_DAIFMT_DSP_A:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
		break;
	case TEGRA_DAIFMT_DSP_B:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_B;
		break;
	case TEGRA_DAIFMT_LEFT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
		break;
	case TEGRA_DAIFMT_RIGHT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
		break;
	default:
		dev_err(card->dev, "Can't configure i2s format\n");
		return -EINVAL;
	}

	err = snd_soc_dai_set_fmt(rtd->cpu_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(rtd->codec->card->dev, "cpu_dai fmt not set\n");
		return err;
	}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) &&
		i2s->is_dam_used)
		tegra_cs42l73_set_dam_cif(i2s->dam_ifc, params_rate(params),
		params_channels(params), sample_size, 0, 0, 0, 0);
#endif

	return 0;
}

static int tegra_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 0);

	return 0;
}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static int tegra_cs42l73_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(rtd->card);
	struct codec_config *codec_info;
	struct codec_config *bb_info;
	struct codec_config *hifi_info;
	int codec_index;

	if (!i2s->is_dam_used)
		return 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/*dam configuration*/
		if (!i2s->dam_ch_refcount)
			i2s->dam_ifc = tegra30_dam_allocate_controller();
		if (i2s->dam_ifc < 0)
			return i2s->dam_ifc;
		tegra30_dam_allocate_channel(i2s->dam_ifc, TEGRA30_DAM_CHIN1);
		i2s->dam_ch_refcount++;
		tegra30_dam_enable_clock(i2s->dam_ifc);

		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX1 +
				(i2s->dam_ifc*2), i2s->txcif);

		/*
		*make the dam tx to i2s rx connection if this is the only client
		*using i2s for playback
		*/
		if (i2s->playback_ref_count == 1)
			tegra30_ahub_set_rx_cif_source(
				TEGRA30_AHUB_RXCIF_I2S0_RX0 + i2s->id,
				TEGRA30_AHUB_TXCIF_DAM0_TX0 + i2s->dam_ifc);

		/* enable the dam*/
		tegra30_dam_enable(i2s->dam_ifc, TEGRA30_DAM_ENABLE,
				TEGRA30_DAM_CHIN1);
	} else {
		i2s->is_call_mode_rec = machine->is_call_mode;

		if (!i2s->is_call_mode_rec)
			return 0;

		if (machine->is_device_bt)
			codec_index = BT_SCO;
		else
			codec_index = VOICE_CODEC;

		codec_info = &machine->codec_info[codec_index];
		bb_info = &machine->codec_info[BASEBAND];
		hifi_info = &machine->codec_info[HIFI_CODEC];

		/* allocate a dam for voice call recording */

		i2s->call_record_dam_ifc = tegra30_dam_allocate_controller();

		if (i2s->call_record_dam_ifc < 0)
			return i2s->call_record_dam_ifc;

		tegra30_dam_allocate_channel(i2s->call_record_dam_ifc,
			TEGRA30_DAM_CHIN0_SRC);
		tegra30_dam_allocate_channel(i2s->call_record_dam_ifc,
			TEGRA30_DAM_CHIN1);
		tegra30_dam_enable_clock(i2s->call_record_dam_ifc);

		/* configure the dam */
		tegra_cs42l73_set_dam_cif(i2s->call_record_dam_ifc,
			codec_info->rate, codec_info->channels,
			codec_info->bitsize, 1, bb_info->rate,
			bb_info->channels, bb_info->bitsize);

		/* setup the connections for voice call record */
		tegra30_ahub_unset_rx_cif_source(i2s->rxcif);
		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
			(i2s->call_record_dam_ifc*2),
			TEGRA30_AHUB_TXCIF_I2S0_TX0 + bb_info->i2s_id);
		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX1 +
			(i2s->call_record_dam_ifc*2),
			TEGRA30_AHUB_TXCIF_I2S0_TX0 + codec_info->i2s_id);
		tegra30_ahub_set_rx_cif_source(i2s->rxcif,
			TEGRA30_AHUB_TXCIF_DAM0_TX0 + i2s->call_record_dam_ifc);

		/* enable the dam*/

		tegra30_dam_enable(i2s->call_record_dam_ifc, TEGRA30_DAM_ENABLE,
				TEGRA30_DAM_CHIN1);
		tegra30_dam_enable(i2s->call_record_dam_ifc, TEGRA30_DAM_ENABLE,
				TEGRA30_DAM_CHIN0_SRC);
	}

	return 0;
}

static void tegra_cs42l73_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);

	if (!i2s->is_dam_used)
		return;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* disable the dam*/
		tegra30_dam_enable(i2s->dam_ifc, TEGRA30_DAM_DISABLE,
				TEGRA30_DAM_CHIN1);

		/* disconnect the ahub connections*/
		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX1 +
					(i2s->dam_ifc*2));

		/* disable the dam and free the controller */
		tegra30_dam_disable_clock(i2s->dam_ifc);
		tegra30_dam_free_channel(i2s->dam_ifc, TEGRA30_DAM_CHIN1);
		i2s->dam_ch_refcount--;
		if (!i2s->dam_ch_refcount)
			tegra30_dam_free_controller(i2s->dam_ifc);
	 } else {
		if (!i2s->is_call_mode_rec)
			return;

		i2s->is_call_mode_rec = 0;

		/* disable the dam*/
		tegra30_dam_enable(i2s->call_record_dam_ifc,
			TEGRA30_DAM_DISABLE, TEGRA30_DAM_CHIN1);
		tegra30_dam_enable(i2s->call_record_dam_ifc,
			TEGRA30_DAM_DISABLE, TEGRA30_DAM_CHIN0_SRC);

		/* disconnect the ahub connections*/
		tegra30_ahub_unset_rx_cif_source(i2s->rxcif);
		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
			(i2s->call_record_dam_ifc*2));
		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX1 +
			(i2s->call_record_dam_ifc*2));

		/* free the dam channels and dam controller */
		tegra30_dam_disable_clock(i2s->call_record_dam_ifc);
		tegra30_dam_free_channel(i2s->call_record_dam_ifc,
			TEGRA30_DAM_CHIN1);
		tegra30_dam_free_channel(i2s->call_record_dam_ifc,
			TEGRA30_DAM_CHIN0_SRC);
		tegra30_dam_free_controller(i2s->call_record_dam_ifc);
	 }

	return;
}
#endif

static int tegra_voice_call_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int srate, mclk, i2s_daifmt;
	int err, rate;

	srate = params_rate(params);
	switch (srate) {
	case 8000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		mclk = 12288000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		mclk = 11289600;
		break;
	default:
		return -EINVAL;
		break;
	}

	i2s_daifmt = SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= pdata->i2s_param[VOICE_CODEC].is_i2s_master ?
			SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;

	switch (pdata->i2s_param[VOICE_CODEC].i2s_mode) {
	case TEGRA_DAIFMT_I2S:
		i2s_daifmt |= SND_SOC_DAIFMT_I2S;
		break;
	case TEGRA_DAIFMT_DSP_A:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
		break;
	case TEGRA_DAIFMT_DSP_B:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_B;
		break;
	case TEGRA_DAIFMT_LEFT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
		break;
	case TEGRA_DAIFMT_RIGHT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
		break;
	default:
		dev_err(card->dev, "Can't configure i2s format\n");
		return -EINVAL;
	}

	a2220_port_path_change(pdata->i2s_param[VOICE_CODEC].is_i2s_master ?
						   A100_msg_PortB_A_PASS :
						   A100_msg_PortA_B_PASS);

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		if (!(machine->util_data.set_mclk % mclk))
			mclk = machine->util_data.set_mclk;
		else {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

	rate = clk_get_rate(machine->util_data.clk_cdev1);

	err = snd_soc_dai_set_fmt(codec_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "codec_dai fmt not set\n");
		return err;
	}

	err = snd_soc_dai_set_fmt(cpu_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai fmt not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(codec_dai, 0, rate, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return err;
	}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	/* codec configuration */
	machine->codec_info[VOICE_CODEC].rate = params_rate(params);
	machine->codec_info[VOICE_CODEC].channels = params_channels(params);
#endif

	machine->is_device_bt = 0;

	return 0;
}

static void tegra_voice_call_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_cs42l73 *machine  =
			snd_soc_card_get_drvdata(rtd->codec->card);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	machine->codec_info[VOICE_CODEC].rate = 0;
	machine->codec_info[VOICE_CODEC].channels = 0;
#endif
	machine->is_device_bt = 0;
	return;
}

static int tegra_bt_voice_call_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	int err, srate, mclk, min_mclk;

	srate = params_rate(params);
	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		mclk = 11289600;
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		mclk = 12288000;
		break;
	default:
		return -EINVAL;
	}
	min_mclk = 64 * srate;

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		if (!(machine->util_data.set_mclk % min_mclk))
			mclk = machine->util_data.set_mclk;
		else {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	/* codec configuration */
	machine->codec_info[BT_SCO].rate = params_rate(params);
	machine->codec_info[BT_SCO].channels = params_channels(params);
#endif

	machine->is_device_bt = 1;

	return 0;
}

static void tegra_bt_voice_call_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_cs42l73 *machine  =
			snd_soc_card_get_drvdata(rtd->codec->card);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	machine->codec_info[BT_SCO].rate = 0;
	machine->codec_info[BT_SCO].channels = 0;
#endif

	machine->is_device_bt = 0;
}

static struct snd_soc_ops tegra_cs42l73_ops = {
	.hw_params = tegra_cs42l73_hw_params,
	.hw_free = tegra_hw_free,
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	.startup = tegra_cs42l73_startup,
	.shutdown = tegra_cs42l73_shutdown,
#endif
};

static struct snd_soc_ops tegra_spdif_ops = {
	.hw_params = tegra_spdif_hw_params,
	.hw_free = tegra_hw_free,
};

static struct snd_soc_ops tegra_voice_call_ops = {
	.hw_params = tegra_voice_call_hw_params,
	.shutdown = tegra_voice_call_shutdown,
	.hw_free = tegra_hw_free,
};

static struct snd_soc_ops tegra_bt_voice_call_ops = {
	.hw_params = tegra_bt_voice_call_hw_params,
	.shutdown = tegra_bt_voice_call_shutdown,
	.hw_free = tegra_hw_free,
};

static struct snd_soc_ops tegra_bt_ops = {
	.hw_params = tegra_bt_hw_params,
	.hw_free = tegra_hw_free,
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	.startup = tegra_cs42l73_startup,
	.shutdown = tegra_cs42l73_shutdown,
#endif
};


/* Headset jack */
struct snd_soc_jack tegra_cs42l73_hp_jack;


#ifdef CONFIG_SWITCH
/* These values are copied from Android WiredAccessoryObserver */
enum headset_state {
	BIT_NO_HEADSET = 0,
	BIT_HEADSET = (1 << 0),
	BIT_HEADSET_NO_MIC = (1 << 1),
};

static struct switch_dev tegra_cs42l73_headset_switch = {
	.name = "h2w",
};

/* Headset jack detection gpios */
static struct snd_soc_jack_gpio tegra_cs42l73_hp_jack_gpio = {
	.name = "headphone detect",
	.report = SND_JACK_HEADPHONE,
	.debounce_time = 150,
	.invert = 1,
};

static int tegra_cs42l73_jack_notifier(struct notifier_block *self,
			      unsigned long action, void *dev)
{
	struct snd_soc_jack *jack = dev;
	struct snd_soc_codec *codec = jack->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	enum headset_state state = BIT_NO_HEADSET;

	if (jack == &tegra_cs42l73_hp_jack) {
		machine->jack_status &= ~SND_JACK_HEADPHONE;
		machine->jack_status |= (action & SND_JACK_HEADPHONE);
	} else {
		machine->jack_status &= ~SND_JACK_MICROPHONE;
		machine->jack_status |= (action & SND_JACK_MICROPHONE);
	}

	switch (machine->jack_status) {
	case SND_JACK_HEADPHONE:
		/*For now force headset mic mode*/
		/*state = BIT_HEADSET_NO_MIC; */
		snd_soc_update_bits(codec, CS42L73_PWRCTL2, PDN_MIC2_BIAS, 0);
		state = BIT_HEADSET;
		break;
	case SND_JACK_HEADSET:
		snd_soc_update_bits(codec, CS42L73_PWRCTL2, PDN_MIC2_BIAS, 0);
		state = BIT_HEADSET;
		break;
	case SND_JACK_MICROPHONE:
		/* mic: would not report */
	default:
		state = BIT_NO_HEADSET;
	}

	switch_set_state(&tegra_cs42l73_headset_switch, state);

	return NOTIFY_OK;
}

static struct notifier_block tegra_cs42l73_jack_detect_nb = {
	.notifier_call = tegra_cs42l73_jack_notifier,
};
#else
/* Headset jack detection DAPM pins */
static struct snd_soc_jack_pin tegra_cs42l73_hs_jack_pins[] = {
	{
		.pin = "Headset Mic",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Headphone",
		.mask = SND_JACK_HEADPHONE,
	},
};
#endif

static int tegra_cs42l73_event_int_mic(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;

	if (machine->dmic_reg && machine->dmic_1v8_reg) {
		if (SND_SOC_DAPM_EVENT_ON(event)) {
			regulator_enable(machine->dmic_reg);
			regulator_enable(machine->dmic_1v8_reg);
		} else {
			regulator_disable(machine->dmic_reg);
			regulator_disable(machine->dmic_1v8_reg);
		}
	}
	if (!(machine->gpio_requested & GPIO_INT_MIC_EN))
		return 0;

	gpio_set_value_cansleep(pdata->gpio_int_mic_en,
				!!SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}

static void tegra_speaker_throttle(unsigned int new_state,  void *priv_data)
{
	struct tegra_cs42l73 *machine = priv_data;
	struct snd_soc_card *card;
	struct snd_soc_codec *codec;

	if (!machine)
		return;

	card = machine->pcard;
	codec = card->rtd[DAI_LINK_HIFI].codec;

	/* set codec voulme to 0 dB, E0 state */
	snd_soc_write(codec, CS42L73_SPKDVOL, 0x0);
	snd_soc_write(codec, CS42L73_ESLDVOL, 0x0);

}

static int tegra_cs42l73_event_int_spk(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_codec *codec = card->rtd[DAI_LINK_HIFI].codec;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	unsigned int approved;
	int ret;

	if (machine->spkr_reg) {
		if (SND_SOC_DAPM_EVENT_ON(event))
			regulator_enable(machine->spkr_reg);
		else
			regulator_disable(machine->spkr_reg);
	}

	if (machine->spk_edp_client == NULL)
		goto err_null_spk_edp_client;

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		ret = edp_update_client_request(
				machine->spk_edp_client,
				TEGRA_SPK_EDP_NEG_1, &approved);
		if (ret || approved != TEGRA_SPK_EDP_NEG_1) {
			/* set codec voulme to 0 dB, E0 state */
			snd_soc_write(codec, CS42L73_SPKDVOL, 0x0);
			snd_soc_write(codec, CS42L73_ESLDVOL, 0x0);
		} else {
			/* set codec voulme to +6 dB, E-1 state */
			snd_soc_write(codec, CS42L73_SPKDVOL, 0x0c);
			snd_soc_write(codec, CS42L73_ESLDVOL, 0x0c);
		}
	} else {
		ret = edp_update_client_request(
					machine->spk_edp_client,
					TEGRA_SPK_EDP_1, NULL);
		if (ret) {
			dev_err(card->dev,
				"E+1 state transition failed\n");
		}
	}
err_null_spk_edp_client:
	if (!(machine->gpio_requested & GPIO_SPKR_EN))
		return 0;

	gpio_set_value_cansleep(pdata->gpio_spkr_en,
				!!SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}

static int tegra_cs42l73_event_ext_mic(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;

	if (machine->hmic_reg) {
		if (SND_SOC_DAPM_EVENT_ON(event))
			regulator_enable(machine->hmic_reg);
		else
			regulator_disable(machine->hmic_reg);
	}

	if (!(machine->gpio_requested & GPIO_EXT_MIC_EN))
		return 0;

	gpio_set_value_cansleep(pdata->gpio_ext_mic_en,
				!!SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}

/* CS42L73 widgets */
static const struct snd_soc_dapm_widget tegra_cs42l73_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Int Spk", tegra_cs42l73_event_int_spk),
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_HP("Earpiece", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", tegra_cs42l73_event_ext_mic),
	SND_SOC_DAPM_MIC("Int D-Mic", tegra_cs42l73_event_int_mic),
};

/* cs42l73 Audio Map */
static const struct snd_soc_dapm_route tegra_cs42l73_audio_map[] = {
	{"Int Spk", NULL, "SPKOUT"},
	{"Int Spk", NULL, "SPKLINEOUT"},
	{"Int Spk", NULL, "EAROUT"},
	{"Earpiece", NULL, "EAROUT"},
	{"MIC2", NULL, "Headset Mic"},
	{"ADC Left", NULL, "Headset Mic"},
	{"ADC Right", NULL, "Headset Mic"},
	/* Headphone (L+R)->  HPOUTA, HPOUTB */
	{"Headphone", NULL, "HPOUTA"},
	{"Headphone", NULL, "HPOUTB"},
	/* DMIC -> DMIC Left/Right */
	{"DMIC Left", NULL, "Int D-Mic"},
	{"DMIC Right", NULL, "Int D-Mic"},
};

static const struct snd_kcontrol_new tegra_cs42l73_controls[] = {
	SOC_DAPM_PIN_SWITCH("Int Spk"),
	SOC_DAPM_PIN_SWITCH("Int D-Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Earpiece"),
};

static int tegra_cs42l73_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = codec->card;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(rtd->cpu_dai);
#endif
	int ret;

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	if (machine->codec_info[BASEBAND].i2s_id != -1)
			i2s->is_dam_used = true;
#endif

	if ((i2s->id == machine->codec_info[HIFI_CODEC].i2s_id) &&
		(i2s->id != machine->codec_info[VOICE_CODEC].i2s_id))
		i2s->is_dam_used = false;

	if (machine->init_done)
		return 0;

	machine->init_done = true;

	if (gpio_is_valid(pdata->gpio_hp_det)) {
		/* Headphone detection */
		tegra_cs42l73_hp_jack_gpio.gpio = pdata->gpio_hp_det;
		snd_soc_jack_new(codec, "Headphone Jack",
				SND_JACK_HEADSET, &tegra_cs42l73_hp_jack);

#ifndef CONFIG_SWITCH
		snd_soc_jack_add_pins(&tegra_cs42l73_hp_jack,
				ARRAY_SIZE(tegra_cs42l73_hs_jack_pins),
				tegra_cs42l73_hs_jack_pins);
#else
		snd_soc_jack_notifier_register(&tegra_cs42l73_hp_jack,
					&tegra_cs42l73_jack_detect_nb);
#endif
		snd_soc_jack_add_gpios(&tegra_cs42l73_hp_jack,
					1,
					&tegra_cs42l73_hp_jack_gpio);
		/* FIX ME  Add Mic Jack notifier */
		machine->gpio_requested |= GPIO_HP_DET;
	}

	if (gpio_is_valid(pdata->gpio_spkr_en)) {
		ret = gpio_request(pdata->gpio_spkr_en, "spkr_en");
		if (ret) {
			dev_err(card->dev, "cannot get spkr_en gpio\n");
			return ret;
		}
		machine->gpio_requested |= GPIO_SPKR_EN;

		gpio_direction_output(pdata->gpio_spkr_en, 0);
	}

	/* Add call mode switch control */
	ret = snd_ctl_add(codec->card->snd_card,
		snd_ctl_new1(&tegra_call_mode_control, machine));

	ret = tegra_asoc_utils_register_ctls(&machine->util_data);
	if (ret < 0)
		return ret;

	snd_soc_dapm_sync(dapm);

	return 0;
}

static struct snd_soc_dai_link tegra_cs42l73_dai[NUM_DAI_LINKS] = {
	[DAI_LINK_HIFI] = {
			.name = "CS42L73",
			.stream_name = "ASP Playback Record",
			.codec_name = "cs42l73.0-004a",
			.platform_name = "tegra-pcm-audio",
		    .cpu_dai_name = "tegra30-i2s.1",
			.codec_dai_name = "cs42l73-asp",
			.init = tegra_cs42l73_init,
			.ops = &tegra_cs42l73_ops,
		},
	[DAI_LINK_SPDIF] = {
			.name = "SPDIF",
			.stream_name = "SPDIF PCM",
			.codec_name = "spdif-dit.0",
			.platform_name = "tegra-pcm-audio",
			.cpu_dai_name = "tegra30-spdif",
			.codec_dai_name = "dit-hifi",
			.ops = &tegra_spdif_ops,
		},
	[DAI_LINK_BTSCO] = {
			.name = "BT SCO",
			.stream_name = "BT SCO PCM",
			.codec_name = "spdif-dit.1",
			.platform_name = "tegra-pcm-audio",
			.codec_dai_name = "dit-hifi",
			.init = tegra_cs42l73_init,
			.ops = &tegra_bt_ops,
		},
	[DAI_LINK_VOICE_CALL] = {
			.name = "VOICE CALL",
			.stream_name = "VOICE CALL PCM",
			.codec_name = "cs42l73.0-004a",
			.platform_name = "tegra-pcm-audio",
			.cpu_dai_name = "dit-hifi",
			.codec_dai_name = "cs42l73-vsp",
			.ops = &tegra_voice_call_ops,
		},
	[DAI_LINK_BT_VOICE_CALL] = {
			.name = "BT VOICE CALL",
			.stream_name = "BT VOICE CALL PCM",
			.codec_name = "spdif-dit.2",
			.platform_name = "tegra-pcm-audio",
			.cpu_dai_name = "dit-hifi",
			.codec_dai_name = "dit-hifi",
			.ops = &tegra_bt_voice_call_ops,
		},
};

static int tegra_cs42l73_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_jack_gpio *gpio = &tegra_cs42l73_hp_jack_gpio;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);

	if (gpio_is_valid(gpio->gpio))
		disable_irq(gpio_to_irq(gpio->gpio));

	if (machine->clock_enabled && !machine->is_call_mode)
		tegra_asoc_utils_clk_disable(&machine->util_data);

	return 0;
}

static int tegra_cs42l73_resume_pre(struct snd_soc_card *card)
{
	int val;
	struct snd_soc_jack_gpio *gpio = &tegra_cs42l73_hp_jack_gpio;
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);

	if (machine->clock_enabled && !machine->is_call_mode)
		tegra_asoc_utils_clk_enable(&machine->util_data);

	if (gpio_is_valid(gpio->gpio)) {
		val = gpio_get_value(gpio->gpio);
		val = gpio->invert ? !val : val;
		snd_soc_jack_report(gpio->jack, val, gpio->report);
		enable_irq(gpio_to_irq(gpio->gpio));
	}

	return 0;
}

static int tegra_cs42l73_set_bias_level(struct snd_soc_card *card,
	struct snd_soc_dapm_context *dapm, enum snd_soc_bias_level level)
{
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);

	if (machine->bias_level == SND_SOC_BIAS_OFF &&
		level != SND_SOC_BIAS_OFF && (!machine->clock_enabled)) {
		machine->clock_enabled = 1;
		tegra_asoc_utils_clk_enable(&machine->util_data);
		machine->bias_level = level;
	}

	return 0;
}

static int tegra_cs42l73_set_bias_level_post(struct snd_soc_card *card,
	struct snd_soc_dapm_context *dapm, enum snd_soc_bias_level level)
{
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);

	if (machine->bias_level != SND_SOC_BIAS_OFF &&
		level == SND_SOC_BIAS_OFF && machine->clock_enabled) {
		machine->clock_enabled = 0;
		tegra_asoc_utils_clk_disable(&machine->util_data);
		machine->bias_level = level;
	}


	return 0 ;
}

static struct snd_soc_card snd_soc_tegra_cs42l73 = {
	.name = "tegra-cs42l73",
	.owner = THIS_MODULE,
	.dai_link = tegra_cs42l73_dai,
	.num_links = ARRAY_SIZE(tegra_cs42l73_dai),
	.suspend_post = tegra_cs42l73_suspend_post,
	.resume_pre = tegra_cs42l73_resume_pre,
	.set_bias_level = tegra_cs42l73_set_bias_level,
	.set_bias_level_post = tegra_cs42l73_set_bias_level_post,
	.controls = tegra_cs42l73_controls,
	.num_controls = ARRAY_SIZE(tegra_cs42l73_controls),
	.dapm_widgets = tegra_cs42l73_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_cs42l73_dapm_widgets),
	.dapm_routes = tegra_cs42l73_audio_map,
	.num_dapm_routes = ARRAY_SIZE(tegra_cs42l73_audio_map),
	.fully_routed = true,
};

static __devinit int tegra_cs42l73_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_cs42l73;
	struct snd_soc_codec *codec;
	struct tegra_cs42l73 *machine;
	struct tegra_asoc_platform_data *pdata;
	struct edp_manager *battery_manager = NULL;
	int ret;
	int i;
	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -EINVAL;
	}

	if (pdata->codec_name)
		card->dai_link->codec_name = pdata->codec_name;

	if (pdata->codec_dai_name)
		card->dai_link->codec_dai_name = pdata->codec_dai_name;

	machine = kzalloc(sizeof(struct tegra_cs42l73), GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_cs42l73 struct\n");
		return -ENOMEM;
	}

	if (gpio_is_valid(pdata->gpio_ldo1_en)) {
		ret = gpio_request(pdata->gpio_ldo1_en, "cs42l73");
		if (ret)
			dev_err(&pdev->dev, "Fail gpio_request AUDIO_LDO1\n");

		ret = gpio_direction_output(pdata->gpio_ldo1_en, 0);
		if (ret)
			dev_err(&pdev->dev, "Fail gpio_direction AUDIO_LDO1\n");

		msleep(200);
	}

	machine->pdata = pdata;
	machine->pcard = card;
	machine->bias_level = SND_SOC_BIAS_STANDBY;
	machine->clock_enabled = 1;

	ret = tegra_asoc_utils_init(&machine->util_data, &pdev->dev, card);
	if (ret)
		goto err_free_machine;

	machine->dmic_reg = regulator_get(&pdev->dev, "vdd_mic");
	if (IS_ERR(machine->dmic_reg)) {
		dev_info(&pdev->dev, "No digital mic regulator found\n");
		machine->dmic_reg = 0;
	}

	machine->dmic_1v8_reg = regulator_get(&pdev->dev, "vdd_1v8_mic");
	if (IS_ERR(machine->dmic_1v8_reg)) {
		dev_info(&pdev->dev, "No digital mic regulator found\n");
		machine->dmic_1v8_reg = 0;
	}

	machine->hmic_reg = regulator_get(&pdev->dev, "mic_ventral");
	if (IS_ERR(machine->hmic_reg)) {
		dev_info(&pdev->dev, "No headset mic regulator found\n");
		machine->hmic_reg = 0;
	}

	machine->spkr_reg = regulator_get(&pdev->dev, "vdd_sys_audio");
	if (IS_ERR(machine->spkr_reg)) {
		dev_info(&pdev->dev, "No speaker regulator found\n");
		machine->spkr_reg = 0;
	}

#ifdef CONFIG_SWITCH
	/* Addd h2w swith class support */
	ret = tegra_asoc_switch_register(&tegra_cs42l73_headset_switch);
	if (ret < 0)
		goto err_fini_utils;
#endif

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	for (i = 0; i < NUM_I2S_DEVICES ; i++) {
		machine->codec_info[i].i2s_id =
			pdata->i2s_param[i].audio_port_id;
		machine->codec_info[i].bitsize =
			pdata->i2s_param[i].sample_size;
		machine->codec_info[i].is_i2smaster =
			pdata->i2s_param[i].is_i2s_master;
		machine->codec_info[i].rate =
			pdata->i2s_param[i].rate;
		machine->codec_info[i].channels =
			pdata->i2s_param[i].channels;
		machine->codec_info[i].i2s_mode =
			pdata->i2s_param[i].i2s_mode;
		machine->codec_info[i].bit_clk =
			pdata->i2s_param[i].bit_clk;
	}

	tegra_cs42l73_dai[DAI_LINK_HIFI].cpu_dai_name =
	tegra_cs42l73_i2s_dai_name[machine->codec_info[HIFI_CODEC].i2s_id];

	tegra_cs42l73_dai[DAI_LINK_BTSCO].cpu_dai_name =
	tegra_cs42l73_i2s_dai_name[machine->codec_info[BT_SCO].i2s_id];

	tegra_cs42l73_dai[DAI_LINK_VOICE_CALL].cpu_dai_name =
	tegra_cs42l73_i2s_dai_name[machine->codec_info[VOICE_CODEC].i2s_id];
#endif
	card->dapm.idle_bias_off = 1;
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_unregister_switch;
	}

	if (!card->instantiated) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "sound card not instantiated (%d)\n",
			ret);
		goto err_unregister_card;
	}

	ret = tegra_asoc_utils_set_parent(&machine->util_data,
				pdata->i2s_param[HIFI_CODEC].is_i2s_master);
	if (ret) {
		dev_err(&pdev->dev, "tegra_asoc_utils_set_parent failed (%d)\n",
			ret);
		goto err_unregister_card;
	}

	if (!pdata->edp_support)
		return 0;

	machine->spk_edp_client = devm_kzalloc(&pdev->dev,
					sizeof(struct edp_client),
					GFP_KERNEL);
	if (IS_ERR_OR_NULL(machine->spk_edp_client)) {
		dev_err(&pdev->dev, "could not allocate edp client\n");
		return 0;
	}
	machine->spk_edp_client->name[EDP_NAME_LEN - 1] = '\0';
	strncpy(machine->spk_edp_client->name, "speaker", EDP_NAME_LEN - 1);
	machine->spk_edp_client->states = pdata->edp_states;
	machine->spk_edp_client->num_states = TEGRA_SPK_EDP_NUM_STATES;
	machine->spk_edp_client->e0_index = TEGRA_SPK_EDP_ZERO;
	machine->spk_edp_client->priority = EDP_MAX_PRIO + 2;
	machine->spk_edp_client->throttle = tegra_speaker_throttle;
	machine->spk_edp_client->private_data = machine;

	battery_manager = edp_get_manager("battery");
	if (!battery_manager) {
		devm_kfree(&pdev->dev, machine->spk_edp_client);
		machine->spk_edp_client = NULL;
		dev_err(&pdev->dev, "unable to get edp manager\n");
	} else {
		/* register speaker edp client */
		ret = edp_register_client(battery_manager,
					machine->spk_edp_client);
		if (ret) {
			dev_err(&pdev->dev, "unable to register edp client\n");
			devm_kfree(&pdev->dev, machine->spk_edp_client);
			machine->spk_edp_client = NULL;
			return 0;
		}
		codec = card->rtd[DAI_LINK_HIFI].codec;
		/* set codec volume to 0 dB , E0 state*/
		snd_soc_write(codec, CS42L73_SPKDVOL, 0x0);
		snd_soc_write(codec, CS42L73_ESLDVOL, 0x0);
		/* request E1 */
		ret = edp_update_client_request(machine->spk_edp_client,
				TEGRA_SPK_EDP_1, NULL);
		if (ret) {
			dev_err(&pdev->dev,
					"unable to set E1 EDP state\n");
			edp_unregister_client(machine->spk_edp_client);
			devm_kfree(&pdev->dev, machine->spk_edp_client);
			machine->spk_edp_client = NULL;
		}
	}

	return 0;

err_unregister_card:
	snd_soc_unregister_card(card);
err_unregister_switch:
#ifdef CONFIG_SWITCH
	tegra_asoc_switch_unregister(&tegra_cs42l73_headset_switch);
err_fini_utils:
#endif
	tegra_asoc_utils_fini(&machine->util_data);
err_free_machine:
	kfree(machine);
	return ret;

}

static int __devexit tegra_cs42l73_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_cs42l73 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;

#ifdef CONFIG_SWITCH
	tegra_asoc_switch_unregister(&tegra_cs42l73_headset_switch);
#endif

	if (machine->gpio_requested & GPIO_HP_DET)
		snd_soc_jack_free_gpios(&tegra_cs42l73_hp_jack,
					1,
					&tegra_cs42l73_hp_jack_gpio);

	machine->gpio_requested = 0;

	if (machine->dmic_reg)
		regulator_put(machine->dmic_reg);
	if (machine->dmic_1v8_reg)
		regulator_put(machine->dmic_1v8_reg);
	if (machine->hmic_reg)
		regulator_put(machine->hmic_reg);
	if (machine->spkr_reg)
		regulator_put(machine->spkr_reg);

	if (gpio_is_valid(pdata->gpio_ldo1_en)) {
		gpio_set_value(pdata->gpio_ldo1_en, 0);
		gpio_free(pdata->gpio_ldo1_en);
	}

	snd_soc_unregister_card(card);

	tegra_asoc_utils_fini(&machine->util_data);

	kfree(machine);

	return 0;
}

static struct platform_driver tegra_cs42l73_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = tegra_cs42l73_driver_probe,
	.remove = __devexit_p(tegra_cs42l73_driver_remove),
};

static int __init tegra_cs42l73_modinit(void)
{
	return platform_driver_register(&tegra_cs42l73_driver);
}
module_init(tegra_cs42l73_modinit);

static void __exit tegra_cs42l73_modexit(void)
{
	platform_driver_unregister(&tegra_cs42l73_driver);
}
module_exit(tegra_cs42l73_modexit);

MODULE_AUTHOR("Vijay Mali <vmali@nvidia.com>");
MODULE_DESCRIPTION("Tegra+CS42L73 machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
