/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion_priv.h"
#include <linux/slab.h>
#include <linux/mutex.h>
#include <mmprofile.h>
#include <mmprofile_function.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include "ion_profile.h"
#include "ion_drv_priv.h"
#include "mtk/ion_drv.h"
#include "mtk/mtk_ion.h"

struct task_struct *ion_comm_kthread;
wait_queue_head_t ion_comm_wq;
atomic_t ion_comm_event = ATOMIC_INIT(0);
atomic_t ion_comm_cache_event = ATOMIC_INIT(0);

static int ion_comm_cache_pool(void *data)
{
	int ret;
	int req_cache_size = 0;
	int cached_size = 0;
	int cache_buffer = 0;
	unsigned int gfp_flags = __GFP_HIGHMEM | __GFP_MOVABLE;
	struct ion_buffer *buffer = NULL;
	struct ion_heap *ion_cam_heap;

	ion_cam_heap = ion_drv_get_heap(g_ion_device,
					ION_HEAP_TYPE_MULTIMEDIA_FOR_CAMERA,
					1);
	if (!ion_cam_heap)
		return -1;

	while (1) {
		if (kthread_should_stop()) {
			IONMSG("stop ion history threak\n");
			break;
		}

		ret = wait_event_interruptible(ion_comm_wq,
					       atomic_read(&ion_comm_event));
		if (ret < 0) {
			IONMSG("%s is waked up error", __func__);
			continue;
		}
		req_cache_size = atomic_read(&ion_comm_event);
		cache_buffer = atomic_read(&ion_comm_cache_event);
		atomic_set(&ion_comm_event, 0);

		cached_size = ion_mm_heap_pool_size(ion_cam_heap,
						    gfp_flags,
						    cache_buffer);

		buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
		if ((!buffer) || (cached_size >= req_cache_size)) {
			kfree(buffer);
			buffer = NULL;
			atomic_set(&ion_comm_event, 0);
			IONMSG("%s is ready: req %d, cached %d\n", __func__,
			       req_cache_size, cached_size);
			continue;
		}

		IONMSG("%s alloc start, req %d, cached %d\n", __func__,
		       req_cache_size, cached_size);

		buffer->heap = ion_cam_heap;
		buffer->flags = 3;
		if (cache_buffer == 0)
			buffer->flags = 0;
		buffer->size = req_cache_size;
		ion_mm_heap_cache_allocate(ion_cam_heap,
					   buffer,
					   buffer->size,
					   0,
					   0);
		IONMSG("%s push buffer to cam_pool\n", __func__);
		ion_mm_heap_cache_free(buffer);
		kfree(buffer);
		buffer = NULL;

		IONMSG("%s alloc done\n", __func__);
	}

	return 0;
}

int ion_comm_init(void)
{
	struct sched_param param = { .sched_priority = 0 };

	init_waitqueue_head(&ion_comm_wq);
	ion_comm_kthread = kthread_run(ion_comm_cache_pool, NULL, "%s",
				       "ion_comm_pool");
	if (IS_ERR(ion_comm_kthread)) {
		IONMSG("%s: creating thread for ion history\n", __func__);
		return PTR_RET(ion_comm_kthread);
	}

	sched_setscheduler(ion_comm_kthread, SCHED_IDLE, &param);
	wake_up_process(ion_comm_kthread);

	return 0;
}

void ion_comm_event_notify(bool cache, size_t len)
{
	int req_cache_size = (int)len;

	IONMSG("%s event %d, new len %zu\n", __func__,
	       atomic_read(&ion_comm_event), len);
	atomic_set(&ion_comm_event, req_cache_size);
	if (cache)
		atomic_set(&ion_comm_cache_event, 1);
	else
		atomic_set(&ion_comm_cache_event, 0);
	wake_up_interruptible(&ion_comm_wq);
}
