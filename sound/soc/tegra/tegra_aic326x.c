/*
 * tegra_aic326x.c - Tegra machine ASoC driver for boards using TI 3262 codec.
 *
 * Author: Vinod G. <vinodg@nvidia.com>
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/edp.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#include <mach/tegra_asoc_pdata.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/mfd/tlv320aic3262-registers.h>
#include <linux/mfd/tlv320aic3xxx-core.h>
#include "../codecs/tlv320aic326x.h"

#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#include "tegra20_das.h"
#else
#include "tegra30_ahub.h"
#include "tegra30_i2s.h"
#include "tegra30_dam.h"
#endif


#define DRV_NAME "tegra-snd-aic326x"

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

extern int g_is_call_mode;

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
const char *tegra_i2s_dai_name[TEGRA30_NR_I2S_IFC] = {
	"tegra30-i2s.0",
	"tegra30-i2s.1",
	"tegra30-i2s.2",
	"tegra30-i2s.3",
	"tegra30-i2s.4",
};
#endif

struct tegra_aic326x {
	struct tegra_asoc_utils_data util_data;
	struct tegra_asoc_platform_data *pdata;
	struct regulator *audio_reg;
	int gpio_requested;
	bool init_done;
	int is_call_mode;
	int is_device_bt;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	struct codec_config codec_info[NUM_I2S_DEVICES];
	struct snd_soc_card *pcard;
	struct regulator *dmic_reg;
	struct regulator *dmic_1v8_reg;
	struct regulator *hmic_reg;
	enum snd_soc_bias_level bias_level;
	struct edp_client *spk_edp_client;
#endif
	int clock_enabled;
};

static int tegra_aic326x_call_mode_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int tegra_aic326x_call_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_aic326x *machine = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = machine->is_call_mode;

	return 0;
}

static int tegra_aic326x_call_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_aic326x *machine = snd_kcontrol_chip(kcontrol);
	int is_call_mode_new = ucontrol->value.integer.value[0];
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	int codec_dap_id, codec_dap_sel, bb_dap_id, bb_dap_sel;
#else /*assumes tegra3*/
	int codec_index;
	unsigned int i;
	int uses_voice_codec;
#endif

	if (machine->is_call_mode == is_call_mode_new)
		return 0;

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	bb_dap_id = TEGRA20_DAS_DAP_ID_3;
	bb_dap_sel = TEGRA20_DAS_DAP_SEL_DAP3;

	if (machine->is_device_bt) {
		codec_dap_id = TEGRA20_DAS_DAP_ID_4;
		codec_dap_sel = TEGRA20_DAS_DAP_SEL_DAP4;
	}
	else {
		codec_dap_id = TEGRA20_DAS_DAP_ID_2;
		codec_dap_sel = TEGRA20_DAS_DAP_SEL_DAP2;
	}
#else /*assumes tegra3*/
	if (machine->is_device_bt) {
		codec_index = BT_SCO;
		uses_voice_codec = 0;
	} else {
		codec_index = VOICE_CODEC;
		uses_voice_codec = 1;
	}
#endif

	if (is_call_mode_new) {
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
		tegra20_das_set_tristate(codec_dap_id, 1);
		tegra20_das_set_tristate(bb_dap_id, 1);
		tegra20_das_connect_dap_to_dap(codec_dap_id,
			bb_dap_sel, 0, 0, 0);
		tegra20_das_connect_dap_to_dap(bb_dap_id,
			codec_dap_sel, 1, 0, 0);
		tegra20_das_set_tristate(codec_dap_id, 0);
		tegra20_das_set_tristate(bb_dap_id, 0);
#else /*assumes tegra3*/
		if (machine->codec_info[codec_index].rate == 0 ||
			machine->codec_info[codec_index].channels == 0)
				return -EINVAL;

		for (i = 0; i < machine->pcard->num_links; i++)
			machine->pcard->dai_link[i].ignore_suspend = 1;

		tegra30_make_voice_call_connections(
			&machine->codec_info[codec_index],
			&machine->codec_info[BASEBAND], uses_voice_codec);
#endif
	} else {
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
		tegra20_das_set_tristate(codec_dap_id, 1);
		tegra20_das_set_tristate(bb_dap_id, 1);
		tegra20_das_connect_dap_to_dap(bb_dap_id,
			bb_dap_sel, 0, 0, 0);
		tegra20_das_connect_dap_to_dap(codec_dap_id,
			codec_dap_sel, 0, 0, 0);
		tegra20_das_set_tristate(codec_dap_id, 0);
		tegra20_das_set_tristate(bb_dap_id, 0);
#else /*assumes tegra3*/
		tegra30_break_voice_call_connections(
			&machine->codec_info[codec_index],
			&machine->codec_info[BASEBAND], uses_voice_codec);

		for (i = 0; i < machine->pcard->num_links; i++)
			machine->pcard->dai_link[i].ignore_suspend = 0;
#endif
	}

	machine->is_call_mode = is_call_mode_new;
	g_is_call_mode = machine->is_call_mode;

	return 1;
}

struct snd_kcontrol_new tegra_aic326x_call_mode_control = {
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Call Mode Switch",
	.private_value = 0xffff,
	.info = tegra_aic326x_call_mode_info,
	.get = tegra_aic326x_call_mode_get,
	.put = tegra_aic326x_call_mode_put
};

