/*
 *  Routines for control of the CS8427 via i2c bus
 *  IEC958 (S/PDIF) receiver & transmitter by Cirrus Logic
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 and
 *  only version 2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/bitrev.h>
#include <linux/bitops.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <asm/unaligned.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/cs8427.h>
#include <sound/asoundef.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#define CS8427_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000)

#define CS8427_FORMATS (SNDRV_PCM_FMTBIT_S24_LE |\
			SNDRV_PCM_FORMAT_S16_LE |\
			SNDRV_PCM_FORMAT_S20_3LE)

struct cs8427_stream {
	struct snd_pcm_substream *substream;
	char hw_status[CHANNEL_STATUS_SIZE];		/* hardware status */
	char def_status[CHANNEL_STATUS_SIZE];		/* default status */
	char pcm_status[CHANNEL_STATUS_SIZE];		/* PCM private status */
	char hw_udata[32];
	struct snd_kcontrol *pcm_ctl;
};

struct cs8427 {
	struct i2c_client *client;
	struct i2c_msg xfer_msg[2];
	unsigned char regmap[0x14];	/* map of first 1 + 13 registers */
	unsigned int reset_timeout;
	struct cs8427_stream playback;
};

static int cs8427_i2c_write_device(struct cs8427 *cs8427_i2c,
				u16 reg, u8 *value, u32 bytes)
{
	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	u8 data[bytes + 1];

	if (cs8427_i2c->client == NULL) {
		pr_err("%s: failed to get device info\n", __func__);
		return -ENODEV;
	}
	reg_addr = (u8)reg;
	msg = &cs8427_i2c->xfer_msg[0];
	msg->addr = cs8427_i2c->client->addr;
	msg->len = bytes + 1;
	msg->flags = 0;
	data[0] = reg_addr;
	data[1] = *value;
	msg->buf = data;
	ret = i2c_transfer(cs8427_i2c->client->adapter,
				cs8427_i2c->xfer_msg, 1);
	/* Try again if the write fails
	 * checking with ebusy and number of bytes executed
	 * for write ret value should be 1
	 */
	if ((ret != 1) || (ret == -EBUSY)) {
		ret = i2c_transfer(
				cs8427_i2c->client->adapter,
				cs8427_i2c->xfer_msg, 1);
		if ((ret != 1) || (ret < 0)) {
			dev_err(&cs8427_i2c->client->dev,
				"failed to write the"
				" device reg %d\n", reg);
			return ret;
		}
	}
	return 0;
}

static int cs8427_i2c_write(struct cs8427 *chip, unsigned short reg,
			 int bytes, void *src)
{
	return cs8427_i2c_write_device(chip, reg, src, bytes);
}
static int cs8427_i2c_read_device(struct cs8427 *cs8427_i2c,
				unsigned short reg,
				  int bytes, unsigned char *dest)
{
	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	u8 i = 0;

	if (cs8427_i2c->client == NULL) {
		pr_err("%s: failed to get device info\n", __func__);
		return -ENODEV;
	}
	for (i = 0; i < bytes; i++) {
		reg_addr = (u8)reg++;
		msg = &cs8427_i2c->xfer_msg[0];
		msg->addr = cs8427_i2c->client->addr;
		msg->len = 1;
		msg->flags = 0;
		msg->buf = &reg_addr;

		msg = &cs8427_i2c->xfer_msg[1];
		msg->addr = cs8427_i2c->client->addr;
		msg->len = 1;
		msg->flags = I2C_M_RD;
		msg->buf = dest++;
		ret = i2c_transfer(cs8427_i2c->client->adapter,
					cs8427_i2c->xfer_msg, 2);

		/* Try again if read fails first time
		checking with ebusy and number of bytes executed
		for read ret value should be 2*/
		if ((ret != 2) || (ret == -EBUSY)) {
			ret = i2c_transfer(
					cs8427_i2c->client->adapter,
					cs8427_i2c->xfer_msg, 2);
			if ((ret != 2) || (ret < 0)) {
				dev_err(&cs8427_i2c->client->dev,
					"failed to read cs8427"
					" register %d\n", reg);
				return ret;
			}
		}
	}
	return 0;
}

