/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mfd/marimba.h>
#include <linux/mfd/timpani-audio.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/soc-dapm.h>
/* Debug purpose */
#include <linux/gpio.h>
#include <linux/clk.h>
#include <mach/mpp.h>
/* End of debug purpose */

#define ADIE_CODEC_MAX 2

struct adie_codec_register {
	u8 reg;
	u8 mask;
	u8 val;
};

static struct adie_codec_register dmic_on[] = {
	{0x80, 0x05, 0x05},
	{0x80, 0x05, 0x00},
	{0x83, 0x0C, 0x00},
	{0x8A, 0xF0, 0x30},
	{0x86, 0xFF, 0xAC},
	{0x87, 0xFF, 0xAC},
	{0x8A, 0xF0, 0xF0},
	{0x82, 0x1F, 0x1E},
	{0x83, 0x0C, 0x0C},
	{0x92, 0x3F, 0x21},
	{0x94, 0x3F, 0x24},
	{0xA3, 0x39, 0x01},
	{0xA8, 0x0F, 0x00},
	{0xAB, 0x3F, 0x00},
	{0x86, 0xFF, 0x00},
	{0x87, 0xFF, 0x00},
	{0x8A, 0xF0, 0xC0},
};

static struct adie_codec_register dmic_off[] = {
	{0x8A, 0xF0, 0xF0},
	{0x83, 0x0C, 0x00},
	{0x92, 0xFF, 0x00},
	{0x94, 0xFF, 0x1B},
};

static struct adie_codec_register spk_on[] = {
	{0x80, 0x02, 0x02},
	{0x80, 0x02, 0x00},
	{0x83, 0x03, 0x00},
	{0x8A, 0x0F, 0x03},
	{0xA3, 0x02, 0x02},
	{0x84, 0xFF, 0x00},
	{0x85, 0xFF, 0x00},
	{0x8A, 0x0F, 0x0C},
	{0x81, 0xFF, 0x0E},
	{0x83, 0x03, 0x03},
	{0x24, 0x6F, 0x6C},
	{0xB7, 0x01, 0x01},
	{0x31, 0x01, 0x01},
	{0x32, 0xF8, 0x08},
	{0x32, 0xF8, 0x48},
	{0x32, 0xF8, 0xF8},
	{0xE0, 0xFE, 0xAC},
	{0xE1, 0xFE, 0xAC},
	{0x3A, 0x24, 0x24},
	{0xE0, 0xFE, 0x3C},
	{0xE1, 0xFE, 0x3C},
	{0xE0, 0xFE, 0x1C},
	{0xE1, 0xFE, 0x1C},
	{0xE0, 0xFE, 0x10},
	{0xE1, 0xFE, 0x10},
};

static struct adie_codec_register spk_off[] = {
	{0x8A, 0x0F, 0x0F},
	{0xE0, 0xFE, 0x1C},
	{0xE1, 0xFE, 0x1C},
	{0xE0, 0xFE, 0x3C},
	{0xE1, 0xFE, 0x3C},
	{0xE0, 0xFC, 0xAC},
	{0xE1, 0xFC, 0xAC},
	{0x32, 0xF8, 0x00},
	{0x31, 0x05, 0x00},
	{0x3A, 0x24, 0x00},
};

static struct adie_codec_register spk_mute[] = {
	{0x84, 0xFF, 0xAC},
	{0x85, 0xFF, 0xAC},
	{0x8A, 0x0F, 0x0C},
};

static struct adie_codec_register spk_unmute[] = {
	{0x84, 0xFF, 0x00},
	{0x85, 0xFF, 0x00},
	{0x8A, 0x0F, 0x0C},
};

struct adie_codec_path {
	int rate; /* sample rate of path */
	u32 reg_owner;
};

struct timpani_drv_data { /* member undecided */
	struct snd_soc_codec codec;
	struct adie_codec_path path[ADIE_CODEC_MAX];
	u32 ref_cnt;
	struct marimba_codec_platform_data *codec_pdata;
};

static struct snd_soc_codec *timpani_codec;