static int tegra_aic326x_get_mclk(int srate)
{
	int mclk = 0;
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
		mclk = -EINVAL;
		break;
	}

	return mclk;
}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static int tegra_aic326x_set_dam_cif(int dam_ifc, int srate,
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

	if (src_on) {
		tegra30_dam_set_gain(dam_ifc, TEGRA30_DAM_CHIN0_SRC, 0x1000);
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

static int tegra_aic326x_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int srate, mclk, sample_size, i2s_daifmt;
	int err, rate;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_size = 16;
		break;
	default:
		return -EINVAL;
	}

	srate = params_rate(params);

	mclk = tegra_aic326x_get_mclk(srate);
	if (mclk < 0)
		return mclk;

	i2s_daifmt = SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= pdata->i2s_param[HIFI_CODEC].is_i2s_master ?
			SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;

	switch (pdata->i2s_param[HIFI_CODEC].i2s_mode) {
		case TEGRA_DAIFMT_I2S :
			i2s_daifmt |= SND_SOC_DAIFMT_I2S;
			break;
		case TEGRA_DAIFMT_DSP_A :
			i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
			break;
		case TEGRA_DAIFMT_DSP_B :
			i2s_daifmt |= SND_SOC_DAIFMT_DSP_B;
			break;
		case TEGRA_DAIFMT_LEFT_J :
			i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
			break;
		case TEGRA_DAIFMT_RIGHT_J :
			i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
			break;
		default :
			dev_err(card->dev, "Can't configure i2s format\n");
			return -EINVAL;
	}

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

	err = snd_soc_dai_set_pll(codec_dai, 0, AIC3262_PLL_CLKIN_MCLK1 , rate,
		params_rate(params));

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	err = tegra20_das_connect_dac_to_dap(TEGRA20_DAS_DAP_SEL_DAC1,
					TEGRA20_DAS_DAP_ID_1);
	if (err < 0) {
		dev_err(card->dev, "failed to set dap-dac path\n");
		return err;
	}

	err = tegra20_das_connect_dap_to_dac(TEGRA20_DAS_DAP_ID_1,
					TEGRA20_DAS_DAP_SEL_DAC1);
	if (err < 0) {
		dev_err(card->dev, "failed to set dac-dap path\n");
		return err;
	}
#endif
	return 0;
}

static int tegra_aic326x_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);
	int srate, mclk, min_mclk;
	int err;

	srate = params_rate(params);

	mclk = tegra_aic326x_get_mclk(srate);
	if (mclk < 0)
		return mclk;

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

static int tegra_aic326x_bt_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int err, srate, mclk, min_mclk, sample_size, i2s_daifmt;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(rtd->cpu_dai);
#endif

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_size = 16;
		break;
	default:
		return -EINVAL;
	}

	srate = params_rate(params);

	mclk = tegra_aic326x_get_mclk(srate);
	if (mclk < 0)
		return mclk;

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
		case TEGRA_DAIFMT_I2S :
			i2s_daifmt |= SND_SOC_DAIFMT_I2S;
			break;
		case TEGRA_DAIFMT_DSP_A :
			i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
			break;
		case TEGRA_DAIFMT_DSP_B :
			i2s_daifmt |= SND_SOC_DAIFMT_DSP_B;
			break;
		case TEGRA_DAIFMT_LEFT_J :
			i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
			break;
		case TEGRA_DAIFMT_RIGHT_J :
			i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
			break;
		default :
			dev_err(card->dev, "Can't configure i2s format\n");
			return -EINVAL;
	}

	err = snd_soc_dai_set_fmt(rtd->cpu_dai, i2s_daifmt);

	if (err < 0) {
		dev_err(rtd->codec->card->dev, "cpu_dai fmt not set\n");
		return err;
	}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	err = tegra20_das_connect_dac_to_dap(TEGRA20_DAS_DAP_SEL_DAC2,
					TEGRA20_DAS_DAP_ID_4);
	if (err < 0) {
		dev_err(card->dev, "failed to set dac-dap path\n");
		return err;
	}

	err = tegra20_das_connect_dap_to_dac(TEGRA20_DAS_DAP_ID_4,
					TEGRA20_DAS_DAP_SEL_DAC2);
	if (err < 0) {
		dev_err(card->dev, "failed to set dac-dap path\n");
		return err;
	}
#else
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && i2s->is_dam_used)
		tegra_aic326x_set_dam_cif(i2s->dam_ifc, params_rate(params),
			params_channels(params), sample_size, 0, 0, 0, 0);