static int cs8427_i2c_read(struct cs8427 *chip,
				unsigned short reg,
				int bytes, void *dest)
{
	return cs8427_i2c_read_device(chip, reg,
					bytes, dest);
}

static int cs8427_i2c_sendbytes(struct cs8427 *chip,
			char *reg_addr, char *data,
			int bytes)
{
	u32 ret = 0;
	u8 i = 0;

	if (!chip) {
		pr_err("%s, invalid device info\n", __func__);
		return -ENODEV;
	}
	if (!data) {
		dev_err(&chip->client->dev, "%s:"
			"invalid data pointer\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < bytes; i++) {
		ret = cs8427_i2c_write_device(chip, (*reg_addr + i),
						&data[i], 1);
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"%s: failed to send the data to"
				" cs8427 chip\n", __func__);
			break;
		}
	}
	return i;
}

/*
 * Reset the chip using run bit, also lock PLL using ILRCK and
 * put back AES3INPUT. This workaround is described in latest
 * CS8427 datasheet, otherwise TXDSERIAL will not work.
 */
static void snd_cs8427_reset(struct cs8427 *chip)
{
	unsigned long end_time;
	int data, aes3input = 0;
	unsigned char val = 0;

	if (snd_BUG_ON(!chip))
		return;
	if ((chip->regmap[CS8427_REG_CLOCKSOURCE] & CS8427_RXDAES3INPUT) ==
	    CS8427_RXDAES3INPUT) /* AES3 bit is set */
		aes3input = 1;
	chip->regmap[CS8427_REG_CLOCKSOURCE] &= ~(CS8427_RUN | CS8427_RXDMASK);
	cs8427_i2c_write(chip, CS8427_REG_CLOCKSOURCE,
			     1, &chip->regmap[CS8427_REG_CLOCKSOURCE]);
	udelay(200);
	chip->regmap[CS8427_REG_CLOCKSOURCE] |= CS8427_RUN | CS8427_RXDILRCK;
	cs8427_i2c_write(chip, CS8427_REG_CLOCKSOURCE,
			     1, &chip->regmap[CS8427_REG_CLOCKSOURCE]);
	udelay(200);
	end_time = jiffies + chip->reset_timeout;
	while (time_after_eq(end_time, jiffies)) {
		data = cs8427_i2c_read(chip, CS8427_REG_RECVERRORS,
				1, &val);
		if (!(val & CS8427_UNLOCK))
			break;
		schedule_timeout_uninterruptible(1);
	}
	chip->regmap[CS8427_REG_CLOCKSOURCE] &= ~CS8427_RXDMASK;
	if (aes3input)
		chip->regmap[CS8427_REG_CLOCKSOURCE] |= CS8427_RXDAES3INPUT;
	cs8427_i2c_write(chip, CS8427_REG_CLOCKSOURCE,
			     1, &chip->regmap[CS8427_REG_CLOCKSOURCE]);
}

static int snd_cs8427_in_status_info(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 255;
	return 0;
}

static int snd_cs8427_in_status_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct cs8427 *chip = kcontrol->private_data;
	unsigned char val = 0;
	int err = 0;

	err = cs8427_i2c_read(chip, kcontrol->private_value, 1, &val);
	if (err < 0)
		return err;
	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int snd_cs8427_qsubcode_info(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = 10;
	return 0;
}

static int snd_cs8427_qsubcode_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct cs8427 *chip = kcontrol->private_data;
	unsigned char reg = CS8427_REG_QSUBCODE;
	int err;
	unsigned char val[20];

	if (!chip) {
		pr_err("%s: invalid device info\n", __func__);
		return -ENODEV;
	}

	err = cs8427_i2c_write(chip, reg, 1, &val[0]);
	if (err != 1) {
		dev_err(&chip->client->dev, "unable to send register"
			" 0x%x byte to CS8427\n", reg);
		return err < 0 ? err : -EIO;
	}
	err = cs8427_i2c_read(chip, *ucontrol->value.bytes.data, 10, &val);
	if (err != 10) {
		dev_err(&chip->client->dev, "unable to read"
			" Q-subcode bytes from CS8427\n");
		return err < 0 ? err : -EIO;
	}
	return 0;
}

