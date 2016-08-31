#include <asm/mach-types.h>

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
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

#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"
#include <linux/tfa9887.h>
#include "tegra30_ahub.h"
#include "tegra30_i2s.h"
#include "tegra_dmic.h"

#define DRV_NAME "tegra-snd-dmic_rev1.0"

#define DAI_LINK_CAPTURE_FRONT	0
#define DAI_LINK_CAPTURE_BACK	1


static int tegra_dmic_rev1_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_dmic_cif dmic_cif_info = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	struct tegra_dmic_cif dmic_rx_cif_info = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	int err;
	u32 sampling_rate;
	u32 channels;
	u32 data_format;
	u32 clock_freq;

	sampling_rate = params_rate(params);
	channels = params_channels(params);
	data_format = params_format(params);

	if (sampling_rate == 8000)
		clock_freq = sampling_rate * 128;/* OSR = 128 */
	else
		clock_freq = sampling_rate * 64;/* OSR = 64 */

	err = snd_soc_dai_set_sysclk(cpu_dai, 0, clock_freq,
			SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai clock not set\n");
		return err;
	}

	/*
	 * Now set all cif ports.
	 */

	if (data_format == SNDRV_PCM_FORMAT_S16_LE) {
		dmic_cif_info.audio_bits = (16 >> 2) - 1;
		dmic_cif_info.client_bits = (16 >> 2) - 1;
	} else if (data_format == SNDRV_PCM_FORMAT_S20_3LE) {
		dmic_cif_info.audio_bits = (32 >> 2) - 1;
		dmic_cif_info.client_bits = (20 >> 2) - 1;
	} else if (data_format == SNDRV_PCM_FORMAT_S24_LE) {
		dmic_cif_info.audio_bits = (32 >> 2) - 1;
		dmic_cif_info.client_bits = (24 >> 2) - 1;
	}

	/*
	 * Always operate DMIC in stereo mode. Conversion to mono,
	 * can be done at later stage.
	 */
	dmic_cif_info.audio_channels = 2;
	dmic_cif_info.client_channels = 2;
	tegra_dmic_set_acif(cpu_dai, &dmic_cif_info);

	dmic_rx_cif_info.audio_bits = dmic_cif_info.audio_bits;
	dmic_rx_cif_info.client_bits = dmic_cif_info.audio_bits;
	dmic_rx_cif_info.audio_channels = 2;
	dmic_rx_cif_info.client_channels = channels;
	tegra_dmic_set_rx_cif(cpu_dai, &dmic_rx_cif_info, data_format);

	return 0;
}

static int tegra_dmic_rev1_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static int tegra_dmic_rev1_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static void tegra_dmic_rev1_shutdown(
		struct snd_pcm_substream *substream)
{
}

static struct snd_soc_ops tegra_dmic_rev1_ops = {
	.hw_params = tegra_dmic_rev1_hw_params,
	.hw_free = tegra_dmic_rev1_hw_free,
	.startup = tegra_dmic_rev1_startup,
	.shutdown = tegra_dmic_rev1_shutdown,
};

static const struct snd_soc_dapm_widget dmic_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Digital Mic", NULL),
	SND_SOC_DAPM_MICBIAS("Digital Mic Bias", SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_dapm_route dmic_audio_map[] = {
	{"DMic", NULL, "Digital Mic"},
	{"Digital Mic", NULL, "Digital Mic Bias"},
};

static int tegra_dmic_rev1_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;

	ret = snd_soc_dapm_new_controls(dapm, dmic_dapm_widgets,
		ARRAY_SIZE(dmic_dapm_widgets));
	if (ret)
		return ret;

	return snd_soc_dapm_add_routes(dapm, dmic_audio_map,
		ARRAY_SIZE(dmic_audio_map));
}

static struct snd_soc_dai_link tegra_dmic_rev1_dai[TEGRA_DMIC_COUNT] = {
	[DAI_LINK_CAPTURE_FRONT] = {
		.name = "DMic Front",
		.stream_name = "DMIC_FRONT_CAPTURE",
		.codec_name = "dmic-codec.0",
		.platform_name = "tegra-dmic.0",
		.codec_dai_name = "dmic-hifi",
		.cpu_dai_name = "tegra-dmic.0",
		.init = tegra_dmic_rev1_init,
		.ops = &tegra_dmic_rev1_ops,
	},
	[DAI_LINK_CAPTURE_BACK] = {
		.name = "DMic Back",
		.stream_name = "DMIC_BACK_CAPTURE",
		.codec_name = "dmic-codec.0",
		.platform_name = "tegra-dmic.1",
		.codec_dai_name = "dmic-hifi",
		.cpu_dai_name = "tegra-dmic.1",
		.init = tegra_dmic_rev1_init,
		.ops = &tegra_dmic_rev1_ops,
	}
};

static struct snd_soc_card snd_soc_tegra_dmic_rev1 = {
	.name = "tegra-dmic-rev1.0",
	.owner = THIS_MODULE,
	.dai_link = tegra_dmic_rev1_dai,
	.num_links = TEGRA_DMIC_COUNT,
};

static __devinit int tegra_dmic_rev1_driver_probe(
		struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_dmic_rev1;
	int ret;
	card->dev = &pdev->dev;

	platform_set_drvdata(pdev, card);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_unregister_card;
	}

	if (!card->instantiated) {
		dev_err(&pdev->dev, "No DMIC codec\n");
		goto err_unregister_card;
	}

	return 0;

err_unregister_card:
	snd_soc_unregister_card(card);
	return ret;
}

static int __devexit tegra_dmic_rev1_driver_remove(
		struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);
	return 0;
}

static struct platform_driver tegra_dmic_rev1_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tegra_dmic_rev1_driver_probe,
	.remove = __devexit_p(tegra_dmic_rev1_driver_remove),
};

static int __init tegra_dmic_rev1_modinit(void)
{
	return platform_driver_register(&tegra_dmic_rev1_driver);
}
module_init(tegra_dmic_rev1_modinit);

static void __exit tegra_dmic_rev1_modexit(void)
{
	platform_driver_unregister(&tegra_dmic_rev1_driver);
}
module_exit(tegra_dmic_rev1_modexit);

MODULE_AUTHOR("Ankit Gupta <ankitgupta@nvidia.com>");
MODULE_DESCRIPTION("Tegra + DMicRev1.0 machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);

