/*
 * tegra_asoc_utils.c - Harmony machine ASoC driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2010-12, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <mach/clk.h>

#include <sound/soc.h>

#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"

int g_is_call_mode;

#ifdef CONFIG_SWITCH
static bool is_switch_registered;
struct switch_dev *psdev;
/* These values are copied from WiredAccessoryObserver */
enum headset_state {
	BIT_NO_HEADSET = 0,
	BIT_HEADSET = (1 << 0),
	BIT_HEADSET_NO_MIC = (1 << 1),
};
#endif

bool tegra_is_voice_call_active(void)
{
	if (g_is_call_mode)
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(tegra_is_voice_call_active);

static int tegra_get_avp_device(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct  tegra_asoc_utils_data *data = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = data->avp_device_id;
	return 0;
}

static int tegra_set_avp_device(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct  tegra_asoc_utils_data *data = snd_kcontrol_chip(kcontrol);
	struct snd_soc_card *card = data->card;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_pcm_substream *substream;
	struct tegra_runtime_data *prtd;
	int id, old_id = data->avp_device_id;

	id = ucontrol->value.integer.value[0];
	if ((id >= card->num_rtd) || (id < 0))
		id = -1;

	if (old_id >= 0) {
		rtd = &card->rtd[old_id];
		substream =
			rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
		if (substream && substream->runtime) {
			prtd = substream->runtime->private_data;
			if (!prtd)
				return -EINVAL;
			if (prtd->running)
				return -EBUSY;
			prtd->disable_intr = false;
		}
	}

	if (id >= 0) {
		rtd = &card->rtd[id];
		substream =
			rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
		if (substream && substream->runtime) {
			prtd = substream->runtime->private_data;
			if (!prtd)
				return -EINVAL;
			if (prtd->running)
				return -EBUSY;
			prtd->disable_intr = true;
			if (data->avp_dma_addr || prtd->avp_dma_addr)
				prtd->avp_dma_addr = data->avp_dma_addr;
		}
	}
	data->avp_device_id = id;
	return 1;
}

static int tegra_get_dma_ch_id(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct  tegra_asoc_utils_data *data = snd_kcontrol_chip(kcontrol);
	struct snd_soc_card *card = data->card;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_pcm_substream *substream;
	struct tegra_runtime_data *prtd;

	ucontrol->value.integer.value[0] = -1;
	if (data->avp_device_id < 0)
		return 0;

	rtd = &card->rtd[data->avp_device_id];
	substream = rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (!substream || !substream->runtime)
		return 0;

	prtd = substream->runtime->private_data;
	if (!prtd || !prtd->dma_chan)
		return 0;

	ucontrol->value.integer.value[0] =
		tegra_dma_get_channel_id(prtd->dma_chan);
	return 0;
}

static int tegra_set_dma_addr(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_asoc_utils_data *data = snd_kcontrol_chip(kcontrol);
	struct snd_soc_card *card = data->card;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_pcm_substream *substream;
	struct tegra_runtime_data *prtd;

	if (data->avp_device_id < 0)
		return 0;

	data->avp_dma_addr = ucontrol->value.integer.value[0];

	rtd = &card->rtd[data->avp_device_id];
	substream = rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (!substream || !substream->runtime)
		return 0;

	prtd = substream->runtime->private_data;
	if (!prtd)
		return 0;

	prtd->avp_dma_addr = data->avp_dma_addr;
	return 1;
}

static int tegra_get_dma_addr(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct  tegra_asoc_utils_data *data = snd_kcontrol_chip(kcontrol);
	struct snd_soc_card *card = data->card;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_pcm_substream *substream;
	struct tegra_runtime_data *prtd;

	ucontrol->value.integer.value[0] = 0;
	if (data->avp_device_id < 0)
		return 0;

	rtd = &card->rtd[data->avp_device_id];
	substream = rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (!substream || !substream->runtime)
		return 0;

	prtd = substream->runtime->private_data;
	if (!prtd || !prtd->dma_chan)
		return 0;

	ucontrol->value.integer.value[0] = prtd->avp_dma_addr ?
					   prtd->avp_dma_addr :
					   substream->runtime->dma_addr;

	return 0;
}

struct snd_kcontrol_new tegra_avp_controls[] = {
	SOC_SINGLE_EXT("AVP alsa device select", 0, 0, TEGRA_ALSA_MAX_DEVICES, \
			0, tegra_get_avp_device, tegra_set_avp_device),
	SOC_SINGLE_EXT("AVP DMA channel id", 0, 0, TEGRA_DMA_MAX_CHANNELS, \
			0, tegra_get_dma_ch_id, NULL),
	SOC_SINGLE_EXT("AVP DMA address", 0, 0, 0xFFFFFFFF, \
			0, tegra_get_dma_addr, tegra_set_dma_addr),
};

static int tegra_set_headset_plug_state(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_asoc_utils_data *data = snd_kcontrol_chip(kcontrol);
	int switch_state;

	data->headset_plug_state = ucontrol->value.integer.value[0];
	switch_state = data->headset_plug_state == 1 ? BIT_HEADSET
		: BIT_NO_HEADSET;
	if (psdev)
		switch_set_state(psdev, switch_state);

	return 1;
}

static int tegra_get_headset_plug_state(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct  tegra_asoc_utils_data *data = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = data->headset_plug_state;

	return 0;
}

struct snd_kcontrol_new tegra_switch_controls =
	SOC_SINGLE_EXT("Headset Plug State", 0, 0, 1, \
	0, tegra_get_headset_plug_state, tegra_set_headset_plug_state);


int tegra_asoc_utils_set_rate(struct tegra_asoc_utils_data *data, int srate,
			      int mclk)
{
	int new_baseclock;
	bool clk_change;
	int err;
	bool reenable_clock;

	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
		new_baseclock = 56448000;
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
		new_baseclock = 564480000;
#else
		new_baseclock = 282240000;
#endif
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
		new_baseclock = 73728000;
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
		new_baseclock = 552960000;
#else
		new_baseclock = 368640000;
#endif
		break;
	default:
		return -EINVAL;
	}

	clk_change = ((new_baseclock != data->set_baseclock) ||
			(mclk != data->set_mclk));
	if (!clk_change)
		return 0;

	/* Don't change rate if already one dai-link is using it */
	if (data->lock_count)
		return -EINVAL;

	data->set_baseclock = 0;
	data->set_mclk = 0;

	reenable_clock = false;
	if(tegra_is_clk_enabled(data->clk_pll_a)) {
		clk_disable(data->clk_pll_a);
		reenable_clock = true;
	}
	err = clk_set_rate(data->clk_pll_a, new_baseclock);
	if (err) {
		dev_err(data->dev, "Can't set pll_a rate: %d\n", err);
		return err;
	}
	if(reenable_clock)
		clk_enable(data->clk_pll_a);

	reenable_clock = false;
	if(tegra_is_clk_enabled(data->clk_pll_a_out0)) {
		clk_disable(data->clk_pll_a_out0);
		reenable_clock = true;
	}
	err = clk_set_rate(data->clk_pll_a_out0, mclk);
	if (err) {
		dev_err(data->dev, "Can't set clk_pll_a_out0 rate: %d\n", err);
		return err;
	}
	if(reenable_clock)
		clk_enable(data->clk_pll_a_out0);


	data->set_baseclock = new_baseclock;
	data->set_mclk = mclk;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_set_rate);

