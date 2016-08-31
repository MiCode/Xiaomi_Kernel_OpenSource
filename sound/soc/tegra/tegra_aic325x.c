/*
 * tegra_aic325x.c - Tegra ASoC machine driver for boards using TI 325x codec.
 *
 * Author: Ravindra Lokhande <rlokhande@nvidia.com>
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#include <mach/tegra_asoc_pdata.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <linux/mfd/tlv320aic3256-registers.h>
#include "../codecs/tlv320aic325x.h"

#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"

#include "tegra30_ahub.h"
#include "tegra30_i2s.h"
#include "tegra30_dam.h"

#define DRV_NAME "tegra-snd-aic325x"

#define GPIO_SPKR_EN    BIT(0)
#define GPIO_HP_MUTE    BIT(1)
#define GPIO_INT_MIC_EN BIT(2)
#define GPIO_EXT_MIC_EN BIT(3)
#define GPIO_HP_DET     BIT(4)

#define DAI_LINK_HIFI		0
#define DAI_LINK_BTSCO		1
#define DAI_LINK_VOICE_CALL	2
#define DAI_LINK_BT_VOICE_CALL	3

/*
 * ALSA SoC TPA2054D4A amplifier
 *
 */
/* Register Offsets */
#define TPA2054D4A_FAULT_REG				0x01
#define TPA2054D4A_POWER_MGMT_REG			0x02
#define TPA2054D4A_MUX_OUTPUT_CNTRL_REG			0x03
#define TPA2054D4A_MON_VOL_CTRL_REG			0x04
#define TPA2054D4A_ST1_VOL_CTRL_REG			0x05
#define TPA2054D4A_ST2_VOL_CTRL_REG			0x06
#define TPA2054D4A_HP_OUTPUT_CNTRL_REG			0x07

/* Register Bits */
#define TPA2054D4A_PA_EN				(0x01 << 1)
#define TPA2054D4A_HPR_EN				(0x01 << 2)
#define TPA2054D4A_HPL_EN				(0x01 << 3)
#define TPA2054D4A_SWS					(0x01 << 4)

#define TPA2054D4A_MODE(x)				(x << 0)

#define TPA2054D4A_MODE_MONO_INPUT			(0x00)
#define TPA2054D4A_MODE_STEREO_1_INPUT			(0x01)
#define TPA2054D4A_MODE_STEREO_2_INPUT			(0x02)
#define TPA2054D4A_MODE_STEREO_DIFF			(0x03)
#define TPA2054D4A_MODE_STEREO_DIFF_MONO		(0x04)
#define TPA2054D4A_MODE_STEREO_1_MONO			(0x05)
#define TPA2054D4A_MODE_STEREO_2_MONO			(0x06)
#define TPA2054D4A_MODE_MUTE				(0x07)

#define TPA2054D4A_MUX_OUTPUT_MODE(x)			((x & 0x07) << 0)

#define TPA2054D4A_VOLUME(x)				((x & 0x1f) << 0)

#define TPA2054D4A_HP_GAIN(x)				((x & 0x03) << 0)
#define TPA2054D4A_HP_VOUT(x)				((x & 0x07) << 2)

extern int g_is_call_mode;


const char *tegra_i2s_dai_name[TEGRA30_NR_I2S_IFC] = {
	"tegra30-i2s.0",
	"tegra30-i2s.1",
	"tegra30-i2s.2",
	"tegra30-i2s.3",
	"tegra30-i2s.4",
};

struct tegra_aic325x {
	struct tegra_asoc_utils_data util_data;
	struct tegra_asoc_platform_data *pdata;
	int gpio_requested;
	bool init_done;
	int is_call_mode;
	int is_device_bt;

	struct i2c_client *tpa2054d4a_client;
	struct i2c_adaptor *tpa2054d4a_adapter;

	struct codec_config codec_info[NUM_I2S_DEVICES];
	struct ahub_bbc1_config ahub_bbc1_info;
	struct snd_soc_card *pcard;
	struct regulator *vdd_aud_dgtl;
	struct regulator *avdd_aud;
	enum snd_soc_bias_level bias_level;
	int clock_enabled;
};

static struct i2c_board_info tpa2054d4a_hwinfo = {
	I2C_BOARD_INFO("tpa2054d4a", 0x70),
};

/* Function Prototypes */
static int tpa2054d4a_i2c_read_device(struct i2c_client *tpa2054d4a_client,
					unsigned int reg, u16 bytes,
					void *dest);
static int tpa2054d4a_i2c_write_device(struct i2c_client *tpa2054d4a_client,
					unsigned int reg, u16 bytes,
					const void *src);
static int tpa2054d4a_init(struct tegra_aic325x *machine);

/*
 * tpa2054d4a_i2c_read_device: read tpa2054 registers using i2c interface
 * @tpa2054d4a_client: Handle to slave device
 * @reg: register offset to be read
 * @dest: Where to store data read from slave
 * @bytes: How many bytes to read, must be less than 64k since msg.len is u16
 */
static int tpa2054d4a_i2c_read_device(struct i2c_client *tpa2054d4a_client,
					unsigned int reg, u16 bytes,
					void *dest)
{
	int ret;

	/* Send the required register offset to be read */
	ret = i2c_master_send(tpa2054d4a_client, (unsigned char *)&reg, 1);
	if (ret < 0)
		return ret;
	if (ret != 1)
		return -EIO;

	ret = i2c_master_recv(tpa2054d4a_client, (unsigned char *)dest, bytes);
	if (ret < 0)
		return ret;
	if (ret != bytes)
		return -EIO;

	return 0;
}

