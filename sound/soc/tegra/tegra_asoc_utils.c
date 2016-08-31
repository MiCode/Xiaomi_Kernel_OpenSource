/*
 * tegra_asoc_utils.c - Harmony machine ASoC driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved.
 *
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
#include <linux/of.h>
#include <linux/clk/tegra.h>

#include <mach/pinmux.h>
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#include <mach/pinmux-tegra20.h>
#endif
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
#include <mach/pinmux-tegra30.h>
#endif
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
#include <mach/pinmux-t11.h>
#endif
#ifdef CONFIG_ARCH_TEGRA_12x_SOC
#include <mach/pinmux-t12.h>
#endif
#ifdef CONFIG_ARCH_TEGRA_14x_SOC
#include <mach/pinmux-t14.h>
#endif

#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"

int g_is_call_mode;
static atomic_t dap_ref_count[5];
int tegra_i2sloopback_func;

static const char * const loopback_function[] = {
	"Off",
	"On"
};

static const struct soc_enum tegra_enum =
	SOC_ENUM_SINGLE_EXT(2, loopback_function);
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

#define TRISTATE_DAP_PORT(n) \
static void tristate_dap_##n(bool tristate) \
{ \
	enum tegra_pingroup fs, sclk, din, dout; \
	fs = TEGRA_PINGROUP_DAP##n##_FS; \
	sclk = TEGRA_PINGROUP_DAP##n##_SCLK; \
	din = TEGRA_PINGROUP_DAP##n##_DIN; \
	dout = TEGRA_PINGROUP_DAP##n##_DOUT; \
	if (tristate) { \
		if (atomic_dec_return(&dap_ref_count[n-1]) == 0) {\
			tegra_pinmux_set_tristate(fs, TEGRA_TRI_TRISTATE); \
			tegra_pinmux_set_tristate(sclk, TEGRA_TRI_TRISTATE); \
			tegra_pinmux_set_tristate(din, TEGRA_TRI_TRISTATE); \
			tegra_pinmux_set_tristate(dout, TEGRA_TRI_TRISTATE); \
		} \
	} else { \
		if (atomic_inc_return(&dap_ref_count[n-1]) == 1) {\
			tegra_pinmux_set_tristate(fs, TEGRA_TRI_NORMAL); \
			tegra_pinmux_set_tristate(sclk, TEGRA_TRI_NORMAL); \
			tegra_pinmux_set_tristate(din, TEGRA_TRI_NORMAL); \
			tegra_pinmux_set_tristate(dout, TEGRA_TRI_NORMAL); \
		} \
	} \
}

TRISTATE_DAP_PORT(1)
TRISTATE_DAP_PORT(2)
/*I2S2 and I2S3 for other chips do not map to DAP3 and DAP4 (also
these pinmux dont exist for other chips), they map to some
other pinmux*/
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)\
		|| defined(CONFIG_ARCH_TEGRA_12x_SOC)\
		|| defined(CONFIG_ARCH_TEGRA_3x_SOC)
	TRISTATE_DAP_PORT(3)
	TRISTATE_DAP_PORT(4)
#endif

int tegra_asoc_utils_tristate_dap(int id, bool tristate)
{
	switch (id) {
	case 0:
		tristate_dap_1(tristate);
		break;
	case 1:
		tristate_dap_2(tristate);
		break;
/*I2S2 and I2S3 for other chips do not map to DAP3 and DAP4 (also
these pinmux dont exist for other chips), they map to some
other pinmux*/
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)\
	|| defined(CONFIG_ARCH_TEGRA_12x_SOC)\
	|| defined(CONFIG_ARCH_TEGRA_3x_SOC)
	case 2:
		tristate_dap_3(tristate);
		break;
	case 3:
		tristate_dap_4(tristate);
		break;
#endif
	default:
		pr_warn("Invalid DAP port\n");
		break;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_tristate_dap);

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
			prtd = (struct tegra_runtime_data *)
				snd_dmaengine_pcm_get_data(substream);
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
			prtd = (struct tegra_runtime_data *)
				snd_dmaengine_pcm_get_data(substream);
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
	struct dma_chan *chan;

	if (data->avp_device_id < 0)
		return 0;

	rtd = &card->rtd[data->avp_device_id];
	substream = rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;

	if (!substream || !substream->runtime)
		return 0;

	prtd = (struct tegra_runtime_data *)
		snd_dmaengine_pcm_get_data(substream);

	if (!prtd)
		return 0;

	chan = snd_dmaengine_pcm_get_chan(substream);

	ucontrol->value.integer.value[0] = chan->chan_id;

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

	rtd = &card->rtd[data->avp_device_id];
	substream =
	rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;

	if (!substream || !substream->runtime)
		return 0;

	prtd = (struct tegra_runtime_data *)
		snd_dmaengine_pcm_get_data(substream);

	if (!prtd)
		return 0;

	data->avp_dma_addr = (dma_addr_t)ucontrol->value.integer.value[0];
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

	if (data->avp_device_id < 0)
		return 0;

	rtd = &card->rtd[data->avp_device_id];
	substream = rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;

	if (!substream || !substream->runtime)
		return 0;

	prtd = (struct tegra_runtime_data *)
		snd_dmaengine_pcm_get_data(substream);

	if (!prtd)
		return 0;

	ucontrol->value.integer.value[0] = 0;
	ucontrol->value.integer.value[0] = prtd->avp_dma_addr ?
					   (long)prtd->avp_dma_addr :
					   (long)substream->runtime->dma_addr;

	return 0;
}

