/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#include <linux/qdsp6v2/audio-anc-dev-mgr.h>

#define DEVICE_NAME "msm_audio_anc"

struct audio_anc_info {
	struct cdev myc;
	struct class *anc_class;
};

static int major;

static struct audio_anc_info	audio_anc;

static size_t get_user_anc_cmd_size(int32_t anc_cmd)
{
	size_t size = 0;

	switch (anc_cmd) {
	case ANC_CMD_START:
	case ANC_CMD_STOP:
		size = 0;
		break;
	case ANC_CMD_ALGO_MODULE:
		size = sizeof(struct audio_anc_algo_module_info);
		break;
	case ANC_CMD_ALGO_CALIBRATION:
		size = sizeof(struct audio_anc_algo_calibration_header);
		break;
	default:
		pr_err("%s:Invalid anc cmd %d!",
			__func__, anc_cmd);
	}
	return size;
}

static int call_set_anc(int32_t anc_cmd,
				size_t anc_cmd_size, void *data)
{
	int				ret = 0;

	pr_err("%s EXT_ANC anc_cmd %x\n", __func__, anc_cmd);

	switch (anc_cmd) {
	case ANC_CMD_START:
		ret = msm_anc_dev_start();
		break;
	case ANC_CMD_STOP:
		ret = msm_anc_dev_stop();
		break;
	case ANC_CMD_ALGO_MODULE:
	case ANC_CMD_ALGO_CALIBRATION:
		ret = msm_anc_dev_set_info(data, anc_cmd);
		break;
	default:
		break;
	}

	pr_err("%s EXT_ANC ret %x\n", __func__, ret);

	return ret;
}

static int call_get_anc(int32_t anc_cmd,
				size_t anc_cmd_size, void *data)
{
	int				ret = 0;

	switch (anc_cmd) {
	case ANC_CMD_ALGO_CALIBRATION:
		ret = msm_anc_dev_get_info(data, anc_cmd);
		break;
	default:
		break;
	}

	return ret;
}

static int audio_anc_open(struct inode *inode, struct file *f)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	return ret;
}

static int audio_anc_close(struct inode *inode, struct file *f)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	return ret;
}

static long audio_anc_shared_ioctl(struct file *file, unsigned int cmd,
							void __user *arg)
{
	int				ret = 0;
	int32_t				size;
	struct audio_anc_packet		*data = NULL;

	pr_err("%s EXT_ANC cmd %x\n", __func__, cmd);

	switch (cmd) {
	case AUDIO_ANC_SET_PARAM:
	case AUDIO_ANC_GET_PARAM:
		break;
	default:
		pr_err("%s: ioctl not found!\n", __func__);
		ret = -EFAULT;
		goto done;
	}

	if (copy_from_user(&size, (void *)arg, sizeof(size))) {
		pr_err("%s: Could not copy size value from user\n", __func__);
		ret = -EFAULT;
		goto done;
	} else if (size < sizeof(struct audio_anc_header)) {
		pr_err("%s: Invalid size sent to driver: %d, min size is %zd\n",
			__func__, size, sizeof(struct audio_anc_header));
		ret = -EINVAL;
		goto done;
	}

	data = kmalloc(size, GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		pr_err("%s: Could not allocate memory of size %d for ioctl\n",
			__func__, size);
		goto done;
	} else if (copy_from_user(data, (void *)arg, size)) {
		pr_err("%s: Could not copy data from user\n",
			__func__);
		ret = -EFAULT;
		goto done;
	} else if ((data->hdr.anc_cmd < 0) ||
		(data->hdr.anc_cmd >= ANC_CMD_MAX)) {
		pr_err("%s: anc_cmd %d is Invalid!\n",
			__func__, data->hdr.anc_cmd);
		ret = -EINVAL;
		goto done;
	} else if ((data->hdr.anc_cmd_size <
		get_user_anc_cmd_size(data->hdr.anc_cmd)) ||
		(data->hdr.anc_cmd_size >
		sizeof(union audio_anc_data))) {
		pr_err("%s: anc_cmd size %d is Invalid! Min is %zd Max is %zd!\n",
			__func__, data->hdr.anc_cmd_size,
			get_user_anc_cmd_size(data->hdr.anc_cmd),
			sizeof(union audio_anc_data));
		ret = -EINVAL;
		goto done;
	} else if ((data->hdr.anc_cmd_size + sizeof(data->hdr)) > size) {
		pr_err("%s: anc_cmd size %d + anc cmd hdr size %zd is is greater than user buffer siz %d!\n",
			__func__, data->hdr.anc_cmd_size, sizeof(data->hdr),
			size);
		ret = -EFAULT;
		goto done;
	}

	switch (cmd) {
	case AUDIO_ANC_SET_PARAM:
		ret = call_set_anc(data->hdr.anc_cmd,
		      data->hdr.anc_cmd_size, &data->anc_data);
		break;
	case AUDIO_ANC_GET_PARAM:
		ret = call_get_anc(data->hdr.anc_cmd,
			data->hdr.anc_cmd_size, &data->anc_data);
		break;
	}

	if (cmd == AUDIO_ANC_GET_PARAM) {
		if (data->hdr.anc_cmd_size == 0)
			goto done;
		if (data == NULL)
			goto done;
		if (copy_to_user(arg, data,
			sizeof(data->hdr) + data->hdr.anc_cmd_size)) {
			pr_err("%s: Could not copy anc data to user\n",
				__func__);
			ret = -EFAULT;
			goto done;
		}
	}

done:
	kfree(data);

	pr_err("%s EXT_ANC ret %x\n", __func__, ret);

	return ret;
}

