// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
 */

#include "kgsl_device.h"
#include "kgsl_compat.h"
#include "kgsl_sync.h"

static long
kgsl_ioctl_device_getproperty_compat(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data)
{
	struct kgsl_device_getproperty_compat *param32 = data;
	struct kgsl_device_getproperty param;

	param.type = param32->type;
	param.value = compat_ptr(param32->value);
	param.sizebytes = (size_t)param32->sizebytes;

	return kgsl_ioctl_device_getproperty(dev_priv, cmd, &param);
}

static long
kgsl_ioctl_device_setproperty_compat(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data)
{
	struct kgsl_device_getproperty_compat *param32 = data;
	struct kgsl_device_getproperty param;

	param.type = param32->type;
	param.value = compat_ptr(param32->value);
	param.sizebytes = (size_t)param32->sizebytes;

	return kgsl_ioctl_device_setproperty(dev_priv, cmd, &param);
}

static long
kgsl_ioctl_submit_commands_compat(struct kgsl_device_private *dev_priv,
				      unsigned int cmd, void *data)
{
	int result;
	struct kgsl_submit_commands_compat *param32 = data;
	struct kgsl_submit_commands param;

	param.context_id = param32->context_id;
	param.flags = param32->flags;
	param.cmdlist = compat_ptr(param32->cmdlist);
	param.numcmds = param32->numcmds;
	param.synclist = compat_ptr(param32->synclist);
	param.numsyncs = param32->numsyncs;
	param.timestamp = param32->timestamp;

	result = kgsl_ioctl_submit_commands(dev_priv, cmd, &param);

	param32->timestamp = param.timestamp;

	return result;
}

static long
kgsl_ioctl_rb_issueibcmds_compat(struct kgsl_device_private *dev_priv,
				      unsigned int cmd, void *data)
{
	int result;
	struct kgsl_ringbuffer_issueibcmds_compat *param32 = data;
	struct kgsl_ringbuffer_issueibcmds param;

	param.drawctxt_id = param32->drawctxt_id;
	param.flags = param32->flags;
	param.ibdesc_addr = (unsigned long)param32->ibdesc_addr;
	param.numibs = param32->numibs;
	param.timestamp = param32->timestamp;

	result = kgsl_ioctl_rb_issueibcmds(dev_priv, cmd, &param);

	param32->timestamp = param.timestamp;

	return result;
}

static long kgsl_ioctl_cmdstream_freememontimestamp_ctxtid_compat(
						struct kgsl_device_private
						*dev_priv, unsigned int cmd,
						void *data)
{
	struct kgsl_cmdstream_freememontimestamp_ctxtid_compat *param32 = data;
	struct kgsl_cmdstream_freememontimestamp_ctxtid param;

	param.context_id = param32->context_id;
	param.gpuaddr = (unsigned long)param32->gpuaddr;
	param.type = param32->type;
	param.timestamp = param32->timestamp;

	return kgsl_ioctl_cmdstream_freememontimestamp_ctxtid(dev_priv, cmd,
								&param);
}

static long kgsl_ioctl_sharedmem_free_compat(struct kgsl_device_private
					*dev_priv, unsigned int cmd,
					void *data)
{
	struct kgsl_sharedmem_free_compat *param32 = data;
	struct kgsl_sharedmem_free param;

	param.gpuaddr = (unsigned long)param32->gpuaddr;

	return kgsl_ioctl_sharedmem_free(dev_priv, cmd, &param);
}

static long kgsl_ioctl_map_user_mem_compat(struct kgsl_device_private
					*dev_priv, unsigned int cmd,
					void *data)
{
	int result = 0;
	struct kgsl_map_user_mem_compat *param32 = data;
	struct kgsl_map_user_mem param;

	param.fd = param32->fd;
	param.gpuaddr = (unsigned long)param32->gpuaddr;
	param.len = (size_t)param32->len;
	param.offset = (size_t)param32->offset;
	param.hostptr = (unsigned long)param32->hostptr;
	param.memtype = param32->memtype;
	param.flags = param32->flags;

	result = kgsl_ioctl_map_user_mem(dev_priv, cmd, &param);

	param32->gpuaddr = gpuaddr_to_compat(param.gpuaddr);
	param32->flags = param.flags;
	return result;
}