static int snd_cs8427_spdif_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_cs8427_select_corudata(struct cs8427 *cs8427_i2c, int udata)
{
	struct cs8427 *chip = cs8427_i2c;
	int err;

	udata = udata ? CS8427_BSEL : 0;
	if (udata != (chip->regmap[CS8427_REG_CSDATABUF] & udata)) {
		chip->regmap[CS8427_REG_CSDATABUF] &= ~CS8427_BSEL;
		chip->regmap[CS8427_REG_CSDATABUF] |= udata;
		err = cs8427_i2c_write(cs8427_i2c, CS8427_REG_CSDATABUF,
				   1, &chip->regmap[CS8427_REG_CSDATABUF]);
		if (err < 0)
			return err;
	}
	return 0;
}

static int snd_cs8427_send_corudata(struct cs8427 *obj,
				    int udata,
				    unsigned char *ndata,
				    int count)
{
	struct cs8427 *chip = obj;
	char *hw_data = udata ?
		chip->playback.hw_udata : chip->playback.hw_status;
	char data[32];
	int err, idx;
	unsigned char addr = 0;
	int ret = 0;

	if (!memcmp(hw_data, ndata, count))
		return 0;
	err = snd_cs8427_select_corudata(chip, udata);
	if (err < 0)
		return err;
	memcpy(hw_data, ndata, count);
	if (udata) {
		memset(data, 0, sizeof(data));
		if (memcmp(hw_data, data, count) == 0) {
			chip->regmap[CS8427_REG_UDATABUF] &= ~CS8427_UBMMASK;
			chip->regmap[CS8427_REG_UDATABUF] |= CS8427_UBMZEROS |
				CS8427_EFTUI;
			err = cs8427_i2c_write(chip, CS8427_REG_UDATABUF,
				   1, &chip->regmap[CS8427_REG_UDATABUF]);
			return err < 0 ? err : 0;
		}
	}
	idx = 0;
	memcpy(data, ndata, CHANNEL_STATUS_SIZE);
	/* address from where the bufferhas to write*/
	addr = 0x20;
	ret = cs8427_i2c_sendbytes(chip, &addr, data, count);
	if (ret != count)
		return -EIO;
	return 1;
}

static int snd_cs8427_spdif_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct cs8427 *chip = kcontrol->private_data;
	if (!chip) {
		pr_err("%s: invalid device info\n", __func__);
		return -ENODEV;
	}

	memcpy(ucontrol->value.iec958.status,
			chip->playback.def_status, CHANNEL_STATUS_SIZE);
	return 0;
}

static int snd_cs8427_spdif_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct cs8427 *chip = kcontrol->private_data;
	unsigned char *status;
	int err, change;

	if (!chip) {
		pr_err("%s: invalid device info\n", __func__);
		return -ENODEV;
	}
	status = kcontrol->private_value ?
		chip->playback.pcm_status : chip->playback.def_status;

	change = memcmp(ucontrol->value.iec958.status, status,
			CHANNEL_STATUS_SIZE) != 0;

	if (!change) {
		memcpy(status, ucontrol->value.iec958.status,
			CHANNEL_STATUS_SIZE);
		err = snd_cs8427_send_corudata(chip, 0, status,
			CHANNEL_STATUS_SIZE);
		if (err < 0)
			change = err;
	}
	return change;
}

static int snd_cs8427_spdif_mask_info(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_cs8427_spdif_mask_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	memset(ucontrol->value.iec958.status, 0xff, CHANNEL_STATUS_SIZE);
	return 0;
}

