// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
/* alsa sound header */
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "mtk-sp-common.h"
#include "mtk-sp-spk-amp.h"
#if defined(CONFIG_SND_SOC_RT5509)
#include "../../codecs/rt5509.h"
#endif
#ifdef CONFIG_SND_SOC_MT6660
#include "../../codecs/mt6660.h"
#endif /* CONFIG_SND_SOC_MT6660 */

#define MTK_SPK_NAME "Speaker Codec"
#define MTK_SPK_REF_NAME "Speaker Codec Ref"
static unsigned int mtk_spk_type;
static int mtk_spk_i2s_out, mtk_spk_i2s_in;
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

int mtk_spk_get_i2s_out_type(void)
{
	return mtk_spk_i2s_out;
}
EXPORT_SYMBOL(mtk_spk_get_i2s_out_type);

int mtk_spk_get_i2s_in_type(void)
{
	return mtk_spk_i2s_in;
}
EXPORT_SYMBOL(mtk_spk_get_i2s_in_type);

int mtk_spk_update_dai_link(struct snd_soc_card *card,
			    struct platform_device *pdev,
			    const struct snd_soc_ops *i2s_ops)
{
	int ret, i;
	int spk_ref_dai_link_idx = -1;
	int spk_dai_link_idx = -1;
	int i2s_mck;
	struct snd_soc_dai_link *dai_link;

	dev_info(&pdev->dev, "%s(), mtk_spk_type %d\n",
		 __func__, mtk_spk_type);

	/* get spk i2s out number */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "mtk_spk_i2s_out", &mtk_spk_i2s_out);
	if (ret) {
		mtk_spk_i2s_out = MTK_SPK_I2S_3;
		dev_err(&pdev->dev,
			"%s(), get mtk_spk_i2s_out fail, use defalut i2s3\n",
			__func__);
	}

	/* get spk i2s in number */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "mtk_spk_i2s_in", &mtk_spk_i2s_in);
	if (ret) {
		mtk_spk_i2s_in = MTK_SPK_I2S_0;
		dev_err(&pdev->dev,
			"%s(), get mtk_spk_i2s_in fail, use defalut i2s0\n",
			 __func__);
	}

	/* get spk i2s mck number */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "mtk_spk_i2s_mck", &i2s_mck);
	if (ret) {
		i2s_mck = MTK_SPK_I2S_TYPE_INVALID;
		dev_warn(&pdev->dev, "%s(), mtk_spk_i2s_mck no use\n",
			 __func__);
	}

	if (mtk_spk_i2s_out > MTK_SPK_I2S_TYPE_NUM ||
	    mtk_spk_i2s_in > MTK_SPK_I2S_TYPE_NUM) {
		dev_err(&pdev->dev, "%s(), get mtk spk i2s fail\n",
			__func__);
		return -ENODEV;
	}

	/* find dai link of i2s in and i2s out */
	for (i = 0; i < card->num_links; i++) {
		dai_link = &card->dai_link[i];

		if (spk_dai_link_idx < 0 &&
		    strcmp(dai_link->cpu_dai_name, "I2S1") == 0 &&
		    mtk_spk_i2s_out == MTK_SPK_I2S_1) {
			spk_dai_link_idx = i;
		} else if (spk_dai_link_idx < 0 &&
			   strcmp(dai_link->cpu_dai_name, "I2S3") == 0 &&
			   mtk_spk_i2s_out == MTK_SPK_I2S_3) {
			spk_dai_link_idx = i;
		} else if (spk_dai_link_idx < 0 &&
			   strcmp(dai_link->cpu_dai_name, "I2S5") == 0 &&
			   mtk_spk_i2s_out == MTK_SPK_I2S_5) {
			spk_dai_link_idx = i;
		}

		if (spk_ref_dai_link_idx < 0 &&
		    strcmp(dai_link->cpu_dai_name, "I2S0") == 0 &&
		    mtk_spk_i2s_in == MTK_SPK_I2S_0) {
			spk_ref_dai_link_idx = i;
		} else if (spk_ref_dai_link_idx < 0 &&
			   strcmp(dai_link->cpu_dai_name, "I2S2") == 0 &&
			   mtk_spk_i2s_in == MTK_SPK_I2S_2) {
			spk_ref_dai_link_idx = i;
		}

		if (spk_dai_link_idx >= 0 && spk_ref_dai_link_idx >= 0)
			break;
	}

	if (spk_dai_link_idx < 0 || spk_ref_dai_link_idx < 0) {
		dev_err(&pdev->dev,
			"%s(), i2s cpu dai name error, spk_dai_link_idx = %d, spk_ref_dai_link_idx = %d",
			__func__, spk_dai_link_idx, spk_ref_dai_link_idx);
		return -ENODEV;
	}

	/* update spk codec dai name and codec name */
	dai_link = &card->dai_link[spk_dai_link_idx];
	dai_link->name = MTK_SPK_NAME;
	dai_link->codec_dai_name =
		mtk_spk_list[mtk_spk_type].codec_dai_name;
	dai_link->codec_name =
		mtk_spk_list[mtk_spk_type].codec_name;
	dai_link->ignore_pmdown_time = 1;
	if (i2s_mck == mtk_spk_i2s_out)
		dai_link->ops = i2s_ops;

	dev_info(&pdev->dev,
		 "%s(), %s, codec dai name = %s, codec name = %s, cpu dai name: %s\n",
		 __func__, dai_link->name,
		 dai_link->codec_dai_name,
		 dai_link->codec_name,
		 dai_link->cpu_dai_name);

	dai_link = &card->dai_link[spk_ref_dai_link_idx];
	dai_link->name = MTK_SPK_REF_NAME;
	dai_link->codec_dai_name =
		mtk_spk_list[mtk_spk_type].codec_dai_name;
	dai_link->codec_name =
		mtk_spk_list[mtk_spk_type].codec_name;
	dai_link->ignore_pmdown_time = 1;
	if (i2s_mck == mtk_spk_i2s_in)
		dai_link->ops = i2s_ops;

	dev_info(&pdev->dev,
		 "%s(), %s, codec dai name = %s, codec name = %s, cpu dai name: %s\n",
		 __func__, dai_link->name,
		 dai_link->codec_dai_name,
		 dai_link->codec_name,
		 dai_link->cpu_dai_name);


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
