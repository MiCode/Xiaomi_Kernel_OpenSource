/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <asm/ioctl.h>

#include "kgsl.h"
#include "kgsl_compat.h"
#include "kgsl_device.h"
#include "kgsl_sync.h"

#include "adreno.h"

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

	param.id_list = (unsigned int __user *)(uintptr_t)param32->id_list;
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

static long kgsl_ioctl_cff_syncmem_compat(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_cff_syncmem_compat *param32 = data;
	struct kgsl_cff_syncmem param;

	param.gpuaddr = (unsigned long)param32->gpuaddr;
	param.len = (size_t)param32->len;

	return kgsl_ioctl_cff_syncmem(dev_priv, cmd, &param);
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
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CFF_SYNCMEM_COMPAT,
			kgsl_ioctl_cff_syncmem_compat),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CFF_USER_EVENT,
			kgsl_ioctl_cff_user_event),
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
};

long kgsl_compat_ioctl(struct file *filep, unsigned int cmd,
				unsigned long arg)
{
	return kgsl_ioctl_helper(filep, cmd, kgsl_compat_ioctl_funcs,
				ARRAY_SIZE(kgsl_compat_ioctl_funcs), arg);
}

/**
 * kgsl_cmdbatch_create_compat() - Compat helper to _kgsl_cmdbatch_create()
 * @device: Pointer to the KGSL device struct for the GPU
 * @flags: Flags passed in from the user command
 * @cmdlist: Pointer to the list of commands from the user. Should point to a
 * kgsl_ibdesc_compat struct
 * @numcmds: Number of commands in the list
 * @synclist: Pointer to the list of syncpoints from the user. Should point to
 * a kgsl_cmd_syncpoint_compat struct
 * @numsyncs: Number of syncpoints in the list
 *
 * This function is called from _kgsl_cmdbatch_create(), if the user process
 * submitting cmds is 32 bit, instead of executing rest of the function.
 * It is needed since we do multiple copy_from_user() calls which would
 * otherwise be copying user data into the wrongly sized/structured struct.
 */
int kgsl_cmdbatch_create_compat(struct kgsl_device *device, unsigned int flags,
			struct kgsl_cmdbatch *cmdbatch, void __user *cmdlist,
			unsigned int numcmds, void __user *synclist,
			unsigned int numsyncs)
{
	int ret = 0, i;

	if (!(flags & (KGSL_CMDBATCH_SYNC | KGSL_CMDBATCH_MARKER))) {
		struct kgsl_ibdesc_compat ibdesc32;
		struct kgsl_ibdesc ibdesc;
		void __user *uptr = cmdlist;

		for (i = 0; i < numcmds; i++) {
			memset(&ibdesc32, 0, sizeof(ibdesc32));

			if (copy_from_user(&ibdesc32, uptr, sizeof(ibdesc32))) {
				ret = -EFAULT;
				goto done;
			}

			ibdesc.gpuaddr = (unsigned long)ibdesc32.gpuaddr;
			ibdesc.sizedwords = (size_t)ibdesc32.sizedwords;
			ibdesc.ctrl = (unsigned int)ibdesc32.ctrl;

			ret = kgsl_cmdbatch_add_memobj(cmdbatch, &ibdesc);
			if (ret)
				goto done;

			uptr += sizeof(ibdesc32);
		}
	}
	if (synclist && numsyncs) {

		struct kgsl_cmd_syncpoint_compat sync32;
		struct kgsl_cmd_syncpoint sync;
		void __user *uptr = synclist;

		for (i = 0; i < numsyncs; i++) {
			memset(&sync32, 0, sizeof(sync32));

			if (copy_from_user(&sync32, uptr, sizeof(sync32)))
				return -EFAULT;

			sync.type = sync32.type;
			sync.priv = compat_ptr(sync32.priv);
			sync.size = (size_t)sync32.size;

			ret = kgsl_cmdbatch_add_sync(device, cmdbatch, &sync);
			if (ret)
				return ret;
			uptr += sizeof(sync32);
		}
	}

done:
	return ret;
}
