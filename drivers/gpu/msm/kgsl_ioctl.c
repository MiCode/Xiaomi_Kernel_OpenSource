// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2008-2019, The Linux Foundation. All rights reserved.
 */

#include "kgsl_device.h"
#include "kgsl_sync.h"

static const struct kgsl_ioctl kgsl_ioctl_funcs[] = {
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DEVICE_GETPROPERTY,
			kgsl_ioctl_device_getproperty),
	/* IOCTL_KGSL_DEVICE_WAITTIMESTAMP is no longer supported */
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DEVICE_WAITTIMESTAMP_CTXTID,
			kgsl_ioctl_device_waittimestamp_ctxtid),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS,
			kgsl_ioctl_rb_issueibcmds),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SUBMIT_COMMANDS,
			kgsl_ioctl_submit_commands),
	/* IOCTL_KGSL_CMDSTREAM_READTIMESTAMP is no longer supported */
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CMDSTREAM_READTIMESTAMP_CTXTID,
			kgsl_ioctl_cmdstream_readtimestamp_ctxtid),
	/* IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP is no longer supported */
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP_CTXTID,
			kgsl_ioctl_cmdstream_freememontimestamp_ctxtid),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DRAWCTXT_CREATE,
			kgsl_ioctl_drawctxt_create),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DRAWCTXT_DESTROY,
			kgsl_ioctl_drawctxt_destroy),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_MAP_USER_MEM,
			kgsl_ioctl_map_user_mem),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FROM_PMEM,
			kgsl_ioctl_map_user_mem),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FREE,
			kgsl_ioctl_sharedmem_free),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FLUSH_CACHE,
			kgsl_ioctl_sharedmem_flush_cache),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_ALLOC,
			kgsl_ioctl_gpumem_alloc),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_TIMESTAMP_EVENT,
			kgsl_ioctl_timestamp_event),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SETPROPERTY,
			kgsl_ioctl_device_setproperty),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_ALLOC_ID,
			kgsl_ioctl_gpumem_alloc_id),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_FREE_ID,
			kgsl_ioctl_gpumem_free_id),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_GET_INFO,
			kgsl_ioctl_gpumem_get_info),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_SYNC_CACHE,
			kgsl_ioctl_gpumem_sync_cache),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_SYNC_CACHE_BULK,
			kgsl_ioctl_gpumem_sync_cache_bulk),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SYNCSOURCE_CREATE,
			kgsl_ioctl_syncsource_create),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SYNCSOURCE_DESTROY,
			kgsl_ioctl_syncsource_destroy),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SYNCSOURCE_CREATE_FENCE,
			kgsl_ioctl_syncsource_create_fence),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SYNCSOURCE_SIGNAL_FENCE,
			kgsl_ioctl_syncsource_signal_fence),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUOBJ_ALLOC,
			kgsl_ioctl_gpuobj_alloc),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUOBJ_FREE,
			kgsl_ioctl_gpuobj_free),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUOBJ_INFO,
			kgsl_ioctl_gpuobj_info),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUOBJ_IMPORT,
			kgsl_ioctl_gpuobj_import),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUOBJ_SYNC,
			kgsl_ioctl_gpuobj_sync),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPU_COMMAND,
			kgsl_ioctl_gpu_command),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUOBJ_SET_INFO,
			kgsl_ioctl_gpuobj_set_info),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SPARSE_PHYS_ALLOC,
			kgsl_ioctl_sparse_phys_alloc),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SPARSE_PHYS_FREE,
			kgsl_ioctl_sparse_phys_free),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SPARSE_VIRT_ALLOC,
			kgsl_ioctl_sparse_virt_alloc),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SPARSE_VIRT_FREE,
			kgsl_ioctl_sparse_virt_free),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SPARSE_BIND,
			kgsl_ioctl_sparse_bind),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPU_SPARSE_COMMAND,
			kgsl_ioctl_gpu_sparse_command),
};

long kgsl_ioctl_copy_in(unsigned int kernel_cmd, unsigned int user_cmd,
		unsigned long arg, unsigned char *ptr)
{
	unsigned int usize = _IOC_SIZE(user_cmd);
	unsigned int ksize = _IOC_SIZE(kernel_cmd);
	unsigned int copy = ksize < usize ? ksize : usize;

	if ((kernel_cmd & IOC_IN) && (user_cmd & IOC_IN)) {
		if (copy > 0 && copy_from_user(ptr, (void __user *) arg, copy))
			return -EFAULT;
	}

	return 0;
}

long kgsl_ioctl_copy_out(unsigned int kernel_cmd, unsigned int user_cmd,
		unsigned long arg, unsigned char *ptr)
{
	unsigned int usize = _IOC_SIZE(user_cmd);
	unsigned int ksize = _IOC_SIZE(kernel_cmd);
	unsigned int copy = ksize < usize ? ksize : usize;

	if ((kernel_cmd & IOC_OUT) && (user_cmd & IOC_OUT)) {
		if (copy > 0 && copy_to_user((void __user *) arg, ptr, copy))
			return -EFAULT;
	}

	return 0;
}

long kgsl_ioctl_helper(struct file *filep, unsigned int cmd, unsigned long arg,
		const struct kgsl_ioctl *cmds, int len)
{
	struct kgsl_device_private *dev_priv = filep->private_data;
	unsigned char data[128] = { 0 };
	unsigned int nr = _IOC_NR(cmd);
	long ret;

	if (nr >= len || cmds[nr].func == NULL)
		return -ENOIOCTLCMD;

	if (_IOC_SIZE(cmds[nr].cmd) > sizeof(data)) {
		dev_err_ratelimited(dev_priv->device->dev,
			"data too big for ioctl 0x%08x: %d/%zu\n",
			cmd, _IOC_SIZE(cmds[nr].cmd), sizeof(data));
		return -EINVAL;
	}

	if (_IOC_SIZE(cmds[nr].cmd)) {
		ret = kgsl_ioctl_copy_in(cmds[nr].cmd, cmd, arg, data);
		if (ret)
			return ret;
	}

	ret = cmds[nr].func(dev_priv, cmd, data);

	if (ret == 0 && _IOC_SIZE(cmds[nr].cmd))
		ret = kgsl_ioctl_copy_out(cmds[nr].cmd, cmd, arg, data);

	return ret;
}

long kgsl_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct kgsl_device_private *dev_priv = filep->private_data;
	struct kgsl_device *device = dev_priv->device;
	long ret;

	ret = kgsl_ioctl_helper(filep, cmd, arg, kgsl_ioctl_funcs,
		ARRAY_SIZE(kgsl_ioctl_funcs));

	/*
	 * If the command was unrecognized in the generic core, try the device
	 * specific function
	 */

	if (ret == -ENOIOCTLCMD) {
		if (is_compat_task() && device->ftbl->compat_ioctl != NULL)
			return device->ftbl->compat_ioctl(dev_priv, cmd, arg);
		else if (device->ftbl->ioctl != NULL)
			return device->ftbl->ioctl(dev_priv, cmd, arg);

		dev_err(device->dev, "invalid ioctl code 0x%08X\n", cmd);
	}

	return ret;
}
