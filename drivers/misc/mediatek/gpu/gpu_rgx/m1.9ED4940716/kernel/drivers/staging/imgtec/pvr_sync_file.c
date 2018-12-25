/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File           pvr_sync_file.c
@Title          Kernel driver for Android's sync mechanism
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/version.h>

#include "pvr_sync.h"
#include "pvr_fence.h"
#include "services_kernel_client.h"

#include "pvr_counting_timeline.h"

/* FIXME: Proper interface file? */
#include <linux/types.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)) && !defined(CHROMIUMOS_KERNEL)
#define sync_file_user_name(s)	((s)->name)
#else
#define sync_file_user_name(s)	((s)->user_name)
#endif

#define	FILE_NAME "pvr_sync_file"

struct sw_sync_create_fence_data {
	__u32 value;
	char name[32];
	__s32 fence;
};
#define SW_SYNC_IOC_MAGIC 'W'
#define SW_SYNC_IOC_CREATE_FENCE \
	(_IOWR(SW_SYNC_IOC_MAGIC, 0, struct sw_sync_create_fence_data))
#define SW_SYNC_IOC_INC _IOW(SW_SYNC_IOC_MAGIC, 1, __u32)

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sync_file.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

/* This is the actual timeline metadata. We might keep this around after the
 * base sync driver has destroyed the pvr_sync_timeline_wrapper object.
 */
struct pvr_sync_timeline {
	char name[32];
	struct file *file;
	bool is_sw;
	/* Fence context used for hw fences */
	struct pvr_fence_context *hw_fence_context;
	/* Timeline and context for sw fences */
	struct pvr_counting_fence_timeline *sw_fence_timeline;
};

/* Global data for the sync driver */
static struct {
	void *dev_cookie;
	struct pvr_fence_context *foreign_fence_context;
} pvr_sync_data;

static const struct file_operations pvr_sync_fops;

static
void pvr_sync_free_checkpoint_list_mem(void *mem_ptr)
{
	kfree(mem_ptr);
}

static bool is_pvr_timeline(struct file *file)
{
	return file->f_op == &pvr_sync_fops;
}

static struct pvr_sync_timeline *pvr_sync_timeline_fget(int fd)
{
	struct file *file = fget(fd);

	if (!file)
		return NULL;

	if (!is_pvr_timeline(file)) {
		fput(file);
		return NULL;
	}

	return file->private_data;
}

static void pvr_sync_timeline_fput(struct pvr_sync_timeline *timeline)
{
	fput(timeline->file);
}

/* ioctl and fops handling */

static int pvr_sync_open(struct inode *inode, struct file *file)
{
	struct pvr_sync_timeline *timeline;
	char task_comm[TASK_COMM_LEN];
	int err = -ENOMEM;

	get_task_comm(task_comm, current);

	timeline = kzalloc(sizeof(*timeline), GFP_KERNEL);
	if (!timeline)
		goto err_out;

	strlcpy(timeline->name, task_comm, sizeof(timeline->name));
	timeline->file = file;
	timeline->is_sw = false;

	file->private_data = timeline;
	err = 0;
err_out:
	return err;
}

static int pvr_sync_close(struct inode *inode, struct file *file)
{
	struct pvr_sync_timeline *timeline = file->private_data;

	if (timeline->sw_fence_timeline) {
		/* This makes sure any outstanding SW syncs are marked as
		 * complete at timeline close time. Otherwise it'll leak the
		 * timeline (as outstanding fences hold a ref) and possibly
		 * wedge the system if something is waiting on one of those
		 * fences
		 */
		pvr_counting_fence_timeline_force_complete(
			timeline->sw_fence_timeline);
		pvr_counting_fence_timeline_put(timeline->sw_fence_timeline);
	}

	if (timeline->hw_fence_context)
		pvr_fence_context_destroy(timeline->hw_fence_context);

	kfree(timeline);

	return 0;
}

