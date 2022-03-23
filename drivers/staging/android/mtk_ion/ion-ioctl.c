// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/staging/android/mtk_ion/ion_ioctl.c
 *
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sched/clock.h>

#include "ion.h"
#include "ion_priv.h"
#include "compat_ion.h"
#include "mtk_ion.h"

union ion_ioctl_arg {
	struct ion_fd_data fd;
	struct ion_allocation_data allocation;
	struct ion_handle_data handle;
	struct ion_custom_data custom;
	struct ion_heap_query query;
};

static int validate_ioctl_arg(unsigned int cmd, union ion_ioctl_arg *arg)
{
	int ret = 0;

	switch (cmd) {
	case ION_IOC_HEAP_QUERY:
		ret = arg->query.reserved0 != 0;
		ret |= arg->query.reserved1 != 0;
		ret |= arg->query.reserved2 != 0;
		break;
	default:
		break;
	}

	return ret ? -EINVAL : 0;
}

/* fix up the cases where the ioctl direction bits are incorrect */
static unsigned int ion_ioctl_dir(unsigned int cmd)
{
	switch (cmd) {
	case ION_IOC_SYNC:
	case ION_IOC_FREE:
	case ION_IOC_CUSTOM:
		return _IOC_WRITE;
	default:
		return _IOC_DIR(cmd);
	}
}

long ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ion_client *client = filp->private_data;
	struct ion_device *dev = client->dev;
	struct ion_handle *cleanup_handle = NULL;
	int ret = 0;
	unsigned int dir;
	union ion_ioctl_arg data;

	dir = ion_ioctl_dir(cmd);

	if (_IOC_SIZE(cmd) > sizeof(data)) {
		ion_info("%s cmd = %d, _IOC_SIZE(cmd) = %d, sizeof(data) = %zd.\n",
			 __func__, cmd, _IOC_SIZE(cmd), sizeof(data));
		return -EINVAL;
	}

	/*
	 * The copy_from_user is unconditional here for both read and write
	 * to do the validate. If there is no write for the ioctl, the
	 * buffer is cleared
	 */
	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd))) {
		ion_info("%s copy_from_user fail!. cmd = %d, n = %d.\n",
			 __func__, cmd, _IOC_SIZE(cmd));
		return -EFAULT;
	}

	ret = validate_ioctl_arg(cmd, &data);
	if (WARN_ON_ONCE(ret))
		return ret;

	if (!(dir & _IOC_WRITE))
		memset(&data, 0, sizeof(data));

	switch (cmd) {
	case ION_IOC_ALLOC:
	{
		struct ion_handle *handle;
		int heap_mask;

		heap_mask = data.allocation.heap_id_mask;

		handle = __ion_alloc(client, data.allocation.len,
				     data.allocation.align,
				     data.allocation.heap_id_mask,
				     data.allocation.flags, true);

		if (IS_ERR(handle)) {
			IONMSG("IOC_ALLOC handle invalid. ret = %d\n", ret);
			return PTR_ERR(handle);
		}
		data.allocation.handle = handle->id;
		cleanup_handle = handle;
		pass_to_user(handle);
		break;
	}
	case ION_IOC_FREE:
	{
		struct ion_handle *handle;
		unsigned long long time_s, time_e;
		unsigned long long time_s_lock, time_e_lock;
		struct task_struct *task = current->group_leader;
		char task_comm[TASK_COMM_LEN];
		pid_t pid;

		get_task_comm(task_comm, task);
		pid = task_pid_nr(task);
		time_s_lock = sched_clock();
		mutex_lock(&client->lock);
		time_s = sched_clock();
		handle = ion_handle_get_by_id_nolock(client, data.handle.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			IONDBG("ION_IOC_FREE handle is invalid. handle = %d, ret = %d.\n",
			       data.handle.handle, ret);
			return PTR_ERR(handle);
		}
		user_ion_free_nolock(client, handle);
		ion_handle_put_nolock(handle);
		time_e = sched_clock();
		if ((time_e - time_s) > 100000000) //100ms
			IONMSG("ion_free unlock warnning, time:%llu, task:%s (%d)\n",
			       (time_e - time_s), task_comm, pid);

		mutex_unlock(&client->lock);
		time_e_lock = sched_clock();
		if ((time_e_lock - time_s_lock) > 150000000) //150ms
			IONMSG("ion_free warnning, time:%llu, task:%s (%d)\n",
			       (time_e_lock - time_s_lock), task_comm, pid);
		break;
	}
	case ION_IOC_SHARE:
	case ION_IOC_MAP:
	{
		struct ion_handle *handle;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client,
						     data.handle.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			ret = PTR_ERR(handle);
			IONMSG("ION_IOC_SHARE handle(%d) is invalid, ret %d\n",
			       data.handle.handle, ret);
			return ret;
		}
		data.fd.fd = ion_share_dma_buf_fd_nolock(client, handle);
		ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		if (data.fd.fd < 0) {
			IONMSG("ION_IOC_SHARE fd = %d.\n", data.fd.fd);
			ret = data.fd.fd;
		}
		break;
	}
	case ION_IOC_IMPORT:
	{
		struct ion_handle *handle;

		handle = ion_import_dma_buf_fd(client, data.fd.fd);
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
			IONMSG("ion_import fail: fd=%d, ret=%d\n",
			       data.fd.fd, ret);
			return ret;
		} else {
			data.handle.handle = handle->id;
			handle = pass_to_user(handle);
			if (IS_ERR(handle)) {
				ret = PTR_ERR(handle);
				data.handle.handle = 0;
			}
		}
		break;
	}
	case ION_IOC_SYNC:
	{
		ret = ion_sync_for_device(client, data.fd.fd);
		break;
	}
	case ION_IOC_CUSTOM:
	{
		if (!dev->custom_ioctl) {
			IONMSG("ION_IOC_CUSTOM dev has no custom ioctl!.\n");
			return -ENOTTY;
		}
		ret = dev->custom_ioctl(client, data.custom.cmd,
						data.custom.arg);
		break;
	}
	case ION_IOC_HEAP_QUERY:
		ret = ion_query_heaps(client, &data.query);
		break;
	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd))) {
			if (cleanup_handle) {
				mutex_lock(&client->lock);
				user_ion_free_nolock(client, cleanup_handle);
				ion_handle_put_nolock(cleanup_handle);
				mutex_unlock(&client->lock);
			}
			IONMSG("%s %d fail! cmd = %d, n = %d.\n",
			       __func__, __LINE__,
			       cmd, _IOC_SIZE(cmd));
			return -EFAULT;
		}
	}
	if (cleanup_handle)
		ion_handle_put(cleanup_handle);
	return ret;
}
