/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
/* alsa sound header */
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "mtk-soc-speaker-amp.h"
#if defined(CONFIG_SND_SOC_RT5509)
#include "../../codecs/rt5509.h"
#endif
#ifdef CONFIG_SND_SOC_MT6660
#include "../../codecs/mt6660.h"
#endif /* CONFIG_SND_SOC_MT6660 */

static unsigned int mtk_spk_type;
static struct mtk_spk_i2c_ctrl mtk_spk_list[MTK_SPK_TYPE_NUM] = {
	[MTK_SPK_NOT_SMARTPA] = {
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
#if defined(CONFIG_SND_SOC_RT5509)
	[MTK_SPK_RICHTEK_RT5509] = {
		.i2c_probe = rt5509_i2c_probe,
		.i2c_remove = rt5509_i2c_remove,
		.i2c_shutdown = rt5509_i2c_shutdown,
		.codec_dai_name = "rt5509-aif1",
		.codec_name = "RT5509_MT_0",
	},
#endif
#ifdef CONFIG_SND_SOC_MT6660
	[MTK_SPK_MEDIATEK_MT6660] = {
		.i2c_probe = mt6660_i2c_probe,
		.i2c_remove = mt6660_i2c_remove,
		.codec_dai_name = "mt6660-aif",
		.codec_name = "MT6660_MT_0",
	},
#endif /* CONFIG_SND_SOC_MT6660 */
};

static int mtk_spk_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	int i, ret = 0;

	dev_info(&client->dev, "%s()\n", __func__);

	mtk_spk_type = MTK_SPK_NOT_SMARTPA;
	for (i = 0; i < MTK_SPK_TYPE_NUM; i++) {
		if (!mtk_spk_list[i].i2c_probe)
			continue;

		ret = mtk_spk_list[i].i2c_probe(client, id);
		if (ret)
			continue;

		mtk_spk_type = i;
		break;
	}

	return ret;
}

static int mtk_spk_i2c_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "%s()\n", __func__);

	if (mtk_spk_list[mtk_spk_type].i2c_remove)
		mtk_spk_list[mtk_spk_type].i2c_remove(client);

	return 0;
}

static void mtk_spk_i2c_shutdown(struct i2c_client *client)
{
	dev_info(&client->dev, "%s()\n", __func__);

	if (mtk_spk_list[mtk_spk_type].i2c_shutdown)
		mtk_spk_list[mtk_spk_type].i2c_shutdown(client);
}

int mtk_spk_get_type(void)
{
	return mtk_spk_type;
}
EXPORT_SYMBOL(mtk_spk_get_type);

int mtk_spk_update_dai_link(struct snd_soc_dai_link *mtk_spk_dai_link,
			    struct platform_device *pdev)
{
	struct snd_soc_dai_link *dai_link = mtk_spk_dai_link;

	dev_info(&pdev->dev, "%s(), mtk_spk_type %d\n",
		 __func__, mtk_spk_type);

	/* update spk codec dai name and codec name */
	dai_link[0].codec_dai_name =
		mtk_spk_list[mtk_spk_type].codec_dai_name;
	dai_link[0].codec_name =
		mtk_spk_list[mtk_spk_type].codec_name;
	dai_link[0].ignore_pmdown_time = 1;
	dev_info(&pdev->dev,
		 "%s(), %s, codec dai name = %s, codec name = %s\n",
		 __func__, dai_link[0].name,
		 dai_link[0].codec_dai_name,
		 dai_link[0].codec_name);

	return 0;
}
EXPORT_SYMBOL(mtk_spk_update_dai_link);


static const struct i2c_device_id mtk_spk_i2c_id[] = {
	{ "speaker_amp", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, mtk_spk_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id mtk_spk_match_table[] = {
	{.compatible = "mediatek,speaker_amp",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_spk_match_table);
#endif /* #ifdef CONFIG_OF */

static struct i2c_driver mtk_spk_i2c_driver = {
	.driver = {
		.name = "speaker_amp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_spk_match_table),
	},
	.probe = mtk_spk_i2c_probe,
	.remove = mtk_spk_i2c_remove,
	.shutdown = mtk_spk_i2c_shutdown,
	.id_table = mtk_spk_i2c_id,
};

module_i2c_driver(mtk_spk_i2c_driver);

MODULE_DESCRIPTION("Mediatek speaker amp register driver");
MODULE_AUTHOR("Shane Chien <shane.chien@mediatek.com>");
MODULE_LICENSE("GPL v2");
