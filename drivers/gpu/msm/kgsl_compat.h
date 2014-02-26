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
#ifndef __KGSL_COMPAT_H
#define __KGSL_COMPAT_H

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#include "kgsl.h"
#include "kgsl_device.h"

struct kgsl_ibdesc_compat {
	compat_ulong_t gpuaddr;
	unsigned int __pad;
	compat_size_t sizedwords;
	unsigned int ctrl;
};

struct kgsl_cmd_syncpoint_compat {
	int type;
	compat_uptr_t priv;
	compat_size_t size;
};

struct kgsl_devinfo_compat {
	unsigned int device_id;
	unsigned int chip_id;
	unsigned int mmu_enabled;
	compat_ulong_t gmem_gpubaseaddr;
	unsigned int gpu_id;
	compat_size_t gmem_sizebytes;
};

struct kgsl_shadowprop_compat {
	compat_ulong_t gpuaddr;
	compat_size_t size;
	unsigned int flags;
};

struct kgsl_device_constraint_compat {
	unsigned int type;
	unsigned int context_id;
	compat_uptr_t data;
	compat_size_t size;
};

struct kgsl_device_getproperty_compat {
	unsigned int type;
	compat_uptr_t value;
	compat_size_t sizebytes;
};

#define IOCTL_KGSL_DEVICE_GETPROPERTY_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x2, struct kgsl_device_getproperty_compat)

#define IOCTL_KGSL_SETPROPERTY_COMPAT \
	_IOW(KGSL_IOC_TYPE, 0x32, struct kgsl_device_getproperty_compat)


struct kgsl_submit_commands_compat {
	unsigned int context_id;
	unsigned int flags;
	compat_uptr_t cmdlist;
	unsigned int numcmds;
	compat_uptr_t synclist;
	unsigned int numsyncs;
	unsigned int timestamp;
/* private: reserved for future use */
	unsigned int __pad[4];
};

#define IOCTL_KGSL_SUBMIT_COMMANDS_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x3D, struct kgsl_submit_commands_compat)

struct kgsl_ringbuffer_issueibcmds_compat {
	unsigned int drawctxt_id;
	compat_ulong_t ibdesc_addr;
	unsigned int numibs;
	unsigned int timestamp; /* output param */
	unsigned int flags;
};

#define IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x10, struct kgsl_ringbuffer_issueibcmds_compat)

struct kgsl_cmdstream_freememontimestamp_compat {
	compat_ulong_t gpuaddr;
	unsigned int type;
	unsigned int timestamp;
};

#define IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP_COMPAT \
	_IOW(KGSL_IOC_TYPE, 0x12, \
	struct kgsl_cmdstream_freememontimestamp_compat)

struct kgsl_cmdstream_freememontimestamp_ctxtid_compat {
	unsigned int context_id;
	compat_ulong_t gpuaddr;
	unsigned int type;
	unsigned int timestamp;
};

#define IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP_CTXTID_COMPAT \
	_IOW(KGSL_IOC_TYPE, 0x17, \
	struct kgsl_cmdstream_freememontimestamp_ctxtid_compat)

struct kgsl_map_user_mem_compat {
	int fd;
	compat_ulong_t gpuaddr;
	compat_size_t len;
	compat_size_t offset;
	compat_ulong_t hostptr;
	enum kgsl_user_mem_type memtype;
	unsigned int flags;
};

#define IOCTL_KGSL_MAP_USER_MEM_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x15, struct kgsl_map_user_mem_compat)

struct kgsl_sharedmem_free_compat {
	compat_ulong_t gpuaddr;
};

#define IOCTL_KGSL_SHAREDMEM_FLUSH_CACHE_COMPAT \
	_IOW(KGSL_IOC_TYPE, 0x24, struct kgsl_sharedmem_free_compat)

#define IOCTL_KGSL_SHAREDMEM_FREE_COMPAT \
	_IOW(KGSL_IOC_TYPE, 0x21, struct kgsl_sharedmem_free_compat)

struct kgsl_gpumem_alloc_compat {
	compat_ulong_t gpuaddr; /* output param */
	compat_size_t size;
	unsigned int flags;
};

#define IOCTL_KGSL_GPUMEM_ALLOC_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x2f, struct kgsl_gpumem_alloc_compat)

struct kgsl_cff_syncmem_compat {
	compat_ulong_t gpuaddr;
	compat_size_t len;
	unsigned int __pad[2]; /* For future binary compatibility */
};

#define IOCTL_KGSL_CFF_SYNCMEM_COMPAT \
	_IOW(KGSL_IOC_TYPE, 0x30, struct kgsl_cff_syncmem_compat)

struct kgsl_timestamp_event_compat {
	int type;                /* Type of event (see list below) */
	unsigned int timestamp;  /* Timestamp to trigger event on */
	unsigned int context_id; /* Context for the timestamp */
	compat_uptr_t priv;      /* Pointer to the event specific blob */
	compat_size_t len;       /* Size of the event specific blob */
};