enum PVRSRV_ERROR pvr_sync_finalise_fence(PVRSRV_FENCE fence_fd,
	void *finalise_data)
{
	struct sync_file *sync_file = finalise_data;

	if (!sync_file || (fence_fd < 0)) {
		pr_err(FILE_NAME ": %s: Invalid input fence\n", __func__);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	fd_install(fence_fd, sync_file->file);

	return PVRSRV_OK;
}

enum PVRSRV_ERROR pvr_sync_create_fence(const char *fence_name,
	PVRSRV_TIMELINE new_fence_timeline,
	PSYNC_CHECKPOINT_CONTEXT psSyncCheckpointContext,
	PVRSRV_FENCE *new_fence, u32 *fence_uid, void **fence_finalise_data,
	PSYNC_CHECKPOINT *new_checkpoint_handle, void **timeline_update_sync,
	__u32 *timeline_update_value)
{
	PVRSRV_ERROR err = PVRSRV_OK;
	PVRSRV_FENCE new_fence_fd = -1;
	struct pvr_sync_timeline *timeline;
	struct pvr_fence *pvr_fence;
	PSYNC_CHECKPOINT checkpoint;
	struct sync_file *sync_file;

	if (new_fence_timeline < 0 || !new_fence || !new_checkpoint_handle
		|| !fence_finalise_data) {
		pr_err(FILE_NAME ": %s: Invalid input params\n", __func__);
		err =  PVRSRV_ERROR_INVALID_PARAMS;
		goto err_out;
	}

	/* We reserve the new fence FD before taking any operations
	 * as we do not want to fail (e.g. run out of FDs)
	 */
	new_fence_fd = get_unused_fd_flags(0);
	if (new_fence_fd < 0) {
		pr_err(FILE_NAME ": %s: Failed to get fd\n", __func__);
		err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	timeline = pvr_sync_timeline_fget(new_fence_timeline);
	if (!timeline) {
		pr_err(FILE_NAME ": %s: Failed to open supplied timeline fd (%d)\n",
			__func__, new_fence_timeline);
		err = PVRSRV_ERROR_HANDLE_NOT_FOUND;
		goto err_put_fd;
	}

	if (timeline->is_sw) {
		/* This should never happen! */
		pr_err(FILE_NAME ": %s: Somebody tries to create a fence on sw timeline (%d)\n",
			__func__, new_fence_timeline);
		err = PVRSRV_ERROR_HANDLE_NOT_FOUND;
		goto err_put_timeline;
	}

	if (!timeline->hw_fence_context) {
		/* First time we use this timeline, so create a context. */
		timeline->hw_fence_context =
			pvr_fence_context_create(pvr_sync_data.dev_cookie,
						 timeline->name);
		if (!timeline->hw_fence_context) {
			pr_err(FILE_NAME ": %s: Failed to create fence context (%d)\n",
			       __func__, new_fence_timeline);
			err = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto err_put_timeline;
		}
	}

	pvr_fence = pvr_fence_create(timeline->hw_fence_context, fence_name);
	if (!pvr_fence) {
		pr_err(FILE_NAME ": %s: Failed to create new pvr_fence\n",
			__func__);
		err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_put_timeline;
	}

	checkpoint = pvr_fence_get_checkpoint(pvr_fence);
	if (!checkpoint) {
		pr_err(FILE_NAME ": %s: Failed to get fence checkpoint\n",
			__func__);
		err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_destroy_fence;
	}

	/* pvr fences can be signalled any time after creation */
	dma_fence_enable_sw_signaling(&pvr_fence->base);

	sync_file = sync_file_create(&pvr_fence->base);
	if (!sync_file) {
		pr_err(FILE_NAME ": %s: Failed to create sync_file\n",
			__func__);
		err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_destroy_fence;
	}
	strlcpy(sync_file_user_name(sync_file),
		pvr_fence->name,
		sizeof(sync_file_user_name(sync_file)));
	dma_fence_put(&pvr_fence->base);

	*new_fence = new_fence_fd;
	*fence_finalise_data = sync_file;
	*new_checkpoint_handle = checkpoint;
	/* TODO FDs are not unique! */
	*fence_uid = new_fence_fd;

	pvr_sync_timeline_fput(timeline);
err_out:
	return err;

err_destroy_fence:
	pvr_fence_destroy(pvr_fence);
err_put_timeline:
	pvr_sync_timeline_fput(timeline);
err_put_fd:
	put_unused_fd(new_fence_fd);
	*fence_uid = PVRSRV_FENCE_INVALID;
	goto err_out;
}

enum PVRSRV_ERROR pvr_sync_rollback_fence_data(PVRSRV_FENCE fence_to_rollback,
	void *fence_data_to_rollback)
{
	struct sync_file *sync_file = fence_data_to_rollback;
	struct pvr_fence *pvr_fence;

	if (!sync_file || fence_to_rollback < 0) {
		pr_err(FILE_NAME ": %s: Invalid fence (%d)\n", __func__,
			fence_to_rollback);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	pvr_fence = to_pvr_fence(sync_file->fence);
	if (!pvr_fence) {
		pr_err(FILE_NAME
			": %s: Non-PVR fence (%p)\n",
			__func__, sync_file->fence);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	pvr_fence_sw_signal(pvr_fence);

	fput(sync_file->file);

	put_unused_fd(fence_to_rollback);

	return PVRSRV_OK;
}

enum PVRSRV_ERROR pvr_sync_resolve_fence(
	PSYNC_CHECKPOINT_CONTEXT psSyncCheckpointContext,
	PVRSRV_FENCE fence_to_resolve, u32 *nr_checkpoints,
	PSYNC_CHECKPOINT **checkpoint_handles, u32 *fence_uid)
{
	PSYNC_CHECKPOINT *checkpoints = NULL;
	unsigned int i, num_fences, num_used_fences = 0;
	struct dma_fence **fences = NULL;
	struct dma_fence *fence;
	PVRSRV_ERROR err = PVRSRV_OK;

	if (!nr_checkpoints || !checkpoint_handles || !fence_uid) {
		pr_err(FILE_NAME ": %s: Invalid input checkpoint pointer\n",
			__func__);
		err =  PVRSRV_ERROR_INVALID_PARAMS;
		goto err_out;
	}

	*nr_checkpoints = 0;
	*checkpoint_handles = NULL;
	*fence_uid = 0;

	if (fence_to_resolve < 0)
		goto err_out;

	fence = sync_file_get_fence(fence_to_resolve);
	if (!fence) {
		pr_err(FILE_NAME ": %s: Failed to read sync private data for fd %d\n",
			__func__, fence_to_resolve);
		err = PVRSRV_ERROR_HANDLE_NOT_FOUND;
		goto err_out;
	}

	if (dma_fence_is_array(fence)) {
		struct dma_fence_array *array = to_dma_fence_array(fence);

		if (!array) {
			pr_err(FILE_NAME ": %s: Failed to resolve fence array %d\n",
			       __func__, fence_to_resolve);
			err = PVRSRV_ERROR_HANDLE_NOT_FOUND;
			goto err_put_fence;
		}
		fences = array->fences;
		num_fences = array->num_fences;
	} else {
		fences = &fence;
		num_fences = 1;
	}

	checkpoints = kmalloc_array(num_fences, sizeof(PSYNC_CHECKPOINT),
			      GFP_KERNEL);
	if (!checkpoints) {
		err = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_put_fence;
	}
	for (i = 0; i < num_fences; i++) {
		/* Only return the checkpoint if the fence is still active. */
		if (!test_bit(DMA_FENCE_FLAG_SIGNALED_BIT,
			      &fences[i]->flags)) {
			struct pvr_fence *pvr_fence =
				pvr_fence_create_from_fence(
					pvr_sync_data.foreign_fence_context,
					fences[i],
					"foreign");
			if (!pvr_fence) {
				pr_err(FILE_NAME ": %s: Failed to create fence\n",
				       __func__);
				err = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto err_free_checkpoints;
			}
			checkpoints[num_used_fences] =
				pvr_fence_get_checkpoint(pvr_fence);
			SyncCheckpointTakeRef(checkpoints[num_used_fences]);
			++num_used_fences;
			dma_fence_put(&pvr_fence->base);
		}
	}
	/* If we don't return any checkpoints, delete the array because
	 * the caller will not.
	 */
	if (num_used_fences == 0) {
		kfree(checkpoints);
		checkpoints = NULL;
	}

	*checkpoint_handles = checkpoints;
	*nr_checkpoints = num_used_fences;
	/* TODO FDs are not unique! */
	*fence_uid = fence_to_resolve;

err_put_fence:
	dma_fence_put(fence);
err_out:
	return err;

err_free_checkpoints:
	for (i = 0; i < num_used_fences; i++) {
		if (checkpoints[i])
			SyncCheckpointDropRef(checkpoints[i]);
	}
	kfree(checkpoints);
	goto err_put_fence;
}

static long pvr_sync_ioctl_rename(struct pvr_sync_timeline *timeline,
	void __user *user_data)
{
	int err = 0;
	struct pvr_sync_rename_ioctl_data data;

	if (!access_ok(VERIFY_READ, user_data, sizeof(data))) {
		err = -EFAULT;
		goto err;
	}

	if (copy_from_user(&data, user_data, sizeof(data))) {
		err = -EFAULT;
		goto err;
	}

	data.szName[sizeof(data.szName) - 1] = '\0';
	strlcpy(timeline->name, data.szName, sizeof(timeline->name));
	if (timeline->hw_fence_context)
		strlcpy(timeline->hw_fence_context->name, data.szName,
			sizeof(timeline->hw_fence_context->name));

err:
	return err;
}

static long pvr_sync_ioctl_force_sw_only(struct pvr_sync_timeline *timeline,
	void **private_data)
{
	/* Already in SW mode? */
	if (timeline->sw_fence_timeline)
		return 0;

	/* Create a sw_sync timeline with the old GPU timeline's name */
	timeline->sw_fence_timeline = pvr_counting_fence_timeline_create(
		pvr_sync_data.dev_cookie,
		timeline->name);
	if (!timeline->sw_fence_timeline)
		return -ENOMEM;

	timeline->is_sw = true;

	return 0;
}

static long pvr_sync_ioctl_sw_create_fence(struct pvr_sync_timeline *timeline,
	void __user *user_data)
{
	struct sw_sync_create_fence_data data;
	struct sync_file *sync_file;
	int fd = get_unused_fd_flags(0);
	struct dma_fence *fence;
	int err = -EFAULT;

	if (fd < 0) {
		pr_err(FILE_NAME ": %s: Failed to find unused fd (%d)\n",
		       __func__, fd);
		goto err_out;
	}

	if (copy_from_user(&data, user_data, sizeof(data))) {
		pr_err(FILE_NAME ": %s: Failed copy from user\n", __func__);
		goto err_put_fd;
	}

	fence = pvr_counting_fence_create(timeline->sw_fence_timeline,
					  data.value);
	if (!fence) {
		pr_err(FILE_NAME ": %s: Failed to create a sync point (%d)\n",
		       __func__, fd);
		err = -ENOMEM;
		goto err_put_fd;
	}

	sync_file = sync_file_create(fence);
	if (!sync_file) {
		pr_err(FILE_NAME ": %s: Failed to create a sync point (%d)\n",
			__func__, fd);
		 err = -ENOMEM;
		goto err_put_fence;
	}

	data.fence = fd;

	if (copy_to_user(user_data, &data, sizeof(data))) {
		pr_err(FILE_NAME ": %s: Failed copy to user\n", __func__);
		goto err_put_fence;
	}

	fd_install(fd, sync_file->file);
	err = 0;

	dma_fence_put(fence);
err_out:
	return err;

err_put_fence:
	dma_fence_put(fence);
err_put_fd:
	put_unused_fd(fd);
	goto err_out;
}

static long pvr_sync_ioctl_sw_inc(struct pvr_sync_timeline *timeline,
	void __user *user_data)
{
	u32 value;

	if (copy_from_user(&value, user_data, sizeof(value)))
		return -EFAULT;

	pvr_counting_fence_timeline_inc(timeline->sw_fence_timeline, value);
	return 0;
}

static long
pvr_sync_ioctl(struct file *file, unsigned int cmd, unsigned long __user arg)
{
	void __user *user_data = (void __user *)arg;
	long err = -ENOTTY;
	struct pvr_sync_timeline *timeline = file->private_data;

	if (!timeline->is_sw) {

		switch (cmd) {
		case PVR_SYNC_IOC_RENAME:
			err = pvr_sync_ioctl_rename(timeline, user_data);
			break;
		case PVR_SYNC_IOC_FORCE_SW_ONLY:
			err = pvr_sync_ioctl_force_sw_only(timeline,
				&file->private_data);
			break;
		default:
			break;
		}
	} else {

		switch (cmd) {
		case SW_SYNC_IOC_CREATE_FENCE:
			err = pvr_sync_ioctl_sw_create_fence(timeline,
							     user_data);
			break;
		case SW_SYNC_IOC_INC:
			err = pvr_sync_ioctl_sw_inc(timeline, user_data);
			break;
		default:
			break;
		}
	}

	return err;
}

static const struct file_operations pvr_sync_fops = {
	.owner          = THIS_MODULE,
	.open           = pvr_sync_open,
	.release        = pvr_sync_close,
	.unlocked_ioctl = pvr_sync_ioctl,
	.compat_ioctl   = pvr_sync_ioctl,
};

static struct miscdevice pvr_sync_device = {
	.minor          = MISC_DYNAMIC_MINOR,
	.name           = PVRSYNC_MODNAME,
	.fops           = &pvr_sync_fops,
};

enum PVRSRV_ERROR pvr_sync_init(void *device_cookie)
{
	enum PVRSRV_ERROR error;
	int err;

	pvr_sync_data.dev_cookie = device_cookie;

	pvr_sync_data.foreign_fence_context =
		pvr_fence_context_create(device_cookie, "foreign_sync");
	if (!pvr_sync_data.foreign_fence_context) {
		pr_err(FILE_NAME ": %s: Failed to create foreign sync context\n",
			__func__);
		error = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	/* Register the resolve fence and create fence functions with
	 * sync_checkpoint.c
	 * The pvr_fence context registers its own EventObject callback to
	 * update sync status
	 */
	SyncCheckpointRegisterFunctions(pvr_sync_resolve_fence,
		pvr_sync_create_fence, pvr_sync_rollback_fence_data,
		pvr_sync_finalise_fence, NULL,
		pvr_sync_free_checkpoint_list_mem);

	err = misc_register(&pvr_sync_device);
	if (err) {
		pr_err(FILE_NAME ": %s: Failed to register pvr_sync device (%d)\n",
		       __func__, err);
		error = PVRSRV_ERROR_RESOURCE_UNAVAILABLE;
		goto err_unregister_checkpoint_funcs;
	}
	error = PVRSRV_OK;

err_out:
	return error;

err_unregister_checkpoint_funcs:
	SyncCheckpointRegisterFunctions(NULL, NULL, NULL, NULL, NULL, NULL);
	pvr_fence_context_destroy(pvr_sync_data.foreign_fence_context);
	goto err_out;
}

void pvr_sync_deinit(void)
{
	SyncCheckpointRegisterFunctions(NULL, NULL, NULL, NULL, NULL, NULL);
	misc_deregister(&pvr_sync_device);
	pvr_fence_context_destroy(pvr_sync_data.foreign_fence_context);
}

struct pvr_counting_fence_timeline *pvr_sync_get_sw_timeline(int fd)
{
	struct pvr_sync_timeline *timeline;
	struct pvr_counting_fence_timeline *sw_timeline = NULL;

	timeline = pvr_sync_timeline_fget(fd);
	if (!timeline)
		return NULL;

	sw_timeline =
		pvr_counting_fence_timeline_get(timeline->sw_fence_timeline);

	pvr_sync_timeline_fput(timeline);
	return sw_timeline;
}
