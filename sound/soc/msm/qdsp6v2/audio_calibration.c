/* Copyright (c) 2014, 2016 The Linux Foundation. All rights reserved.
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
#include <sound/audio_calibration.h>
#include <sound/audio_cal_utils.h>

struct audio_cal_client_info {
	struct list_head		list;
	struct audio_cal_callbacks	*callbacks;
};

struct audio_cal_info {
	struct mutex			common_lock;
	struct mutex			cal_mutex[MAX_CAL_TYPES];
	struct list_head		client_info[MAX_CAL_TYPES];
	int				ref_count;
};

static struct audio_cal_info	audio_cal;


static bool callbacks_are_equal(struct audio_cal_callbacks *callback1,
				struct audio_cal_callbacks *callback2)
{
	bool				ret = true;
	struct audio_cal_callbacks	*call1 = callback1;
	struct audio_cal_callbacks	*call2 = callback2;
	pr_debug("%s\n", __func__);

	if ((call1 == NULL) && (call2 == NULL))
		ret = true;
	else if ((call1 == NULL) || (call2 == NULL))
		ret = false;
	else if ((call1->alloc != call2->alloc) ||
		(call1->dealloc != call2->dealloc) ||
		(call1->pre_cal != call2->pre_cal) ||
		(call1->set_cal != call2->set_cal) ||
		(call1->get_cal != call2->get_cal) ||
		(call1->post_cal != call2->post_cal))
		ret = false;
	return ret;
}

int audio_cal_deregister(int num_cal_types,
			 struct audio_cal_reg *reg_data)
{
	int				ret = 0;
	int				i = 0;
	struct list_head		*ptr, *next;
	struct audio_cal_client_info	*client_info_node = NULL;
	pr_debug("%s\n", __func__);

	if (reg_data == NULL) {
		pr_err("%s: reg_data is NULL!\n", __func__);
		ret = -EINVAL;
		goto done;
	} else if ((num_cal_types <= 0) ||
		(num_cal_types > MAX_CAL_TYPES)) {
		pr_err("%s: num_cal_types of %d is Invalid!\n",
			__func__, num_cal_types);
		ret = -EINVAL;
		goto done;
	}

	for (; i < num_cal_types; i++) {
		if ((reg_data[i].cal_type < 0) ||
			(reg_data[i].cal_type >= MAX_CAL_TYPES)) {
			pr_err("%s: cal type %d at index %d is Invalid!\n",
				__func__, reg_data[i].cal_type, i);
			ret = -EINVAL;
			continue;
		}

		mutex_lock(&audio_cal.cal_mutex[reg_data[i].cal_type]);
		list_for_each_safe(ptr, next,
			&audio_cal.client_info[reg_data[i].cal_type]) {

			client_info_node = list_entry(ptr,
				struct audio_cal_client_info, list);
			if (callbacks_are_equal(client_info_node->callbacks,
				&reg_data[i].callbacks)) {
				list_del(&client_info_node->list);
				kfree(client_info_node->callbacks);
				client_info_node->callbacks = NULL;
				kfree(client_info_node);
				client_info_node = NULL;
				break;
			}
		}
		mutex_unlock(&audio_cal.cal_mutex[reg_data[i].cal_type]);
	}
done:
	return ret;
}


int audio_cal_register(int num_cal_types,
			 struct audio_cal_reg *reg_data)
{
	int				ret = 0;
	int				i = 0;
	struct audio_cal_client_info	*client_info_node = NULL;
	struct audio_cal_callbacks	*callback_node = NULL;
	pr_debug("%s\n", __func__);

	if (reg_data == NULL) {
		pr_err("%s: callbacks are NULL!\n", __func__);
		ret = -EINVAL;
		goto done;
	} else if ((num_cal_types <= 0) ||
		(num_cal_types > MAX_CAL_TYPES)) {
		pr_err("%s: num_cal_types of %d is Invalid!\n",
			__func__, num_cal_types);
		ret = -EINVAL;
		goto done;
	}

	for (; i < num_cal_types; i++) {
		if ((reg_data[i].cal_type < 0) ||
			(reg_data[i].cal_type >= MAX_CAL_TYPES)) {
			pr_err("%s: cal type %d at index %d is Invalid!\n",
				__func__, reg_data[i].cal_type, i);
			ret = -EINVAL;
			goto err;
		}

		client_info_node = kmalloc(sizeof(*client_info_node),
			GFP_KERNEL);
		if (client_info_node == NULL) {
			pr_err("%s: could not allocated client_info_node!\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		INIT_LIST_HEAD(&client_info_node->list);

		callback_node = kmalloc(sizeof(*callback_node),
			GFP_KERNEL);
		if (callback_node == NULL) {
			pr_err("%s: could not allocated callback_node!\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}

		memcpy(callback_node, &reg_data[i].callbacks,
			sizeof(*callback_node));
		client_info_node->callbacks = callback_node;

		mutex_lock(&audio_cal.cal_mutex[reg_data[i].cal_type]);
		list_add_tail(&client_info_node->list,
			&audio_cal.client_info[reg_data[i].cal_type]);
		mutex_unlock(&audio_cal.cal_mutex[reg_data[i].cal_type]);
	}
done:
	return ret;
err:
	audio_cal_deregister(num_cal_types, reg_data);
	return ret;
}

static int call_allocs(int32_t cal_type,
				size_t cal_type_size, void *data)
{
	int				ret = 0;
	int				ret2 = 0;
	struct list_head		*ptr, *next;
	struct audio_cal_client_info	*client_info_node = NULL;
	pr_debug("%s\n", __func__);

	list_for_each_safe(ptr, next,
			&audio_cal.client_info[cal_type]) {

		client_info_node = list_entry(ptr,
			struct audio_cal_client_info, list);

		if (client_info_node->callbacks->alloc == NULL)
			continue;

		ret2 = client_info_node->callbacks->
			alloc(cal_type, cal_type_size, data);
		if (ret2 < 0) {
			pr_err("%s: alloc failed!\n", __func__);
			ret = ret2;
		}
	}
	return ret;
}

static int call_deallocs(int32_t cal_type,
				size_t cal_type_size, void *data)
{
	int				ret = 0;
	int				ret2 = 0;
	struct list_head		*ptr, *next;
	struct audio_cal_client_info	*client_info_node = NULL;
	pr_debug("%s cal type %d\n", __func__, cal_type);

	list_for_each_safe(ptr, next,
			&audio_cal.client_info[cal_type]) {

		client_info_node = list_entry(ptr,
			struct audio_cal_client_info, list);

		if (client_info_node->callbacks->dealloc == NULL)
			continue;

		ret2 = client_info_node->callbacks->
			dealloc(cal_type, cal_type_size, data);
		if (ret2 < 0) {
			pr_err("%s: dealloc failed!\n", __func__);
			ret = ret2;
		}
	}
	return ret;
}

static int call_pre_cals(int32_t cal_type,
				size_t cal_type_size, void *data)
{
	int				ret = 0;
	int				ret2 = 0;
	struct list_head		*ptr, *next;
	struct audio_cal_client_info	*client_info_node = NULL;
	pr_debug("%s cal type %d\n", __func__, cal_type);

	list_for_each_safe(ptr, next,
			&audio_cal.client_info[cal_type]) {

		client_info_node = list_entry(ptr,
			struct audio_cal_client_info, list);

		if (client_info_node->callbacks->pre_cal == NULL)
			continue;

		ret2 = client_info_node->callbacks->
			pre_cal(cal_type, cal_type_size, data);
		if (ret2 < 0) {
			pr_err("%s: pre_cal failed!\n", __func__);
			ret = ret2;
		}
	}
	return ret;
}

static int call_post_cals(int32_t cal_type,
				size_t cal_type_size, void *data)
{
	int				ret = 0;
	int				ret2 = 0;
	struct list_head		*ptr, *next;
	struct audio_cal_client_info	*client_info_node = NULL;
	pr_debug("%s cal type %d\n", __func__, cal_type);

	list_for_each_safe(ptr, next,
			&audio_cal.client_info[cal_type]) {

		client_info_node = list_entry(ptr,
			struct audio_cal_client_info, list);

		if (client_info_node->callbacks->post_cal == NULL)
			continue;

		ret2 = client_info_node->callbacks->
			post_cal(cal_type, cal_type_size, data);
		if (ret2 < 0) {
			pr_err("%s: post_cal failed!\n", __func__);
			ret = ret2;
		}
	}
	return ret;
}

static int call_set_cals(int32_t cal_type,
				size_t cal_type_size, void *data)
{
	int				ret = 0;
	int				ret2 = 0;
	struct list_head		*ptr, *next;
	struct audio_cal_client_info	*client_info_node = NULL;
	pr_debug("%s cal type %d\n", __func__, cal_type);

	list_for_each_safe(ptr, next,
			&audio_cal.client_info[cal_type]) {

		client_info_node = list_entry(ptr,
			struct audio_cal_client_info, list);

		if (client_info_node->callbacks->set_cal == NULL)
			continue;

		ret2 = client_info_node->callbacks->
			set_cal(cal_type, cal_type_size, data);
		if (ret2 < 0) {
			pr_err("%s: set_cal failed!\n", __func__);
			ret = ret2;
		}
	}
	return ret;
}

static int call_get_cals(int32_t cal_type,
				size_t cal_type_size, void *data)
{
	int				ret = 0;
	int				ret2 = 0;
	struct list_head		*ptr, *next;
	struct audio_cal_client_info	*client_info_node = NULL;
	pr_debug("%s cal type %d\n", __func__, cal_type);

	list_for_each_safe(ptr, next,
			&audio_cal.client_info[cal_type]) {

		client_info_node = list_entry(ptr,
			struct audio_cal_client_info, list);

		if (client_info_node->callbacks->get_cal == NULL)
			continue;

		ret2 = client_info_node->callbacks->
			get_cal(cal_type, cal_type_size, data);
		if (ret2 < 0) {
			pr_err("%s: get_cal failed!\n", __func__);
			ret = ret2;
		}
	}
	return ret;
}

static int audio_cal_open(struct inode *inode, struct file *f)
{
	int ret = 0;
	pr_debug("%s\n", __func__);

	mutex_lock(&audio_cal.common_lock);
	audio_cal.ref_count++;
	mutex_unlock(&audio_cal.common_lock);

	return ret;
}

static void dealloc_all_clients(void)
{
	int				i = 0;
	struct audio_cal_type_dealloc	dealloc_data;
	pr_debug("%s\n", __func__);

	dealloc_data.cal_hdr.version = VERSION_0_0;
	dealloc_data.cal_hdr.buffer_number = ALL_CAL_BLOCKS;
	dealloc_data.cal_data.mem_handle = -1;

	for (; i < MAX_CAL_TYPES; i++)
		call_deallocs(i, sizeof(dealloc_data), &dealloc_data);
}

static int audio_cal_release(struct inode *inode, struct file *f)
{
	int ret = 0;
	pr_debug("%s\n", __func__);

	mutex_lock(&audio_cal.common_lock);
	audio_cal.ref_count--;
	if (audio_cal.ref_count <= 0) {
		audio_cal.ref_count = 0;
		dealloc_all_clients();
	}
	mutex_unlock(&audio_cal.common_lock);

	return ret;
}

static long audio_cal_shared_ioctl(struct file *file, unsigned int cmd,
							void __user *arg)
{
	int				ret = 0;
	int32_t				size;
	struct audio_cal_basic		*data = NULL;
	pr_debug("%s\n", __func__);

	switch (cmd) {
	case AUDIO_ALLOCATE_CALIBRATION:
	case AUDIO_DEALLOCATE_CALIBRATION:
	case AUDIO_PREPARE_CALIBRATION:
	case AUDIO_SET_CALIBRATION:
	case AUDIO_GET_CALIBRATION:
	case AUDIO_POST_CALIBRATION:
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
	} else if ((size < sizeof(struct audio_cal_basic))
		|| (size > MAX_IOCTL_CMD_SIZE)) {
		pr_err("%s: Invalid size sent to driver: %d, max size is %d, min size is %zd\n",
			__func__, size, MAX_IOCTL_CMD_SIZE,
			sizeof(struct audio_cal_basic));
		ret = -EINVAL;
		goto done;
	}

	data = kmalloc(size, GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s: Could not allocate memory of size %d for ioctl\n",
			__func__, size);
		ret = -ENOMEM;
		goto done;
	} else if (copy_from_user(data, (void *)arg, size)) {
		pr_err("%s: Could not copy data from user\n",
			__func__);
		ret = -EFAULT;
		goto done;
	} else if ((data->hdr.cal_type < 0) ||
		(data->hdr.cal_type >= MAX_CAL_TYPES)) {
		pr_err("%s: cal type %d is Invalid!\n",
			__func__, data->hdr.cal_type);
		ret = -EINVAL;
		goto done;
	} else if ((data->hdr.cal_type_size <
		sizeof(struct audio_cal_type_basic)) ||
		(data->hdr.cal_type_size >
		get_user_cal_type_size(data->hdr.cal_type))) {
		pr_err("%s: cal type size %d is Invalid! Max is %zd!\n",
			__func__, data->hdr.cal_type_size,
			get_user_cal_type_size(data->hdr.cal_type));
		ret = -EINVAL;
		goto done;
	} else if (data->cal_type.cal_hdr.buffer_number < 0) {
		pr_err("%s: cal type %d Invalid buffer number %d!\n",
			__func__, data->hdr.cal_type,
			data->cal_type.cal_hdr.buffer_number);
		ret = -EINVAL;
		goto done;
	}


	mutex_lock(&audio_cal.cal_mutex[data->hdr.cal_type]);

	switch (cmd) {
	case AUDIO_ALLOCATE_CALIBRATION:
		ret = call_allocs(data->hdr.cal_type,
			data->hdr.cal_type_size, &data->cal_type);
		break;
	case AUDIO_DEALLOCATE_CALIBRATION:
		ret = call_deallocs(data->hdr.cal_type,
			data->hdr.cal_type_size, &data->cal_type);
		break;
	case AUDIO_PREPARE_CALIBRATION:
		ret = call_pre_cals(data->hdr.cal_type,
			data->hdr.cal_type_size, &data->cal_type);
		break;
	case AUDIO_SET_CALIBRATION:
		ret = call_set_cals(data->hdr.cal_type,
			data->hdr.cal_type_size, &data->cal_type);
		break;
	case AUDIO_GET_CALIBRATION:
		ret = call_get_cals(data->hdr.cal_type,
			data->hdr.cal_type_size, &data->cal_type);
		break;
	case AUDIO_POST_CALIBRATION:
		ret = call_post_cals(data->hdr.cal_type,
			data->hdr.cal_type_size, &data->cal_type);
		break;
	}

	if (cmd == AUDIO_GET_CALIBRATION) {
		if (data->hdr.cal_type_size == 0)
			goto unlock;
		if (data == NULL)
			goto unlock;
		if ((sizeof(data->hdr) + data->hdr.cal_type_size) > size) {
			pr_err("%s: header size %zd plus cal type size %d are greater than data buffer size %d\n",
				__func__, sizeof(data->hdr),
				data->hdr.cal_type_size, size);
			ret = -EFAULT;
			goto unlock;
		} else if (copy_to_user((void *)arg, data,
			sizeof(data->hdr) + data->hdr.cal_type_size)) {
			pr_err("%s: Could not copy cal type to user\n",
				__func__);
			ret = -EFAULT;
			goto unlock;
		}
	}

unlock:
	mutex_unlock(&audio_cal.cal_mutex[data->hdr.cal_type]);
done:
	kfree(data);
	return ret;
}

static long audio_cal_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	return audio_cal_shared_ioctl(f, cmd, (void __user *)arg);
}

#ifdef CONFIG_COMPAT

#define AUDIO_ALLOCATE_CALIBRATION32	_IOWR(CAL_IOCTL_MAGIC, \
							200, compat_uptr_t)
#define AUDIO_DEALLOCATE_CALIBRATION32	_IOWR(CAL_IOCTL_MAGIC, \
							201, compat_uptr_t)
#define AUDIO_PREPARE_CALIBRATION32	_IOWR(CAL_IOCTL_MAGIC, \
							202, compat_uptr_t)
#define AUDIO_SET_CALIBRATION32		_IOWR(CAL_IOCTL_MAGIC, \
							203, compat_uptr_t)
#define AUDIO_GET_CALIBRATION32		_IOWR(CAL_IOCTL_MAGIC, \
							204, compat_uptr_t)
#define AUDIO_POST_CALIBRATION32	_IOWR(CAL_IOCTL_MAGIC, \
							205, compat_uptr_t)

static long audio_cal_compat_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	unsigned int cmd64;
	int ret = 0;

	switch (cmd) {
	case AUDIO_ALLOCATE_CALIBRATION32:
		cmd64 = AUDIO_ALLOCATE_CALIBRATION;
		break;
	case AUDIO_DEALLOCATE_CALIBRATION32:
		cmd64 = AUDIO_DEALLOCATE_CALIBRATION;
		break;
	case AUDIO_PREPARE_CALIBRATION32:
		cmd64 = AUDIO_PREPARE_CALIBRATION;
		break;
	case AUDIO_SET_CALIBRATION32:
		cmd64 = AUDIO_SET_CALIBRATION;
		break;
	case AUDIO_GET_CALIBRATION32:
		cmd64 = AUDIO_GET_CALIBRATION;
		break;
	case AUDIO_POST_CALIBRATION32:
		cmd64 = AUDIO_POST_CALIBRATION;
		break;
	default:
		pr_err("%s: ioctl not found!\n", __func__);
		ret = -EFAULT;
		goto done;
	}

	ret = audio_cal_shared_ioctl(f, cmd64, compat_ptr(arg));
done:
	return ret;
}
#endif

static const struct file_operations audio_cal_fops = {
	.owner = THIS_MODULE,
	.open = audio_cal_open,
	.release = audio_cal_release,
	.unlocked_ioctl = audio_cal_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =   audio_cal_compat_ioctl,
#endif
};

struct miscdevice audio_cal_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "msm_audio_cal",
	.fops	= &audio_cal_fops,
};

static int __init audio_cal_init(void)
{
	int i = 0;
	pr_debug("%s\n", __func__);

	memset(&audio_cal, 0, sizeof(audio_cal));
	mutex_init(&audio_cal.common_lock);
	for (; i < MAX_CAL_TYPES; i++) {
		INIT_LIST_HEAD(&audio_cal.client_info[i]);
		mutex_init(&audio_cal.cal_mutex[i]);
	}

	return misc_register(&audio_cal_misc);
}

static void __exit audio_cal_exit(void)
{
	int				i = 0;
	struct list_head		*ptr, *next;
	struct audio_cal_client_info	*client_info_node;

	for (; i < MAX_CAL_TYPES; i++) {
		list_for_each_safe(ptr, next,
			&audio_cal.client_info[i]) {
			client_info_node = list_entry(ptr,
				struct audio_cal_client_info, list);
			list_del(&client_info_node->list);
			kfree(client_info_node->callbacks);
			client_info_node->callbacks = NULL;
			kfree(client_info_node);
			client_info_node = NULL;
		}
	}
}

subsys_initcall(audio_cal_init);
module_exit(audio_cal_exit);

MODULE_DESCRIPTION("SoC QDSP6v2 Audio Calibration driver");
MODULE_LICENSE("GPL v2");
