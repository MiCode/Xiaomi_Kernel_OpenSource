/*
 * ALSA SoC Texas Instruments TPA6130A2 headset stereo amplifier driver
 *
 * Copyright (C) Nokia Corporation
 * Copyright (C) 2015 XiaoMi, Inc.
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
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <sound/tpa6130a2-plat.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <asm/bootinfo.h>
#include <linux/delay.h>

#include "tpa6130a2.h"

enum tpa_model {
	TPA6130A2,
	TPA6140A2,
};

static struct i2c_client *tpa6130a2_client;

/* This struct is used to save the context */
struct tpa6130a2_data {
	struct mutex mutex;
	unsigned char regs[TPA6130A2_CACHEREGNUM];
	struct regulator *supply;
	int power_gpio;
	u8 power_state:1;
	enum tpa_model id;
};

static int tpa6130a2_i2c_read(int reg)
{
	struct tpa6130a2_data *data;
	int val;

	BUG_ON(tpa6130a2_client == NULL);
	data = i2c_get_clientdata(tpa6130a2_client);

	/* If powered off, return the cached value */
	if (data->power_state) {
		val = i2c_smbus_read_byte_data(tpa6130a2_client, reg);
		if (val < 0)
			dev_err(&tpa6130a2_client->dev, "Read failed\n");
		else
			data->regs[reg] = val;
	} else {
		val = data->regs[reg];
	}

	return val;
}

static int tpa6130a2_i2c_write(int reg, u8 value)
{
	struct tpa6130a2_data *data;
	int val = 0;

	BUG_ON(tpa6130a2_client == NULL);
	data = i2c_get_clientdata(tpa6130a2_client);

	if (data->power_state) {
		val = i2c_smbus_write_byte_data(tpa6130a2_client, reg, value);
		if (val < 0) {
			dev_err(&tpa6130a2_client->dev, "Write failed\n");
			return val;
		}
	}

	/* Either powered on or off, we save the context */
	data->regs[reg] = value;

	return val;
}

static u8 tpa6130a2_read(int reg)
{
	struct tpa6130a2_data *data;

	BUG_ON(tpa6130a2_client == NULL);
	data = i2c_get_clientdata(tpa6130a2_client);

	return data->regs[reg];
}

static int tpa6130a2_initialize(void)
{
	struct tpa6130a2_data *data;
	int i, ret = 0;

	BUG_ON(tpa6130a2_client == NULL);
	data = i2c_get_clientdata(tpa6130a2_client);

	for (i = 1; i < TPA6130A2_REG_VERSION; i++) {
		ret = tpa6130a2_i2c_write(i, data->regs[i]);
		if (ret < 0)
			break;
	}

	return ret;
}

static int tpa6130a2_power(u8 power)
{
	struct	tpa6130a2_data *data;
	u8	val;
	int	ret = 0;

	BUG_ON(tpa6130a2_client == NULL);
	data = i2c_get_clientdata(tpa6130a2_client);

	mutex_lock(&data->mutex);
	if (power == data->power_state)
		goto exit;

	if (power) {
		/* Power on */
		if (data->power_gpio >= 0)
			gpio_set_value(data->power_gpio, 1);

		data->power_state = 1;
		ret = tpa6130a2_initialize();
		if (ret < 0) {
			dev_err(&tpa6130a2_client->dev,
				"Failed to initialize chip\n");
			if (data->power_gpio >= 0)
				gpio_set_value(data->power_gpio, 0);
			data->power_state = 0;
			goto exit;
		}
	} else {
		/* set channel to high impedance mode */
		tpa6130a2_i2c_write(TPA6130A2_REG_OUT_IMPEDANCE, 0x3);
		/* set SWS */
		val = tpa6130a2_read(TPA6130A2_REG_CONTROL);
		val |= TPA6130A2_SWS;
		tpa6130a2_i2c_write(TPA6130A2_REG_CONTROL, val);

		/* not fully power off PA, let it stay in SWS mode */
		/* Power off:
		 * if (data->power_gpio >= 0)
		 *	gpio_set_value(data->power_gpio, 0);
		 */

		data->power_state = 0;
	}

exit:
	mutex_unlock(&data->mutex);
	return ret;
}