#endif

	return 0;
}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static int tegra_aic326x_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(rtd->card);
	struct codec_config *codec_info;
	struct codec_config *bb_info;
	struct codec_config *hifi_info;

	if (!i2s->is_dam_used)
		return 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (i2s->id == machine->codec_info[BT_SCO].i2s_id) {
			/*dam configuration*/
			if (!i2s->dam_ch_refcount)
				i2s->dam_ifc =
					tegra30_dam_allocate_controller();
			if (i2s->dam_ifc < 0)
				return i2s->dam_ifc;
			tegra30_dam_allocate_channel(i2s->dam_ifc,
				TEGRA30_DAM_CHIN1);
			i2s->dam_ch_refcount++;
			tegra30_dam_enable_clock(i2s->dam_ifc);

			tegra30_ahub_set_rx_cif_source(
			  TEGRA30_AHUB_RXCIF_DAM0_RX1 + (i2s->dam_ifc*2),
			  i2s->txcif);

			/* make the dam tx to i2s rx connection if this is
			 * the only client using i2s for playback */
			if (i2s->playback_ref_count == 1)
				tegra30_ahub_set_rx_cif_source(
				  TEGRA30_AHUB_RXCIF_I2S0_RX0 + i2s->id,
				  TEGRA30_AHUB_TXCIF_DAM0_TX0 + i2s->dam_ifc);

			/* enable the dam*/
			tegra30_dam_enable(i2s->dam_ifc, TEGRA30_DAM_ENABLE,
				TEGRA30_DAM_CHIN1);
		} else {
			/* make apbif tx to i2s rx connection if this is
			 * the only client using i2s for playback */
			if (i2s->playback_ref_count == 1) {
				tegra30_ahub_set_rx_cif_source(
					TEGRA30_AHUB_RXCIF_I2S0_RX0 + i2s->id,
					i2s->txcif);
			tegra30_ahub_enable_clocks();
			}
		}
	} else {
		i2s->is_call_mode_rec = machine->is_call_mode;
		if (!i2s->is_call_mode_rec)
			return 0;

		bb_info = &machine->codec_info[BASEBAND];
		hifi_info = &machine->codec_info[HIFI_CODEC];

		/* allocate a dams for voice call recording */

		i2s->call_record_dam_ifc = tegra30_dam_allocate_controller();

		if (i2s->call_record_dam_ifc < 0)
			return i2s->call_record_dam_ifc;

		tegra30_dam_allocate_channel(i2s->call_record_dam_ifc,
			TEGRA30_DAM_CHIN0_SRC);
		tegra30_dam_allocate_channel(i2s->call_record_dam_ifc,
			TEGRA30_DAM_CHIN1);
		tegra30_dam_enable_clock(i2s->call_record_dam_ifc);

		if (machine->is_device_bt) {
			codec_info = &machine->codec_info[BT_SCO];

			/* configure the dam */
			/* SRC bb rate to bt rate */
			tegra_aic326x_set_dam_cif(i2s->call_record_dam_ifc,
				bb_info->rate, bb_info->channels,
				bb_info->bitsize, 1, codec_info->rate,
				codec_info->channels, codec_info->bitsize);

			/* set ahub connections for bt call record*/
			tegra30_ahub_unset_rx_cif_source(i2s->rxcif);

			tegra30_ahub_set_rx_cif_source(
				TEGRA30_AHUB_RXCIF_DAM0_RX0 +
				(i2s->call_record_dam_ifc*2),
				TEGRA30_AHUB_TXCIF_I2S0_TX0 + bb_info->i2s_id);

			tegra30_ahub_set_rx_cif_source(
			  TEGRA30_AHUB_RXCIF_DAM0_RX1 +
			  (i2s->call_record_dam_ifc*2),
			  TEGRA30_AHUB_TXCIF_I2S0_TX0 + codec_info->i2s_id);

			tegra30_ahub_set_rx_cif_source(i2s->rxcif,
			TEGRA30_AHUB_TXCIF_DAM0_TX0 + i2s->call_record_dam_ifc);

#ifndef CONFIG_ARCH_TEGRA_3x_SOC
		/* Configure DAM for SRC */
		if (bb_info->rate != codec_info->rate) {
			tegra30_dam_write_coeff_ram(i2s->call_record_dam_ifc,
					bb_info->rate, codec_info->rate);
			tegra30_dam_set_farrow_param(i2s->call_record_dam_ifc,
					bb_info->rate, codec_info->rate);
			tegra30_dam_set_biquad_fixed_coef(
						i2s->call_record_dam_ifc);
			tegra30_dam_enable_coeff_ram(i2s->call_record_dam_ifc);
			tegra30_dam_set_filter_stages(i2s->call_record_dam_ifc,
							bb_info->rate,
							codec_info->rate);
		}
#endif
		} else {
			codec_info = &machine->codec_info[VOICE_CODEC];

			i2s->call_record_dam_ifc2 =
				tegra30_dam_allocate_controller();

			if (i2s->call_record_dam_ifc2 < 0)
				return i2s->call_record_dam_ifc2;

			tegra30_dam_allocate_channel(i2s->call_record_dam_ifc2,
				TEGRA30_DAM_CHIN0_SRC);
			tegra30_dam_allocate_channel(i2s->call_record_dam_ifc2,
				TEGRA30_DAM_CHIN1);
			tegra30_dam_enable_clock(i2s->call_record_dam_ifc2);

			/* configure the dams */
			/* DAM0 SRC bb rate to hifi rate */
			tegra_aic326x_set_dam_cif(i2s->call_record_dam_ifc,
				codec_info->rate, codec_info->channels,
				codec_info->bitsize, 1, hifi_info->rate,
				hifi_info->channels, hifi_info->bitsize);
			/* DAM1 UL + DL Mix */
			tegra_aic326x_set_dam_cif(i2s->call_record_dam_ifc2,
				codec_info->rate, codec_info->channels,
				codec_info->bitsize, 1, bb_info->rate,
				bb_info->channels, bb_info->bitsize);

			/* setup the connections for voice call record */
			tegra30_ahub_unset_rx_cif_source(i2s->rxcif);
			tegra30_ahub_set_rx_cif_source(
				TEGRA30_AHUB_RXCIF_DAM0_RX0 +
				(i2s->call_record_dam_ifc2*2),
				TEGRA30_AHUB_TXCIF_I2S0_TX0 + bb_info->i2s_id);
			tegra30_ahub_set_rx_cif_source(
			  TEGRA30_AHUB_RXCIF_DAM0_RX1 +
			  (i2s->call_record_dam_ifc2*2),
			  TEGRA30_AHUB_TXCIF_I2S0_TX0 + codec_info->i2s_id);
			tegra30_ahub_set_rx_cif_source(
				TEGRA30_AHUB_RXCIF_DAM0_RX0 +
				(i2s->call_record_dam_ifc*2),
				TEGRA30_AHUB_TXCIF_DAM0_TX0 +
				i2s->call_record_dam_ifc2);
			tegra30_ahub_set_rx_cif_source(i2s->rxcif,
				TEGRA30_AHUB_TXCIF_DAM0_TX0 +
				i2s->call_record_dam_ifc);
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
			/* Configure DAM0 for SRC */
			if (bb_info->rate != hifi_info->rate) {
				tegra30_dam_write_coeff_ram(
					i2s->call_record_dam_ifc,
					bb_info->rate, hifi_info->rate);
				tegra30_dam_set_farrow_param(
					i2s->call_record_dam_ifc,
					bb_info->rate, hifi_info->rate);
				tegra30_dam_set_biquad_fixed_coef(
					i2s->call_record_dam_ifc);
				tegra30_dam_enable_coeff_ram(
					i2s->call_record_dam_ifc);
				tegra30_dam_set_filter_stages(
					i2s->call_record_dam_ifc,
					bb_info->rate, hifi_info->rate);
			}
#endif
		/* enable the dam */
			tegra30_dam_enable(i2s->call_record_dam_ifc2,
					TEGRA30_DAM_ENABLE,
					TEGRA30_DAM_CHIN1);
			tegra30_dam_enable(i2s->call_record_dam_ifc2,
					TEGRA30_DAM_ENABLE,
					TEGRA30_DAM_CHIN0_SRC);
		}

		/* enable the dam */
		tegra30_dam_enable(i2s->call_record_dam_ifc, TEGRA30_DAM_ENABLE,
				TEGRA30_DAM_CHIN1);
		tegra30_dam_enable(i2s->call_record_dam_ifc, TEGRA30_DAM_ENABLE,
				TEGRA30_DAM_CHIN0_SRC);
	}

	return 0;
}

