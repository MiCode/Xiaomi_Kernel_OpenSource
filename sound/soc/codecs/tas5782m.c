/*
 * tas5782m.c - Driver for the TAS5782M Audio Amplifier
 *
 * Copyright (C) 2017 Texas Instruments Incorporated -  http://www.ti.com
 *
 * Author: Andy Liu <andy-liu@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>


#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>

#include "tas5782m.h"


#define TAS5872M_DRV_NAME    "tas5782m"

#define TAS5872M_RATES		(SNDRV_PCM_RATE_8000_96000)
#define TAS5782M_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE |\
	SNDRV_PCM_FMTBIT_S20_3LE |\
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

#define TAS5782M_REG_00      (0x00)
#define TAS5782M_REG_03      (0x03)
#define TAS5782M_REG_7F      (0x7F)
#define TAS5782M_REG_14      (0x14)
#define TAS5782M_REG_15      (0x15)
#define TAS5782M_REG_16      (0x16)
#define TAS5782M_REG_17      (0x17)

#define TAS5782M_REG_44      (0x44)
#define TAS5782M_REG_45      (0x45)
#define TAS5782M_REG_46      (0x46)
#define TAS5782M_REG_47      (0x47)
#define TAS5782M_REG_48      (0x48)
#define TAS5782M_REG_49      (0x49)
#define TAS5782M_REG_4A      (0x4A)
#define TAS5782M_REG_4B      (0x4B)

#define TAS5782M_PAGE_00     (0x00)
#define TAS5782M_PAGE_8C     (0x8C)
#define TAS5782M_PAGE_1E     (0x1E)
#define TAS5782M_PAGE_23     (0x23)

static const struct reg_sequence tas5782m_init_sequence[] = {
	{ 0x00, 0x00 },
	{ 0x7f, 0x00 },
	{ 0x02, 0x11 },
	{ 0x01, 0x11 },
	{ 0x00, 0x00 },
	{ 0x03, 0x11 },
	{ 0x2a, 0x00 },
	{ 0x25, 0x18 },
	{ 0x0d, 0x10 },
	{ 0x00, 0x00 },
	{ 0x14, 0x00 },
	{ 0x15, 0x00 },
	{ 0x16, 0x00 },
	{ 0x17, 0x01 },
	{ 0x00, 0x00 },
	{ 0x7f, 0x00 },
	{ 0x07, 0x00 },
	{ 0x08, 0x20 },
	{ 0x55, 0x07 },
	{ 0x3c, 0x01 },
	{ 0x3d, 0x4e },
	{ 0x28, 0x03 },
	{ 0x09, 0x00 },
	{ 0x00, 0x00 },
	{ 0x7f, 0x00 },
	{ 0x02, 0x00 },
	{ 0x03, 0x00 },
	{ 0x2a, 0x11 },
};

static const uint32_t tas5782m_volume[] = {
	0x07ECA9CD,
	0x07100C4D,
	0x064B6CAE,
	0x059C2F02,
	0x05000000,
	0x0474CD1B,
	0x03F8BD7A,
	0x038A2BAD,
	0x0327A01A,
	0x02CFCC01,
	0x02818508,
	0x023BC148,
	0x01FD93C2,
	0x01C62940,
	0x0194C584,
	0x0168C0C6,
	0x0141857F,
	0x011E8E6A,
	0x00FF64C1,
	0x00E39EA9,
	0x00CADDC8,
	0x00B4CE08,
	0x00A12478,
	0x008F9E4D,
	0x00800000,
	0x00721483,
	0x0065AC8C,
	0x005A9DF8,
	0x0050C336,
	0x0047FACD,
	0x004026E7,
	0x00392CEE,
	0x0032F52D,
	0x002D6A86,
	0x00287A27,
	0x00241347,
	0x002026F3,
	0x001CA7D7,
	0x00198A13,
	0x0016C311,
	0x00144961,
	0x0012149A,
	0x00101D3F,
	0x000E5CA1,
	0x000CCCCD,
	0x000B6873,
	0x000A2ADB,
	0x00090FCC,
	0x00081385,
	0x000732AE,
	0x00066A4A,
	0x0005B7B1,
	0x00051884,
	0x00048AA7,
	0x00040C37,
	0x00039B87,
	0x00033718,
	0x0002DD96,
	0x00028DCF,
	0x000246B5,
	0x00020756,
	0x0001CEDC,
	0x00019C86,
	0x00016FAA,
	0x000147AE,
	0x0001240C,
	0x00010449,
	0x0000E7FB,
	0x0000CEC1,
	0x0000B845,
	0x0000A43B,
	0x0000925F,
	0x00008274,
	0x00007444,
	0x0000679F,
	0x00005C5A,
	0x0000524F,
	0x0000495C,
	0x00004161,
	0x00003A45,
	0x000033EF,
	0x00002E49,
	0x00002941,
	0x000024C4,
	0x000020C5,
	0x00001D34,
	0x00001A07,
	0x00001733,
	0x000014AD,
	0x0000126D,
	0x0000106C,
	0x00000EA3,
	0x00000D0C,
	0x00000BA0,
	0x00000A5D,
	0x0000093C,
	0x0000083B,
	0x00000756,
	0x0000068A,
	0x000005D4,
	0x00000532,
	0x000004A1,
	0x00000420,
	0x000003AD,
	0x00000347,
	0x000002EC,
	0x0000029A,
	0x00000252,
	0x00000211,
	0x000001D8,
	0x000001A4,
	0x00000177,
	0x0000014E,
	0x0000012A,
	0x00000109,
	0x000000EC,
	0x000000D3,
	0x000000BC,
	0x000000A7,
	0x00000095,
	0x00000085,
	0x00000076,
	0x0000006A,
	0x0000005E,
	0x00000054,
	0x0000004B,
	0x00000043,
	0x0000003B,
	0x00000035,
	0x0000002F,
	0x0000002A,
	0x00000025,
	0x00000021,
	0x0000001E,
	0x0000001B,
};

struct tas5782m_priv {
	struct regmap *regmap;
	struct regulator *extamp_supply;

	int gpio_reset;
	int gpio_power;
	int volume_db;
};

const struct regmap_config tas5782m_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};
static inline int get_db(int index)
{
	/*input  : 0  -- 134*/
	/*output : 24 -- -110*/

	return (24 - index);
}