/*
 * tpa2054d4a_i2c_write_device: write to tpa2054 registers using i2c interface
 * @tpa2054d4a_client: Handle to slave device
 * @reg: register offset where data is to be written
 * @bytes: How many bytes to write, must be less than 64k since msg.len is u16
 * @src: Data that will be written to the slave
 */
static int tpa2054d4a_i2c_write_device(struct i2c_client *tpa2054d4a_client,
					unsigned int reg, u16 bytes,
					const void *src)
{
	int ret;
	u8 write_buf[bytes + 1];
	write_buf[0] = reg;
	memcpy(&write_buf[1], src, bytes);

	ret = i2c_master_send(tpa2054d4a_client, (unsigned char *)write_buf,
					(bytes + 1));
	if (ret < 0)
		return ret;
	if (ret != (bytes + 1))
		return -EIO;

	return 0;
}

struct i2c_client *tpa2054d4a_client;
/*
 * tpa2054d4a_init: TPA2054D4A i2c integration & register configurations
 * @machine: struct tegra_aic325x
 */
static int tpa2054d4a_init(struct tegra_aic325x *machine)
{
	int ret;
	int i2c_bus_num = 5;
	machine->tpa2054d4a_adapter = (struct i2c_adaptor *)
					i2c_get_adapter(i2c_bus_num);
	if (!machine->tpa2054d4a_adapter)
		return -ENODEV;

	machine->tpa2054d4a_client = i2c_new_device(
			((struct i2c_adapter *)(machine->tpa2054d4a_adapter)),
			&tpa2054d4a_hwinfo);
	if (!machine->tpa2054d4a_client) {
		i2c_put_adapter(
			(struct i2c_adapter *)(machine->tpa2054d4a_adapter));
		return -ENODEV;
	}

	/* tmp hack : to be removed in independant tpa driver */
	tpa2054d4a_client = machine->tpa2054d4a_client;
	/* Write Reg2 of TPA2054 */
	ret = 0x00;
	tpa2054d4a_i2c_write_device(machine->tpa2054d4a_client,
					TPA2054D4A_POWER_MGMT_REG, 1, &ret);
	ret = 0x01;
	tpa2054d4a_i2c_write_device(machine->tpa2054d4a_client,
					TPA2054D4A_MUX_OUTPUT_CNTRL_REG,
					1, &ret);
	ret = TPA2054D4A_HPR_EN | TPA2054D4A_HPL_EN;
	tpa2054d4a_i2c_write_device(tpa2054d4a_client,
		TPA2054D4A_POWER_MGMT_REG, 1, &ret);
	return 0;
}

static int get_dai_fmt(int i2s_mode)
{
	switch (i2s_mode) {
	case TEGRA_DAIFMT_I2S:
		return SND_SOC_DAIFMT_I2S;
	case TEGRA_DAIFMT_DSP_A:
		return SND_SOC_DAIFMT_DSP_A;
	case TEGRA_DAIFMT_DSP_B:
		return SND_SOC_DAIFMT_DSP_B;
	case TEGRA_DAIFMT_LEFT_J:
		return SND_SOC_DAIFMT_LEFT_J;
	case TEGRA_DAIFMT_RIGHT_J:
		return SND_SOC_DAIFMT_RIGHT_J;
	default:
		pr_err("Can't configure i2s format\n");
		return -EINVAL;
	}
}

static int tegra_aic325x_call_mode_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int tegra_aic325x_call_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_aic325x *machine = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = machine->is_call_mode;

	return 0;
}

static int tegra_aic325x_call_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_aic325x *machine = snd_kcontrol_chip(kcontrol);
	int is_call_mode_new = ucontrol->value.integer.value[0];
	int codec_index;
	unsigned int i;

	if (machine->is_call_mode == is_call_mode_new)
		return 0;

	if (machine->is_device_bt)
		codec_index = BT_SCO;
	else
		codec_index = HIFI_CODEC;

	if (is_call_mode_new) {
		if (machine->codec_info[codec_index].rate == 0 ||
			machine->codec_info[codec_index].channels == 0)
				return -EINVAL;

		for (i = 0; i < machine->pcard->num_links; i++)
			machine->pcard->dai_link[i].ignore_suspend = 1;

#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_asoc_utils_tristate_dap(
		machine->codec_info[codec_index].i2s_id, false);
	if (machine->is_device_bt) {
		t14x_make_bt_voice_call_connections(
			&machine->codec_info[codec_index],
			&machine->ahub_bbc1_info, 0);
	} else {
		t14x_make_voice_call_connections(
			&machine->codec_info[codec_index],
			&machine->ahub_bbc1_info, 0);
	}
#else
		tegra30_make_voice_call_connections(
			&machine->codec_info[codec_index],
			&machine->codec_info[BASEBAND], 0);
#endif
	} else {
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
		if (machine->is_device_bt) {
			t14x_break_bt_voice_call_connections(
				&machine->codec_info[codec_index],
				&machine->ahub_bbc1_info, 0);
		} else {
			t14x_break_voice_call_connections(
			&machine->codec_info[codec_index],
			&machine->ahub_bbc1_info, 0);
		}
		tegra_asoc_utils_tristate_dap(
			machine->codec_info[codec_index].i2s_id, true);
#else
		tegra30_break_voice_call_connections(
			&machine->codec_info[codec_index],
			&machine->codec_info[BASEBAND], 0);
#endif

		for (i = 0; i < machine->pcard->num_links; i++)
			machine->pcard->dai_link[i].ignore_suspend = 0;
	}

	machine->is_call_mode = is_call_mode_new;
	g_is_call_mode = machine->is_call_mode;

	return 1;
}