static void tegra_aic326x_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(rtd->card);

	if (!i2s->is_dam_used)
		return;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (i2s->id == machine->codec_info[BT_SCO].i2s_id) {
			/* disable the dam*/
			tegra30_dam_enable(i2s->dam_ifc, TEGRA30_DAM_DISABLE,
				TEGRA30_DAM_CHIN1);

			/* disconnect the ahub connections*/
			tegra30_ahub_unset_rx_cif_source(
			  TEGRA30_AHUB_RXCIF_DAM0_RX1 + (i2s->dam_ifc*2));

			/* disable the dam and free the controller */
			tegra30_dam_disable_clock(i2s->dam_ifc);
			tegra30_dam_free_channel(i2s->dam_ifc,
				TEGRA30_DAM_CHIN1);
			i2s->dam_ch_refcount--;
			if (!i2s->dam_ch_refcount)
				tegra30_dam_free_controller(i2s->dam_ifc);

		} else {
			tegra30_ahub_unset_rx_cif_source(
					TEGRA30_AHUB_RXCIF_I2S0_RX0 + i2s->id);
			tegra30_ahub_disable_clocks();
		}
	 } else {
		if (!i2s->is_call_mode_rec)
			return;

		i2s->is_call_mode_rec = 0;

		/* disable the dams*/
		tegra30_dam_enable(i2s->call_record_dam_ifc,
			TEGRA30_DAM_DISABLE, TEGRA30_DAM_CHIN1);
		tegra30_dam_enable(i2s->call_record_dam_ifc,
			TEGRA30_DAM_DISABLE, TEGRA30_DAM_CHIN0_SRC);

		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
						(i2s->call_record_dam_ifc*2));
		tegra30_ahub_unset_rx_cif_source(i2s->rxcif);

		/* free the dam channels and dam controllers */
		tegra30_dam_disable_clock(i2s->call_record_dam_ifc);
		tegra30_dam_free_channel(i2s->call_record_dam_ifc,
					TEGRA30_DAM_CHIN1);
		tegra30_dam_free_channel(i2s->call_record_dam_ifc,
					TEGRA30_DAM_CHIN0_SRC);
		tegra30_dam_free_controller(i2s->call_record_dam_ifc);

		if (!machine->is_device_bt) {
			tegra30_dam_enable(i2s->call_record_dam_ifc2,
				TEGRA30_DAM_DISABLE, TEGRA30_DAM_CHIN1);
			tegra30_dam_enable(i2s->call_record_dam_ifc2,
				TEGRA30_DAM_DISABLE, TEGRA30_DAM_CHIN0_SRC);

			/* disconnect the ahub connections*/
			tegra30_ahub_unset_rx_cif_source(
				TEGRA30_AHUB_RXCIF_DAM0_RX0 +
				(i2s->call_record_dam_ifc2*2));
			tegra30_ahub_unset_rx_cif_source(
				TEGRA30_AHUB_RXCIF_DAM0_RX1 +
				(i2s->call_record_dam_ifc2*2));

			tegra30_dam_disable_clock(i2s->call_record_dam_ifc2);
			tegra30_dam_free_channel(i2s->call_record_dam_ifc2,
					TEGRA30_DAM_CHIN1);
			tegra30_dam_free_channel(i2s->call_record_dam_ifc2,
					TEGRA30_DAM_CHIN0_SRC);
			tegra30_dam_free_controller(i2s->call_record_dam_ifc2);
		}
	 }

	return;
}
#endif


static int tegra_aic326x_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 0);

	return 0;
}

static int tegra_aic326x_voice_call_hw_params(
			struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int srate, mclk, rate, i2s_daifmt;
	int err, pcmdiv, vxclkdiv;

	srate = params_rate(params);
	mclk = tegra_aic326x_get_mclk(srate);
	if (mclk < 0)
		return mclk;

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
		dev_err(card->dev,
		"Can't configure i2s format\n");
		return -EINVAL;
	}

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


	err = snd_soc_dai_set_pll(codec_dai, 0, AIC3262_PLL_CLKIN_MCLK1 , rate,
			params_rate(params));

	if (err < 0) {
		dev_err(card->dev, "codec_dai PLL clock not set\n");
		return err;
	}

	if (!machine_is_tegra_enterprise()) {
		if (params_rate(params) == 8000) {
			/* Change these Settings for 8KHz*/
			pcmdiv = 1;
			/* BB expecting 2048Khz bclk */
			vxclkdiv = 27;
		} else if (params_rate(params) == 16000) {
			pcmdiv = 1;
			/* BB expecting 2048Khz bclk */
			vxclkdiv = 27;
		} else {
			dev_err(card->dev, "codec_dai unsupported voice rate\n");
			return -EINVAL;
		}
	}

	//snd_soc_dai_set_clkdiv(codec_dai, ASI2_BCLK_N, vxclkdiv);
	//snd_soc_dai_set_clkdiv(codec_dai, ASI2_WCLK_N, pcmdiv);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	/* codec configuration */
	machine->codec_info[VOICE_CODEC].rate = params_rate(params);
	machine->codec_info[VOICE_CODEC].channels = params_channels(params);