enum /* regaccess blk id */
{
	RA_BLOCK_RX1 = 0,
	RA_BLOCK_RX2,
	RA_BLOCK_TX1,
	RA_BLOCK_TX2,
	RA_BLOCK_LB,
	RA_BLOCK_SHARED_RX_LB,
	RA_BLOCK_SHARED_TX,
	RA_BLOCK_TXFE1,
	RA_BLOCK_TXFE2,
	RA_BLOCK_PA_COMMON,
	RA_BLOCK_PA_EAR,
	RA_BLOCK_PA_HPH,
	RA_BLOCK_PA_LINE,
	RA_BLOCK_PA_AUX,
	RA_BLOCK_ADC,
	RA_BLOCK_DMIC,
	RA_BLOCK_TX_I2S,
	RA_BLOCK_DRV,
	RA_BLOCK_TEST,
	RA_BLOCK_RESERVED,
	RA_BLOCK_NUM,
};

enum /* regaccess onwer ID */
{
	RA_OWNER_NONE = 0,
	RA_OWNER_PATH_RX1,
	RA_OWNER_PATH_RX2,
	RA_OWNER_PATH_TX1,
	RA_OWNER_PATH_TX2,
	RA_OWNER_PATH_LB,
	RA_OWNER_DRV,
	RA_OWNER_NUM,
};

struct reg_acc_blk_cfg {
	u8 valid_owners[RA_OWNER_NUM];
};

struct timpani_regaccess {
	u8 reg_addr;
	u8 blk_mask[RA_BLOCK_NUM];
	u8 reg_mask;
	u8 reg_default;
};

static unsigned int timpani_codec_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	struct marimba *pdrv = codec->control_data;
	int rc;
	u8 val;

	rc = marimba_read(pdrv, reg, &val, 1);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to write reg %x\n", __func__, reg);
		return 0;
	}
	return val;
}

static int timpani_codec_write(struct snd_soc_codec *codec, unsigned int reg,
			unsigned int value)
{
	struct marimba *pdrv = codec->control_data;
	int rc;

	rc = marimba_write_bit_mask(pdrv, reg,  (u8 *)&value, 1, 0xFF);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to write reg %x\n", __func__, reg);
		return -EIO;
	}
	pr_debug("%s: write reg %x val %x\n", __func__, reg, value);
	return 0;
}

static void timpani_codec_bring_up(struct snd_soc_codec *codec)
{
	struct timpani_drv_data *timpani = snd_soc_codec_get_drvdata(codec);
	int rc;

	if (timpani->codec_pdata &&
	    timpani->codec_pdata->marimba_codec_power) {
		if (timpani->ref_cnt)
			return;
		/* Codec power up sequence */
		rc = timpani->codec_pdata->marimba_codec_power(1);
		if (rc)
			pr_err("%s: could not power up timpani "
				"codec\n", __func__);
		else {
			timpani_codec_write(codec, 0xFF, 0x08);
			timpani_codec_write(codec, 0xFF, 0x0A);
			timpani_codec_write(codec, 0xFF, 0x0E);
			timpani_codec_write(codec, 0xFF, 0x07);
			timpani_codec_write(codec, 0xFF, 0x17);
			timpani_codec_write(codec, TIMPANI_A_MREF, 0x22);
			msleep(15);
			timpani->ref_cnt++;
		}
	}
}

static void timpani_codec_bring_down(struct snd_soc_codec *codec)
{
	struct timpani_drv_data *timpani = snd_soc_codec_get_drvdata(codec);
	int rc;

	if (timpani->codec_pdata &&
	    timpani->codec_pdata->marimba_codec_power) {
		timpani->ref_cnt--;
		if (timpani->ref_cnt >= 1)
			return;
		timpani_codec_write(codec, TIMPANI_A_MREF, TIMPANI_MREF_POR);
		timpani_codec_write(codec, 0xFF, 0x07);
		timpani_codec_write(codec, 0xFF, 0x06);
		timpani_codec_write(codec, 0xFF, 0x0E);
		timpani_codec_write(codec, 0xFF, 0x08);
		rc = timpani->codec_pdata->marimba_codec_power(0);
		if (rc)
			pr_err("%s: could not power down timpani "
			"codec\n", __func__);
	}
}

static void timpani_dmic_config(struct snd_soc_codec *codec, int on)
{
	struct adie_codec_register *regs;
	int regs_sz, i;

	if (on) {
		regs = dmic_on;
		regs_sz = ARRAY_SIZE(dmic_on);
	} else {
		regs = dmic_off;
		regs_sz = ARRAY_SIZE(dmic_off);
	}

	for (i = 0; i < regs_sz; i++)
		timpani_codec_write(codec, regs[i].reg,
				(regs[i].mask & regs[i].val));
}

