// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/sync_file.h>
#include <uapi/linux/qcom_sync_file.h>

#define CLASS_NAME	"sync"
#define DRV_NAME	"spec_sync"
#define DRV_VERSION	1
#define NAME_LEN	32

#define SPEC_FENCE_FLAG_FENCE_ARRAY 0x10 /* user flags for debug */
#define SPEC_FENCE_FLAG_ARRAY_BIND 0x11
#define FENCE_MIN	1
#define FENCE_MAX	32

struct sync_device {
	/* device info */
	struct class *dev_class;
	dev_t dev_num;
	struct device *dev;
	struct cdev *cdev;
	struct mutex lock;

	/* device drv data */
	atomic_t device_available;
	char name[NAME_LEN];
	uint32_t version;
	struct mutex l_lock;
	struct list_head fence_array_list;
};

struct fence_array_node {
	struct dma_fence_array *fence_array;
	struct list_head list;
};

/* Speculative Sync Device Driver State */
static struct sync_device sync_dev;

static bool sanitize_fence_array(struct dma_fence_array *fence)
{
	struct fence_array_node *node;
	int ret = false;

	mutex_lock(&sync_dev.l_lock);
	list_for_each_entry(node, &sync_dev.fence_array_list, list) {
		if (node->fence_array == fence) {
			ret = true;
			break;
		}
	}
	mutex_unlock(&sync_dev.l_lock);

	return ret;
}

static void clear_fence_array_tracker(bool force_clear)
{
	struct fence_array_node *node, *temp;
	struct dma_fence_array *array;
	struct dma_fence *fence;
	bool is_signaled;

	mutex_lock(&sync_dev.l_lock);
	list_for_each_entry_safe(node, temp, &sync_dev.fence_array_list, list) {
		array = node->fence_array;
		fence = &array->base;
		is_signaled = dma_fence_is_signaled(fence);

		if (force_clear && !array->fences)
			array->num_fences = 0;

		pr_debug("force_clear:%d is_signaled:%d pending:%d\n", force_clear, is_signaled,
			atomic_read(&array->num_pending));

		if (force_clear && !is_signaled && atomic_dec_and_test(&array->num_pending))
			dma_fence_signal(fence);

		if (force_clear || is_signaled) {
			dma_fence_put(fence);
			list_del(&node->list);
			kfree(node);
		}
	}
	mutex_unlock(&sync_dev.l_lock);
}

static struct sync_device *spec_fence_init_locked(struct sync_device *obj, const char *name)
{
	if (atomic_read(&obj->device_available))
		return NULL;

	atomic_inc(&obj->device_available);

	memset(obj->name, 0, NAME_LEN);
	strlcpy(obj->name, name, sizeof(obj->name));

	return obj;
}

static int spec_sync_open(struct inode *inode, struct file *file)
{
	char task_comm[TASK_COMM_LEN];
	struct sync_device *obj = &sync_dev;
	int ret = 0;

	if (!inode || !inode->i_cdev || !file) {
		pr_err("NULL pointer passed\n");
		return -EINVAL;
	}

	mutex_lock(&sync_dev.lock);

	get_task_comm(task_comm, current);

	obj = spec_fence_init_locked(obj, task_comm);
	if (!obj) {
		pr_err("Spec device exists owner:%s caller:%s\n", sync_dev.name, task_comm);
		ret = -EEXIST;
		goto end;
	}

	file->private_data = obj;

end:
	mutex_unlock(&sync_dev.lock);
	return ret;
}

static int spec_sync_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct sync_device *obj = file->private_data;

	mutex_lock(&sync_dev.lock);

	if (!atomic_read(&obj->device_available)) {
		pr_err("sync release failed !!\n");
		ret = -ENODEV;
		goto end;
	}

	clear_fence_array_tracker(true);
	atomic_dec(&obj->device_available);

end:
	mutex_unlock(&sync_dev.lock);
	return ret;
}

static int spec_sync_ioctl_get_ver(struct sync_device *obj, unsigned long __user arg)
{
	uint32_t version = obj->version;

	if (copy_to_user((void __user *)arg, &version, sizeof(uint32_t)))
		return -EFAULT;

	return 0;
}

