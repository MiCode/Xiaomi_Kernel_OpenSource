/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef QTI_VIRTIO_MEM_PRIVATE_H
#define QTI_VIRTIO_MEM_PRIVATE_H

#include <linux/platform_device.h>
#include <linux/virtio.h>
#include <linux/virtio_mem.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/crash_dump.h>
#include <linux/mutex.h>

/*
 * State of a Linux memory block in SBM.
 */
enum virtio_mem_sbm_mb_state {
	/* Unplugged, not added to Linux. Can be reused later. */
	VIRTIO_MEM_SBM_MB_UNUSED = 0,
	/* (Partially) plugged, not added to Linux. Error on add_memory(). */
	VIRTIO_MEM_SBM_MB_PLUGGED,
	/* Fully plugged, fully added to Linux, offline. */
	VIRTIO_MEM_SBM_MB_OFFLINE,
	/* Partially plugged, fully added to Linux, offline. */
	VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL,
	/* Fully plugged, fully added to Linux, onlined to a kernel zone. */
	VIRTIO_MEM_SBM_MB_KERNEL,
	/* Partially plugged, fully added to Linux, online to a kernel zone */
	VIRTIO_MEM_SBM_MB_KERNEL_PARTIAL,
	/* Fully plugged, fully added to Linux, onlined to ZONE_MOVABLE. */
	VIRTIO_MEM_SBM_MB_MOVABLE,
	/* Partially plugged, fully added to Linux, onlined to ZONE_MOVABLE. */
	VIRTIO_MEM_SBM_MB_MOVABLE_PARTIAL,
	VIRTIO_MEM_SBM_MB_COUNT
};

/*
 * State of a Big Block (BB) in BBM, covering 1..X Linux memory blocks.
 */
enum virtio_mem_bbm_bb_state {
	/* Unplugged, not added to Linux. Can be reused later. */
	VIRTIO_MEM_BBM_BB_UNUSED = 0,
	/* Plugged, not added to Linux. Error on add_memory(). */
	VIRTIO_MEM_BBM_BB_PLUGGED,
	/* Plugged and added to Linux. */
	VIRTIO_MEM_BBM_BB_ADDED,
	/* All online parts are fake-offline, ready to remove. */
	VIRTIO_MEM_BBM_BB_FAKE_OFFLINE,
	VIRTIO_MEM_BBM_BB_COUNT
};

struct virtio_mem {
	struct platform_device *vdev;

	/* We might first have to unplug all memory when starting up. */
	bool unplug_all_required;

	/* Workqueue that processes the plug/unplug requests. */
	struct work_struct wq;
	atomic_t wq_active;
	atomic_t config_changed;

	/* Wait for a host response to a guest request. */
	wait_queue_head_t host_resp;

	/* Space for one guest request and the host response. */
	struct virtio_mem_req req;
	struct virtio_mem_resp resp;

	/* The current size of the device. */
	uint64_t plugged_size;
	/* The requested size of the device. */
	uint64_t requested_size;

	/* The device block size (for communicating with the device). */
	uint64_t device_block_size;
	/* The determined node id for all memory of the device. */
	int nid;
	/* Physical start address of the memory region. */
	uint64_t addr;
	/* Maximum region size in bytes. */
	uint64_t region_size;
	/*
	 * When memmap_on_memory is enabled, the maximum usable size is less
	 * than the region_size.
	 */
	uint64_t max_pluggable_size;

	/* The parent resource for all memory added via this device. */
	struct resource *parent_resource;
	/*
	 * Copy of "System RAM (virtio_mem)" to be used for
	 * add_memory_driver_managed().
	 */
	const char *resource_name;
	/* Memory group identification. */
	int mgid;

	/*
	 * We don't want to add too much memory if it's not getting onlined,
	 * to avoid running OOM. Besides this threshold, we allow to have at
	 * least two offline blocks at a time (whatever is bigger).
	 */
#define VIRTIO_MEM_DEFAULT_OFFLINE_THRESHOLD		(1024 * 1024 * 1024)
	atomic64_t offline_size;
	uint64_t offline_threshold;

	/* If set, the driver is in SBM, otherwise in BBM. */
	bool in_sbm;