static void timpani_spk_config(struct snd_soc_codec *codec, int on)
{
	struct adie_codec_register *regs;
	int regs_sz, i;

	if (on) {
		regs = spk_on;
		regs_sz = ARRAY_SIZE(spk_on);
	} else {
		regs = spk_off;
		regs_sz = ARRAY_SIZE(spk_off);
	}

	for (i = 0; i < regs_sz; i++)
		timpani_codec_write(codec, regs[i].reg,
				(regs[i].mask & regs[i].val));
}

static int timpani_startup(struct snd_pcm_substream *substream,
		      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;

	pr_info("%s()\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_info("%s: playback\n", __func__);
		timpani_codec_bring_up(codec);
		timpani_spk_config(codec, 1);
	} else {
		pr_info("%s: Capture\n", __func__);
		timpani_codec_bring_up(codec);
		timpani_dmic_config(codec, 1);
	}
	return 0;
}

static void timpani_shutdown(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;

	pr_info("%s()\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		timpani_codec_bring_down(codec);
		timpani_spk_config(codec, 0);
	} else {
		timpani_codec_bring_down(codec);
		timpani_dmic_config(codec, 0);
	}
	return;
}

int digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct adie_codec_register *regs;
	int regs_sz, i;

	if (mute) {
		regs = spk_mute;
		regs_sz = ARRAY_SIZE(spk_mute);
	} else {
		regs = spk_unmute;
		regs_sz = ARRAY_SIZE(spk_unmute);
	}

	for (i = 0; i < regs_sz; i++) {
		timpani_codec_write(codec, regs[i].reg,
			(regs[i].mask & regs[i].val));
		msleep(10);
	}

	return 0;
}

static struct snd_soc_dai_ops timpani_dai_ops = {
	.startup	= timpani_startup,
	.shutdown	= timpani_shutdown,
};

struct snd_soc_dai timpani_codec_dai[] = {
	{
		.name = "TIMPANI Rx",
		.playback = {
			.stream_name = "Handset Playback",
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max =	96000,
			.rate_min =	8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &timpani_dai_ops,
	},
	{
		.name = "TIMPANI Tx",
		.capture = {
			.stream_name = "Handset Capture",
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.rate_max =	96000,
			.rate_min =	8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &timpani_dai_ops,
	}
};

static int timpani_soc_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;

	if (!timpani_codec) {
		dev_err(&pdev->dev, "core driver not yet probed\n");
		return -ENODEV;
	}

	socdev->card->codec = timpani_codec;
	codec = timpani_codec;

	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0)
		dev_err(codec->dev, "failed to create pcms\n");
	return ret;
}

/* power down chip */
static int timpani_soc_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	snd_soc_free_pcms(socdev);
	return 0;
}

struct snd_soc_codec_device soc_codec_dev_timpani = {
	.probe =	timpani_soc_probe,
	.remove =	timpani_soc_remove,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_timpani);

static int timpani_codec_probe(struct platform_device *pdev)
{
	struct snd_soc_codec *codec;
	struct timpani_drv_data *priv;

	pr_info("%s()\n", __func__);
	priv = kzalloc(sizeof(struct timpani_drv_data), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	codec = &priv->codec;
	snd_soc_codec_set_drvdata(codec, priv);
	priv->codec_pdata = pdev->dev.platform_data;

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->name = "TIMPANI";
	codec->owner = THIS_MODULE;
	codec->read = timpani_codec_read;
	codec->write = timpani_codec_write;
	codec->dai = timpani_codec_dai;
	codec->num_dai = ARRAY_SIZE(timpani_codec_dai);
	codec->control_data = platform_get_drvdata(pdev);
	timpani_codec = codec;

	snd_soc_register_dais(timpani_codec_dai, ARRAY_SIZE(timpani_codec_dai));
	snd_soc_register_codec(codec);

	return 0;
}

static struct platform_driver timpani_codec_driver = {
	.probe = timpani_codec_probe,
	.driver = {
		.name = "timpani_codec",
		.owner = THIS_MODULE,
	},
};

static int __init timpani_codec_init(void)
{
	return platform_driver_register(&timpani_codec_driver);
}

static void __exit timpani_codec_exit(void)
{
	platform_driver_unregister(&timpani_codec_driver);
}

module_init(timpani_codec_init);
module_exit(timpani_codec_exit);

MODULE_DESCRIPTION("Timpani codec driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