static int tegra_get_i2sloopback(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = tegra_i2sloopback_func;
	return 0;
}

static int tegra_set_i2sloopback(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	if (tegra_i2sloopback_func == ucontrol->value.integer.value[0])
		return 0;

	tegra_i2sloopback_func = ucontrol->value.integer.value[0];
	return 1;
}

struct snd_kcontrol_new tegra_avp_controls[] = {
	SOC_SINGLE_EXT("AVP alsa device select", 0, 0, TEGRA_ALSA_MAX_DEVICES, \
			0, tegra_get_avp_device, tegra_set_avp_device),
	SOC_SINGLE_EXT("AVP DMA channel id", 0, 0, TEGRA_DMA_MAX_CHANNELS, \
			0, tegra_get_dma_ch_id, NULL),
	SOC_SINGLE_EXT("AVP DMA address", 0, 0, 0xFFFFFFFF, \
			0, tegra_get_dma_addr, tegra_set_dma_addr),
};

static const struct snd_kcontrol_new tegra_i2s_lpbk_control =
	SOC_ENUM_EXT("I2S LoopBack", tegra_enum, tegra_get_i2sloopback,
			tegra_set_i2sloopback);

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
	case 176400:
		if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
			new_baseclock = 56448000;
		else if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA30)
			new_baseclock = 564480000;
		else
			new_baseclock = 282240000;
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
	case 192000:
		if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
			new_baseclock = 73728000;
		else if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA30)
			new_baseclock = 552960000;
		else
			new_baseclock = 368640000;
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
		clk_disable_unprepare(data->clk_pll_a);
		reenable_clock = true;
	}

	err = clk_set_rate(data->clk_pll_a, new_baseclock);
	if (err) {
		dev_err(data->dev, "Can't set pll_a rate: %d\n", err);
		return err;
	}
	if(reenable_clock)
		clk_prepare_enable(data->clk_pll_a);

	reenable_clock = false;
	if(tegra_is_clk_enabled(data->clk_pll_a_out0)) {
		clk_disable_unprepare(data->clk_pll_a_out0);
		reenable_clock = true;
	}
	err = clk_set_rate(data->clk_pll_a_out0, mclk);
	if (err) {
		dev_err(data->dev, "Can't set clk_pll_a_out0 rate: %d\n", err);
		return err;
	}
	if(reenable_clock)
		clk_prepare_enable(data->clk_pll_a_out0);

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

	err = clk_prepare_enable(data->clk_cdev1);
	if (err) {
		dev_err(data->dev, "Can't enable cdev1: %d\n", err);
		return err;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_clk_enable);

int tegra_asoc_utils_clk_disable(struct tegra_asoc_utils_data *data)
{
	clk_disable_unprepare(data->clk_cdev1);
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_clk_disable);

int tegra_asoc_utils_register_ctls(struct tegra_asoc_utils_data *data)
{
	int ret = 0;
	int i;

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

	ret = snd_ctl_add(data->card->snd_card,
			snd_ctl_new1(&tegra_i2s_lpbk_control, data));
	if (ret < 0) {
		dev_err(data->dev, "Can't add i2s loopback control");
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

	if (of_machine_is_compatible("nvidia,tegra20"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA20;
	else if (of_machine_is_compatible("nvidia,tegra30"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA30;
	else if (of_machine_is_compatible("nvidia,tegra114"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA11x;
	else if (of_machine_is_compatible("nvidia,tegra148"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA14x;
	else if (of_machine_is_compatible("nvidia,tegra124"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA12x;
	else if (!dev->of_node) {
		/* non-DT is always Tegra20 */
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA20;
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA30;
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA11x;
#elif defined(CONFIG_ARCH_TEGRA_14x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA14x;
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA12x;
#endif
	} else
		/* DT boot, but unknown SoC */
		return -EINVAL;

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

	if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
		data->clk_cdev1 = clk_get_sys(NULL, "cdev1");
	else
		data->clk_cdev1 = clk_get_sys("extern1", NULL);

	if (IS_ERR(data->clk_cdev1)) {
		dev_err(data->dev, "Can't retrieve clk cdev1\n");
		ret = PTR_ERR(data->clk_cdev1);
		goto err_put_pll_a_out0;
	}

	if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
		data->clk_out1 = ERR_PTR(-ENOENT);
	else {
		data->clk_out1 = clk_get_sys("clk_out_1", "extern1");
		if (IS_ERR(data->clk_out1)) {
			dev_err(data->dev, "Can't retrieve clk out1\n");
			ret = PTR_ERR(data->clk_out1);
			goto err_put_cdev1;
		}
	}

	ret = clk_prepare_enable(data->clk_cdev1);
	if (ret) {
		dev_err(data->dev, "Can't enable clk cdev1/extern1");
		goto err_put_out1;
	}

	if (!IS_ERR(data->clk_out1)) {
		ret = clk_prepare_enable(data->clk_out1);
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
err_put_cdev1:
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

int tegra_asoc_utils_set_parent (struct tegra_asoc_utils_data *data,
				int is_i2s_master)
{
	int ret = -ENODEV;

	if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
		return ret;

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