static struct snd_kcontrol_new snd_cs8427_iec958_controls[] = {
	{
		.iface	=	SNDRV_CTL_ELEM_IFACE_PCM,
		.info	=	snd_cs8427_in_status_info,
		.name	=	"IEC958 CS8427 Input Status",
		.access =	(SNDRV_CTL_ELEM_ACCESS_READ |
				 SNDRV_CTL_ELEM_ACCESS_VOLATILE),
		.get	=	snd_cs8427_in_status_get,
		.private_value = 15,
	},
	{
		.iface	=	SNDRV_CTL_ELEM_IFACE_PCM,
		.info	=	snd_cs8427_in_status_info,
		.name	=	"IEC958 CS8427 Error Status",
		.access =	(SNDRV_CTL_ELEM_ACCESS_READ |
				 SNDRV_CTL_ELEM_ACCESS_VOLATILE),
		.get	=	snd_cs8427_in_status_get,
		.private_value = 16,
	},
	{
		.access	=	SNDRV_CTL_ELEM_ACCESS_READ,
		.iface	=	SNDRV_CTL_ELEM_IFACE_PCM,
		.name	=	SNDRV_CTL_NAME_IEC958("", PLAYBACK, MASK),
		.info	=	snd_cs8427_spdif_mask_info,
		.get	=	snd_cs8427_spdif_mask_get,
	},
	{
		.iface	=	SNDRV_CTL_ELEM_IFACE_PCM,
		.name	=	SNDRV_CTL_NAME_IEC958("", PLAYBACK,
							DEFAULT),
		.info	=	snd_cs8427_spdif_info,
		.get	=	snd_cs8427_spdif_get,
		.put	=	snd_cs8427_spdif_put,
		.private_value = 0
	},
	{
		.access	=	(SNDRV_CTL_ELEM_ACCESS_READWRITE |
				 SNDRV_CTL_ELEM_ACCESS_INACTIVE),
		.iface	=	SNDRV_CTL_ELEM_IFACE_PCM,
		.name	=	SNDRV_CTL_NAME_IEC958("", PLAYBACK, PCM_STREAM),
		.info	=	snd_cs8427_spdif_info,
		.get	=	snd_cs8427_spdif_get,
		.put	=	snd_cs8427_spdif_put,
		.private_value = 1
	},
	{
		.iface	=	SNDRV_CTL_ELEM_IFACE_PCM,
		.info	=	snd_cs8427_qsubcode_info,
		.name	=	"IEC958 Q-subcode Capture Default",
		.access =	(SNDRV_CTL_ELEM_ACCESS_READ |
				 SNDRV_CTL_ELEM_ACCESS_VOLATILE),
		.get	=	snd_cs8427_qsubcode_get
	}
};

static int cs8427_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs8427 *chip = dev_get_drvdata(codec->dev);
	int ret = 0;
	if (chip == NULL) {
		pr_err("invalid device private data\n");
		return -ENODEV;
	}
	chip->regmap[CS8427_REG_SERIALINPUT] &= CS8427_BITWIDTH_MASK;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		chip->regmap[CS8427_REG_SERIALINPUT] |= CS8427_SIRES16;
		ret = cs8427_i2c_write(chip, CS8427_REG_SERIALINPUT, 1,
					&chip->regmap[CS8427_REG_SERIALINPUT]);
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		chip->regmap[CS8427_REG_SERIALINPUT] |= CS8427_SIRES20;
		ret = cs8427_i2c_write(chip, CS8427_REG_SERIALINPUT, 1,
					&chip->regmap[CS8427_REG_SERIALINPUT]);

		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		chip->regmap[CS8427_REG_SERIALINPUT] |= CS8427_SIRES24;
		ret = cs8427_i2c_write(chip, CS8427_REG_SERIALINPUT, 1,
					&chip->regmap[CS8427_REG_SERIALINPUT]);
		break;
	default:
		pr_err("invalid format\n");
		break;
	}
	dev_dbg(&chip->client->dev,
		"%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);
	return ret;
}

static int snd_cs8427_iec958_register_kcontrol(struct cs8427 *cs8427,
			    struct snd_card *card)
{
	struct cs8427 *chip = cs8427;
	struct snd_kcontrol *kctl;
	unsigned int idx;
	int err;

	for (idx = 0; idx < ARRAY_SIZE(snd_cs8427_iec958_controls); idx++) {
		kctl = snd_ctl_new1(&snd_cs8427_iec958_controls[idx], chip);
		if (kctl == NULL)
			return -ENOMEM;
		err = snd_ctl_add(card, kctl);
		if (err < 0) {
			dev_err(&chip->client->dev,
				"failed to add the kcontrol\n");
			return err;
		}
	}
	return err;
}