struct snd_kcontrol_new tegra_aic325x_call_mode_control = {
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Call Mode Switch",
	.private_value = 0xffff,
	.info = tegra_aic325x_call_mode_info,
	.get = tegra_aic325x_call_mode_get,
	.put = tegra_aic325x_call_mode_put
};

static int tegra_aic325x_get_mclk(int srate)
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

static int tegra_aic325x_set_dam_cif(int dam_ifc, int srate,
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

static int tegra_aic325x_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int srate, mclk, sample_size, i2s_daifmt;
	int err, rate;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_size = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		sample_size = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		sample_size = 32;
		break;
	default:
		return -EINVAL;
	}

	srate = params_rate(params);

	mclk = tegra_aic325x_get_mclk(srate);
	if (mclk < 0) {
		dev_err(card->dev, "Invalid mclk\n");
		return mclk;
	}

	i2s_daifmt = get_dai_fmt(pdata->i2s_param[HIFI_CODEC].i2s_mode);
	if (i2s_daifmt < 0) {
		dev_err(card->dev, "Invalid dai format\n");
		return i2s_daifmt;
	}
	i2s_daifmt |= SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= pdata->i2s_param[HIFI_CODEC].is_i2s_master ?
			SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;

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

	rate = clk_get_rate(machine->util_data.clk_cdev1);
	snd_soc_dai_set_pll(codec_dai, 0, AIC3256_CLK_REG_1 , rate,
		params_rate(params));

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && i2s->is_dam_used)
		tegra_aic325x_set_dam_cif(i2s->dam_ifc, srate,
			params_channels(params), sample_size, 0, 0, 0, 0);

	return 0;
}

static int tegra_aic325x_bt_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int err, srate, mclk, min_mclk, sample_size, i2s_daifmt;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(rtd->cpu_dai);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_size = 16;
		break;
	default:
		return -EINVAL;
	}

	srate = params_rate(params);

	mclk = tegra_aic325x_get_mclk(srate);
	if (mclk < 0) {
		dev_err(card->dev, "Invalid mclk\n");
		return mclk;
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

	i2s_daifmt = get_dai_fmt(pdata->i2s_param[BT_SCO].i2s_mode);
	if (i2s_daifmt < 0) {
		dev_err(card->dev, "Invalid dai format\n");
		return i2s_daifmt;
	}
	i2s_daifmt |= SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= pdata->i2s_param[BT_SCO].is_i2s_master ?
			SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;

	err = snd_soc_dai_set_fmt(rtd->cpu_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(rtd->codec->card->dev, "cpu_dai fmt not set\n");
		return err;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && i2s->is_dam_used)
		tegra_aic325x_set_dam_cif(i2s->dam_ifc, params_rate(params),
			params_channels(params), sample_size, 0, 0, 0, 0);

	return 0;
}

static int tegra_aic325x_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(rtd->card);
	struct codec_config *codec_info;
	struct codec_config *bb_info;
	int codec_index;

	tegra_asoc_utils_tristate_dap(i2s->id, false);

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
			codec_index = HIFI_CODEC;

		codec_info = &machine->codec_info[codec_index];
		bb_info = &machine->codec_info[BASEBAND];

		/* allocate a dam for voice call recording */
		i2s->call_record_dam_ifc = tegra30_dam_allocate_controller();

		if (i2s->call_record_dam_ifc < 0)
			return i2s->call_record_dam_ifc;

		tegra30_dam_allocate_channel(i2s->call_record_dam_ifc,
			TEGRA30_DAM_CHIN0_SRC);
		tegra30_dam_allocate_channel(i2s->call_record_dam_ifc,
			TEGRA30_DAM_CHIN1);
		tegra30_dam_enable_clock(i2s->call_record_dam_ifc);

		/* setup the connections for voice call record */
		tegra30_ahub_unset_rx_cif_source(i2s->rxcif);
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
		/* configure the dam */
		tegra_aic325x_set_dam_cif(i2s->call_record_dam_ifc,
			codec_info->rate, codec_info->channels,
			codec_info->bitsize, 1,
			machine->ahub_bbc1_info.rate,
			machine->ahub_bbc1_info.channels,
			machine->ahub_bbc1_info.sample_size);

		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
			(i2s->call_record_dam_ifc*2),
			TEGRA30_AHUB_TXCIF_BBC1_TX0);
#else
		/* configure the dam */
		tegra_aic325x_set_dam_cif(i2s->call_record_dam_ifc,
			codec_info->rate, codec_info->channels,
			codec_info->bitsize, 1, bb_info->rate,
			bb_info->channels, bb_info->bitsize);

		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
			(i2s->call_record_dam_ifc*2),
			TEGRA30_AHUB_TXCIF_I2S0_TX0 + bb_info->i2s_id);
