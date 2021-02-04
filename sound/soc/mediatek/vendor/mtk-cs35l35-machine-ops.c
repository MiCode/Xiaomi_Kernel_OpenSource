/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */


/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mtk-cs35l35-machine-ops.c
 *
 * Project:
 * --------
 *   Audio soc machine vendor ops
 *
 * Description:
 * ------------
 *   Audio machine driver
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 ******************************************************************************
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>


#if defined(CONFIG_SND_SOC_CS35L35)
static int cs35l35_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static int cs35l35_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd[0].codec;
	struct snd_soc_dai *codec_dai = rtd[0].codec_dai;
	struct snd_soc_dai_link *dai_link = rtd[0].dai_link;
	int ret;

	ret = snd_soc_dai_set_fmt(codec_dai, dai_link->dai_fmt);
	if (ret != 0) {
		dev_err(codec_dai->dev, "Failed to set %s's dai_fmt: ret = %d",
			codec_dai->name, ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, 0, 3072000, SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(codec_dai->dev, "Failed to set %s's clock: ret = %d\n",
			codec_dai->name, ret);
		return ret;
	}

	ret = snd_soc_codec_set_sysclk(codec, 0, 0, 12288000, SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(codec_dai->dev, "Failed to set %s's clock: ret = %d\n",
			codec_dai->name, ret);
		return ret;
	}

	return ret;
}

static int cs35l35_free(struct snd_pcm_substream *substream)
{
	return 0;
}

const struct snd_soc_ops cs35l35_ops = {
	.startup = cs35l35_startup,
	.hw_params = cs35l35_hw_params,
	.hw_free = cs35l35_free,
};
EXPORT_SYMBOL_GPL(cs35l35_ops);

#endif /* #if defined(CONFIG_SND_SOC_CS35L35)*/