#endif

	machine->is_device_bt = 0;
	return 0;
}

static void tegra_aic326x_voice_call_shutdown(
					struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_aic326x *machine  =
			snd_soc_card_get_drvdata(rtd->codec->card);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	machine->codec_info[VOICE_CODEC].rate = 0;
	machine->codec_info[VOICE_CODEC].channels = 0;
#endif
}

static int tegra_aic326x_bt_voice_call_hw_params(
			struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);
	int err, srate, mclk, min_mclk;

	srate = params_rate(params);

	mclk = tegra_aic326x_get_mclk(srate);
	if (mclk < 0)
		return mclk;

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

static void tegra_aic326x_bt_voice_call_shutdown(
				struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_aic326x *machine  =
			snd_soc_card_get_drvdata(rtd->codec->card);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	machine->codec_info[BT_SCO].rate = 0;
	machine->codec_info[BT_SCO].channels = 0;
#endif
}

static struct snd_soc_ops tegra_aic326x_hifi_ops = {
	.hw_params = tegra_aic326x_hw_params,
	.hw_free = tegra_aic326x_hw_free,
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	.startup = tegra_aic326x_startup,
	.shutdown = tegra_aic326x_shutdown,
#endif
};

static struct snd_soc_ops tegra_aic326x_spdif_ops = {
	.hw_params = tegra_aic326x_spdif_hw_params,
	.hw_free = tegra_aic326x_hw_free,
};

static struct snd_soc_ops tegra_aic326x_voice_call_ops = {
	.hw_params = tegra_aic326x_voice_call_hw_params,
	.shutdown = tegra_aic326x_voice_call_shutdown,
	.hw_free = tegra_aic326x_hw_free,
};

static struct snd_soc_ops tegra_aic326x_bt_voice_call_ops = {
	.hw_params = tegra_aic326x_bt_voice_call_hw_params,
	.shutdown = tegra_aic326x_bt_voice_call_shutdown,
	.hw_free = tegra_aic326x_hw_free,
};

static struct snd_soc_ops tegra_aic326x_bt_ops = {
	.hw_params = tegra_aic326x_bt_hw_params,
	.hw_free = tegra_aic326x_hw_free,
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	.startup = tegra_aic326x_startup,
	.shutdown = tegra_aic326x_shutdown,
#endif
};

static struct snd_soc_jack tegra_aic326x_hp_jack;

#ifdef CONFIG_SWITCH
static struct switch_dev aic326x_wired_switch_dev = {
	.name = "h2w",
};

/* Headset jack detection gpios */
static struct snd_soc_jack_gpio tegra_aic326x_hp_jack_gpio = {
	.name = "headphone detect",
	.report = SND_JACK_HEADPHONE,
	.debounce_time = 150,
	.invert = 1,
};

/* These values are copied from WiredAccessoryObserver */
enum headset_state {
	BIT_NO_HEADSET = 0,
	BIT_HEADSET = (1 << 0),
	BIT_HEADSET_NO_MIC = (1 << 1),
};

static int aic326x_headset_switch_notify(struct notifier_block *self,
	unsigned long action, void *dev)
{
	int state = BIT_NO_HEADSET;

	switch (action) {
	case SND_JACK_HEADPHONE:
	/*
	 * FIX ME: For now force headset mic mode
	 * Known HW issue Mic detection is not working
	 */
		state |= BIT_HEADSET;
		break;
	case SND_JACK_HEADSET:
		state |= BIT_HEADSET;
		break;
	default:
		state |= BIT_NO_HEADSET;
	}

	switch_set_state(&aic326x_wired_switch_dev, state);

	return NOTIFY_OK;
}

static struct notifier_block aic326x_headset_switch_nb = {
	.notifier_call = aic326x_headset_switch_notify,
};
#else
static struct snd_soc_jack_pin tegra_aic326x_hp_jack_pins[] = {
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};
#endif

static void tegra_speaker_throttle(unsigned int new_state,  void *priv_data)
{
	struct tegra_aic326x *machine = priv_data;
	struct snd_soc_card *card;
	struct snd_soc_codec *codec;

	if (!machine)
		return;

	card = machine->pcard;
	codec = card->rtd[DAI_LINK_HIFI].codec;

	/* set speaker amplifier voulme to 6dB, E0 state */
	snd_soc_write(codec, AIC3262_SPK_AMP_CNTL_R4, 0x11);

}

static int tegra_aic326x_event_int_spk(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_codec *codec = card->rtd[DAI_LINK_HIFI].codec;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	unsigned int approved;
	int ret;

	if (machine->spk_edp_client == NULL)
		goto err_null_spk_edp_client;

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		ret = edp_update_client_request(
				machine->spk_edp_client,
				TEGRA_SPK_EDP_NEG_1, &approved);
		if (ret || approved != TEGRA_SPK_EDP_NEG_1) {
			/*  set speaker amplifier voulme to 6 dB, E0 state */
			snd_soc_write(codec, AIC3262_SPK_AMP_CNTL_R4, 0x11);
		} else {
			/*  set speaker amplifier voulme to 18 dB, E-1 state */
			snd_soc_write(codec, AIC3262_SPK_AMP_CNTL_R4, 0x33);
		}
		if (machine->audio_reg)
			regulator_enable(machine->audio_reg);
	} else {
		ret = edp_update_client_request(
					machine->spk_edp_client,
					TEGRA_SPK_EDP_1, NULL);
		if (ret) {
			dev_err(card->dev,
				"E+1 state transition failed\n");
		}
		if (machine->audio_reg)
			regulator_disable(machine->audio_reg);
	}
