/*
 * Driver for the TAS5805M Audio Amplifier
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/

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
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tas5805m.h"

#define TAS5805M_DRV_NAME    "TAS5805m"

#define TAS5805M_RATES (SNDRV_PCM_RATE_44100 | \
		SNDRV_PCM_RATE_48000 | \
		SNDRV_PCM_RATE_88200 | \
		SNDRV_PCM_RATE_96000)

#define TAS5805M_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
		SNDRV_PCM_FMTBIT_S20_3LE | \
		SNDRV_PCM_FMTBIT_S24_LE | \
		SNDRV_PCM_FMTBIT_S32_LE)

#define TAS5805M_REG_00      (0x00)
#define TAS5805M_REG_03      (0x03)
#define TAS5805M_REG_0C      (0x0c)
#define TAS5805M_REG_0D      (0x0d)
#define TAS5805M_REG_0E      (0x0e)
#define TAS5805M_REG_0F      (0x0f)
#define TAS5805M_REG_10      (0x10)
#define TAS5805M_REG_11      (0x11)
#define TAS5805M_REG_12      (0x12)
#define TAS5805M_REG_13      (0x13)
#define TAS5805M_REG_35      (0x35)
#define TAS5805M_REG_4C      (0x4C)

#define TAS5805M_REG_7F      (0x7f)

#define TAS5805M_PAGE_00     (0x00)
#define TAS5805M_PAGE_0B     (0x0b)

#define TAS5805M_BOOK_00     (0x00)
#define TAS5805M_BOOK_8C     (0x8c)

#define TAS5805M_VOLUME_MAX  (158)
#define TAS5805M_VOLUME_MIN  (0)

#define TAS5805M_REG_00      (0x00)
#define TAS5805M_REG_03      (0x03)
#define TAS5805M_REG_24      (0x24)
#define TAS5805M_REG_25      (0x25)
#define TAS5805M_REG_26      (0x26)
#define TAS5805M_REG_27      (0x27)
#define TAS5805M_REG_28      (0x28)
#define TAS5805M_REG_29      (0x29)
#define TAS5805M_REG_2A      (0x2a)
#define TAS5805M_REG_2B      (0x2b)
#define TAS5805M_REG_35      (0x35)
#define TAS5805M_REG_7E      (0x7e)
#define TAS5805M_REG_7F      (0x7f)

#define TAS5805M_PAGE_00     (0x00)
#define TAS5805M_PAGE_2A     (0x2a)

#define TAS5805M_BOOK_00     (0x00)
#define TAS5805M_BOOK_8C     (0x8c)

#define RETRIES              (3)
#define CONFIG_CRC_CHECKSUM  (0x9c)

static void TAS5805m_switch_book(struct snd_soc_component *component,
		unsigned int book_id, unsigned int page_id)
{
	//w 98 00 00 switch to page 0
	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	//w 98 7f <book_id> switch to book |book_id|
	snd_soc_component_write(component, TAS5805M_REG_7F, book_id);
	//w 98 00 <page_id> switch to page |page_id|
	snd_soc_component_write(component, TAS5805M_REG_00, page_id);
}

const uint32_t tas5805m_volume[] = {
	0x0000001B,    //0, -110dB
	0x0000001E,    //1, -109dB
	0x00000021,    //2, -108dB
	0x00000025,    //3, -107dB
	0x0000002A,    //4, -106dB
	0x0000002F,    //5, -105dB
	0x00000035,    //6, -104dB
	0x0000003B,    //7, -103dB
	0x00000043,    //8, -102dB
	0x0000004B,    //9, -101dB
	0x00000054,    //10, -100dB
	0x0000005E,    //11, -99dB
	0x0000006A,    //12, -98dB
	0x00000076,    //13, -97dB
	0x00000085,    //14, -96dB
	0x00000095,    //15, -95dB
	0x000000A7,    //16, -94dB
	0x000000BC,    //17, -93dB
	0x000000D3,    //18, -92dB
	0x000000EC,    //19, -91dB
	0x00000109,    //20, -90dB
	0x0000012A,    //21, -89dB
	0x0000014E,    //22, -88dB
	0x00000177,    //23, -87dB
	0x000001A4,    //24, -86dB
	0x000001D8,    //25, -85dB
	0x00000211,    //26, -84dB
	0x00000252,    //27, -83dB
	0x0000029A,    //28, -82dB
	0x000002EC,    //29, -81dB
	0x00000347,    //30, -80dB
	0x000003AD,    //31, -79dB
	0x00000420,    //32, -78dB
	0x000004A1,    //33, -77dB
	0x00000532,    //34, -76dB
	0x000005D4,    //35, -75dB
	0x0000068A,    //36, -74dB
	0x00000756,    //37, -73dB
	0x0000083B,    //38, -72dB
	0x0000093C,    //39, -71dB
	0x00000A5D,    //40, -70dB
	0x00000BA0,    //41, -69dB
	0x00000D0C,    //42, -68dB
	0x00000EA3,    //43, -67dB
	0x0000106C,    //44, -66dB
	0x0000126D,    //45, -65dB
	0x000014AD,    //46, -64dB
	0x00001733,    //47, -63dB
	0x00001A07,    //48, -62dB
	0x00001D34,    //49, -61dB
	0x000020C5,    //50, -60dB
	0x000024C4,    //51, -59dB
	0x00002941,    //52, -58dB
	0x00002E49,    //53, -57dB
	0x000033EF,    //54, -56dB
	0x00003A45,    //55, -55dB
	0x00004161,    //56, -54dB
	0x0000495C,    //57, -53dB
	0x0000524F,    //58, -52dB
	0x00005C5A,    //59, -51dB
	0x0000679F,    //60, -50dB
	0x00007444,    //61, -49dB
	0x00008274,    //62, -48dB
	0x0000925F,    //63, -47dB
	0x0000A43B,    //64, -46dB
	0x0000B845,    //65, -45dB
	0x0000CEC1,    //66, -44dB
	0x0000E7FB,    //67, -43dB
	0x00010449,    //68, -42dB
	0x0001240C,    //69, -41dB
	0x000147AE,    //70, -40dB
	0x00016FAA,    //71, -39dB
	0x00019C86,    //72, -38dB
	0x0001CEDC,    //73, -37dB
	0x00020756,    //74, -36dB
	0x000246B5,    //75, -35dB
	0x00028DCF,    //76, -34dB
	0x0002DD96,    //77, -33dB
	0x00033718,    //78, -32dB
	0x00039B87,    //79, -31dB
	0x00040C37,    //80, -30dB
	0x00048AA7,    //81, -29dB
	0x00051884,    //82, -28dB
	0x0005B7B1,    //83, -27dB
	0x00066A4A,    //84, -26dB
	0x000732AE,    //85, -25dB
	0x00081385,    //86, -24dB
	0x00090FCC,    //87, -23dB
	0x000A2ADB,    //88, -22dB
	0x000B6873,    //89, -21dB
	0x000CCCCD,    //90, -20dB
	0x000E5CA1,    //91, -19dB
	0x00101D3F,    //92, -18dB
	0x0012149A,    //93, -17dB
	0x00144961,    //94, -16dB
	0x0016C311,    //95, -15dB
	0x00198A13,    //96, -14dB
	0x001CA7D7,    //97, -13dB
	0x002026F3,    //98, -12dB
	0x00241347,    //99, -11dB
	0x00287A27,    //100, -10dB
	0x002D6A86,    //101, -9dB
	0x0032F52D,    //102, -8dB
	0x00392CEE,    //103, -7dB
	0x004026E7,    //104, -6dB
	0x0047FACD,    //105, -5dB
	0x0050C336,    //106, -4dB
	0x005A9DF8,    //107, -3dB
	0x0065AC8C,    //108, -2dB
	0x00721483,    //109, -1dB
	0x00800000,    //110, 0dB
	0x008F9E4D,    //111, 1dB
	0x00A12478,    //112, 2dB
	0x00B4CE08,    //113, 3dB
	0x00CADDC8,    //114, 4dB
	0x00E39EA9,    //115, 5dB
	0x00FF64C1,    //116, 6dB
	0x011E8E6A,    //117, 7dB
	0x0141857F,    //118, 8dB
	0x0168C0C6,    //119, 9dB
	0x0194C584,    //120, 10dB
	0x01C62940,    //121, 11dB
	0x01FD93C2,    //122, 12dB
	0x023BC148,    //123, 13dB
	0x02818508,    //124, 14dB
	0x02CFCC01,    //125, 15dB
	0x0327A01A,    //126, 16dB
	0x038A2BAD,    //127, 17dB
	0x03F8BD7A,    //128, 18dB
	0x0474CD1B,    //129, 19dB
	0x05000000,    //130, 20dB
	0x059C2F02,    //131, 21dB
	0x064B6CAE,    //132, 22dB
	0x07100C4D,    //133, 23dB
	0x07ECA9CD,    //134, 24dB
	0x08E43299,    //135, 25dB
	0x09F9EF8E,    //136, 26dB
	0x0B319025,    //137, 27dB
	0x0C8F36F2,    //138, 28dB
	0x0E1787B8,    //139, 29dB
	0x0FCFB725,    //140, 30dB
	0x11BD9C84,    //141, 31dB
	0x13E7C594,    //142, 32dB
	0x16558CCB,    //143, 33dB
	0x190F3254,    //144, 34dB
	0x1C1DF80E,    //145, 35dB
	0x1F8C4107,    //146, 36dB
	0x2365B4BF,    //147, 37dB
	0x27B766C2,    //148, 38dB
	0x2C900313,    //149, 39dB
	0x32000000,    //150, 40dB
	0x3819D612,    //151, 41dB
	0x3EF23ECA,    //152, 42dB
	0x46A07B07,    //153, 43dB
	0x4F3EA203,    //154, 44dB
	0x58E9F9F9,    //155, 45dB
	0x63C35B8E,    //156, 46dB
	0x6FEFA16D,    //157, 47dB
	0x7D982575,    //158, 48dB
};

static cfg_reg tas5805m_reset_checksum_sequence[] = {
	{ 0x00, 0x00 },
	{ 0x7f, 0x8c },
	{ 0x7e, 0x00 },
};

static cfg_reg tas5805m_check_checksum_sequence[] = {
	{ 0x00, 0x00 },
	{ 0x7f, 0x8c },
};

struct TAS5805m_priv {
	struct i2c_client *client;

	struct regmap *regmap;

	struct mutex lock;

	int software_volume;

	int pdn_gpio;

	int hpd_gpio;
	int irq;

	int init_done;
};

const struct regmap_config TAS5805m_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static void tas5805m_i2c_mute(struct i2c_client *client, int mute)
{
	uint8_t buf[2];

	buf[0] = TAS5805M_REG_00;
	buf[1] = TAS5805M_PAGE_00;
	i2c_master_send(client, buf, 2);

	buf[0] = TAS5805M_REG_7F;
	buf[1] = TAS5805M_BOOK_00;
	i2c_master_send(client, buf, 2);

	buf[0] = TAS5805M_REG_00;
	buf[1] = TAS5805M_PAGE_00;
	i2c_master_send(client, buf, 2);

	if (mute) {
		buf[0] = TAS5805M_REG_03;
		buf[1] = 0x0B;
		i2c_master_send(client, buf, 2);

		buf[0] = TAS5805M_REG_35;
		buf[1] = 0x00;
		i2c_master_send(client, buf, 2);
	} else {
		buf[0] = TAS5805M_REG_03;
		buf[1] = 0x03;
		i2c_master_send(client, buf, 2);

		buf[0] = TAS5805M_REG_35;
		buf[1] = 0x11;
		i2c_master_send(client, buf, 2);
	}
}

static irqreturn_t tas5805m_hpd_thread_handler(int irq, void *p)
{
	struct TAS5805m_priv *pdata = (struct TAS5805m_priv *)p;

	tas5805m_i2c_mute(pdata->client,
			(gpio_get_value(pdata->hpd_gpio) == 1));
	return IRQ_HANDLED;
}

static int TAS5805m_vol_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type   = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access = (SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count  = 1;

	uinfo->value.integer.min  = TAS5805M_VOLUME_MIN;
	uinfo->value.integer.max  = TAS5805M_VOLUME_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int TAS5805m_vol_locked_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct TAS5805m_priv *priv = snd_soc_component_get_drvdata(component);

	mutex_lock(&priv->lock);
	ucontrol->value.integer.value[0] = priv->software_volume;
	mutex_unlock(&priv->lock);

	return 0;
}

static inline int get_volume_index(int vol)
{
	int index;

	index = vol;

	if (index < TAS5805M_VOLUME_MIN)
		index = TAS5805M_VOLUME_MIN;

	if (index > TAS5805M_VOLUME_MAX)
		index = TAS5805M_VOLUME_MAX;

	return index;
}

static void tas5805m_set_volume(struct snd_soc_component *component, int vol)
{
	unsigned int index;
	uint32_t volume_hex;
	uint8_t byte4;
	uint8_t byte3;
	uint8_t byte2;
	uint8_t byte1;

	index = get_volume_index(vol);
	volume_hex = tas5805m_volume[index];

	byte4 = ((volume_hex >> 24) & 0xFF);
	byte3 = ((volume_hex >> 16) & 0xFF);
	byte2 = ((volume_hex >> 8)	& 0xFF);
	byte1 = ((volume_hex >> 0)	& 0xFF);

	//w 58 00 00
	//snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	//w 58 7f 8c
	//snd_soc_write(codec, TAS5805M_REG_7F, TAS5805M_BOOK_8C);
	//w 58 00 2a
	//snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_2A);
	TAS5805m_switch_book(component, TAS5805M_BOOK_8C,
			TAS5805M_PAGE_2A);
	//w 58 24 xx xx xx xx
	snd_soc_component_write(component, TAS5805M_REG_24, byte4);
	snd_soc_component_write(component, TAS5805M_REG_25, byte3);
	snd_soc_component_write(component, TAS5805M_REG_26, byte2);
	snd_soc_component_write(component, TAS5805M_REG_27, byte1);
	//w 58 28 xx xx xx xx
	snd_soc_component_write(component, TAS5805M_REG_28, byte4);
	snd_soc_component_write(component, TAS5805M_REG_29, byte3);
	snd_soc_component_write(component, TAS5805M_REG_2A, byte2);
	snd_soc_component_write(component, TAS5805M_REG_2B, byte1);
}

static int TAS5805m_vol_locked_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct TAS5805m_priv *priv = snd_soc_component_get_drvdata(component);

	if (priv->init_done) {
		mutex_lock(&priv->lock);

		priv->software_volume = ucontrol->value.integer.value[0];
		tas5805m_set_volume(component, priv->software_volume);

		mutex_unlock(&priv->lock);
	}

	return 0;
}

static int TAS5805m_get_volsw(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *uinfo)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct TAS5805m_priv *priv = snd_soc_component_get_drvdata(component);
	int ret = 0;

	mutex_lock(&priv->lock);
	TAS5805m_switch_book(component, TAS5805M_BOOK_00,
			TAS5805M_PAGE_00);
	ret = snd_soc_get_volsw(kcontrol, uinfo);
	mutex_unlock(&priv->lock);
	return ret;
}

static int TAS5805m_put_volsw(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *uinfo)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct TAS5805m_priv *priv = snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (priv->init_done) {
		mutex_lock(&priv->lock);
		TAS5805m_switch_book(component, TAS5805M_BOOK_00,
				TAS5805M_PAGE_00);
		ret = snd_soc_put_volsw(kcontrol, uinfo);
		mutex_unlock(&priv->lock);
	}
	return ret;
}

static DECLARE_TLV_DB_SCALE(TAS5805m_digital_tlv, -10350, 50, 1 /* mute */);