static int cs8427_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct cs8427 *chip = dev_get_drvdata(dai->codec->dev);

	if (chip == NULL) {
		pr_err("invalid device private data\n");
		return -ENODEV;
	}
	/*
	 * we need to make the pll lock for the I2S tranfers
	 * reset the cs8427 chip for this.
	 */
	snd_cs8427_reset(chip);
	dev_dbg(&chip->client->dev,
		"%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);

	return 0;
}

static void cs8427_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct cs8427 *chip = dev_get_drvdata(dai->codec->dev);

	if (chip == NULL) {
		pr_err("invalid device private data\n");
		return;
	}
	dev_dbg(&chip->client->dev,
		"%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);
}

static int cs8427_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct cs8427 *chip = dev_get_drvdata(dai->codec->dev);

	if (chip == NULL) {
		pr_err("invalid device private data\n");
		return -ENODEV;
	}
	dev_dbg(&chip->client->dev, "%s\n", __func__);
	return 0;
}

static struct snd_soc_dai_ops cs8427_dai_ops = {
	.startup = cs8427_startup,
	.shutdown = cs8427_shutdown,
	.hw_params = cs8427_hw_params,
	.set_fmt = cs8427_set_dai_fmt,
};

static struct snd_soc_dai_driver cs8427_dai[] = {
	{
		.name = "spdif_rx",
		.id = 1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = CS8427_RATES,
			.formats = CS8427_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &cs8427_dai_ops,
	},
};


static unsigned int cs8427_soc_i2c_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	struct cs8427 *chip = dev_get_drvdata(codec->dev);

	if (chip == NULL) {
		pr_err("invalid device private data\n");
		return -ENODEV;
	}
	dev_dbg(&chip->client->dev, "cs8427 soc i2c read\n");
	return 0;
}

static int cs8427_soc_i2c_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	struct cs8427 *chip = dev_get_drvdata(codec->dev);

	if (chip == NULL) {
		pr_err("invalid device private data\n");
		return -ENODEV;
	}
	dev_dbg(&chip->client->dev, "cs8427 soc i2c write\n");
	return 0;
}

static int cs8427_soc_probe(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct cs8427 *chip;
	codec->control_data = dev_get_drvdata(codec->dev);
	chip = codec->control_data;

	if (chip == NULL) {
		pr_err("invalid device private data\n");
		return -ENODEV;
	}
	snd_cs8427_iec958_register_kcontrol(chip, codec->card->snd_card);
	dev_set_drvdata(codec->dev, chip);
	return ret;
}

static struct snd_soc_codec_driver soc_codec_dev_cs8427 = {
	.read = cs8427_soc_i2c_read,
	.write = cs8427_soc_i2c_write,
	.probe = cs8427_soc_probe,
};

int poweron_cs8427(struct cs8427 *chip)
{
	struct cs8427_platform_data *pdata = chip->client->dev.platform_data;
	int ret = 0;

	/*enable the 100KHz level shifter*/
	if (pdata->enable) {
		ret = pdata->enable(1);
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"failed to enable the level shifter\n");
			return ret;
		}
	}

	ret = gpio_request(pdata->reset_gpio, "cs8427 reset");
	if (ret < 0) {
		dev_err(&chip->client->dev,
			 "failed to request the gpio %d\n",
				pdata->reset_gpio);
		return ret;
	}
	/*bring the chip out of reset*/
	gpio_direction_output(pdata->reset_gpio, 1);
	msleep(20);
	gpio_direction_output(pdata->reset_gpio, 0);
	msleep(20);
	gpio_direction_output(pdata->reset_gpio, 1);
	msleep(20);
	return ret;
}