static inline unsigned int get_volume_index(int vol)
{
	/*input  : 24 -- -110*/
	/*output : 0  -- 134*/
	return (24 - vol);
}

static void tas5782m_set_volume(struct snd_soc_codec *codec, int db)
{
	unsigned int index;
	uint32_t volume_hex;
	uint8_t byte4;
	uint8_t byte3;
	uint8_t byte2;
	uint8_t byte1;

	index = get_volume_index(db);
	volume_hex = tas5782m_volume[index];

	byte4 = ((volume_hex >> 24) & 0xFF);
	byte3 = ((volume_hex >> 16) & 0xFF);
	byte2 = ((volume_hex >> 8)	& 0xFF);
	byte1 = ((volume_hex >> 0)	& 0xFF);
#if 1
	/*w 90 00 00*/
	snd_soc_write(codec, TAS5782M_REG_00, TAS5782M_PAGE_00);
	/*w 90 7f 8c*/
	snd_soc_write(codec, TAS5782M_REG_7F, TAS5782M_PAGE_8C);
	/*w 90 00 1e*/
	snd_soc_write(codec, TAS5782M_REG_00, TAS5782M_PAGE_1E);
	/*w 90 44 xx xx xx xx*/
	snd_soc_write(codec, TAS5782M_REG_44, byte4);
	snd_soc_write(codec, TAS5782M_REG_45, byte3);
	snd_soc_write(codec, TAS5782M_REG_46, byte2);
	snd_soc_write(codec, TAS5782M_REG_47, byte1);

	/*w 90 00 00*/
	snd_soc_write(codec, TAS5782M_REG_00, TAS5782M_PAGE_00);
	/*w 90 7f 8c*/
	snd_soc_write(codec, TAS5782M_REG_7F, TAS5782M_PAGE_8C);
	/*w 90 00 1e*/
	snd_soc_write(codec, TAS5782M_REG_00, TAS5782M_PAGE_1E);
	/*w 90 48 xx xx xx xx*/
	snd_soc_write(codec, TAS5782M_REG_48, byte4);
	snd_soc_write(codec, TAS5782M_REG_49, byte3);
	snd_soc_write(codec, TAS5782M_REG_4A, byte2);
	snd_soc_write(codec, TAS5782M_REG_4B, byte1);

	/*w 90 00 00*/
	snd_soc_write(codec, TAS5782M_REG_00, TAS5782M_PAGE_00);
	/*w 90 7f 8c*/
	snd_soc_write(codec, TAS5782M_REG_7F, TAS5782M_PAGE_8C);
	/*w 90 00 23*/
	snd_soc_write(codec, TAS5782M_REG_00, TAS5782M_PAGE_23);
	/*w 90 14 00 00 00 01*/
	snd_soc_write(codec, TAS5782M_REG_14, 0x00);
	snd_soc_write(codec, TAS5782M_REG_15, 0x00);
	snd_soc_write(codec, TAS5782M_REG_16, 0x00);
	snd_soc_write(codec, TAS5782M_REG_17, 0x01);
#endif
}

