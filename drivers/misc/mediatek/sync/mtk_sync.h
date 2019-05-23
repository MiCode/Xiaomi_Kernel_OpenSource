/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */


#ifndef _MTK_SYNC_H_
#define _MTK_SYNC_H_

/*
 * TIMEOUT_NEVER may be passed to the wait method to indicate that it
 * should wait indefinitely for the fence to signal.
 */
#define TIMEOUT_NEVER   -1

/* ---------------------------------------------------------------- */

#ifdef __KERNEL__

#include <linux/version.h>
#include <../../../../drivers/dma-buf/sync_debug.h>

//#include <../drivers/staging/android/sw_sync.h>

/*
 * sync_timeline, sync_fence data structure
 */

struct mtk_sync_create_fence_data {
	__u32 value;
	char name[32];
	__s32 fence;	/* fd of new fence */
};


/*
 * sync_timeline, sync_fence API
 */
//extern struct sync_timeline *sync_timeline_create(const char *name); //cc

/**
 * timeline_create() - creates a sync object
 * @name:   sync_timeline name
 *
 * The timeline_create() function creates a sync object named @name,
 * which represents a 32-bit monotonically increasing counter.
 */
struct sync_timeline *timeline_create(const char *name);

/**
 * timeline_destroy() - releases a sync object
 * @obj:    sync_timeline obj
 *
 * The timeline_destroy() function releases a sync object.
 * The remaining active points would be put into signaled list,
 * and their statuses are set to VENOENT.
 */
void timeline_destroy(struct sync_timeline *obj);

/**
 * timeline_inc() - increases timeline
 * @obj:    sync_timeline obj
 * @value:  the increment to a sync object
 *
 * The timeline_inc() function increase the counter of @obj by @value
 * Each sync point contains a value. A sync point on a parent timeline transits
 * from active to signaled status when the counter of a timeline reaches
 * to that of a sync point.
 */
void timeline_inc(struct sync_timeline *obj, u32 value);

/**
 * fence_create() - create a fence
 * @obj:    sync_timeline obj
 * @data:   fence struct with its name and the number a sync point bears
 *
 * The fence_create() function creates a new sync point with @data->value,
 * and assign the sync point to a newly created fence named @data->name.
 * A file descriptor binded with the fence is stored in @data->fence.
 */
int fence_create(struct sync_timeline *obj,
		 struct mtk_sync_create_fence_data *data);

#endif /* __KERNEL __ */

#endif /* _MTK_SYNC_H_ */