static __devinit int cs8427_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	static unsigned char initvals1[] = {
	  CS8427_REG_CONTROL1 | CS8427_REG_AUTOINC,
	  /* CS8427_REG_CONTROL1: RMCK to OMCK, valid PCM audio, disable mutes,
	   * TCBL=output
	   */
	  CS8427_SWCLK | CS8427_TCBLDIR,
	  /* CS8427_REG_CONTROL2: hold last valid audio sample, RMCK=256*Fs,
	   * normal stereo operation
	   */
	  0x08,
	  /* CS8427_REG_DATAFLOW:
	   * AES3 Transmitter data source => Serial Audio input port
	   * Serial audio output port data source => reserved
	   */
	  CS8427_TXDSERIAL,
	  /* CS8427_REG_CLOCKSOURCE: Run off, CMCK=256*Fs,
	   * output time base = OMCK, input time base = recovered input clock,
	   * recovered input clock source is ILRCK changed to AES3INPUT
	   * (workaround, see snd_cs8427_reset)
	   */
	  CS8427_RXDILRCK | CS8427_OUTC,
	  /* CS8427_REG_SERIALINPUT: Serial audio input port data format = I2S,
	   * 24-bit, 64*Fsi
	   */
	  CS8427_SIDEL | CS8427_SILRPOL | CS8427_SORES16,
	  /* CS8427_REG_SERIALOUTPUT: Serial audio output port data format
	   *  = I2S, 24-bit, 64*Fsi
	   */
	  CS8427_SODEL | CS8427_SOLRPOL | CS8427_SIRES16,
	};
	static unsigned char initvals2[] = {
	  CS8427_REG_RECVERRMASK | CS8427_REG_AUTOINC,
	  /* CS8427_REG_RECVERRMASK: unmask the input PLL clock, V, confidence,
	   * biphase, parity status bits
	   * CS8427_UNLOCK | CS8427_V | CS8427_CONF | CS8427_BIP | CS8427_PAR,
	   */
	  0xff, /* set everything */
	  /* CS8427_REG_CSDATABUF:
	   * Registers 32-55 window to CS buffer
	   * Inhibit D->E transfers from overwriting first 5 bytes of CS data.
	   * Inhibit D->E transfers (all) of CS data.
	   * Allow E->F transfer of CS data.
	   * One byte mode; both A/B channels get same written CB data.
	   * A channel info is output to chip's EMPH* pin.
	   */
	  CS8427_CBMR | CS8427_DETCI,
	  /* CS8427_REG_UDATABUF:
	   * Use internal buffer to transmit User (U) data.
	   * Chip's U pin is an output.
	   * Transmit all O's for user data.
	   * Inhibit D->E transfers.
	   * Inhibit E->F transfers.
	   */
	  CS8427_UD | CS8427_EFTUI | CS8427_DETUI,
	};
	int err;
	unsigned char buf[CHANNEL_STATUS_SIZE];
	unsigned char val = 0;
	char addr = 0;
	unsigned int reset_timeout = 100;
	int ret = 0;
	struct cs8427 *chip;

	if (!client) {
		pr_err("%s: invalid device info\n", __func__);
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct cs8427), GFP_KERNEL);
	if (chip == NULL) {
		dev_err(&client->dev,
			"%s: error, allocation failed\n", __func__);
		return -ENOMEM;
	}

	chip->client = client;

	dev_set_drvdata(&chip->client->dev, chip);

	ret = poweron_cs8427(chip);

	if (ret) {
		dev_err(&chip->client->dev,
			"failed to bring chip out of reset\n");
		return -ENODEV;
	}

	err = cs8427_i2c_read(chip, CS8427_REG_ID_AND_VER, 1, &val);
	if (err < 0) {
		/* give second chance */
		dev_err(&chip->client->dev,
			"failed to read cs8427 trying once again\n");
		err = cs8427_i2c_read(chip, CS8427_REG_ID_AND_VER,
							1, &val);
		if (err < 0) {
			dev_err(&chip->client->dev,
				"failed to read version number\n");
			return -ENODEV;
		}
		dev_dbg(&chip->client->dev,
			"version number read = %x\n", val);
	}
	if (val != CS8427_VER8427A) {
		dev_err(&chip->client->dev,
			"unable to find CS8427 signature "
			"(expected 0x%x, read 0x%x),\n",
			CS8427_VER8427A, val);
		dev_err(&chip->client->dev,
			" initialization is not completed\n");
		return -EFAULT;
	}
	val = 0;
	/* turn off run bit while making changes to configuration */
	err = cs8427_i2c_write(chip, CS8427_REG_CLOCKSOURCE, 1, &val);
	if (err < 0)
		goto __fail;
	/* send initial values */
	memcpy(chip->regmap + (initvals1[0] & 0x7f), initvals1 + 1, 6);
	addr = 1;
	err = cs8427_i2c_sendbytes(chip, &addr, &initvals1[1], 6);
	if (err != 6) {
		err = err < 0 ? err : -EIO;
		goto __fail;
	}
	/* Turn off CS8427 interrupt stuff that is not used in hardware */
	memset(buf, 0, 7);
	/* from address 9 to 15 */
	addr = 9;
	err = cs8427_i2c_sendbytes(chip, &addr, buf, 7);
	if (err != 7)
		goto __fail;
	/* send transfer initialization sequence */
	addr = 0x11;
	memcpy(chip->regmap + (initvals2[0] & 0x7f), initvals2 + 1, 3);
	err = cs8427_i2c_sendbytes(chip, &addr, &initvals2[1], 3);
	if (err != 3) {
		err = err < 0 ? err : -EIO;
		goto __fail;
	}
	/* write default channel status bytes */
	put_unaligned_le32(SNDRV_PCM_DEFAULT_CON_SPDIF, buf);
	memset(buf + 4, 0, CHANNEL_STATUS_SIZE - 4);
	if (snd_cs8427_send_corudata(chip, 0, buf, CHANNEL_STATUS_SIZE) < 0)
		goto __fail;
	memcpy(chip->playback.def_status, buf, CHANNEL_STATUS_SIZE);
	memcpy(chip->playback.pcm_status, buf, CHANNEL_STATUS_SIZE);

	/* turn on run bit and rock'n'roll */
	if (reset_timeout < 1)
		reset_timeout = 1;
	chip->reset_timeout = reset_timeout;
	snd_cs8427_reset(chip);

	ret = snd_soc_register_codec(&chip->client->dev, &soc_codec_dev_cs8427,
					cs8427_dai, ARRAY_SIZE(cs8427_dai));

	return 0;

