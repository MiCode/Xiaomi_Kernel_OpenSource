/* Copyright (c) 2002,2007-2012, Code Aurora Forum. All rights reserved.
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
#ifndef __KGSL_DEVICE_H
#define __KGSL_DEVICE_H

#include <linux/idr.h>
#include <linux/wakelock.h>
#include <linux/pm_qos_params.h>
#include <linux/earlysuspend.h>

#include "kgsl.h"
#include "kgsl_mmu.h"
#include "kgsl_pwrctrl.h"
#include "kgsl_log.h"
#include "kgsl_pwrscale.h"

#define KGSL_TIMEOUT_NONE       0
#define KGSL_TIMEOUT_DEFAULT    0xFFFFFFFF

#define FIRST_TIMEOUT (HZ / 2)


/* KGSL device state is initialized to INIT when platform_probe		*
 * sucessfully initialized the device.  Once a device has been opened	*
 * (started) it becomes active.  NAP implies that only low latency	*
 * resources (for now clocks on some platforms) are off.  SLEEP implies	*
 * that the KGSL module believes a device is idle (has been inactive	*
 * past its timer) and all system resources are released.  SUSPEND is	*
 * requested by the kernel and will be enforced upon all open devices.	*/

#define KGSL_STATE_NONE		0x00000000
#define KGSL_STATE_INIT		0x00000001
#define KGSL_STATE_ACTIVE	0x00000002
#define KGSL_STATE_NAP		0x00000004
#define KGSL_STATE_SLEEP	0x00000008
#define KGSL_STATE_SUSPEND	0x00000010
#define KGSL_STATE_HUNG		0x00000020
#define KGSL_STATE_DUMP_AND_RECOVER	0x00000040
#define KGSL_STATE_SLUMBER	0x00000080

#define KGSL_GRAPHICS_MEMORY_LOW_WATERMARK  0x1000000

#define KGSL_IS_PAGE_ALIGNED(addr) (!((addr) & (~PAGE_MASK)))

struct kgsl_device;
struct platform_device;
struct kgsl_device_private;
struct kgsl_context;
struct kgsl_power_stats;

struct kgsl_functable {
	/* Mandatory functions - these functions must be implemented
	   by the client device.  The driver will not check for a NULL
	   pointer before calling the hook.
	 */
	void (*regread) (struct kgsl_device *device,
		unsigned int offsetwords, unsigned int *value);
	void (*regwrite) (struct kgsl_device *device,
		unsigned int offsetwords, unsigned int value);
	int (*idle) (struct kgsl_device *device, unsigned int timeout);
	unsigned int (*isidle) (struct kgsl_device *device);
	int (*suspend_context) (struct kgsl_device *device);
	int (*start) (struct kgsl_device *device, unsigned int init_ram);
	int (*stop) (struct kgsl_device *device);
	int (*getproperty) (struct kgsl_device *device,
		enum kgsl_property_type type, void *value,
		unsigned int sizebytes);
	int (*waittimestamp) (struct kgsl_device *device,
		unsigned int timestamp, unsigned int msecs);
	unsigned int (*readtimestamp) (struct kgsl_device *device,
		enum kgsl_timestamp_type type);
	int (*issueibcmds) (struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_ibdesc *ibdesc,
		unsigned int sizedwords, uint32_t *timestamp,
		unsigned int flags);
	int (*setup_pt)(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable);
	void (*cleanup_pt)(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable);
	void (*power_stats)(struct kgsl_device *device,
		struct kgsl_power_stats *stats);
	void (*irqctrl)(struct kgsl_device *device, int state);
	unsigned int (*gpuid)(struct kgsl_device *device);
	void * (*snapshot)(struct kgsl_device *device, void *snapshot,
		int *remain, int hang);
	/* Optional functions - these functions are not mandatory.  The
	   driver will check that the function pointer is not NULL before
	   calling the hook */
	void (*setstate) (struct kgsl_device *device, uint32_t flags);
	int (*drawctxt_create) (struct kgsl_device *device,
		struct kgsl_pagetable *pagetable, struct kgsl_context *context,
		uint32_t flags);
	void (*drawctxt_destroy) (struct kgsl_device *device,
		struct kgsl_context *context);
	long (*ioctl) (struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data);
};

struct kgsl_memregion {
	unsigned char *mmio_virt_base;
	unsigned int mmio_phys_base;
	uint32_t gpu_base;
	unsigned int sizebytes;
};

/* MH register values */
struct kgsl_mh {
	unsigned int     mharb;
	unsigned int     mh_intf_cfg1;
	unsigned int     mh_intf_cfg2;
	uint32_t         mpu_base;
	int              mpu_range;
};

struct kgsl_event {
	uint32_t timestamp;
	void (*func)(struct kgsl_device *, void *, u32);
	void *priv;
	struct list_head list;
	struct kgsl_device_private *owner;
};


struct kgsl_device {
	struct device *dev;
	const char *name;
	unsigned int ver_major;
	unsigned int ver_minor;
	uint32_t flags;
	enum kgsl_deviceid id;
	struct kgsl_memregion regspace;
	struct kgsl_memdesc memstore;
	const char *iomemname;

	struct kgsl_mh mh;
	struct kgsl_mmu mmu;
	struct completion hwaccess_gate;
	const struct kgsl_functable *ftbl;
	struct work_struct idle_check_ws;
	struct timer_list idle_timer;
	struct kgsl_pwrctrl pwrctrl;
	int open_count;

	struct atomic_notifier_head ts_notifier_list;
	struct mutex mutex;
	uint32_t state;
	uint32_t requested_state;