err_null_spk_edp_client:
	if (!(machine->gpio_requested & GPIO_SPKR_EN))
		return 0;

	gpio_set_value_cansleep(pdata->gpio_spkr_en,
				!!SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}

static int tegra_aic326x_event_hp(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;

	if (machine->audio_reg) {
		if (SND_SOC_DAPM_EVENT_ON(event))
			regulator_enable(machine->audio_reg);
		else
			regulator_disable(machine->audio_reg);
	}

	if (!(machine->gpio_requested & GPIO_HP_MUTE))
		return 0;

	gpio_set_value_cansleep(pdata->gpio_hp_mute,
				!SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}

static int tegra_aic326x_event_dmic(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);
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

static const struct snd_soc_dapm_widget tegra_aic326x_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Int Spk", tegra_aic326x_event_int_spk),
	SND_SOC_DAPM_HP("Earpiece", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", tegra_aic326x_event_hp),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_INPUT("Ext Mic"),
	SND_SOC_DAPM_LINE("Linein", NULL),
	SND_SOC_DAPM_MIC("Int Mic", tegra_aic326x_event_dmic),
	SND_SOC_DAPM_MIC("DMIC", NULL),
};

static const struct snd_soc_dapm_route aic326x_audio_map[] = {
	{"Int Spk", NULL, "SPK Left Playback"},
	{"Int Spk", NULL, "SPK Right Playback"},
	{"Earpiece", NULL, "RECP Playback"},
	{"Earpiece", NULL, "RECM Playback"},
	{"Headphone Jack", NULL, "HP Left Playback"},
	{"Headphone Jack", NULL, "HP Right Playback"},
	/* internal (IN2L/IN2R) mic is stero */
	{"Mic Bias Int" ,NULL, "Int Mic"},
	{"IN2 Left Capture", NULL, "Mic Bias Int"},
	{"Mic Bias Int" ,NULL, "Int Mic"},
	{"IN2 Right Capture", NULL, "Mic Bias Int"},
	{"IN1 Left Capture", NULL, "Mic Bias Ext"},
	{"Mic Bias Ext" , NULL, "Mic Jack"},
	/* Connect LDMIC and RDMIC to DMIC widget*/
	{"Left DMIC Capture", NULL, "Mic Bias Int"},
	{"Right DMIC Capture", NULL, "Mic Bias Int"},
	{"Mic Bias Int", NULL, "Int Mic"},
};

static const struct snd_kcontrol_new tegra_aic326x_controls[] = {
	SOC_DAPM_PIN_SWITCH("Int Spk"),
	SOC_DAPM_PIN_SWITCH("Earpiece"),
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Mic Jack"),
	SOC_DAPM_PIN_SWITCH("Ext Mic"),
	SOC_DAPM_PIN_SWITCH("Linein"),
	SOC_DAPM_PIN_SWITCH("Int Mic"),
};

static int tegra_aic326x_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = codec->card;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(rtd->cpu_dai);
#endif
	int ret;

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	if (machine->codec_info[BASEBAND].i2s_id != -1)
		i2s->is_dam_used = true;
#endif

	if (machine->init_done)
		return 0;

	machine->init_done = true;

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	machine->pcard = card;
#endif

	if (machine_is_whistler()) {
		machine->audio_reg = regulator_get(NULL, "avddio_audio");
		if (IS_ERR(machine->audio_reg)) {
			dev_err(card->dev, "cannot get avddio_audio reg\n");
			ret = PTR_ERR(machine->audio_reg);
			return ret;
		}

		ret = regulator_enable(machine->audio_reg);
		if (ret) {
			dev_err(card->dev, "cannot enable avddio_audio reg\n");
			regulator_put(machine->audio_reg);
			machine->audio_reg = NULL;
			return ret;
		}
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

	if (gpio_is_valid(pdata->gpio_hp_mute)) {
		ret = gpio_request(pdata->gpio_hp_mute, "hp_mute");
		if (ret) {
			dev_err(card->dev, "cannot get hp_mute gpio\n");
			return ret;
		}
		machine->gpio_requested |= GPIO_HP_MUTE;

		gpio_direction_output(pdata->gpio_hp_mute, 0);
	}

	if (gpio_is_valid(pdata->gpio_int_mic_en)) {
		ret = gpio_request(pdata->gpio_int_mic_en, "int_mic_en");
		if (ret) {
			dev_err(card->dev, "cannot get int_mic_en gpio\n");
			return ret;
		}
		machine->gpio_requested |= GPIO_INT_MIC_EN;

		/* Disable int mic; enable signal is active-high */
		gpio_direction_output(pdata->gpio_int_mic_en, 0);
	}

	if (gpio_is_valid(pdata->gpio_ext_mic_en)) {
		ret = gpio_request(pdata->gpio_ext_mic_en, "ext_mic_en");
		if (ret) {
			dev_err(card->dev, "cannot get ext_mic_en gpio\n");
			return ret;
		}
		machine->gpio_requested |= GPIO_EXT_MIC_EN;

		/* Enable ext mic; enable signal is active-low */
		gpio_direction_output(pdata->gpio_ext_mic_en, 0);
	}

	ret = snd_soc_jack_new(codec, "Headset Jack", SND_JACK_HEADSET,
			&tegra_aic326x_hp_jack);
	if (ret < 0)
		return ret;

	if (gpio_is_valid(pdata->gpio_hp_det)) {
		/* Headphone detection */
		tegra_aic326x_hp_jack_gpio.gpio = pdata->gpio_hp_det;
		snd_soc_jack_new(codec, "Headphone Jack",
				SND_JACK_HEADSET, &tegra_aic326x_hp_jack);

#ifndef CONFIG_SWITCH
	snd_soc_jack_add_pins(&tegra_aic326x_hp_jack,
		ARRAY_SIZE(tegra_aic326x_hp_jack_pins),
		tegra_aic326x_hp_jack_pins);
#else
		snd_soc_jack_notifier_register(&tegra_aic326x_hp_jack,
					&aic326x_headset_switch_nb);
#endif
		snd_soc_jack_add_gpios(&tegra_aic326x_hp_jack,
					1,
					&tegra_aic326x_hp_jack_gpio);

		machine->gpio_requested |= GPIO_HP_DET;
	}

#ifndef CONFIG_ARCH_TEGRA_11x_SOC
	/* update jack status during boot */
	aic3262_hs_jack_detect(codec, &tegra_aic326x_hp_jack,
		SND_JACK_HEADSET);
#endif

	/* Add call mode switch control */
	ret = snd_ctl_add(codec->card->snd_card,
			snd_ctl_new1(&tegra_aic326x_call_mode_control,
				machine));
	if (ret < 0)
		return ret;

	ret = tegra_asoc_utils_register_ctls(&machine->util_data);
	if (ret < 0)
		return ret;

	snd_soc_dapm_sync(dapm);

	return 0;
}

static struct snd_soc_dai_link tegra_aic326x_dai[] = {
	[DAI_LINK_HIFI] = {
		.name = "AIC3262",
		.stream_name = "AIC3262 PCM HIFI",
		.codec_name = "tlv320aic3262-codec",
		.platform_name = "tegra-pcm-audio",
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
		.cpu_dai_name = "tegra20-i2s.0",
#else
		.cpu_dai_name = "tegra30-i2s.1",
#endif
		.codec_dai_name = "aic326x-asi1",
		.init = tegra_aic326x_init,
		.ops = &tegra_aic326x_hifi_ops,
		},
	[DAI_LINK_SPDIF] = {
		.name = "SPDIF",
		.stream_name = "SPDIF PCM",
		.codec_name = "spdif-dit.0",
		.platform_name = "tegra-pcm-audio",
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
		.cpu_dai_name = "tegra20-spdif",
#else
		.cpu_dai_name = "tegra30-spdif",
#endif
		.codec_dai_name = "dit-hifi",
		.ops = &tegra_aic326x_spdif_ops,
		},
	[DAI_LINK_BTSCO] = {
		.name = "BT-SCO",
		.stream_name = "BT SCO PCM",
		.codec_name = "spdif-dit.1",
		.platform_name = "tegra-pcm-audio",
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
		.cpu_dai_name = "tegra20-i2s.1",
#else
		.cpu_dai_name = "tegra30-i2s.3",
#endif
		.codec_dai_name = "dit-hifi",
		.init = tegra_aic326x_init,
		.ops = &tegra_aic326x_bt_ops,
		},
	[DAI_LINK_VOICE_CALL] = {
			.name = "VOICE CALL",
			.stream_name = "VOICE CALL PCM",
			.codec_name = "tlv320aic3262-codec",
			.platform_name = "tegra-pcm-audio",
			.cpu_dai_name = "dit-hifi",
			.codec_dai_name = "aic326x-asi3",
			.ops = &tegra_aic326x_voice_call_ops,
		},
	[DAI_LINK_BT_VOICE_CALL] = {
			.name = "BT VOICE CALL",
			.stream_name = "BT VOICE CALL PCM",
			.codec_name = "spdif-dit.2",
			.platform_name = "tegra-pcm-audio",
			.cpu_dai_name = "dit-hifi",
			.codec_dai_name = "dit-hifi",
			.ops = &tegra_aic326x_bt_voice_call_ops,
		},
};

static int tegra_aic326x_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_jack_gpio *gpio = &tegra_aic326x_hp_jack_gpio;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);

	if (gpio_is_valid(gpio->gpio))
		disable_irq(gpio_to_irq(gpio->gpio));

	if (machine->clock_enabled && !machine->is_call_mode) {
		machine->clock_enabled = 0;
		tegra_asoc_utils_clk_disable(&machine->util_data);
	}


	return 0;
}