#define IOCTL_KGSL_TIMESTAMP_EVENT_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x33, struct kgsl_timestamp_event_compat)

struct kgsl_gpumem_alloc_id_compat {
	unsigned int id;
	unsigned int flags;
	compat_size_t size;
	compat_size_t mmapsize;
	compat_ulong_t gpuaddr;
/* private: reserved for future use*/
	unsigned int __pad[2];
};

#define IOCTL_KGSL_GPUMEM_ALLOC_ID_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x34, struct kgsl_gpumem_alloc_id_compat)

struct kgsl_gpumem_get_info_compat {
	compat_ulong_t gpuaddr;
	unsigned int id;
	unsigned int flags;
	compat_size_t size;
	compat_size_t mmapsize;
	compat_ulong_t useraddr;
/* private: reserved for future use*/
	unsigned int __pad[4];
};

#define IOCTL_KGSL_GPUMEM_GET_INFO_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x36, struct kgsl_gpumem_get_info_compat)

struct kgsl_gpumem_sync_cache_compat {
	compat_ulong_t gpuaddr;
	unsigned int id;
	unsigned int op;
	compat_size_t length;
	compat_size_t offset;
};

#define IOCTL_KGSL_GPUMEM_SYNC_CACHE_COMPAT \
	_IOW(KGSL_IOC_TYPE, 0x37, struct kgsl_gpumem_sync_cache_compat)

struct kgsl_gpumem_sync_cache_bulk_compat {
	compat_uptr_t id_list;
	unsigned int count;
	unsigned int op;
/* private: reserved for future use */
	unsigned int __pad[2]; /* For future binary compatibility */
};

#define IOCTL_KGSL_GPUMEM_SYNC_CACHE_BULK_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x3C, struct kgsl_gpumem_sync_cache_bulk_compat)

struct kgsl_perfcounter_query_compat {
	unsigned int groupid;
	compat_uptr_t countables;
	unsigned int count;
	unsigned int max_counters;
	unsigned int __pad[2];
};

#define IOCTL_KGSL_PERFCOUNTER_QUERY_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x3A, struct kgsl_perfcounter_query_compat)

struct kgsl_perfcounter_read_compat {
	compat_uptr_t reads;
	unsigned int count;
	unsigned int __pad[2];
};

#define IOCTL_KGSL_PERFCOUNTER_READ_COMPAT \
	_IOWR(KGSL_IOC_TYPE, 0x3B, struct kgsl_perfcounter_read_compat)

static inline compat_ulong_t gpuaddr_to_compat(unsigned long gpuaddr)
{
	WARN(gpuaddr >> 32, "Top 32 bits of gpuaddr have been set\n");
	return (compat_ulong_t)gpuaddr;
}

static inline compat_size_t sizet_to_compat(size_t size)
{
	WARN(size >> 32, "Size greater than 4G\n");
	return (compat_size_t)size;
}

int kgsl_cmdbatch_create_compat(struct kgsl_device *device, unsigned int flags,
			struct kgsl_cmdbatch *cmdbatch, void __user *cmdlist,
			unsigned int numcmds, void __user *synclist,
			unsigned int numsyncs);

long kgsl_compat_ioctl(struct file *filep, unsigned int cmd,
			unsigned long arg);

int adreno_getproperty_compat(struct kgsl_device *device,
			enum kgsl_property_type type,
			void __user *value,
			size_t sizebytes);

int adreno_setproperty_compat(struct kgsl_device_private *dev_priv,
				enum kgsl_property_type type,
				void __user *value,
				unsigned int sizebytes);

long adreno_compat_ioctl(struct kgsl_device_private *dev_priv,
			unsigned int cmd, void *data);

#else
static inline int kgsl_cmdbatch_create_compat(struct kgsl_device *device,
			unsigned int flags, struct kgsl_cmdbatch *cmdbatch,
			void __user *cmdlist, unsigned int numcmds,
			void __user *synclist, unsigned int numsyncs)
{
	BUG();
}

static inline long kgsl_compat_ioctl(struct file *filep, unsigned int cmd,
			unsigned long arg)
{
	BUG();
}

static inline int adreno_getproperty_compat(struct kgsl_device *device,
				enum kgsl_property_type type,
				void __user *value, size_t sizebytes)
{
	BUG();
}

static inline int adreno_setproperty_compat(struct kgsl_device_private
				*dev_priv, enum kgsl_property_type type,
				void __user *value, unsigned int sizebytes)
{
	BUG();
}

static inline long adreno_compat_ioctl(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data)
{
	BUG();
}

#endif /* CONFIG_COMPAT */
#endif /* __KGSL_COMPAT_H */