static int tas5782m_snd_probe(struct snd_soc_codec *codec)
{
	tas5782m_set_volume(codec, 1);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_tas5782m = {
	.probe = tas5782m_snd_probe,
};

static int tas5782m_mute(struct snd_soc_dai *dai, int mute)
{

	u8 reg3_value = 0;
	struct snd_soc_codec *codec = dai->codec;

	if (mute)
		reg3_value = 0x11;

	snd_soc_write(codec, TAS5782M_REG_00, 0x00);
	snd_soc_write(codec, TAS5782M_REG_7F, 0x00);
	snd_soc_write(codec, TAS5782M_REG_03, reg3_value);

	return 0;
}

static void tas5782m_reset(struct device *dev, struct tas5782m_priv *priv)
{
	if (gpio_is_valid(priv->gpio_reset)) {
		gpio_direction_output(priv->gpio_reset, 1);
		gpio_set_value(priv->gpio_reset, 0);
	}
}

static void tas5782m_power(struct device *dev,
		struct tas5782m_priv *priv, bool enable)
{
	if (gpio_is_valid(priv->gpio_power)) {
		if (enable)
			gpio_direction_output(priv->gpio_power, 1);
		else
			gpio_direction_output(priv->gpio_power, 0);
	}
}

static const struct snd_soc_dai_ops tas5782m_dai_ops = {
	.digital_mute = tas5782m_mute,
};

static struct snd_soc_dai_driver tas5782m_dai = {
	.name		= "tas5782m-amplifier",
	.playback	= {
		.stream_name	= "Playback",
		.channels_min	= 2,
		.channels_max	= 2,
		.rates		= TAS5872M_RATES,
		.formats	= TAS5782M_FORMATS,
	},
	.ops = &tas5782m_dai_ops,
};

static int tas5782m_probe(struct device *dev, struct regmap *regmap)
{
	struct tas5782m_priv *tas5782m;
	int ret;

	tas5782m = devm_kzalloc(dev, sizeof(struct tas5782m_priv), GFP_KERNEL);
	if (!tas5782m)
		return -ENOMEM;

	dev_set_drvdata(dev, tas5782m);
	tas5782m->regmap = regmap;
	tas5782m->volume_db = -30;

	if (!dev->of_node) {
		pr_debug("%s, cannot find dts node!\n", __func__);
		return ret;
	}

	dev_set_name(dev, "%s", TAS5872M_DRV_NAME);

	tas5782m->gpio_power = of_get_named_gpio(dev->of_node, "power_gpio", 0);
	if (!gpio_is_valid(tas5782m->gpio_power))
		pr_debug("%s get invalid tas5782m_power_gpio %d\n",
				__func__, tas5782m->gpio_power);

	tas5782m_power(dev, tas5782m, 1);

	tas5782m->gpio_reset = of_get_named_gpio(dev->of_node, "rst_gpio", 0);
	if (!gpio_is_valid(tas5782m->gpio_reset))
		pr_debug("%s get invalid tas5782m_rst_gpio %d\n",
				__func__, tas5782m->gpio_reset);

	tas5782m_reset(dev, tas5782m);


	ret = regmap_register_patch(regmap, tas5782m_init_sequence,
		ARRAY_SIZE(tas5782m_init_sequence));
	if (ret != 0) {
		pr_debug("Failed to initialize TAS5782M: %d\n", ret);
		goto err;
	}

	ret = snd_soc_register_codec(dev,
					&soc_codec_tas5782m,
					&tas5782m_dai,
					1);
	if (ret != 0) {
		pr_debug("Failed to register CODEC: %d\n", ret);
		goto err;
	}

	return 0;

err:
	return ret;

}

static int tas5782m_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config = tas5782m_regmap;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return tas5782m_probe(&i2c->dev, regmap);
}

int tas5782m_speaker_amp_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	return tas5782m_i2c_probe(i2c, id);
}
EXPORT_SYMBOL(tas5782m_speaker_amp_probe);

static int tas5782m_remove(struct device *dev)
{
	snd_soc_unregister_codec(dev);

	return 0;
}

static int tas5782m_i2c_remove(struct i2c_client *i2c)
{
	tas5782m_remove(&i2c->dev);

	return 0;
}

int tas5782m_speaker_amp_remove(struct i2c_client *i2c)
{
	return tas5782m_i2c_remove(i2c);
}
EXPORT_SYMBOL(tas5782m_speaker_amp_remove);

static const struct i2c_device_id tas5782m_i2c_id[] = {
	{ "tas5782m", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas5782m_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id tas5782m_of_match[] = {
	{ .compatible = "ti,tas5782m", },
	{ }
};
MODULE_DEVICE_TABLE(of, tas5782m_of_match);
#endif

static struct i2c_driver tas5782m_i2c_driver = {
	.probe	= tas5782m_i2c_probe,
	.remove	= tas5782m_i2c_remove,
	.id_table	= tas5782m_i2c_id,
	.driver	= {
		.name	= TAS5872M_DRV_NAME,
		.of_match_table	= tas5782m_of_match,
	},
};

module_i2c_driver(tas5782m_i2c_driver);

MODULE_AUTHOR("Andy Liu <andy-liu@ti.com>");
MODULE_DESCRIPTION("TAS5782M Audio Amplifier Driver");
MODULE_LICENSE("GPL");
