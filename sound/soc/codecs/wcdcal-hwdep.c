/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/bitops.h>
#include <sound/hwdep.h>
#include <sound/msmcal-hwdep.h>
#include <sound/soc.h>
#include "wcdcal-hwdep.h"

const int cal_size_info[WCD9XXX_MAX_CAL] = {
	[WCD9XXX_ANC_CAL] = 8192,
	[WCD9XXX_MBHC_CAL] = 4096,
	[WCD9XXX_MAD_CAL] = 4096,
};

const char *cal_name_info[WCD9XXX_MAX_CAL] = {
	[WCD9XXX_ANC_CAL] = "anc",
	[WCD9XXX_MBHC_CAL] = "mbhc",
	[WCD9XXX_MAD_CAL] = "mad",
};

struct firmware_cal *wcdcal_get_fw_cal(struct fw_info *fw_data,
					enum wcd_cal_type type)
{
	if (!fw_data) {
		pr_err("%s: fw_data is NULL\n", __func__);
		return NULL;
	}
	if (type >= WCD9XXX_MAX_CAL ||
		type < WCD9XXX_MIN_CAL) {
		pr_err("%s: wrong cal type sent %d\n", __func__, type);
		return NULL;
	}
	mutex_lock(&fw_data->lock);
	if (!test_bit(WCDCAL_RECIEVED,
		&fw_data->wcdcal_state[type])) {
		pr_err("%s: cal not sent by userspace %d\n",
			__func__, type);
		mutex_unlock(&fw_data->lock);
		return NULL;
	}
	mutex_unlock(&fw_data->lock);
	return fw_data->fw[type];
}
EXPORT_SYMBOL(wcdcal_get_fw_cal);

static int wcdcal_hwdep_ioctl_shared(struct snd_hwdep *hw,
			struct wcdcal_ioctl_buffer fw_user)
{
	struct fw_info *fw_data = hw->private_data;
	struct firmware_cal **fw = fw_data->fw;
	void *data;

	if (!test_bit(fw_user.cal_type, fw_data->cal_bit)) {
		pr_err("%s: codec didn't set this %d!!\n",
				__func__, fw_user.cal_type);
		return -EFAULT;
	}
	if (fw_user.cal_type >= WCD9XXX_MAX_CAL ||
		fw_user.cal_type < WCD9XXX_MIN_CAL) {
		pr_err("%s: wrong cal type sent %d\n",
				__func__, fw_user.cal_type);
		return -EFAULT;
	}
	if (fw_user.size > cal_size_info[fw_user.cal_type] ||
		fw_user.size <= 0) {
		pr_err("%s: incorrect firmware size %d for %s\n",
			__func__, fw_user.size,
			cal_name_info[fw_user.cal_type]);
		return -EFAULT;
	}
	data = fw[fw_user.cal_type]->data;
	memcpy(data, fw_user.buffer, fw_user.size);
	fw[fw_user.cal_type]->size = fw_user.size;
	mutex_lock(&fw_data->lock);
	set_bit(WCDCAL_RECIEVED, &fw_data->wcdcal_state[fw_user.cal_type]);
	mutex_unlock(&fw_data->lock);
	return 0;
}

#ifdef CONFIG_COMPAT
struct wcdcal_ioctl_buffer32 {
	u32 size;
	compat_uptr_t buffer;
	enum wcd_cal_type cal_type;
};

enum {
	SNDRV_CTL_IOCTL_HWDEP_CAL_TYPE32 =
		_IOW('U', 0x1, struct wcdcal_ioctl_buffer32),
};

static int wcdcal_hwdep_ioctl_compat(struct snd_hwdep *hw, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct wcdcal_ioctl_buffer __user *argp = (void __user *)arg;
	struct wcdcal_ioctl_buffer32 fw_user32;
	struct wcdcal_ioctl_buffer fw_user_compat;

	if (cmd != SNDRV_CTL_IOCTL_HWDEP_CAL_TYPE32) {
		pr_err("%s: wrong ioctl command sent %u!\n", __func__, cmd);
		return -ENOIOCTLCMD;
	}
	if (copy_from_user(&fw_user32, argp, sizeof(fw_user32))) {
		pr_err("%s: failed to copy\n", __func__);
		return -EFAULT;
	}
	fw_user_compat.size = fw_user32.size;
	fw_user_compat.buffer = compat_ptr(fw_user32.buffer);
	fw_user_compat.cal_type = fw_user32.cal_type;
	return wcdcal_hwdep_ioctl_shared(hw, fw_user_compat);
}
#else
#define wcdcal_hwdep_ioctl_compat NULL
#endif