static long
kgsl_ioctl_gpumem_sync_cache_compat(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data)
{
	struct kgsl_gpumem_sync_cache_compat *param32 = data;
	struct kgsl_gpumem_sync_cache param;

	param.gpuaddr = (unsigned long)param32->gpuaddr;
	param.id = param32->id;
	param.op = param32->op;
	param.offset = (size_t)param32->offset;
	param.length = (size_t)param32->length;

	return kgsl_ioctl_gpumem_sync_cache(dev_priv, cmd, &param);
}

static long
kgsl_ioctl_gpumem_sync_cache_bulk_compat(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_gpumem_sync_cache_bulk_compat *param32 = data;
	struct kgsl_gpumem_sync_cache_bulk param;

	param.id_list = to_user_ptr(param32->id_list);
	param.count = param32->count;
	param.op = param32->op;

	return kgsl_ioctl_gpumem_sync_cache_bulk(dev_priv, cmd, &param);
}

static long
kgsl_ioctl_sharedmem_flush_cache_compat(struct kgsl_device_private *dev_priv,
				 unsigned int cmd, void *data)
{
	struct kgsl_sharedmem_free_compat *param32 = data;
	struct kgsl_sharedmem_free param;

	param.gpuaddr = (unsigned long)param32->gpuaddr;

	return kgsl_ioctl_sharedmem_flush_cache(dev_priv, cmd, &param);
}

static long
kgsl_ioctl_gpumem_alloc_compat(struct kgsl_device_private *dev_priv,
			unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_gpumem_alloc_compat *param32 = data;
	struct kgsl_gpumem_alloc param;

	param.gpuaddr = (unsigned long)param32->gpuaddr;
	param.size = (size_t)param32->size;
	param.flags = param32->flags;

	/*
	 * Since this is a 32 bit application the page aligned size is expected
	 * to fit inside of 32 bits - check for overflow and return error if so
	 */
	if (PAGE_ALIGN(param.size) >= UINT_MAX)
		return -EINVAL;

	result = kgsl_ioctl_gpumem_alloc(dev_priv, cmd, &param);

	param32->gpuaddr = gpuaddr_to_compat(param.gpuaddr);
	param32->size = sizet_to_compat(param.size);
	param32->flags = param.flags;

	return result;
}

static long
kgsl_ioctl_gpumem_alloc_id_compat(struct kgsl_device_private *dev_priv,
			unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_gpumem_alloc_id_compat *param32 = data;
	struct kgsl_gpumem_alloc_id param;

	param.id = param32->id;
	param.flags = param32->flags;
	param.size = (size_t)param32->size;
	param.mmapsize = (size_t)param32->mmapsize;
	param.gpuaddr = (unsigned long)param32->gpuaddr;

	/*
	 * Since this is a 32 bit application the page aligned size is expected
	 * to fit inside of 32 bits - check for overflow and return error if so
	 */
	if (PAGE_ALIGN(param.size) >= UINT_MAX)
		return -EINVAL;

	result = kgsl_ioctl_gpumem_alloc_id(dev_priv, cmd, &param);

	param32->id = param.id;
	param32->flags = param.flags;
	param32->size = sizet_to_compat(param.size);
	param32->mmapsize = sizet_to_compat(param.mmapsize);
	param32->gpuaddr = gpuaddr_to_compat(param.gpuaddr);

	return result;
}