	unsigned int active_cnt;
	struct completion suspend_gate;

	wait_queue_head_t wait_queue;
	struct workqueue_struct *work_queue;
	struct device *parentdev;
	struct completion recovery_gate;
	struct dentry *d_debugfs;
	struct idr context_idr;
	struct early_suspend display_off;

	void *snapshot;		/* Pointer to the snapshot memory region */
	int snapshot_maxsize;   /* Max size of the snapshot region */
	int snapshot_size;      /* Current size of the snapshot region */
	u32 snapshot_timestamp;	/* Timestamp of the last valid snapshot */
	int snapshot_frozen;	/* 1 if the snapshot output is frozen until
				   it gets read by the user.  This avoids
				   losing the output on multiple hangs  */
	struct kobject snapshot_kobj;

	/* Logging levels */
	int cmd_log;
	int ctxt_log;
	int drv_log;
	int mem_log;
	int pwr_log;
	struct wake_lock idle_wakelock;
	struct kgsl_pwrscale pwrscale;
	struct kobject pwrscale_kobj;
	struct pm_qos_request_list pm_qos_req_dma;
	struct work_struct ts_expired_ws;
	struct list_head events;
	s64 on_time;
};

struct kgsl_context {
	uint32_t id;

	/* Pointer to the owning device instance */
	struct kgsl_device_private *dev_priv;

	/* Pointer to the device specific context information */
	void *devctxt;
	/*
	 * Status indicating whether a gpu reset occurred and whether this
	 * context was responsible for causing it
	 */
	unsigned int reset_status;
};

struct kgsl_process_private {
	unsigned int refcnt;
	pid_t pid;
	spinlock_t mem_lock;
	struct list_head mem_list;
	struct kgsl_pagetable *pagetable;
	struct list_head list;
	struct kobject kobj;

	struct {
		unsigned int cur;
		unsigned int max;
	} stats[KGSL_MEM_ENTRY_MAX];
};

struct kgsl_device_private {
	struct kgsl_device *device;
	struct kgsl_process_private *process_priv;
};

struct kgsl_power_stats {
	s64 total_time;
	s64 busy_time;
};

struct kgsl_device *kgsl_get_device(int dev_idx);

static inline void kgsl_process_add_stats(struct kgsl_process_private *priv,
	unsigned int type, size_t size)
{
	priv->stats[type].cur += size;
	if (priv->stats[type].max < priv->stats[type].cur)
		priv->stats[type].max = priv->stats[type].cur;
}

static inline void kgsl_regread(struct kgsl_device *device,
				unsigned int offsetwords,
				unsigned int *value)
{
	device->ftbl->regread(device, offsetwords, value);
}

static inline void kgsl_regwrite(struct kgsl_device *device,
				 unsigned int offsetwords,
				 unsigned int value)
{
	device->ftbl->regwrite(device, offsetwords, value);
}

static inline int kgsl_idle(struct kgsl_device *device, unsigned int timeout)
{
	return device->ftbl->idle(device, timeout);
}

static inline unsigned int kgsl_gpuid(struct kgsl_device *device)
{
	return device->ftbl->gpuid(device);
}

static inline int kgsl_create_device_sysfs_files(struct device *root,
	const struct device_attribute **list)
{
	int ret = 0, i;
	for (i = 0; list[i] != NULL; i++)
		ret |= device_create_file(root, list[i]);
	return ret;
}

static inline void kgsl_remove_device_sysfs_files(struct device *root,
	const struct device_attribute **list)
{
	int i;
	for (i = 0; list[i] != NULL; i++)
		device_remove_file(root, list[i]);
}

static inline struct kgsl_mmu *
kgsl_get_mmu(struct kgsl_device *device)
{
	return (struct kgsl_mmu *) (device ? &device->mmu : NULL);
}

static inline struct kgsl_device *kgsl_device_from_dev(struct device *dev)
{
	int i;

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		if (kgsl_driver.devp[i] && kgsl_driver.devp[i]->dev == dev)
			return kgsl_driver.devp[i];
	}

	return NULL;
}

static inline int kgsl_create_device_workqueue(struct kgsl_device *device)
{
	device->work_queue = create_singlethread_workqueue(device->name);
	if (!device->work_queue) {
		KGSL_DRV_ERR(device,
			     "create_singlethread_workqueue(%s) failed\n",
			     device->name);
		return -EINVAL;
	}
	return 0;
}

static inline struct kgsl_context *
kgsl_find_context(struct kgsl_device_private *dev_priv, uint32_t id)
{
	struct kgsl_context *ctxt =
		idr_find(&dev_priv->device->context_idr, id);

	/* Make sure that the context belongs to the current instance so
	   that other processes can't guess context IDs and mess things up */

	return  (ctxt && ctxt->dev_priv == dev_priv) ? ctxt : NULL;
}

int kgsl_check_timestamp(struct kgsl_device *device, unsigned int timestamp);

int kgsl_register_ts_notifier(struct kgsl_device *device,
			      struct notifier_block *nb);

int kgsl_unregister_ts_notifier(struct kgsl_device *device,
				struct notifier_block *nb);

int kgsl_device_platform_probe(struct kgsl_device *device,
		irqreturn_t (*dev_isr) (int, void*));
void kgsl_device_platform_remove(struct kgsl_device *device);

const char *kgsl_pwrstate_to_str(unsigned int state);

int kgsl_device_snapshot_init(struct kgsl_device *device);
int kgsl_device_snapshot(struct kgsl_device *device, int hang);
void kgsl_device_snapshot_close(struct kgsl_device *device);

#endif  /* __KGSL_DEVICE_H */
