/*
 * tegra_vcm.c - Tegra machine ASoC driver for P852/P1852/P1853 Boards.
 *
 * Author: Nitin Pai <npai@nvidia.com>
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * Based on code copyright/by:
 * Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#include <asm/mach-types.h>

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <mach/tegra_asoc_vcm_pdata.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"

#ifdef CONFIG_MACH_P1852
#define DRV_NAME "tegra-snd-p1852"
#endif
#ifdef CONFIG_MACH_E1853
#define DRV_NAME "tegra-snd-p1853"
#endif
#ifdef CONFIG_MACH_P852
#define DRV_NAME "tegra-snd-p852"
#endif
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#include "tegra20_das.h"
#endif


struct tegra_vcm {
	struct tegra_asoc_utils_data util_data;
	struct tegra_asoc_vcm_platform_data *pdata;
};

static int tegra_vcm_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params,
					int codec_id)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm *machine = snd_soc_card_get_drvdata(card);
	int srate, mclk;
	int i2s_daifmt = 0;
	int err;
	struct tegra_asoc_vcm_platform_data *pdata;
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	int dac_id;
	int dap_id;
#endif
	pdata = machine->pdata;

	srate = params_rate(params);
	switch (srate) {
	case 64000:
	case 88200:
	case 96000:
		mclk = 128 * srate;
		break;
	default:
		mclk = 256 * srate;
		break;
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

	if (pdata->codec_info[codec_id].master)
		i2s_daifmt |= SND_SOC_DAIFMT_CBM_CFM;
	else
		i2s_daifmt |= SND_SOC_DAIFMT_CBS_CFS;

	switch (pdata->codec_info[codec_id].i2s_format) {
	case format_tdm:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
		break;
	case format_i2s:
		i2s_daifmt |= SND_SOC_DAIFMT_I2S;
		break;
	case format_rjm:
		i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
		break;
	case format_ljm:
		i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
		break;
	default:
		break;
	}

	err = snd_soc_dai_set_fmt(codec_dai, i2s_daifmt);
	if (err < 0)
		dev_info(card->dev, "codec_dai fmt not set\n");

	err = snd_soc_dai_set_fmt(cpu_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai fmt not set\n");
		return err;
	}
	err = snd_soc_dai_set_sysclk(codec_dai, 0, mclk,
					SND_SOC_CLOCK_IN);
	if (err < 0)
		dev_info(card->dev, "codec_dai clock not set\n");

	if (pdata->codec_info[codec_id].i2s_format ==
			format_tdm) {
			err = snd_soc_dai_set_tdm_slot(cpu_dai,
					pdata->codec_info[codec_id].rx_mask,
					pdata->codec_info[codec_id].tx_mask,
					pdata->codec_info[codec_id].num_slots,
					pdata->codec_info[codec_id].slot_width);
		if (err < 0)
			dev_err(card->dev, "cpu_dai tdm mode setting not done\n");
	}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	dac_id = TEGRA20_DAS_DAP_SEL_DAC1 + pdata->dac_info[codec_id].dac_id;
	dap_id = TEGRA20_DAS_DAP_ID_1 + pdata->dac_info[codec_id].dap_id;
	err = tegra20_das_connect_dac_to_dap(dac_id, dap_id);
	if (err < 0) {
		dev_err(card->dev, "failed to set dap-dac path\n");
		return err;
	}

	tegra20_das_set_tristate(dap_id, 0);

	err = tegra20_das_connect_dap_to_dac(dap_id, dac_id);
	if (err < 0) {
		dev_err(card->dev, "failed to set dac-dap path\n");
		return err;
	}
#endif

	return 0;
}

static int tegra_vcm_hw_params_controller1(
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	return tegra_vcm_hw_params(substream, params, 0);
}

static int tegra_vcm_hw_params_controller2(
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	return tegra_vcm_hw_params(substream, params, 1);
}

static int tegra_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_vcm *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 0);

	return 0;
}

static struct snd_soc_ops tegra_vcm_ops_controller1 = {
	.hw_params = tegra_vcm_hw_params_controller1,
	.hw_free = tegra_hw_free,
};
static struct snd_soc_ops tegra_vcm_ops_controller2 = {
	.hw_params = tegra_vcm_hw_params_controller2,
	.hw_free = tegra_hw_free,
};

static struct snd_soc_dai_link tegra_vcm_dai_link[] = {
	{
		.name = "I2S-TDM-1",
		.stream_name = "TEGRA PCM",
		.platform_name = "tegra-pcm-audio",
		.ops = &tegra_vcm_ops_controller1,
	},
	{
		.name = "I2S-TDM-2",
		.stream_name = "TEGRA PCM",
		.platform_name = "tegra-pcm-audio",
		.ops = &tegra_vcm_ops_controller2,
	}
};

static struct snd_soc_card snd_soc_tegra_vcm = {
	.name = "tegra-vcm",
	.owner = THIS_MODULE,
	.dai_link = tegra_vcm_dai_link,
	.num_links = ARRAY_SIZE(tegra_vcm_dai_link),

};

static __devinit int tegra_vcm_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_vcm;
	struct tegra_vcm *machine;
	struct tegra_asoc_vcm_platform_data *pdata;
	int ret;
	int i;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -EINVAL;
	}

	machine = kzalloc(sizeof(struct tegra_vcm), GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_vcm struct\n");
		return -ENOMEM;
	}

	machine->pdata = pdata;

	/* The codec driver and codec dai have to come from the system
	 * level board configuration file
	 * */
	for (i = 0; i < ARRAY_SIZE(tegra_vcm_dai_link); i++) {
		tegra_vcm_dai_link[i].codec_name =
				pdata->codec_info[i].codec_name;
		tegra_vcm_dai_link[i].cpu_dai_name =
				pdata->codec_info[i].cpu_dai_name;
		tegra_vcm_dai_link[i].codec_dai_name =
				pdata->codec_info[i].codec_dai_name;
		tegra_vcm_dai_link[i].name =
				pdata->codec_info[i].name;
		if (pdata->codec_info[i].pcm_driver)
			tegra_vcm_dai_link[i].platform_name =
				pdata->codec_info[i].pcm_driver;
	}

	ret = tegra_asoc_utils_init(&machine->util_data, &pdev->dev, card);
	if (ret)
		goto err_free_machine;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
	}

	if (!card->instantiated) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_unregister_card;
	}

	return 0;

err_unregister_card:
	snd_soc_unregister_card(card);
	tegra_asoc_utils_fini(&machine->util_data);
err_free_machine:
	kfree(machine);
	return ret;
}

static int __devexit tegra_vcm_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_vcm *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);
	tegra_asoc_utils_fini(&machine->util_data);
	kfree(machine);

	return 0;
}

static struct platform_driver tegra_vcm_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = tegra_vcm_driver_probe,
	.remove = __devexit_p(tegra_vcm_driver_remove),
};

static int __init tegra_vcm_modinit(void)
{
	return platform_driver_register(&tegra_vcm_driver);
}
module_init(tegra_vcm_modinit);

static void __exit tegra_vcm_modexit(void)
{
	platform_driver_unregister(&tegra_vcm_driver);
}
module_exit(tegra_vcm_modexit);

MODULE_AUTHOR("Nitin Pai <npai@nvidia.com>");
MODULE_DESCRIPTION("Tegra+VCM machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