static long
kgsl_ioctl_gpumem_get_info_compat(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_gpumem_get_info_compat *param32 = data;
	struct kgsl_gpumem_get_info param;

	param.gpuaddr = (unsigned long)param32->gpuaddr;
	param.id = param32->id;
	param.flags = param32->flags;
	param.size = (size_t)param32->size;
	param.mmapsize = (size_t)param32->mmapsize;
	param.useraddr = (unsigned long)param32->useraddr;

	result = kgsl_ioctl_gpumem_get_info(dev_priv, cmd, &param);

	param32->gpuaddr = gpuaddr_to_compat(param.gpuaddr);
	param32->id = param.id;
	param32->flags = param.flags;
	param32->size = sizet_to_compat(param.size);
	param32->mmapsize = sizet_to_compat(param.mmapsize);
	param32->useraddr = (compat_ulong_t)param.useraddr;

	return result;
}

static long kgsl_ioctl_timestamp_event_compat(struct kgsl_device_private
				*dev_priv, unsigned int cmd, void *data)
{
	struct kgsl_timestamp_event_compat *param32 = data;
	struct kgsl_timestamp_event param;

	param.type = param32->type;
	param.timestamp = param32->timestamp;
	param.context_id = param32->context_id;
	param.priv = compat_ptr(param32->priv);
	param.len = (size_t)param32->len;

	return kgsl_ioctl_timestamp_event(dev_priv, cmd, &param);
}


static const struct kgsl_ioctl kgsl_compat_ioctl_funcs[] = {
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DEVICE_GETPROPERTY_COMPAT,
			kgsl_ioctl_device_getproperty_compat),
	/* IOCTL_KGSL_DEVICE_WAITTIMESTAMP is no longer supported */
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DEVICE_WAITTIMESTAMP_CTXTID,
			kgsl_ioctl_device_waittimestamp_ctxtid),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS_COMPAT,
			kgsl_ioctl_rb_issueibcmds_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SUBMIT_COMMANDS_COMPAT,
			kgsl_ioctl_submit_commands_compat),
	/* IOCTL_KGSL_CMDSTREAM_READTIMESTAMP is no longer supported */
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CMDSTREAM_READTIMESTAMP_CTXTID,
			kgsl_ioctl_cmdstream_readtimestamp_ctxtid),
	/* IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP is no longer supported */
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP_CTXTID_COMPAT,
			kgsl_ioctl_cmdstream_freememontimestamp_ctxtid_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DRAWCTXT_CREATE,
			kgsl_ioctl_drawctxt_create),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DRAWCTXT_DESTROY,
			kgsl_ioctl_drawctxt_destroy),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_MAP_USER_MEM_COMPAT,
			kgsl_ioctl_map_user_mem_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FREE_COMPAT,
			kgsl_ioctl_sharedmem_free_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FLUSH_CACHE_COMPAT,
			kgsl_ioctl_sharedmem_flush_cache_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_ALLOC_COMPAT,
			kgsl_ioctl_gpumem_alloc_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_TIMESTAMP_EVENT_COMPAT,
			kgsl_ioctl_timestamp_event_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SETPROPERTY_COMPAT,
			kgsl_ioctl_device_setproperty_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_ALLOC_ID_COMPAT,
			kgsl_ioctl_gpumem_alloc_id_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_FREE_ID,
			kgsl_ioctl_gpumem_free_id),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_GET_INFO_COMPAT,
			kgsl_ioctl_gpumem_get_info_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_SYNC_CACHE_COMPAT,
			kgsl_ioctl_gpumem_sync_cache_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_SYNC_CACHE_BULK_COMPAT,
			kgsl_ioctl_gpumem_sync_cache_bulk_compat),
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

long kgsl_compat_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct kgsl_device_private *dev_priv = filep->private_data;
	struct kgsl_device *device = dev_priv->device;

	long ret = kgsl_ioctl_helper(filep, cmd, arg, kgsl_compat_ioctl_funcs,
		ARRAY_SIZE(kgsl_compat_ioctl_funcs));

	/*
	 * If the command was unrecognized in the generic core, try the device
	 * specific function
	 */

	if (ret == -ENOIOCTLCMD) {
		if (device->ftbl->compat_ioctl != NULL)
			return device->ftbl->compat_ioctl(dev_priv, cmd, arg);

		dev_err(device->dev, "invalid ioctl code 0x%08X\n", cmd);
	}

	return ret;
}