__fail:
	kfree(chip);
	return err < 0 ? err : -EIO;
}

static int __devexit cs8427_remove(struct i2c_client *client)
{
	struct cs8427 *chip;
	struct cs8427_platform_data *pdata;
	chip = dev_get_drvdata(&client->dev);
	if (!chip) {
		pr_err("invalid device info\n");
		return -ENODEV;
	}
	pdata = chip->client->dev.platform_data;
	gpio_free(pdata->reset_gpio);
	if (pdata->enable)
		pdata->enable(0);
	kfree(chip);
	return 0;
}

static struct i2c_device_id cs8427_id_table[] = {
	{"cs8427", CS8427_ADDR0},
	{"cs8427", CS8427_ADDR2},
	{"cs8427", CS8427_ADDR3},
	{"cs8427", CS8427_ADDR4},
	{"cs8427", CS8427_ADDR5},
	{"cs8427", CS8427_ADDR6},
	{"cs8427", CS8427_ADDR7},
	{}
};
MODULE_DEVICE_TABLE(i2c, cs8427_id_table);

static struct i2c_driver cs8427_i2c_driver = {
	.driver                 = {
		.owner          =       THIS_MODULE,
		.name           =       "cs8427-spdif",
	},
	.id_table               =       cs8427_id_table,
	.probe                  =       cs8427_i2c_probe,
	.remove                 =       __devexit_p(cs8427_remove),
};

static int __init cs8427_module_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&cs8427_i2c_driver);
	if (ret != 0)
		pr_err("failed to add the I2C driver\n");
	return ret;
}

static void __exit cs8427_module_exit(void)
{
	pr_info("module exit\n");
}

module_init(cs8427_module_init)
module_exit(cs8427_module_exit)

MODULE_DESCRIPTION("CS8427 interface driver");
MODULE_LICENSE("GPL v2");