#endif
		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX1 +
			(i2s->call_record_dam_ifc*2),
			TEGRA30_AHUB_TXCIF_I2S0_TX0 + codec_info->i2s_id);
		tegra30_ahub_set_rx_cif_source(i2s->rxcif,
			TEGRA30_AHUB_TXCIF_DAM0_TX0 +
			i2s->call_record_dam_ifc);
		/* enable the dam*/
		tegra30_dam_enable(i2s->call_record_dam_ifc, TEGRA30_DAM_ENABLE,
				TEGRA30_DAM_CHIN1);
		tegra30_dam_enable(i2s->call_record_dam_ifc, TEGRA30_DAM_ENABLE,
				TEGRA30_DAM_CHIN0_SRC);
	}

	return 0;
}

static void tegra_aic325x_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);

	tegra_asoc_utils_tristate_dap(i2s->id, true);

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

static int tegra_aic325x_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 0);

	return 0;
}

static int tegra_aic325x_voice_call_hw_params(
			struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int srate, mclk, rate, i2s_daifmt;
	int err;

	srate = params_rate(params);
	mclk = tegra_aic325x_get_mclk(srate);
	if (mclk < 0) {
		dev_err(card->dev, "Invalid mclk\n");
		return mclk;
	}

	i2s_daifmt = get_dai_fmt(pdata->i2s_param[HIFI_CODEC].i2s_mode);
	if (i2s_daifmt < 0) {
		dev_err(card->dev, "Invalid dai format\n");
		return i2s_daifmt;
	}
	i2s_daifmt |= SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= pdata->i2s_param[HIFI_CODEC].is_i2s_master ?
		SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;

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

	err = snd_soc_dai_set_fmt(codec_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "codec_dai fmt not set\n");
		return err;
	}

	rate = clk_get_rate(machine->util_data.clk_cdev1);
	err = snd_soc_dai_set_pll(codec_dai, 0, AIC3256_CLK_REG_1, rate,
			params_rate(params));

	/* codec configuration */
	machine->codec_info[HIFI_CODEC].rate = params_rate(params);
	machine->codec_info[HIFI_CODEC].channels = params_channels(params);

	machine->is_device_bt = 0;

	return 0;
}

static void tegra_aic325x_voice_call_shutdown(
					struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_aic325x *machine  =
			snd_soc_card_get_drvdata(rtd->codec->card);

	machine->codec_info[HIFI_CODEC].rate = 0;
	machine->codec_info[HIFI_CODEC].channels = 0;

	return;
}

static int tegra_aic325x_bt_voice_call_hw_params(
			struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	int err, srate, mclk, min_mclk;

	srate = params_rate(params);

	mclk = tegra_aic325x_get_mclk(srate);
	if (mclk < 0) {
		dev_err(card->dev, "Invalid mclk\n");
		return mclk;
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

	/* codec configuration */
	machine->codec_info[BT_SCO].rate = params_rate(params);
	machine->codec_info[BT_SCO].channels = params_channels(params);

	machine->is_device_bt = 1;

	return 0;
}

static void tegra_aic325x_bt_voice_call_shutdown(
				struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_aic325x *machine  =
			snd_soc_card_get_drvdata(rtd->codec->card);

	machine->codec_info[BT_SCO].rate = 0;
	machine->codec_info[BT_SCO].channels = 0;

	return;
}

static struct snd_soc_ops tegra_aic325x_hifi_ops = {
	.hw_params = tegra_aic325x_hw_params,
	.hw_free = tegra_aic325x_hw_free,
	.startup = tegra_aic325x_startup,
	.shutdown = tegra_aic325x_shutdown,
};

static struct snd_soc_ops tegra_aic325x_voice_call_ops = {
	.hw_params = tegra_aic325x_voice_call_hw_params,
	.shutdown = tegra_aic325x_voice_call_shutdown,
	.hw_free = tegra_aic325x_hw_free,
};

static struct snd_soc_ops tegra_aic325x_bt_voice_call_ops = {
	.hw_params = tegra_aic325x_bt_voice_call_hw_params,
	.shutdown = tegra_aic325x_bt_voice_call_shutdown,
	.hw_free = tegra_aic325x_hw_free,
};

static struct snd_soc_ops tegra_aic325x_bt_ops = {
	.hw_params = tegra_aic325x_bt_hw_params,
	.hw_free = tegra_aic325x_hw_free,
	.startup = tegra_aic325x_startup,
	.shutdown = tegra_aic325x_shutdown,
};

static struct snd_soc_jack tegra_aic325x_hp_jack;

#ifdef CONFIG_SWITCH
/* Headset jack detection gpios */

static struct switch_dev tegra_aic325x_headset_switch = {
	.name = "h2w",
};

static struct snd_soc_jack_gpio tegra_aic325x_hp_jack_gpio = {
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


static int aic325x_headset_switch_notify(struct notifier_block *self,
	unsigned long action, void *dev)
{

	struct snd_soc_jack *jack = (struct snd_soc_jack *)dev;
	struct snd_soc_codec *codec = jack->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int state = BIT_NO_HEADSET;

	gpio_direction_output(pdata->gpio_ext_mic_en, action);

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

	switch_set_state(&tegra_aic325x_headset_switch, state);

	return NOTIFY_OK;
}

static struct notifier_block aic325x_headset_switch_nb = {
	.notifier_call = aic325x_headset_switch_notify,
};
#else
static struct snd_soc_jack_pin tegra_aic325x_hp_jack_pins[] = {
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};
#endif

/**
 * tpa_hp_event: - To handle hprelated task before and after
 *			powrup and power down
 * @w: pointer variable to dapm_widget
 * @kcontrol: mixer control
 * @event: event element information
 *
 * Returns 0 for success.
 */
static int tpa_hp_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	int val;
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (machine->avdd_aud)
			regulator_enable(machine->avdd_aud);
		val = 0x00;
		tpa2054d4a_i2c_write_device(tpa2054d4a_client,
			TPA2054D4A_HP_OUTPUT_CNTRL_REG, 1, &val);
	} else {
		val = 0x1f;
		tpa2054d4a_i2c_write_device(tpa2054d4a_client,
			TPA2054D4A_HP_OUTPUT_CNTRL_REG, 1, &val);
		if (machine->avdd_aud)
			regulator_disable(machine->avdd_aud);
	}
	return 0;

}