static int tegra_aic326x_resume_pre(struct snd_soc_card *card)
{
	int val;
	struct snd_soc_jack_gpio *gpio = &tegra_aic326x_hp_jack_gpio;
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);

	if (gpio_is_valid(gpio->gpio)) {
		val = gpio_get_value(gpio->gpio);
		val = gpio->invert ? !val : val;
		snd_soc_jack_report(gpio->jack, val, gpio->report);
		enable_irq(gpio_to_irq(gpio->gpio));
	}

	if (!machine->clock_enabled && !machine->is_call_mode) {
		machine->clock_enabled = 1;
		tegra_asoc_utils_clk_enable(&machine->util_data);
	}

	return 0;
}


static int tegra_aic326x_set_bias_level(struct snd_soc_card *card,
	struct snd_soc_dapm_context *dapm, enum snd_soc_bias_level level)
{
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);

	if (machine->bias_level == SND_SOC_BIAS_OFF &&
		level != SND_SOC_BIAS_OFF && (!machine->clock_enabled)) {
		machine->clock_enabled = 1;
		tegra_asoc_utils_clk_enable(&machine->util_data);
		machine->bias_level = level;
	}

	return 0;
}

static int tegra_aic326x_set_bias_level_post(struct snd_soc_card *card,
	struct snd_soc_dapm_context *dapm, enum snd_soc_bias_level level)
{
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);

	if (machine->bias_level != SND_SOC_BIAS_OFF &&
		level == SND_SOC_BIAS_OFF && machine->clock_enabled) {
		machine->clock_enabled = 0;
		tegra_asoc_utils_clk_disable(&machine->util_data);
		machine->bias_level = level;
	}

	return 0 ;
}

