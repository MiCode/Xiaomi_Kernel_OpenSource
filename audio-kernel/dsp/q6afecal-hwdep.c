/*
 * Copyright (c) 2015, 2017 - 2018 The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>
#include <sound/hwdep.h>
#include <sound/msmcal-hwdep.h>
#include <sound/soc.h>
#include "q6afecal-hwdep.h"

const int cal_size_info[Q6AFE_MAX_CAL] = {
	[Q6AFE_VAD_CORE_CAL] = 132,
};

const char *cal_name_info[Q6AFE_MAX_CAL] = {
	[Q6AFE_VAD_CORE_CAL] = "vad_core",
};

#define AFE_HW_NAME_LENGTH 40

/*
 * q6afecal_get_fw_cal -
 *        To get calibration from AFE HW dependent node
 *
 * @fw_data: pointer to firmware data
 * type: AFE calibration type
 *
 */
struct firmware_cal *q6afecal_get_fw_cal(struct afe_fw_info *fw_data,
					enum q6afe_cal_type type)
{
	if (!fw_data) {
		pr_err("%s: fw_data is NULL\n", __func__);
		return NULL;
	}
	if (type >= Q6AFE_MAX_CAL ||
		type < Q6AFE_MIN_CAL) {
		pr_err("%s: wrong cal type sent %d\n", __func__, type);
		return NULL;
	}
	mutex_lock(&fw_data->lock);
	if (!test_bit(Q6AFECAL_RECEIVED,
		&fw_data->q6afecal_state[type])) {
		pr_err("%s: cal not sent by userspace %d\n",
			__func__, type);
		mutex_unlock(&fw_data->lock);
		return NULL;
	}
	set_bit(Q6AFECAL_INITIALISED, &fw_data->q6afecal_state[type]);
	mutex_unlock(&fw_data->lock);
	return fw_data->fw[type];
}
EXPORT_SYMBOL(q6afecal_get_fw_cal);

static int q6afecal_hwdep_ioctl_shared(struct snd_hwdep *hw,
			struct q6afecal_ioctl_buffer fw_user)
{
	struct afe_fw_info *fw_data = hw->private_data;
	struct firmware_cal **fw = fw_data->fw;
	void *data;

