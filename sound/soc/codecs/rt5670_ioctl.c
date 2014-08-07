/*
 * rt5670_ioctl.h  --  RT5670 ALSA SoC audio driver IO control
 *
 * Copyright 2012 Realtek Microelectronics
 * Author: Bard <bardliao@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/spi/spi.h>
#include <sound/soc.h>
#include "rt_codec_ioctl.h"
#include "rt5670_ioctl.h"
#include "rt5670.h"
#include "rt5670-dsp.h"

static struct hweq_s hweq_param[] = {
	{/* NORMAL */
		{0},
		{0},
		0x0000,
	},
	{/* SPK */
		{0},
		{0x1c10, 0x01f4, 0xc5e9, 0x1a98, 0x1d2c, 0xc882, 0x1c10,
		0x01f4, 0xe904, 0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4,
		0x1c10, 0x01f4, 0x2000, 0x0000, 0x2000},
		0x0000,
	},
	{/* HP */
		{0},
		{0x1c10, 0x01f4, 0xc5e9, 0x1a98, 0x1d2c, 0xc882, 0x1c10,
		0x01f4, 0xe904, 0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4,
		0x1c10,	0x01f4,	0x2000,	0x0000,	0x2000},
		0x0000,
	},
};
#define RT5670_HWEQ_LEN ARRAY_SIZE(hweq_param)

int eqreg[EQ_CH_NUM][EQ_REG_NUM] = {
	{0xa4, 0xa5, 0xeb, 0xec, 0xed, 0xee, 0xe7, 0xe8, 0xe9, 0xea, 0xe5,
	 0xe6, 0xae, 0xaf, 0xb0, 0xb4, 0xb5, 0xb6, 0xba, 0xbb, 0xbc, 0xc0,
	 0xc1, 0xc4, 0xc5, 0xc6, 0xca, 0xcc},
	{0xa6, 0xa7, 0xf5, 0xf6, 0xf7, 0xf8, 0xf1, 0xf2, 0xf3, 0xf4, 0xef,
	 0xf0, 0xb1, 0xb2, 0xb3, 0xb7, 0xb8, 0xb9, 0xbd, 0xbe, 0xbf, 0xc2,
	 0xc3, 0xc7, 0xc8, 0xc9, 0xcb, 0xcd},
	{0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
	 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xe1, 0xe2},
};

int rt5670_update_eqmode(
	struct snd_soc_codec *codec, int channel, int mode)
{
	struct rt_codec_ops *ioctl_ops = rt_codec_get_ioctl_ops();
	int i;

	if (codec == NULL ||  mode >= RT5670_HWEQ_LEN)
		return -EINVAL;

	dev_dbg(codec->dev, "%s(): mode=%d\n", __func__, mode);

	for (i = 0; i < EQ_REG_NUM; i++)
		hweq_param[mode].reg[i] = eqreg[channel][i];

	for (i = 0; i < EQ_REG_NUM; i++) {
		if (hweq_param[mode].reg[i])
			ioctl_ops->index_write(codec, hweq_param[mode].reg[i],
					hweq_param[mode].value[i]);
		else
			break;
	}
	snd_soc_update_bits(codec, RT5670_EQ_CTRL2, RT5670_EQ_CTRL_MASK,
					hweq_param[mode].ctrl);
	snd_soc_update_bits(codec, RT5670_EQ_CTRL1,
		RT5670_EQ_UPD, RT5670_EQ_UPD);
	snd_soc_update_bits(codec, RT5670_EQ_CTRL1, RT5670_EQ_UPD, 0);

	return 0;
}

int rt5670_ioctl_common(struct snd_hwdep *hw, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct snd_soc_codec *codec = hw->private_data;
	struct rt_codec_cmd __user *_rt_codec = (struct rt_codec_cmd *)arg;
	struct rt_codec_cmd rt_codec;
	int *buf, ret = 0;
	static int eq_mode[EQ_CH_NUM];

	if (copy_from_user(&rt_codec, _rt_codec, sizeof(rt_codec))) {
		dev_err(codec->dev, "copy_from_user faild\n");
		return -EFAULT;
	}
	dev_dbg(codec->dev, "%s(): rt_codec.number=%zu, cmd=%d\n",
			__func__, rt_codec.number, cmd);
	buf = kmalloc(sizeof(*buf) * rt_codec.number, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;
	if (copy_from_user(buf, rt_codec.buf, sizeof(*buf) * rt_codec.number)) {
		ret = -EFAULT;
		goto err;
	}

	switch (cmd) {
	case RT_SET_CODEC_HWEQ_IOCTL:
		if (eq_mode[*buf] == *(buf + 1))
			break;
		eq_mode[*buf] = *(buf + 1);
		rt5670_update_eqmode(codec, eq_mode[*buf], *buf);
		break;

	case RT_GET_CODEC_ID:
		*buf = snd_soc_read(codec, RT5670_VENDOR_ID2);
		if (copy_to_user(rt_codec.buf, buf,
			sizeof(*buf) * rt_codec.number)) {
			ret = -EFAULT;
			goto err;
		}
		break;
	case RT_READ_CODEC_DSP_IOCTL:
	case RT_WRITE_CODEC_DSP_IOCTL:
	case RT_GET_CODEC_DSP_MODE_IOCTL:
		ret =  rt5670_dsp_ioctl_common(hw, file, cmd, arg);
		break;
	default:
		break;
	}

err:
	kfree(buf);
	return ret;
}
EXPORT_SYMBOL_GPL(rt5670_ioctl_common);