static struct snd_soc_card snd_soc_tegra_aic326x = {
	.name = "tegra-aic326x",
	.owner = THIS_MODULE,
	.dai_link = tegra_aic326x_dai,
	.num_links = ARRAY_SIZE(tegra_aic326x_dai),
	.set_bias_level = tegra_aic326x_set_bias_level,
	.set_bias_level_post = tegra_aic326x_set_bias_level_post,
	.suspend_post = tegra_aic326x_suspend_post,
	.resume_pre = tegra_aic326x_resume_pre,
	.controls = tegra_aic326x_controls,
	.num_controls = ARRAY_SIZE(tegra_aic326x_controls),
	.dapm_widgets = tegra_aic326x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_aic326x_dapm_widgets),
	.dapm_routes = aic326x_audio_map,
	.num_dapm_routes = ARRAY_SIZE(aic326x_audio_map),
	.fully_routed = true,
};

static __devinit int tegra_aic326x_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_aic326x;
	struct snd_soc_codec *codec;
	struct tegra_aic326x *machine;
	struct tegra_asoc_platform_data *pdata;
	struct edp_manager *battery_manager = NULL;
	int ret;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	int i;
#endif
	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -EINVAL;
	}

	machine = kzalloc(sizeof(struct tegra_aic326x), GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_aic326x struct\n");
		return -ENOMEM;
	}

	machine->pdata = pdata;
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

	machine->audio_reg = regulator_get(NULL, "avdd_audio");
	if (IS_ERR(machine->audio_reg)) {
		dev_info(&pdev->dev, "No avdd_audio regulator found\n");
		machine->audio_reg = 0;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

#ifdef CONFIG_SWITCH
	/* Add h2w switch class support */
	ret = tegra_asoc_switch_register(&aic326x_wired_switch_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "not able to register switch device %d\n",
			ret);
		goto err_fini_utils;
	}
#endif

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

	tegra_aic326x_dai[DAI_LINK_HIFI].cpu_dai_name =
	tegra_i2s_dai_name[machine->codec_info[HIFI_CODEC].i2s_id];

	tegra_aic326x_dai[DAI_LINK_BTSCO].cpu_dai_name =
	tegra_i2s_dai_name[machine->codec_info[BT_SCO].i2s_id];
#endif

	if (machine_is_tegra_enterprise()) {
		tegra_aic326x_dai[DAI_LINK_HIFI].codec_name =
						"tlv320aic3262-codec";
		tegra_aic326x_dai[DAI_LINK_VOICE_CALL].codec_name =
						"tlv320aic3262-codec";
		tegra_aic326x_dai[DAI_LINK_VOICE_CALL].codec_dai_name =
						"aic326x-asi1";
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_switch_unregister;
	}

	if (!card->instantiated) {
		dev_err(&pdev->dev, "No TI AIC3262 codec\n");
		goto err_unregister_card;
	}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	ret = tegra_asoc_utils_set_parent(&machine->util_data,
				pdata->i2s_param[HIFI_CODEC].is_i2s_master);
	if (ret) {
		dev_err(&pdev->dev, "tegra_asoc_utils_set_parent failed (%d)\n",
			ret);
		goto err_unregister_card;
	}
#endif

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
		/*  set speaker amplifier volume to 6 dB , E0 state*/
		snd_soc_write(codec, AIC3262_SPK_AMP_CNTL_R4, 0x11);
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
err_switch_unregister:
#ifdef CONFIG_SWITCH
	tegra_asoc_switch_unregister(&aic326x_wired_switch_dev);
#endif
err_fini_utils:
	tegra_asoc_utils_fini(&machine->util_data);
err_free_machine:
	kfree(machine);
	return ret;
}

static int __devexit tegra_aic326x_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_aic326x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;

	if (machine->gpio_requested & GPIO_HP_DET)
		snd_soc_jack_free_gpios(&tegra_aic326x_hp_jack,
					1,
					&tegra_aic326x_hp_jack_gpio);

	snd_soc_unregister_card(card);

#ifdef CONFIG_SWITCH
	tegra_asoc_switch_unregister(&aic326x_wired_switch_dev);
#endif

	tegra_asoc_utils_fini(&machine->util_data);

	if (machine->gpio_requested & GPIO_EXT_MIC_EN)
		gpio_free(pdata->gpio_ext_mic_en);
	if (machine->gpio_requested & GPIO_INT_MIC_EN)
		gpio_free(pdata->gpio_int_mic_en);
	if (machine->gpio_requested & GPIO_HP_MUTE)
		gpio_free(pdata->gpio_hp_mute);
	if (machine->gpio_requested & GPIO_SPKR_EN)
		gpio_free(pdata->gpio_spkr_en);

	kfree(machine);

	return 0;
}

static struct platform_driver tegra_aic326x_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = tegra_aic326x_driver_probe,
	.remove = __devexit_p(tegra_aic326x_driver_remove),
};

static int __init tegra_aic326x_modinit(void)
{
	return platform_driver_register(&tegra_aic326x_driver);
}
module_init(tegra_aic326x_modinit);

static void __exit tegra_aic326x_modexit(void)
{
	platform_driver_unregister(&tegra_aic326x_driver);
}
module_exit(tegra_aic326x_modexit);

/* Module information */
MODULE_AUTHOR("Vinod G. <vinodg@nvidia.com>");
MODULE_DESCRIPTION("Tegra+AIC3262 machine ASoC driver");
MODULE_DESCRIPTION("Tegra ALSA SoC");
MODULE_LICENSE("GPL");

