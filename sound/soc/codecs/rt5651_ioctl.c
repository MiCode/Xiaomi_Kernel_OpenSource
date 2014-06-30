/*
 * rt5651_ioctl.h  --  RT5651 ALSA SoC audio driver IO control
 *
 * Copyright 2012 Realtek Microelectronics
 * Author: Bard <bardliao@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <sound/soc.h>
#include "rt56xx_ioctl.h"
#include "rt5651_ioctl.h"
#include "rt5651.h"

int rt5651_ioctl_common(struct snd_hwdep *hw, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct snd_soc_codec *codec = hw->private_data;
	struct rt56xx_cmd __user *_rt56xx = (struct rt56xx_cmd *)arg;
	struct rt56xx_cmd rt56xx;
	struct rt56xx_ops *ioctl_ops = rt56xx_get_ioctl_ops();
	int *buf, mask1 = 0, mask2 = 0;
	static int eq_mode;

	if (copy_from_user(&rt56xx, _rt56xx, sizeof(rt56xx))) {
		dev_err(codec->dev, "copy_from_user faild\n");
		return -EFAULT;
	}
	dev_dbg(codec->dev, "%s(): rt56xx.number=%d, cmd=%d\n",
			__func__, rt56xx.number, cmd);
	buf = kmalloc(sizeof(*buf) * rt56xx.number, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;
	if (copy_from_user(buf, rt56xx.buf, sizeof(*buf) * rt56xx.number))
		goto err;

	switch (cmd) {
	case RT_GET_CODEC_ID:
		*buf = snd_soc_read(codec, RT5651_DEVICE_ID);
		if (copy_to_user(rt56xx.buf, buf, sizeof(*buf) * rt56xx.number))
			goto err;
		break;


	default:
		break;
	}

	kfree(buf);
	return 0;

err:
	kfree(buf);
	return -EFAULT;
}
EXPORT_SYMBOL_GPL(rt5651_ioctl_common);