static int tpa6130a2_get_volsw(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct tpa6130a2_data *data;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;

	BUG_ON(tpa6130a2_client == NULL);
	data = i2c_get_clientdata(tpa6130a2_client);

	mutex_lock(&data->mutex);

	ucontrol->value.integer.value[0] =
		(tpa6130a2_read(reg) >> shift) & mask;

	if (invert)
		ucontrol->value.integer.value[0] =
			max - ucontrol->value.integer.value[0];

	mutex_unlock(&data->mutex);
	return 0;
}

static int tpa6130a2_put_volsw(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct tpa6130a2_data *data;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val = (ucontrol->value.integer.value[0] & mask);
	unsigned int val_reg;

	BUG_ON(tpa6130a2_client == NULL);
	data = i2c_get_clientdata(tpa6130a2_client);

	if (invert)
		val = max - val;

	pr_info ("tpa6130a2_put_volsw: invert:%d, max:0x%x, mask:0x%x, value:%ld, val:0x%x ",
                    invert, max, mask, ucontrol->value.integer.value[0], val);

	mutex_lock(&data->mutex);

	val_reg = tpa6130a2_read(reg);
	if (((val_reg >> shift) & mask) == val) {
		mutex_unlock(&data->mutex);
		return 0;
	}

	val_reg &= ~(mask << shift);
	val_reg |= val << shift;

	pr_info ("tpa6130a2_put_volsw: reg:0x%x, val_reg:0x%x", reg, val_reg);

	tpa6130a2_i2c_write(reg, val_reg);

	mutex_unlock(&data->mutex);

	return 1;
}

/*
 * TPA6130 volume. From -59.5 to 4 dB with increasing step size when going
 * down in gain.
 */
static const unsigned int tpa6130_tlv[] = {
	TLV_DB_RANGE_HEAD(10),
	0, 1, TLV_DB_SCALE_ITEM(-5950, 600, 0),
	2, 3, TLV_DB_SCALE_ITEM(-5000, 250, 0),
	4, 5, TLV_DB_SCALE_ITEM(-4550, 160, 0),
	6, 7, TLV_DB_SCALE_ITEM(-4140, 190, 0),
	8, 9, TLV_DB_SCALE_ITEM(-3650, 120, 0),
	10, 11, TLV_DB_SCALE_ITEM(-3330, 160, 0),
	12, 13, TLV_DB_SCALE_ITEM(-3040, 180, 0),
	14, 20, TLV_DB_SCALE_ITEM(-2710, 110, 0),
	21, 37, TLV_DB_SCALE_ITEM(-1960, 74, 0),
	38, 63, TLV_DB_SCALE_ITEM(-720, 45, 0),
};

static const struct snd_kcontrol_new tpa6130a2_controls[] = {
	SOC_SINGLE_EXT_TLV("TPA6130A2 Headphone Playback Volume",
		       TPA6130A2_REG_VOL_MUTE, 0, 0x3f, 0,
		       tpa6130a2_get_volsw, tpa6130a2_put_volsw,
		       tpa6130_tlv),

	SOC_SINGLE_EXT("TI PA Gain", TPA6130A2_REG_VOL_MUTE, 0, 0x3f, 0,
		tpa6130a2_get_volsw, tpa6130a2_put_volsw),
};

static const unsigned int tpa6140_tlv[] = {
	TLV_DB_RANGE_HEAD(3),
	0, 8, TLV_DB_SCALE_ITEM(-5900, 400, 0),
	9, 16, TLV_DB_SCALE_ITEM(-2500, 200, 0),
	17, 31, TLV_DB_SCALE_ITEM(-1000, 100, 0),
};

