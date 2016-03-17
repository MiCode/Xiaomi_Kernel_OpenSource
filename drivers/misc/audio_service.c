/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/msm_ion.h>
#include <linux/msm_audio_ion.h>
#include <sound/audio_service.h>
#include <sound/audio_cal_utils.h>

extern int opalum_afe_set_calibration_data(int lowTemp, int highTemp);
extern int opalum_afe_set_preset(int preset);

struct audio_cal_info {
	struct mutex	common_lock;
};

struct audio_smart_pa_cal {
	   unsigned int lowTemp;
	   unsigned int highTemp;
};

static struct audio_cal_info	audio_cal;

static int audio_service_open(struct inode *inode, struct file *f)
{
	int ret = 0;
	pr_info("%s\n", __func__);

	return ret;
}

static int audio_service_release(struct inode *inode, struct file *f)
{
	int ret = 0;
	pr_info("%s\n", __func__);

	return ret;
}

static long audio_service_shared_ioctl(struct file *file, unsigned int cmd,
							void __user *arg)
{
	int ret = 0;
	int preset = 0;
	struct audio_smart_pa_cal smart_pa_cal;
	memset(&smart_pa_cal, 0x00, sizeof(smart_pa_cal));

	pr_info("%s\n", __func__);

	switch (cmd) {
	case AUDIO_SERVICE_CAL_SET:
		if (copy_from_user(&smart_pa_cal, arg, sizeof(smart_pa_cal))) {
			pr_err("%s: copy from user failed.\n", __func__);
				  return -EFAULT;
		}
		pr_info("SmartPA lowTemp:%d, highTemp:%d", smart_pa_cal.lowTemp, smart_pa_cal.highTemp);
		opalum_afe_set_calibration_data(smart_pa_cal.lowTemp, smart_pa_cal.highTemp);
		break;
	case AUDIO_SERVICE_CAL_GET:
		break;
	case AUDIO_SERVICE_PRESET_SET:
		if (copy_from_user(&preset, arg, sizeof(int))) {
			pr_err("%s: copy from user failed.\n", __func__);
				  return -EFAULT;
		}
		pr_info("SmartPA set Preset:%d", preset);
		opalum_afe_set_preset(preset);
		break;
	default:
		pr_err("%s: ioctl not found!\n", __func__);
		ret = -EFAULT;
		goto done;
	}

done:
	return ret;
}

static long audio_service_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	return audio_service_shared_ioctl(f, cmd, (void __user *)arg);
}

#ifdef CONFIG_COMPAT

#define AUDIO_SERVICE_IOCTL_MAGIC 'a'

#define AUDIO_SERVICE_CAL_SET32 _IOWR(AUDIO_SERVICE_IOCTL_MAGIC, \
							1, compat_uptr_t)
#define AUDIO_SERVICE_CAL_GET32 _IOWR(AUDIO_SERVICE_IOCTL_MAGIC, \
							2, compat_uptr_t)
#define AUDIO_SERVICE_PRESET_SET32 _IOWR(AUDIO_SERVICE_IOCTL_MAGIC, \
							3, compat_uptr_t)

static long audio_service_compat_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	unsigned int cmd64;
	int ret = 0;

	switch (cmd) {
	case AUDIO_SERVICE_CAL_SET32:
		cmd64 = AUDIO_SERVICE_CAL_SET;
		break;
	case AUDIO_SERVICE_CAL_GET32:
		cmd64 = AUDIO_SERVICE_CAL_GET;
		break;
	case AUDIO_SERVICE_PRESET_SET32:
		cmd64 = AUDIO_SERVICE_PRESET_SET;
		break;
	default:
		pr_err("%s: ioctl not found!\n", __func__);
		ret = -EFAULT;
		goto done;
	}

	ret = audio_service_shared_ioctl(f, cmd64, compat_ptr(arg));
done:
	return ret;
}
#endif

static const struct file_operations audio_fops = {
	.owner = THIS_MODULE,
	.open = audio_service_open,
	.release = audio_service_release,
	.unlocked_ioctl = audio_service_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =   audio_service_compat_ioctl,
#endif
};

struct miscdevice audio_service_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "audio_service",
	.fops	= &audio_fops,
};

static int __init audio_service_init(void)
{
	pr_info("%s\n", __func__);

	memset(&audio_cal, 0, sizeof(audio_cal));
	mutex_init(&audio_cal.common_lock);

	return misc_register(&audio_service_misc);
}

static void __exit audio_service_exit(void)
{

}

subsys_initcall(audio_service_init);
module_exit(audio_service_exit);

MODULE_DESCRIPTION("Audio service for DSP driver");
MODULE_LICENSE("GPL v2");