static const struct snd_kcontrol_new TAS5805m_snd_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name  = "Master Playback Volume",
		.info  = TAS5805m_vol_info,
		.get   = TAS5805m_vol_locked_get,
		.put   = TAS5805m_vol_locked_put,
	},
	SOC_SINGLE_EXT_TLV("Digital Volume", TAS5805M_REG_4C,
			0, 0xFF, 1 /* invert */,
			TAS5805m_get_volsw, TAS5805m_put_volsw,
			TAS5805m_digital_tlv)
};

static int TAS5805m_snd_probe(struct snd_soc_component *component)
{
	return 0;
}

static struct snd_soc_component_driver soc_component_TAS5805m = {
	.probe = TAS5805m_snd_probe,
};

static int TAS5805m_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	u8 reg03_value = 0;
	u8 reg35_value = 0;
	struct snd_soc_component *component = dai->component;
	struct TAS5805m_priv *priv = snd_soc_component_get_drvdata(component);

	if (priv->init_done) {
		if (mute) {
		    //mute both left & right channels
			reg03_value = 0x0b;
			reg35_value = 0x00;
		} else {
		    //umute
			reg03_value = 0x03;
			reg35_value = 0x11;
		}

		TAS5805m_switch_book(component, TAS5805M_BOOK_00,
				TAS5805M_PAGE_00);
		snd_soc_component_write(component, TAS5805M_REG_03,
				reg03_value);
		snd_soc_component_write(component, TAS5805M_REG_35,
				reg35_value);
	}

	return 0;
}