static const struct snd_kcontrol_new tpa6140a2_controls[] = {
	SOC_SINGLE_EXT_TLV("TPA6140A2 Headphone Playback Volume",
		       TPA6130A2_REG_VOL_MUTE, 1, 0x1f, 0,
		       tpa6130a2_get_volsw, tpa6130a2_put_volsw,
		       tpa6140_tlv),
};

/*
 * Enable or disable channel (left or right)
 * The bit number for mute and amplifier are the same per channel:
 * bit 6: Right channel
 * bit 7: Left channel
 * in both registers.
 */
static void tpa6130a2_channel_enable(u8 channel, int enable)
{
	u8	val;

	if (enable) {
		/* Enable channel */
		/* Enable amplifier */
		val = tpa6130a2_read(TPA6130A2_REG_CONTROL);
		val |= channel;
		val &= ~TPA6130A2_SWS;
		tpa6130a2_i2c_write(TPA6130A2_REG_CONTROL, val);
		usleep(80000);
		/* Unmute channel */
		val = tpa6130a2_read(TPA6130A2_REG_VOL_MUTE);
		val &= ~channel;
		tpa6130a2_i2c_write(TPA6130A2_REG_VOL_MUTE, val);

		/* exit from high impedance mode */
		tpa6130a2_i2c_write(TPA6130A2_REG_OUT_IMPEDANCE, 0);
	} else {
		/* Disable channel */
		/* Mute channel */
		val = tpa6130a2_read(TPA6130A2_REG_VOL_MUTE);
		val |= channel;
		tpa6130a2_i2c_write(TPA6130A2_REG_VOL_MUTE, val);

		/* Disable amplifier */
		val = tpa6130a2_read(TPA6130A2_REG_CONTROL);
		val &= ~channel;
		tpa6130a2_i2c_write(TPA6130A2_REG_CONTROL, val);

		/* set channel to high impedance mode */
		tpa6130a2_i2c_write(TPA6130A2_REG_OUT_IMPEDANCE, 0x3);
	}
}