void tegra_asoc_utils_lock_clk_rate(struct tegra_asoc_utils_data *data,
				    int lock)
{
	if (lock)
		data->lock_count++;
	else if (data->lock_count)
		data->lock_count--;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_lock_clk_rate);

int tegra_asoc_utils_clk_enable(struct tegra_asoc_utils_data *data)
{
	int err;

	err = clk_enable(data->clk_cdev1);
	if (err) {
		dev_err(data->dev, "Can't enable cdev1: %d\n", err);
		return err;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_clk_enable);

int tegra_asoc_utils_clk_disable(struct tegra_asoc_utils_data *data)
{
	clk_disable(data->clk_cdev1);
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_clk_disable);

int tegra_asoc_utils_register_ctls(struct tegra_asoc_utils_data *data)
{
	int i;
	int ret = 0;

	/* Add AVP related alsa controls */
	data->avp_device_id = -1;
	for (i = 0; i < ARRAY_SIZE(tegra_avp_controls); i++) {
		ret = snd_ctl_add(data->card->snd_card,
				snd_ctl_new1(&tegra_avp_controls[i], data));
		if (ret < 0) {
			dev_err(data->dev, "Can't add avp alsa controls");
			return ret;
		}
	}

	ret = snd_ctl_add(data->card->snd_card,
			snd_ctl_new1(&tegra_switch_controls, data));
	if (ret < 0) {
		dev_err(data->dev, "Can't add switch alsa control");
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_register_ctls);

int tegra_asoc_utils_init(struct tegra_asoc_utils_data *data,
			  struct device *dev, struct snd_soc_card *card)
{
	int ret;

	data->dev = dev;
	data->card = card;

	data->clk_pll_p_out1 = clk_get_sys(NULL, "pll_p_out1");
	if (IS_ERR(data->clk_pll_p_out1)) {
		dev_err(data->dev, "Can't retrieve clk pll_p_out1\n");
		ret = PTR_ERR(data->clk_pll_p_out1);
		goto err;
	}

	data->clk_pll_a = clk_get_sys(NULL, "pll_a");
	if (IS_ERR(data->clk_pll_a)) {
		dev_err(data->dev, "Can't retrieve clk pll_a\n");
		ret = PTR_ERR(data->clk_pll_a);
		goto err_put_pll_p_out1;
	}

	data->clk_pll_a_out0 = clk_get_sys(NULL, "pll_a_out0");
	if (IS_ERR(data->clk_pll_a_out0)) {
		dev_err(data->dev, "Can't retrieve clk pll_a_out0\n");
		ret = PTR_ERR(data->clk_pll_a_out0);
		goto err_put_pll_a;
	}

	data->clk_m = clk_get_sys(NULL, "clk_m");
	if (IS_ERR(data->clk_m)) {
		dev_err(data->dev, "Can't retrieve clk clk_m\n");
		ret = PTR_ERR(data->clk_m);
		goto err;
	}

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	data->clk_cdev1 = clk_get_sys(NULL, "cdev1");
#else
	data->clk_cdev1 = clk_get_sys("extern1", NULL);
#endif
	if (IS_ERR(data->clk_cdev1)) {
		dev_err(data->dev, "Can't retrieve clk cdev1\n");
		ret = PTR_ERR(data->clk_cdev1);
		goto err_put_pll_a_out0;
	}

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	data->clk_out1 = ERR_PTR(-ENOENT);
#else
	data->clk_out1 = clk_get_sys("clk_out_1", "extern1");
	if (IS_ERR(data->clk_out1)) {
		dev_err(data->dev, "Can't retrieve clk out1\n");
		ret = PTR_ERR(data->clk_out1);
		goto err_put_cdev1;
	}
#endif

	ret = clk_enable(data->clk_cdev1);
	if (ret) {
		dev_err(data->dev, "Can't enable clk cdev1/extern1");
		goto err_put_out1;
	}

	if (!IS_ERR(data->clk_out1)) {
		ret = clk_enable(data->clk_out1);
		if (ret) {
			dev_err(data->dev, "Can't enable clk out1");
			goto err_put_out1;
		}
	}

	ret = tegra_asoc_utils_set_rate(data, 48000, 256 * 48000);
	if (ret)
		goto err_put_out1;

	return 0;

err_put_out1:
	if (!IS_ERR(data->clk_out1))
		clk_put(data->clk_out1);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
err_put_cdev1:
#endif
	clk_put(data->clk_cdev1);
err_put_pll_a_out0:
	clk_put(data->clk_pll_a_out0);
err_put_pll_a:
	clk_put(data->clk_pll_a);
err_put_pll_p_out1:
	clk_put(data->clk_pll_p_out1);
err:
	return ret;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_init);

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
int tegra_asoc_utils_set_parent (struct tegra_asoc_utils_data *data,
				int is_i2s_master)
{
	int ret = -ENODEV;

	if (is_i2s_master) {
		ret = clk_set_parent(data->clk_cdev1, data->clk_pll_a_out0);
		if (ret) {
			dev_err(data->dev, "Can't set clk cdev1/extern1 parent");
			return ret;
		}
	} else {
		if(clk_get_rate(data->clk_m) == 26000000)
			clk_set_rate(data->clk_cdev1, 13000000);

		ret = clk_set_parent(data->clk_cdev1, data->clk_m);
		if (ret) {
			dev_err(data->dev, "Can't set clk cdev1/extern1 parent");
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_set_parent);
#endif

void tegra_asoc_utils_fini(struct tegra_asoc_utils_data *data)
{
	if (!IS_ERR(data->clk_out1))
		clk_put(data->clk_out1);

	clk_put(data->clk_cdev1);
	/* Just to make sure that clk_cdev1 should turn off in case if it is
	 * switched on by some codec whose hw switch is not registered.*/
	if (tegra_is_clk_enabled(data->clk_cdev1))
		clk_disable(data->clk_cdev1);

	if (!IS_ERR(data->clk_pll_a_out0))
		clk_put(data->clk_pll_a_out0);

	if (!IS_ERR(data->clk_pll_a))
		clk_put(data->clk_pll_a);

	if (!IS_ERR(data->clk_pll_p_out1))
		clk_put(data->clk_pll_p_out1);
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_fini);

#ifdef CONFIG_SWITCH
int tegra_asoc_switch_register(struct switch_dev *sdev)
{
	int ret;

	if (is_switch_registered)
		return -EBUSY;

	ret = switch_dev_register(sdev);

	if (ret >= 0) {
		psdev = sdev;
		is_switch_registered = true;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tegra_asoc_switch_register);

void tegra_asoc_switch_unregister(struct switch_dev *sdev)
{
	if (!is_switch_registered)
		return;

	switch_dev_unregister(sdev);
	is_switch_registered = false;
	psdev = NULL;
}
EXPORT_SYMBOL_GPL(tegra_asoc_switch_unregister);
#endif


MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra ASoC utility code");
MODULE_LICENSE("GPL");