/**
 * tpa_spk_event: - To handle spk related task before and after
 *			powrup and power down
 * @w: pointer variable to dapm_widget
 * @kcontrol: mixer control
 * @event: event element information
 *
 * Returns 0 for success.
 */
static int tpa_spk_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	int val;
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (machine->avdd_aud)
			regulator_enable(machine->avdd_aud);
		val = 0x1f;
		tpa2054d4a_i2c_write_device(tpa2054d4a_client,
			TPA2054D4A_HP_OUTPUT_CNTRL_REG, 1, &val);
		val = TPA2054D4A_HPR_EN | TPA2054D4A_HPL_EN | TPA2054D4A_PA_EN;
		tpa2054d4a_i2c_write_device(tpa2054d4a_client,
			TPA2054D4A_POWER_MGMT_REG, 1, &val);
	} else {
		val = 0x00;
		tpa2054d4a_i2c_write_device(tpa2054d4a_client,
			TPA2054D4A_HP_OUTPUT_CNTRL_REG, 1, &val);
		val = TPA2054D4A_HPR_EN | TPA2054D4A_HPL_EN;
		tpa2054d4a_i2c_write_device(tpa2054d4a_client,
			TPA2054D4A_POWER_MGMT_REG, 1, &val);
		if (machine->avdd_aud)
			regulator_disable(machine->avdd_aud);
	}
	return 0;
}

static int tpa2054d4a_mon_vol_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	int ret = 0;

	ret = tpa2054d4a_i2c_read_device(tpa2054d4a_client,
				TPA2054D4A_MON_VOL_CTRL_REG, 1, &ret);
	return ret;
}

static int tpa2054d4a_mon_vol_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	u16 val;

	val = (u16)ucontrol->value.integer.value[0];
	tpa2054d4a_i2c_write_device(tpa2054d4a_client,
				TPA2054D4A_MON_VOL_CTRL_REG, 1, &val);
	return 0;
}

static int tpa2054d4a_st1_vol_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	int ret = 0;

	ret = tpa2054d4a_i2c_read_device(tpa2054d4a_client,
				TPA2054D4A_ST1_VOL_CTRL_REG, 1, &ret);
	return ret;
}

static int tpa2054d4a_st1_vol_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	u16 val;

	val = (u16)ucontrol->value.integer.value[0];
	tpa2054d4a_i2c_write_device(tpa2054d4a_client,
				TPA2054D4A_ST1_VOL_CTRL_REG, 1, &val);
	return 0;
}

static int tpa2054d4a_st2_vol_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	int ret = 0;

	ret = tpa2054d4a_i2c_read_device(tpa2054d4a_client,
				TPA2054D4A_ST2_VOL_CTRL_REG, 1, &ret);
	return ret;
}

static int tpa2054d4a_st2_vol_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	u16 val;

	val = (u16)ucontrol->value.integer.value[0];

	tpa2054d4a_i2c_write_device(tpa2054d4a_client,
				TPA2054D4A_ST2_VOL_CTRL_REG, 1, &val);
	return 0;
}

static int tpa2054d4a_hp_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	int ret = 0;

	ret = tpa2054d4a_i2c_read_device(tpa2054d4a_client,
				TPA2054D4A_HP_OUTPUT_CNTRL_REG, 1, &ret);
	return ret;
}

static int tpa2054d4a_hp_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	u16 val;

	val = (u16)ucontrol->value.integer.value[0];

	tpa2054d4a_i2c_write_device(tpa2054d4a_client,
				TPA2054D4A_HP_OUTPUT_CNTRL_REG, 1, &val);
	return 0;
}

static const struct snd_soc_dapm_widget tegra_aic325x_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", tpa_hp_event),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_MIC("Int Mic", NULL),
	SND_SOC_DAPM_MIC("D-Mic", NULL),
	SND_SOC_DAPM_SPK("Int Spk", tpa_spk_event),
	SND_SOC_DAPM_OUTPUT("Earpiece"),
};

static const struct snd_soc_dapm_route aic325x_audio_map[] = {
	/* Headphone connected to HPL, HPR */
	{"Headphone Jack", NULL, "LOL"},
	{"Headphone Jack", NULL, "LOR"},

	{"Int Spk", NULL, "LOL"},
	{"Int Spk", NULL, "LOR"},

	{"Earpiece", NULL, "HPL"},
	{"Earpiece", NULL, "HPR"},

	{"IN3_L", NULL, "Mic Jack"},
	{"IN2_L", NULL, "Int Mic"},
	{"IN2_R", NULL, "Int Mic"},
	{"Mic Bias", NULL, "Int Mic"},

	{"Left DMIC", NULL, "D-Mic"},
	{"Right DMIC", NULL, "D-Mic"},
};