static int spec_sync_create_array(struct fence_create_data *f)
{
	int fd = get_unused_fd_flags(O_CLOEXEC);
	struct sync_file *sync_file;
	struct dma_fence_array *fence_array;
	struct fence_array_node *node;
	bool signal_any;
	int ret = 0;

	if (fd < 0) {
		pr_err("failed to get_unused_fd_flags\n");
		return fd;
	}

	if (f->num_fences < FENCE_MIN || f->num_fences > FENCE_MAX) {
		pr_err("invalid arguments num_fences:%d\n", f->num_fences);
		ret = -ERANGE;
		goto error_args;
	}

	signal_any = f->flags & SPEC_FENCE_SIGNAL_ALL ? false : true;

	fence_array = dma_fence_array_create(f->num_fences, NULL,
				dma_fence_context_alloc(1), 0, signal_any);
	if (!fence_array) {
		pr_err("dma fence_array allocation failure\n");
		ret = -ENOMEM;
		goto error_args;
	}

	/* Set the enable signal such that signalling is not done during wait*/
	set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence_array->base.flags);
	set_bit(SPEC_FENCE_FLAG_FENCE_ARRAY, &fence_array->base.flags);

	sync_file = sync_file_create(&fence_array->base);
	if (!sync_file) {
		pr_err("sync_file_create fail\n");
		ret = -EINVAL;
		goto err;
	}
	node = kzalloc((sizeof(struct fence_array_node)), GFP_KERNEL);
	if (!node) {
		fput(sync_file->file);
		ret = -ENOMEM;
		goto err;
	}

	fd_install(fd, sync_file->file);
	node->fence_array = fence_array;

	mutex_lock(&sync_dev.l_lock);
	list_add_tail(&node->list, &sync_dev.fence_array_list);
	mutex_unlock(&sync_dev.l_lock);

	pr_debug("spec fd:%d num_fences:%u\n", fd, f->num_fences);
	return fd;

err:
	fence_array->num_fences = 0;
	dma_fence_put(&fence_array->base);
error_args:
	put_unused_fd(fd);
	return ret;
}

static int spec_sync_ioctl_create_fence(struct sync_device *obj, unsigned long __user arg)
{
	struct fence_create_data f;
	int fd;

	if (copy_from_user(&f, (void __user *)arg, sizeof(f)))
		return -EFAULT;

	fd = spec_sync_create_array(&f);
	if (fd < 0)
		return fd;

	f.out_bind_fd = fd;

	if (copy_to_user((void __user *)arg, &f, sizeof(f)))
		return -EFAULT;

	return 0;
}

static int spec_sync_bind_array(struct fence_bind_data *sync_bind_info)
{
	struct dma_fence_array *fence_array;
	struct dma_fence *fence = NULL;
	struct dma_fence *user_fence = NULL;
	struct dma_fence **fence_list;
	int *user_fds, ret = 0, i;
	u32 num_fences, counter;

	fence = sync_file_get_fence(sync_bind_info->out_bind_fd);
	if (!fence) {
		pr_err("dma fence failure out_fd:%d\n", sync_bind_info->out_bind_fd);
		return -EINVAL;
	}

	fence_array = container_of(fence, struct dma_fence_array, base);
	if (!sanitize_fence_array(fence_array)) {
		pr_err("spec fence not found in the registered list out_fd:%d\n",
				sync_bind_info->out_bind_fd);
		ret = -EINVAL;
		goto end;
	}

	if (fence_array->fences) {
		pr_err("fence array already populated, spec fd:%d status:%d flags:0x%x\n",
			sync_bind_info->out_bind_fd, dma_fence_get_status(fence), fence->flags);
		goto end;
	}

	num_fences = fence_array->num_fences;
	counter = num_fences;

	user_fds = kzalloc(num_fences * (sizeof(int)), GFP_KERNEL);
	if (!user_fds) {
		ret = -ENOMEM;
		goto end;
	}

	fence_list = kmalloc_array(num_fences, sizeof(void *), GFP_KERNEL|__GFP_ZERO);
	if (!fence_list) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(user_fds, (void __user *)sync_bind_info->fds,
						num_fences * sizeof(int))) {
		kfree(fence_list);
		ret = -EFAULT;
		goto out;
	}

	spin_lock(fence->lock);
	fence_array->fences = fence_list;
	for (i = 0; i < num_fences; i++) {
		user_fence = sync_file_get_fence(user_fds[i]);
		if (!user_fence) {
			pr_warn("bind fences are invalid !! user_fd:%d out_bind_fd:%d\n",
				user_fds[i], sync_bind_info->out_bind_fd);
			counter = i;
			ret = -EINVAL;
			goto bind_invalid;
		}
		fence_array->fences[i] = user_fence;
		pr_debug("spec fd:%d i:%d bind fd:%d error:%d\n", sync_bind_info->out_bind_fd,
			 i, user_fds[i], fence_array->fences[i]->error);
	}

	set_bit(SPEC_FENCE_FLAG_ARRAY_BIND, &fence->flags);
	clear_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags);
	spin_unlock(fence->lock);

	dma_fence_enable_sw_signaling(&fence_array->base);

	clear_fence_array_tracker(false);