static int wcdcal_hwdep_ioctl(struct snd_hwdep *hw, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct wcdcal_ioctl_buffer __user *argp = (void __user *)arg;
	struct wcdcal_ioctl_buffer fw_user;

	if (cmd != SNDRV_CTL_IOCTL_HWDEP_CAL_TYPE) {
		pr_err("%s: wrong ioctl command sent %d!\n", __func__, cmd);
		return -ENOIOCTLCMD;
	}
	if (copy_from_user(&fw_user, argp, sizeof(fw_user))) {
		pr_err("%s: failed to copy\n", __func__);
		return -EFAULT;
	}
	return wcdcal_hwdep_ioctl_shared(hw, fw_user);
}

static int wcdcal_hwdep_release(struct snd_hwdep *hw, struct file *file)
{
	struct fw_info *fw_data = hw->private_data;
	mutex_lock(&fw_data->lock);
	/* clear all the calibrations */
	memset(fw_data->wcdcal_state, 0,
		sizeof(fw_data->wcdcal_state));
	mutex_unlock(&fw_data->lock);
	return 0;
}

int wcd_cal_create_hwdep(void *data, int node, struct snd_soc_codec *codec)
{
	char hwname[40];
	struct snd_hwdep *hwdep;
	struct firmware_cal **fw;
	struct fw_info *fw_data = data;
	int err, cal_bit;

	if (!fw_data || !codec) {
		pr_err("%s: wrong arguments passed\n", __func__);
		return -EINVAL;
	}

	fw = fw_data->fw;
	snprintf(hwname, strlen("Codec %s"), "Codec %s", codec->name);
	err = snd_hwdep_new(codec->card->snd_card, hwname, node, &hwdep);
	if (err < 0) {
		dev_err(codec->dev, "%s: new hwdep failed %d\n",
				__func__, err);
		return err;
	}
	snprintf(hwdep->name, strlen("Codec %s"), "Codec %s", codec->name);
	hwdep->iface = SNDRV_HWDEP_IFACE_AUDIO_CODEC;
	hwdep->private_data = fw_data;
	hwdep->ops.ioctl_compat = wcdcal_hwdep_ioctl_compat;
	hwdep->ops.ioctl = wcdcal_hwdep_ioctl;
	hwdep->ops.release = wcdcal_hwdep_release;
	mutex_init(&fw_data->lock);

	for_each_set_bit(cal_bit, fw_data->cal_bit, WCD9XXX_MAX_CAL) {
		set_bit(WCDCAL_UNINITIALISED,
				&fw_data->wcdcal_state[cal_bit]);
		fw[cal_bit] = kzalloc(sizeof *(fw[cal_bit]), GFP_KERNEL);
		if (!fw[cal_bit]) {
			dev_err(codec->dev, "%s: no memory for %s cal\n",
				__func__, cal_name_info[cal_bit]);
			goto end;
		}
	}
	for_each_set_bit(cal_bit, fw_data->cal_bit, WCD9XXX_MAX_CAL) {
		fw[cal_bit]->data = kzalloc(cal_size_info[cal_bit],
						GFP_KERNEL);
		if (!fw[cal_bit]->data) {
			dev_err(codec->dev, "%s: no memory for %s cal data\n",
				__func__, cal_name_info[cal_bit]);
			goto exit;
		}
		set_bit(WCDCAL_INITIALISED,
			&fw_data->wcdcal_state[cal_bit]);
	}
	return 0;
exit:
	for_each_set_bit(cal_bit, fw_data->cal_bit, WCD9XXX_MAX_CAL) {
		kfree(fw[cal_bit]->data);
		fw[cal_bit]->data = NULL;
	}
end:
	for_each_set_bit(cal_bit, fw_data->cal_bit, WCD9XXX_MAX_CAL) {
		kfree(fw[cal_bit]);
		fw[cal_bit] = NULL;
	}
	return -ENOMEM;
}
EXPORT_SYMBOL(wcd_cal_create_hwdep);