static long audio_anc_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	return audio_anc_shared_ioctl(f, cmd, (void __user *)arg);
}

#ifdef CONFIG_COMPAT

#define AUDIO_ANC_SET_PARAM32		_IOWR(ANC_IOCTL_MAGIC, \
							300, compat_uptr_t)
#define AUDIO_ANC_GET_PARAM32		_IOWR(ANC_IOCTL_MAGIC, \
							301, compat_uptr_t)

static long audio_anc_compat_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	unsigned int cmd64;
	int ret = 0;

	switch (cmd) {
	case AUDIO_ANC_SET_PARAM32:
		cmd64 = AUDIO_ANC_SET_PARAM;
		break;
	case AUDIO_ANC_GET_PARAM32:
		cmd64 = AUDIO_ANC_GET_PARAM;
		break;
	default:
		pr_err("%s: ioctl not found!\n", __func__);
		ret = -EFAULT;
		goto done;
	}

	ret = audio_anc_shared_ioctl(f, cmd64, compat_ptr(arg));
done:
	return ret;
}
#endif

static const struct file_operations audio_anc_fops = {
	.owner = THIS_MODULE,
	.open = audio_anc_open,
	.release = audio_anc_close,
	.unlocked_ioctl = audio_anc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =   audio_anc_compat_ioctl,
#endif
};

int msm_anc_dev_create(struct platform_device *pdev)
{
	int result = 0;
	dev_t dev = MKDEV(major, 0);
	struct device *device_handle;

	pr_debug("%s\n", __func__);

	if (major) {
		result = register_chrdev_region(dev, 1, DEVICE_NAME);
	} else {
		result = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
		major = MAJOR(dev);
	}

	if (result < 0) {
		pr_err("%s: Registering msm_audio_anc device failed\n",
			__func__);
		goto done;
	}

	audio_anc.anc_class = class_create(THIS_MODULE, "msm_audio_anc");
	if (IS_ERR(audio_anc.anc_class)) {
		result = PTR_ERR(audio_anc.anc_class);
		pr_err("%s: Error creating anc class: %d\n",
			__func__, result);
		goto unregister_chrdev_region;
	}

	cdev_init(&audio_anc.myc, &audio_anc_fops);
	result = cdev_add(&audio_anc.myc, dev, 1);

	if (result < 0) {
		pr_err("%s: Registering file operations failed\n",
			__func__);
		goto class_destroy;
	}

	device_handle = device_create(audio_anc.anc_class,
			NULL, audio_anc.myc.dev, NULL, "msm_audio_anc");
	if (IS_ERR(device_handle)) {
		result = PTR_ERR(device_handle);
		pr_err("%s: device_create failed: %d\n", __func__, result);
		goto class_destroy;
	}

	pr_debug("exit %s\n", __func__);
	return 0;

class_destroy:
	class_destroy(audio_anc.anc_class);
unregister_chrdev_region:
	unregister_chrdev_region(MKDEV(major, 0), 1);
done:
	pr_err("exit %s\n", __func__);
	return result;
}

int msm_anc_dev_destroy(struct platform_device *pdev)
{
	device_destroy(audio_anc.anc_class, audio_anc.myc.dev);
	cdev_del(&audio_anc.myc);
	class_destroy(audio_anc.anc_class);
	unregister_chrdev_region(MKDEV(major, 0), 1);

	return 0;
}

static int __init audio_anc_init(void)
{
	return msm_anc_dev_init();
}

static void __exit audio_anc_exit(void)
{
	msm_anc_dev_deinit();
}

module_init(audio_anc_init);
module_exit(audio_anc_exit);

MODULE_DESCRIPTION("SoC QDSP6v2 Audio ANC driver");
MODULE_LICENSE("GPL v2");