bind_invalid:
	if (ret) {
		for (i = counter - 1; i >= 0; i--)
			dma_fence_put(fence_array->fences[i]);

		kfree(fence_list);
		fence_array->fences = NULL;
		fence_array->num_fences = 0;
		dma_fence_set_error(fence, -EINVAL);
		spin_unlock(fence->lock);
		dma_fence_signal(fence);
		clear_fence_array_tracker(false);
	}
out:
	kfree(user_fds);
end:
	dma_fence_put(fence);
	return ret;
}

static int spec_sync_ioctl_bind(struct sync_device *obj, unsigned long __user arg)
{
	struct fence_bind_data sync_bind_info;

	if (copy_from_user(&sync_bind_info, (void __user *)arg, sizeof(struct fence_bind_data)))
		return -EFAULT;

	if (sync_bind_info.out_bind_fd < 0) {
		pr_err("Invalid out_fd:%d\n", sync_bind_info.out_bind_fd);
		return -EINVAL;
	}

	return spec_sync_bind_array(&sync_bind_info);
}

static long spec_sync_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	struct sync_device *obj = file->private_data;
	int ret = 0;

	switch (cmd) {
	case SPEC_SYNC_IOC_CREATE_FENCE:
		ret = spec_sync_ioctl_create_fence(obj, arg);
		break;
	case SPEC_SYNC_IOC_BIND:
		ret = spec_sync_ioctl_bind(obj, arg);
		break;
	case SPEC_SYNC_IOC_GET_VER:
		ret = spec_sync_ioctl_get_ver(obj, arg);
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

const struct file_operations spec_sync_fops = {
	.owner = THIS_MODULE,
	.open = spec_sync_open,
	.release = spec_sync_release,
	.unlocked_ioctl = spec_sync_ioctl,
};

static int spec_sync_register_device(void)
{
	int ret;

	sync_dev.dev_class = class_create(THIS_MODULE, CLASS_NAME);
	if (sync_dev.dev_class == NULL) {
		pr_err("%s: class_create fail.\n", __func__);
		goto res_err;
	}

	ret = alloc_chrdev_region(&sync_dev.dev_num, 0, 1, DRV_NAME);
	if (ret) {
		pr_err("%s: alloc_chrdev_region fail.\n", __func__);
		goto alloc_chrdev_region_err;
	}

	sync_dev.dev = device_create(sync_dev.dev_class, NULL,
					 sync_dev.dev_num,
					 &sync_dev, DRV_NAME);
	if (IS_ERR(sync_dev.dev)) {
		pr_err("%s: device_create fail.\n", __func__);
		goto device_create_err;
	}

	sync_dev.cdev = cdev_alloc();
	if (sync_dev.cdev == NULL) {
		pr_err("%s: cdev_alloc fail.\n", __func__);
		goto cdev_alloc_err;
	}
	cdev_init(sync_dev.cdev, &spec_sync_fops);
	sync_dev.cdev->owner = THIS_MODULE;

	ret = cdev_add(sync_dev.cdev, sync_dev.dev_num, 1);
	if (ret) {
		pr_err("%s: cdev_add fail.\n", __func__);
		goto cdev_add_err;
	}

	sync_dev.version = DRV_VERSION;
	mutex_init(&sync_dev.lock);
	mutex_init(&sync_dev.l_lock);
	INIT_LIST_HEAD(&sync_dev.fence_array_list);

	return 0;

cdev_add_err:
	cdev_del(sync_dev.cdev);
cdev_alloc_err:
	device_destroy(sync_dev.dev_class, sync_dev.dev_num);
device_create_err:
	unregister_chrdev_region(sync_dev.dev_num, 1);
alloc_chrdev_region_err:
	class_destroy(sync_dev.dev_class);
res_err:
	return -ENODEV;
}

static int __init spec_sync_init(void)
{
	int ret = 0;

	ret = spec_sync_register_device();
	if (ret) {
		pr_err("%s: speculative sync driver register fail.\n", __func__);
		return ret;
	}
	return ret;
}

static void __exit spec_sync_deinit(void)
{
	cdev_del(sync_dev.cdev);
	device_destroy(sync_dev.dev_class, sync_dev.dev_num);
	unregister_chrdev_region(sync_dev.dev_num, 1);
	class_destroy(sync_dev.dev_class);
}

module_init(spec_sync_init);
module_exit(spec_sync_deinit);

MODULE_DESCRIPTION("QCOM Speculative Sync Driver");
MODULE_LICENSE("GPL v2");