static const struct snd_kcontrol_new tegra_aic325x_controls[] = {
	SOC_DAPM_PIN_SWITCH("Earpiece"),
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Int Spk"),
	SOC_SINGLE_EXT("TPA Mono Vol Cntrl", SND_SOC_NOPM, 0, 32, 0,
			tpa2054d4a_mon_vol_get, tpa2054d4a_mon_vol_put),
	SOC_SINGLE_EXT("TPA Stereo1 Vol Cntrl",	SND_SOC_NOPM, 0, 32, 0,
			tpa2054d4a_st1_vol_get, tpa2054d4a_st1_vol_put),
	SOC_SINGLE_EXT("TPA Stereo2 Vol Cntrl",	SND_SOC_NOPM, 0, 32, 0,
			tpa2054d4a_st2_vol_get, tpa2054d4a_st2_vol_put),
	SOC_SINGLE_EXT("TPA HP Gain Cntrl", SND_SOC_NOPM, 0, 3, 0,
			tpa2054d4a_hp_gain_get, tpa2054d4a_hp_gain_put),
	SOC_DAPM_PIN_SWITCH("Mic Jack"),
	SOC_DAPM_PIN_SWITCH("Int Mic"),
	SOC_DAPM_PIN_SWITCH("D-Mic"),
};

static int tegra_aic325x_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = codec->card;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	int ret;

	i2s->is_dam_used = false;
	if (i2s->id == machine->codec_info[BT_SCO].i2s_id)
		i2s->is_dam_used = true;

	if (machine->init_done)
		return 0;

	machine->init_done = true;
	ret = tpa2054d4a_init(machine);
	if (ret) {
		dev_err(card->dev, "tpa2054d4a_init failed\n");
		/* return ret; */
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

	ret = snd_soc_add_card_controls(card, tegra_aic325x_controls,
				   ARRAY_SIZE(tegra_aic325x_controls));
	if (ret < 0)
		return ret;

	snd_soc_dapm_new_controls(dapm, tegra_aic325x_dapm_widgets,
					ARRAY_SIZE(tegra_aic325x_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, aic325x_audio_map,
					ARRAY_SIZE(aic325x_audio_map));


	if (gpio_is_valid(pdata->gpio_hp_det)) {
		/* Headphone detection */
		tegra_aic325x_hp_jack_gpio.gpio = pdata->gpio_hp_det;
		snd_soc_jack_new(codec, "Headphone Jack",
				SND_JACK_HEADSET, &tegra_aic325x_hp_jack);

#ifndef CONFIG_SWITCH
		snd_soc_jack_add_pins(&tegra_aic325x_hp_jack,
				ARRAY_SIZE(tegra_aic325x_hp_jack_pins),
				tegra_aic325x_hp_jack_pins);
#else
		snd_soc_jack_notifier_register(&tegra_aic325x_hp_jack,
					&aic325x_headset_switch_nb);
#endif
		snd_soc_jack_add_gpios(&tegra_aic325x_hp_jack,
					1,
					&tegra_aic325x_hp_jack_gpio);

		machine->gpio_requested |= GPIO_HP_DET;
	}

	/* Add call mode switch control */
	ret = snd_ctl_add(codec->card->snd_card,
			snd_ctl_new1(&tegra_aic325x_call_mode_control,
				machine));
	if (ret < 0)
		return ret;

	ret = tegra_asoc_utils_register_ctls(&machine->util_data);
	if (ret < 0)
		return ret;

	snd_soc_dapm_sync(dapm);
	return 0;
}

static struct snd_soc_dai_link tegra_aic325x_dai[] = {
	[DAI_LINK_HIFI] = {
		.name = "TLV320AIC325x",
		.stream_name = "TLV320AIC325x",
		.codec_name = "tlv320aic325x-codec",
		.platform_name = "tegra30-i2s.0",
		.cpu_dai_name = "tegra30-i2s.0",
		.codec_dai_name = "tlv320aic325x-MM_EXT",
		.init = tegra_aic325x_init,
		.ops = &tegra_aic325x_hifi_ops,
		},
	[DAI_LINK_BTSCO] = {
		.name = "BT-SCO",
		.stream_name = "BT SCO PCM",
		.codec_name = "spdif-dit.1",
		.platform_name = "tegra30-i2s.3",
		.cpu_dai_name = "tegra30-i2s.3",
		.codec_dai_name = "dit-hifi",
		.init = tegra_aic325x_init,
		.ops = &tegra_aic325x_bt_ops,
		},
	[DAI_LINK_VOICE_CALL] = {
			.name = "VOICE CALL",
			.stream_name = "VOICE CALL PCM",
			.codec_name = "tlv320aic325x-codec",
			.cpu_dai_name = "dit-hifi",
			.codec_dai_name = "tlv320aic325x-MM_EXT",
			.ops = &tegra_aic325x_voice_call_ops,
		},
	[DAI_LINK_BT_VOICE_CALL] = {
			.name = "BT VOICE CALL",
			.stream_name = "BT VOICE CALL PCM",
			.codec_name = "spdif-dit.2",
			.cpu_dai_name = "dit-hifi",
			.codec_dai_name = "dit-hifi",
			.ops = &tegra_aic325x_bt_voice_call_ops,
		},
};

static int tegra_aic325x_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_jack_gpio *gpio = &tegra_aic325x_hp_jack_gpio;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	int val;
	int i, suspend_allowed = 1;

	for (i = 0; i < machine->pcard->num_links; i++) {
		if (machine->pcard->dai_link[i].ignore_suspend) {
			suspend_allowed = 0;
			break;
		}
	}

	if (suspend_allowed) {
		if (gpio_is_valid(gpio->gpio))
			disable_irq(gpio_to_irq(gpio->gpio));

		if (machine->clock_enabled) {
			machine->clock_enabled = 0;
			tegra_asoc_utils_clk_disable(&machine->util_data);

			if (machine->tpa2054d4a_client) {
				tpa2054d4a_i2c_read_device(
						machine->tpa2054d4a_client,
						TPA2054D4A_POWER_MGMT_REG,
						1,
						&val);
				val |= TPA2054D4A_SWS;
				tpa2054d4a_i2c_write_device(
						machine->tpa2054d4a_client,
						TPA2054D4A_POWER_MGMT_REG,
						1,
						&val);
			}
		}
		if (machine->vdd_aud_dgtl)
			regulator_disable(machine->vdd_aud_dgtl);
	}

	return 0;
}