	if (!test_bit(fw_user.cal_type, fw_data->cal_bit)) {
		pr_err("%s: q6afe didn't set this %d!!\n",
				__func__, fw_user.cal_type);
		return -EFAULT;
	}
	if (fw_user.cal_type >= Q6AFE_MAX_CAL ||
		fw_user.cal_type < Q6AFE_MIN_CAL) {
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
	if (copy_from_user(data, fw_user.buffer, fw_user.size))
		return -EFAULT;
	fw[fw_user.cal_type]->size = fw_user.size;
	mutex_lock(&fw_data->lock);
	set_bit(Q6AFECAL_RECEIVED, &fw_data->q6afecal_state[fw_user.cal_type]);
	mutex_unlock(&fw_data->lock);
	return 0;
}

#ifdef CONFIG_COMPAT
struct q6afecal_ioctl_buffer32 {
	u32 size;
	compat_uptr_t buffer;
	enum q6afe_cal_type cal_type;
};

enum {
	SNDRV_CTL_IOCTL_HWDEP_CAL_TYPE32 =
		_IOW('U', 0x1, struct q6afecal_ioctl_buffer32),
};

static int q6afecal_hwdep_ioctl_compat(struct snd_hwdep *hw, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct q6afecal_ioctl_buffer __user *argp = (void __user *)arg;
	struct q6afecal_ioctl_buffer32 fw_user32;
	struct q6afecal_ioctl_buffer fw_user_compat;

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
	return q6afecal_hwdep_ioctl_shared(hw, fw_user_compat);
}
#else
#define q6afecal_hwdep_ioctl_compat NULL
#endif

static int q6afecal_hwdep_ioctl(struct snd_hwdep *hw, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct q6afecal_ioctl_buffer __user *argp = (void __user *)arg;
	struct q6afecal_ioctl_buffer fw_user;

	if (cmd != SNDRV_IOCTL_HWDEP_VAD_CAL_TYPE) {
		pr_err("%s: wrong ioctl command sent %d!\n", __func__, cmd);
		return -ENOIOCTLCMD;
	}
	if (copy_from_user(&fw_user, argp, sizeof(fw_user))) {
		pr_err("%s: failed to copy\n", __func__);
		return -EFAULT;
	}
	return q6afecal_hwdep_ioctl_shared(hw, fw_user);
}

static int q6afecal_hwdep_release(struct snd_hwdep *hw, struct file *file)
{
	struct afe_fw_info *fw_data = hw->private_data;

	mutex_lock(&fw_data->lock);
	/* clear all the calibrations */
	memset(fw_data->q6afecal_state, 0,
		sizeof(fw_data->q6afecal_state));
	mutex_unlock(&fw_data->lock);
	return 0;
}

/**
 * q6afe_cal_create_hwdep -
 *         for creating HW dependent node for AFE
 *
 * @data: Pointer to hold fw data
 * @node: node type
 * @card: Pointer to sound card
 *
 */
int q6afe_cal_create_hwdep(void *data, int node, void *card)
{
	char hwname[AFE_HW_NAME_LENGTH];
	struct snd_hwdep *hwdep;
	struct firmware_cal **fw;
	struct afe_fw_info *fw_data = data;
	int err, cal_bit;

	if (!fw_data || !card) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	fw = fw_data->fw;
	snprintf(hwname, strlen("Q6AFE"), "Q6AFE");
	err = snd_hwdep_new(((struct snd_soc_card *)card)->snd_card,
			    hwname, node, &hwdep);
	if (err < 0) {
		pr_err("%s: new hwdep for q6afe failed %d\n", __func__, err);
		return err;
	}
	snprintf(hwdep->name, strlen("Q6AFECAL"), "Q6AFECAL");
	hwdep->iface = SNDRV_HWDEP_IFACE_AUDIO_BE;
	hwdep->private_data = fw_data;
	hwdep->ops.ioctl_compat = q6afecal_hwdep_ioctl_compat;
	hwdep->ops.ioctl = q6afecal_hwdep_ioctl;
	hwdep->ops.release = q6afecal_hwdep_release;
	mutex_init(&fw_data->lock);

	for_each_set_bit(cal_bit, fw_data->cal_bit, Q6AFE_MAX_CAL) {
		set_bit(Q6AFECAL_UNINITIALISED,
				&fw_data->q6afecal_state[cal_bit]);
		fw[cal_bit] = kzalloc(sizeof *(fw[cal_bit]), GFP_KERNEL);
		if (!fw[cal_bit]) {
			pr_err("%s: no memory for %s cal\n",
				__func__, cal_name_info[cal_bit]);
			goto end;
		}
	}
	for_each_set_bit(cal_bit, fw_data->cal_bit, Q6AFE_MAX_CAL) {
		fw[cal_bit]->data = kzalloc(cal_size_info[cal_bit],
						GFP_KERNEL);
		if (!fw[cal_bit]->data)
			goto exit;
		set_bit(Q6AFECAL_INITIALISED,
			&fw_data->q6afecal_state[cal_bit]);
	}
	return 0;
exit:
	for_each_set_bit(cal_bit, fw_data->cal_bit, Q6AFE_MAX_CAL) {
		kfree(fw[cal_bit]->data);
		fw[cal_bit]->data = NULL;
	}
end:
	for_each_set_bit(cal_bit, fw_data->cal_bit, Q6AFE_MAX_CAL) {
		kfree(fw[cal_bit]);
		fw[cal_bit] = NULL;
	}
	return -ENOMEM;
}
EXPORT_SYMBOL(q6afe_cal_create_hwdep);