static int i2c_write(struct i2c_client *client, u8 *buf, int len)
{
	int ret;

	ret = i2c_master_send(client, buf, len);

	if (ret == len)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int i2c_read_reg(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret;
	struct device *dev = &client->dev;

	struct i2c_msg msgs[] = {
		{
			.addr   = client->addr,
			.flags  = 0,
			.len    = 1,
			.buf    = &reg,
		},
		{
			.addr   = client->addr,
			.flags  = I2C_M_RD,
			.len    = 1,
			.buf    = data,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0)
		dev_err(dev, "%s error: %d\n", __func__, ret);

	return ret;
}

static void transmit_registers(struct i2c_client *client,
		const cfg_reg *r, int n)
{
	int i = 0;

	while (i < n) {
		switch (r[i].offset) {
		case CFG_META_BURST:
			i2c_write(client, (u8 *)&r[i+1], r[i].value);
			i +=  (r[i].value / 2) + 1;
			break;

		default:
			i2c_write(client, (u8 *)&r[i], 2);
			break;
		}
		i++;
	}
}

static void tas5805m_priv_init(struct TAS5805m_priv *priv)
{
	int i;
	u8 crc;

	if (priv->init_done)
		return;

	for (i = 0; i < RETRIES; i++) {
		/* reset CRC checksum */
		transmit_registers(priv->client,
				tas5805m_reset_checksum_sequence,
				ARRAY_SIZE(tas5805m_reset_checksum_sequence));

		transmit_registers(priv->client, tas5805m_init_sequence1,
				ARRAY_SIZE(tas5805m_init_sequence1));

		/* 5ms delay for uCDSP to boot up */
		usleep_range(3500, 5000);

		transmit_registers(priv->client, tas5805m_init_sequence2,
				ARRAY_SIZE(tas5805m_init_sequence2));

		/* read back CRC checksum */
		transmit_registers(priv->client,
				tas5805m_check_checksum_sequence,
				ARRAY_SIZE(tas5805m_check_checksum_sequence));
		i2c_read_reg(priv->client, TAS5805M_REG_7E, &crc);

		if (crc == CONFIG_CRC_CHECKSUM) {
			priv->init_done = 1;
			break;
		}
	}
}

static const struct snd_soc_dai_ops TAS5805m_dai_ops = {
	.mute_stream = TAS5805m_mute,
};

static struct snd_soc_dai_driver TAS5805m_dai = {
	.name		= "TAS5805 ASI1",
	.playback	= {
		.stream_name	= "Playback",
		.channels_min	= 2,
		.channels_max	= 2,
		.rates		= TAS5805M_RATES,
		.formats	= TAS5805M_FORMATS,
	},
	.ops = &TAS5805m_dai_ops,
};

static int TAS5805m_probe(struct i2c_client *client, struct regmap *regmap)
{
	struct TAS5805m_priv *priv;
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;

	priv = devm_kzalloc(dev, sizeof(struct TAS5805m_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	i2c_set_clientdata(client, priv);

	priv->client = client;
	priv->regmap = regmap;
	priv->software_volume = 0;

	priv->pdn_gpio = of_get_named_gpio(np, "gpio,pdn", 0);
	if (priv->pdn_gpio < 0) {
		dev_err(dev, "Failed to find pdn gpio!\n");
		return -EINVAL;
	}

	priv->hpd_gpio = of_get_named_gpio(np, "gpio,hpd", 0);
	if (priv->hpd_gpio < 0) {
		dev_info(dev, "Failed to find hpd gpio!\n");
	} else {
		priv->irq = gpio_to_irq(priv->hpd_gpio);
		if (priv->irq < 0) {
			dev_err(dev, "failed to get irq %d\n", ret);
			return -EINVAL;
		}

		ret = request_threaded_irq(priv->irq, NULL,
			tas5805m_hpd_thread_handler,
			IRQF_TRIGGER_FALLING |
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"tas5805_hpd_irq", priv);
		if (ret) {
			dev_err(dev, "failed to request irq %d\n", ret);
			return -EINVAL;
		}
	}

	ret = gpio_request_one(priv->pdn_gpio, GPIOF_OUT_INIT_LOW,
			"TAS5805 pdn");
	if (ret < 0) {
		dev_err(dev, "Failed to claim pdn gpio!\n");
		return -EINVAL;
	}

	/* pull /PDN high */
	gpio_set_value(priv->pdn_gpio, 1);

	priv->init_done = 0;
	tas5805m_priv_init(priv);
	if (priv->init_done == 0) {
		dev_err(dev, "Failed to initialize the tas5805 device!\n");
		goto config_err;
	}

	mutex_init(&priv->lock);

	ret = snd_soc_register_component(dev,
			&soc_component_TAS5805m,
			&TAS5805m_dai,
			1);
	if (ret != 0) {
		dev_err(dev, "Failed to register CODEC: %d\n", ret);
		goto err;
	}

	if (priv->hpd_gpio >= 0)
		tas5805m_i2c_mute(priv->client, gpio_get_value
				(priv->hpd_gpio) == 1);

	return 0;

err:
	return ret;

config_err:
	if (priv->pdn_gpio)
		gpio_free(priv->pdn_gpio);
	return -EINVAL;

}

static int TAS5805m_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config = TAS5805m_regmap;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return TAS5805m_probe(i2c, regmap);
}

void tas5805m_init(void)
{
    // new AQ setting
}
EXPORT_SYMBOL(tas5805m_init);

static int TAS5805m_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_component(&i2c->dev);

	return 0;
}

static const struct i2c_device_id TAS5805m_i2c_id[] = {
	{ "TAS5805m", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, TAS5805m_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id TAS5805m_of_match[] = {
	{ .compatible = "ti,tas5805m", },
	{ }
};
MODULE_DEVICE_TABLE(of, TAS5805m_of_match);
#endif

static struct i2c_driver TAS5805m_i2c_driver = {
	.probe		= TAS5805m_i2c_probe,
	.remove		= TAS5805m_i2c_remove,
	.id_table	= TAS5805m_i2c_id,
	.driver		= {
		.name	= TAS5805M_DRV_NAME,
		.of_match_table = TAS5805m_of_match,
	},
};

module_i2c_driver(TAS5805m_i2c_driver);

MODULE_AUTHOR("Andy Liu <andy-liu@ti.com>");
MODULE_DESCRIPTION("TAS5805M Audio Amplifier Driver");
MODULE_LICENSE("GPL v2");