static int tegra_aic325x_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_jack_gpio *gpio = &tegra_aic325x_hp_jack_gpio;
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	int val;
	int i, suspend_allowed = 1;

	for (i = 0; i < machine->pcard->num_links; i++) {
		if (machine->pcard->dai_link[i].ignore_suspend) {
			suspend_allowed = 0;
			break;
		}
	}

	if (suspend_allowed) {
		if (gpio_is_valid(gpio->gpio)) {
			val = gpio_get_value_cansleep(gpio->gpio);
			val = gpio->invert ? !val : val;
			snd_soc_jack_report(gpio->jack, val, gpio->report);
			enable_irq(gpio_to_irq(gpio->gpio));
		}

		if (!machine->clock_enabled) {
			machine->clock_enabled = 1;
			tegra_asoc_utils_clk_enable(&machine->util_data);
			if (machine->tpa2054d4a_client) {
				tpa2054d4a_i2c_read_device(
						machine->tpa2054d4a_client,
						TPA2054D4A_POWER_MGMT_REG,
						1,
						&val);
				val &= ~TPA2054D4A_SWS;
				tpa2054d4a_i2c_write_device(
						machine->tpa2054d4a_client,
						TPA2054D4A_POWER_MGMT_REG,
						1,
						&val);
			}
		}
		if (machine->vdd_aud_dgtl)
			regulator_enable(machine->vdd_aud_dgtl);
	}

	return 0;
}

static int tegra_aic325x_set_bias_level(struct snd_soc_card *card,
	struct snd_soc_dapm_context *dapm, enum snd_soc_bias_level level)
{
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	int val;

	if (machine->bias_level == SND_SOC_BIAS_OFF &&
		level != SND_SOC_BIAS_OFF && (!machine->clock_enabled)) {
		machine->clock_enabled = 1;
		tegra_asoc_utils_clk_enable(&machine->util_data);
		machine->bias_level = level;

		if (machine->tpa2054d4a_client) {
			tpa2054d4a_i2c_read_device(machine->tpa2054d4a_client,
					TPA2054D4A_POWER_MGMT_REG, 1, &val);
			val &= ~TPA2054D4A_SWS;
			tpa2054d4a_i2c_write_device(machine->tpa2054d4a_client,
					TPA2054D4A_POWER_MGMT_REG, 1, &val);
		}
	}

	return 0;
}

static int tegra_aic325x_set_bias_level_post(struct snd_soc_card *card,
	struct snd_soc_dapm_context *dapm, enum snd_soc_bias_level level)
{
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	int val;

	if (machine->bias_level != SND_SOC_BIAS_OFF &&
		level == SND_SOC_BIAS_OFF && machine->clock_enabled) {
		machine->clock_enabled = 0;
		tegra_asoc_utils_clk_disable(&machine->util_data);

		if (machine->tpa2054d4a_client) {
			tpa2054d4a_i2c_read_device(machine->tpa2054d4a_client,
					TPA2054D4A_POWER_MGMT_REG, 1, &val);
			val |= TPA2054D4A_SWS;
			tpa2054d4a_i2c_write_device(machine->tpa2054d4a_client,
					TPA2054D4A_POWER_MGMT_REG, 1, &val);
		}
	}

	machine->bias_level = level;

	return 0 ;
}

static struct snd_soc_card snd_soc_tegra_aic325x = {
	.name = "tegra-aic325x",
	.owner = THIS_MODULE,
	.dai_link = tegra_aic325x_dai,
	.num_links = ARRAY_SIZE(tegra_aic325x_dai),
	.set_bias_level = tegra_aic325x_set_bias_level,
	.set_bias_level_post = tegra_aic325x_set_bias_level_post,
	.suspend_post = tegra_aic325x_suspend_post,
	.resume_pre = tegra_aic325x_resume_pre,
};