	/*
	 * The first group of pages in a memory_block are used for memmap.
	 * If sbm mode is used, sb_size must equal memmap size, and sb_id == 0
	 * is located at offset sb_size in a memory_block.
	 */
	bool memmap_on_memory;

	/*
	 * Indicates the virtio_mem driver should enable memory encryption on
	 * any transferred memory regions.
	 */
	bool use_memory_encryption;

	union {
		struct {
			/* Id of the first memory block of this device. */
			unsigned long first_mb_id;
			/* Id of the last usable memory block of this device. */
			unsigned long last_usable_mb_id;
			/* Id of the next memory bock to prepare when needed. */
			unsigned long next_mb_id;

			/* The subblock size. */
			uint64_t sb_size;
			/* The number of subblocks per Linux memory block. */
			uint32_t sbs_per_mb;

			/*
			 * Some of the Linux memory blocks tracked as "partially
			 * plugged" are completely unplugged and can be offlined
			 * and removed -- which previously failed.
			 */
			bool have_unplugged_mb;

			/* Summary of all memory block states. */
			unsigned long mb_count[VIRTIO_MEM_SBM_MB_COUNT];

			/*
			 * One byte state per memory block. Allocated via
			 * vmalloc(). Resized (alloc+copy+free) on demand.
			 *
			 * With 128 MiB memory blocks, we have states for 512
			 * GiB of memory in one 4 KiB page.
			 */
			uint8_t *mb_states;

			/*
			 * Bitmap: one bit per subblock. Allocated similar to
			 * sbm.mb_states.
			 *
			 * A set bit means the corresponding subblock is
			 * plugged, otherwise it's unblocked.
			 *
			 * With 4 MiB subblocks, we manage 128 GiB of memory
			 * in one 4 KiB page.
			 */
			unsigned long *sb_states;
		} sbm;

		struct {
			/* Id of the first big block of this device. */
			unsigned long first_bb_id;
			/* Id of the last usable big block of this device. */
			unsigned long last_usable_bb_id;
			/* Id of the next device bock to prepare when needed. */
			unsigned long next_bb_id;

			/* Summary of all big block states. */
			unsigned long bb_count[VIRTIO_MEM_BBM_BB_COUNT];

			/* One byte state per big block. See sbm.mb_states. */
			uint8_t *bb_states;

			/* The block size used for plugging/adding/removing. */
			uint64_t bb_size;
		} bbm;
	};

	/*
	 * Mutex that protects the sbm.mb_count, sbm.mb_states,
	 * sbm.sb_states, bbm.bb_count, and bbm.bb_states
	 *
	 * When this lock is held the pointers can't change, ONLINE and
	 * OFFLINE blocks can't change the state and no subblocks will get
	 * plugged/unplugged.
	 *
	 * In kdump mode, used to serialize requests, last_block_addr and
	 * last_block_plugged.
	 */
	struct mutex hotplug_mutex;
	bool hotplug_active;

	/* An error occurred we cannot handle - stop processing requests. */
	bool broken;

	/* Cached valued of is_kdump_kernel() when the device was probed. */
	bool in_kdump;

	/* The driver is being removed. */
	spinlock_t removal_lock;
	bool removing;

	/* Timer for retrying to plug/unplug memory. */
	struct hrtimer retry_timer;
	unsigned int retry_timer_ms;
#define VIRTIO_MEM_RETRY_TIMER_MIN_MS		50000
#define VIRTIO_MEM_RETRY_TIMER_MAX_MS		300000

	/* Memory notifier (online/offline events). */
	struct notifier_block memory_notifier;

#ifdef CONFIG_PROC_VMCORE
	/* vmcore callback for /proc/vmcore handling in kdump mode */
	struct vmcore_cb vmcore_cb;
	uint64_t last_block_addr;
	bool last_block_plugged;
#endif /* CONFIG_PROC_VMCORE */

	/* Next device in the list of virtio-mem devices. */
	struct list_head next;
};

void virtio_mem_config_changed(struct platform_device *vdev);
int qti_virtio_mem_init(struct platform_device *pdev);
void qti_virtio_mem_exit(struct platform_device *pdev);

/* For now, only allow one virtio-mem device */
extern struct virtio_mem *virtio_mem_dev;

#endif