int tpa6130a2_stereo_enable(struct snd_soc_codec *codec, int enable)
{
	int ret = 0;

	if (tpa6130a2_client == NULL)
		return -1;

	pr_info ("tpa6130a2_stereo_enable: %s, enable:%d", codec->name, enable);
	if (enable) {
		ret = tpa6130a2_power(1);
		if (ret < 0)
			return ret;
		tpa6130a2_channel_enable(TPA6130A2_HP_EN_R | TPA6130A2_HP_EN_L,
					 1);
	} else {
		tpa6130a2_channel_enable(TPA6130A2_HP_EN_R | TPA6130A2_HP_EN_L,
					 0);
		ret = tpa6130a2_power(0);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tpa6130a2_stereo_enable);

int tpa6130a2_add_controls(struct snd_soc_codec *codec)
{
	struct	tpa6130a2_data *data;

	if (tpa6130a2_client == NULL)
		return -ENODEV;

	data = i2c_get_clientdata(tpa6130a2_client);

	if (data->id == TPA6140A2)
		return snd_soc_add_codec_controls(codec, tpa6140a2_controls,
						ARRAY_SIZE(tpa6140a2_controls));
	else
		return snd_soc_add_codec_controls(codec, tpa6130a2_controls,
						ARRAY_SIZE(tpa6130a2_controls));
}
EXPORT_SYMBOL_GPL(tpa6130a2_add_controls);

static int __devinit tpa6130a2_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct device *dev;
	struct tpa6130a2_data *data;
	struct tpa6130a2_platform_data *pdata;
	int en_gpio;
	int ret;
	const char *status;
	int statlen;

	dev = &client->dev;

	if (client->dev.of_node) {
		status = of_get_property(client->dev.of_node, "status", &statlen);
		if (status && (statlen > 0))
			if (!strcmp(status, "disabled"))
				return -ENODEV;
		if (get_hw_version_major() == 3 && get_hw_version_minor() == 1)
			en_gpio = of_get_named_gpio_flags(client->dev.of_node,
						"ti,enable-gpio-3_1", 0, NULL);
		else
			en_gpio = of_get_named_gpio_flags(client->dev.of_node,
						"ti,enable-gpio", 0, NULL);
		dev_info(dev, "probe from device tree mode: en-gpio=%d\n", en_gpio);
	} else if(client->dev.platform_data != NULL) {
		pdata = client->dev.platform_data;
		en_gpio = pdata->power_gpio;
		dev_info(dev, "probe from board file mode: en-gpio=%d\n", en_gpio);
	} else {
		dev_err(dev, "probe fatal error\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(dev, "Can not allocate memory\n");
		return -ENOMEM;
	}

	tpa6130a2_client = client;

	i2c_set_clientdata(tpa6130a2_client, data);

	data->power_gpio = en_gpio;
	data->id = id->driver_data;

	mutex_init(&data->mutex);

	/* Set default register values */
	data->regs[TPA6130A2_REG_CONTROL] =	TPA6130A2_SWS;
	data->regs[TPA6130A2_REG_VOL_MUTE] =	TPA6130A2_MUTE_R |
						TPA6130A2_MUTE_L;

	if (data->power_gpio >= 0) {
		ret = gpio_request(data->power_gpio, "tpa6130a2 enable");
		if (ret < 0) {
			dev_err(dev, "Failed to request power GPIO (%d)\n",
				data->power_gpio);
			goto err_gpio;
		}
		gpio_direction_output(data->power_gpio, 0);
	}

	ret = tpa6130a2_power(1);
	if (ret != 0)
		goto err_power;

	/* Read version */
	ret = tpa6130a2_i2c_read(TPA6130A2_REG_VERSION) &
				 TPA6130A2_VERSION_MASK;
	if ((ret != 1) && (ret != 2))
		dev_warn(dev, "UNTESTED version detected (%d)\n", ret);

	/* Disable the chip */
	ret = tpa6130a2_power(0);
	if (ret != 0)
		goto err_power;

	return 0;
err_power:
	dev_err(dev, "err_power\n");
err_gpio:
	tpa6130a2_client = NULL;

	return ret;
}

static int __devexit tpa6130a2_remove(struct i2c_client *client)
{
	struct tpa6130a2_data *data = i2c_get_clientdata(client);

	tpa6130a2_power(0);

	if (data->power_gpio >= 0)
		gpio_free(data->power_gpio);

	tpa6130a2_client = NULL;

	return 0;
}

static const struct i2c_device_id tpa6130a2_id[] = {
	{ "tpa6130a2", TPA6130A2 },
	{ "tpa6140a2", TPA6140A2 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tpa6130a2_id);

static struct of_device_id tpa6130a2_match_table[] = {
	{ .compatible = "ti,tpa6130a2",},
	{ },
};

static struct i2c_driver tpa6130a2_i2c_driver = {
	.driver = {
		.name = "tpa6130a2",
		.owner = THIS_MODULE,
		.of_match_table = tpa6130a2_match_table,
	},
	.probe = tpa6130a2_probe,
	.remove = __devexit_p(tpa6130a2_remove),
	.id_table = tpa6130a2_id,
};

static int __init tpa6130a2_init(void)
{
	return i2c_add_driver(&tpa6130a2_i2c_driver);
}

static void __exit tpa6130a2_exit(void)
{
	i2c_del_driver(&tpa6130a2_i2c_driver);
}

MODULE_AUTHOR("Peter Ujfalusi <peter.ujfalusi@ti.com>");
MODULE_DESCRIPTION("TPA6130A2 Headphone amplifier driver");
MODULE_LICENSE("GPL");

module_init(tpa6130a2_init);
module_exit(tpa6130a2_exit);