static __devinit int tegra_aic325x_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_aic325x;
	struct tegra_aic325x *machine;
	struct tegra_asoc_platform_data *pdata;
	int ret;
	int i;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -EINVAL;
	}

	machine = kzalloc(sizeof(struct tegra_aic325x), GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_aic325x struct\n");
		return -ENOMEM;
	}

	machine->pdata = pdata;
	machine->pcard = card;
	machine->bias_level = SND_SOC_BIAS_STANDBY;
	machine->clock_enabled = 1;

	ret = tegra_asoc_utils_init(&machine->util_data, &pdev->dev, card);
	if (ret)
		goto err_free_machine;

	machine->vdd_aud_dgtl = regulator_get(&pdev->dev, "vdd_aud_dgtl");
	if (IS_ERR(machine->vdd_aud_dgtl)) {
		dev_info(&pdev->dev, "No vdd_aud_dgtl regulator found\n");
		machine->vdd_aud_dgtl = 0;
	}

	if (machine->vdd_aud_dgtl)
		regulator_enable(machine->vdd_aud_dgtl);

	machine->avdd_aud = regulator_get(NULL, "avdd_aud");
	if (IS_ERR(machine->avdd_aud)) {
		dev_info(&pdev->dev, "No avdd_aud regulator found\n");
		machine->avdd_aud = 0;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

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

	if (pdata->ahub_bbc1_param) {
		machine->ahub_bbc1_info.port_id =
			pdata->ahub_bbc1_param->port_id;
		machine->ahub_bbc1_info.sample_size =
			pdata->ahub_bbc1_param->sample_size;
		machine->ahub_bbc1_info.rate =
			pdata->ahub_bbc1_param->rate;
		machine->ahub_bbc1_info.channels =
			pdata->ahub_bbc1_param->channels;
		machine->ahub_bbc1_info.bit_clk =
			pdata->ahub_bbc1_param->bit_clk;
	}

	tegra_aic325x_dai[DAI_LINK_HIFI].cpu_dai_name =
	tegra_i2s_dai_name[machine->codec_info[HIFI_CODEC].i2s_id];
	tegra_aic325x_dai[DAI_LINK_HIFI].platform_name =
	tegra_i2s_dai_name[machine->codec_info[HIFI_CODEC].i2s_id];

	tegra_aic325x_dai[DAI_LINK_BTSCO].cpu_dai_name =
	tegra_i2s_dai_name[machine->codec_info[BT_SCO].i2s_id];
	tegra_aic325x_dai[DAI_LINK_BTSCO].platform_name =
	tegra_i2s_dai_name[machine->codec_info[BT_SCO].i2s_id];

	tegra_aic325x_dai[DAI_LINK_VOICE_CALL].platform_name =
	tegra_i2s_dai_name[machine->codec_info[HIFI_CODEC].i2s_id];
	tegra_aic325x_dai[DAI_LINK_BT_VOICE_CALL].platform_name =
	tegra_i2s_dai_name[machine->codec_info[BT_SCO].i2s_id];

	card->dapm.idle_bias_off = 1;

#ifdef CONFIG_SWITCH
	/* Add h2w switch class support */
	ret = tegra_asoc_switch_register(&tegra_aic325x_headset_switch);
	if (ret < 0) {
		dev_err(&pdev->dev, "not able to register switch device\n");
		goto err_switch_unregister;
	}

#endif

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_fini_utils;
	}

	if (!card->instantiated) {
		dev_err(&pdev->dev, "No TI AIC325x codec\n");
		goto err_unregister_card;
	}

	ret = tegra_asoc_utils_set_parent(&machine->util_data,
				pdata->i2s_param[HIFI_CODEC].is_i2s_master);
	if (ret) {
		dev_err(&pdev->dev, "tegra_asoc_utils_set_parent failed (%d)\n",
			ret);
		goto err_unregister_card;
	}


	return 0;

err_unregister_card:
	snd_soc_unregister_card(card);
err_switch_unregister:
#ifdef CONFIG_SWITCH
	tegra_asoc_switch_unregister(&tegra_aic325x_headset_switch);
#endif
err_fini_utils:
	tegra_asoc_utils_fini(&machine->util_data);
err_free_machine:
	kfree(machine);
	return ret;
}

static int __devexit tegra_aic325x_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_aic325x *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;

#ifdef CONFIG_SWITCH
	tegra_asoc_switch_unregister(&tegra_aic325x_headset_switch);
#endif
	if (machine->gpio_requested & GPIO_HP_DET)
		snd_soc_jack_free_gpios(&tegra_aic325x_hp_jack,
					1,
					&tegra_aic325x_hp_jack_gpio);

	snd_soc_unregister_card(card);

	tegra_asoc_utils_fini(&machine->util_data);

	if (machine->gpio_requested & GPIO_INT_MIC_EN)
		gpio_free(pdata->gpio_int_mic_en);
	if (machine->gpio_requested & GPIO_SPKR_EN)
		gpio_free(pdata->gpio_spkr_en);

	if (machine->vdd_aud_dgtl)
		regulator_put(machine->vdd_aud_dgtl);

	if (machine->avdd_aud)
		regulator_put(machine->avdd_aud);

	kfree(machine);

	return 0;
}

static struct platform_driver tegra_aic325x_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = tegra_aic325x_driver_probe,
	.remove = __devexit_p(tegra_aic325x_driver_remove),
};

static int __init tegra_aic325x_modinit(void)
{
	return platform_driver_register(&tegra_aic325x_driver);
}
module_init(tegra_aic325x_modinit);

static void __exit tegra_aic325x_modexit(void)
{
	platform_driver_unregister(&tegra_aic325x_driver);
}
module_exit(tegra_aic325x_modexit);

/* Module information */
MODULE_AUTHOR("Ravindra Lokhande <rlokhande@nvidia.com>");
MODULE_DESCRIPTION("Tegra+AIC325x machine ASoC driver");
MODULE_DESCRIPTION("Tegra ALSA SoC");
MODULE_LICENSE("GPL");
