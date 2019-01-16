/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _MTK_SYNC_H
#define _MTK_SYNC_H

/*
 * TIMEOUT_NEVER may be passed to the wait method to indicate that it
 * should wait indefinitely for the fence to signal.
 */
#define TIMEOUT_NEVER   -1

/* --------------------------------------------------------------------------- */

#ifdef __KERNEL__

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#include <linux/sw_sync.h>
#else
#include <../drivers/staging/android/sw_sync.h>
#endif	/* (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)) */

/*
 * sync_timeline, sync_fence data structure
 */

struct fence_data {
	__u32 value;
	char name[32];
	__s32 fence;	/* fd of new fence */
};


/*
 * sync_timeline, sync_fence API
 */

/**
 * timeline_create() - creates a sync object
 * @name:   sync_timeline name
 *
 * The timeline_create() function creates a sync object named @name,
 * which represents a 32-bit monotonically increasing counter.
 */
struct sw_sync_timeline *timeline_create(const char *name);

/**
 * timeline_destroy() - releases a sync object
 * @obj:    sync_timeline obj
 *
 * The timeline_destroy() function releases a sync object.
 * The remaining active points would be put into signaled list,
 * and their statuses are set to ¡VENOENT.
 */
void timeline_destroy(struct sw_sync_timeline *obj);

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
void timeline_inc(struct sw_sync_timeline *obj, u32 value);

/**
 * fence_create() - create a fence
 * @obj:    sync_timeline obj
 * @data:   fence struct with its name and the number a sync point bears
 *
 * The fence_create() function creates a new sync point with @data->value,
 * and assign the sync point to a newly created fence named @data->name.
 * A file descriptor binded with the fence is stored in @data->fence.
 */
int fence_create(struct sw_sync_timeline *obj, struct fence_data *data);

/**
 * fence_merge() - merge two fences into a new one
 * @name:   fence name
 * @fd1:    file descriptor of the first fence
 * @fd2:    file descriptor of the second fence
 *
 * The fence_merge() function creates a new fence which contains copies of all
 * the sync_pts in both @fd1 and @fd2.
 * @fd1 and @fd2 remain valid, independent fences.
 * On success, the newly created fd is returned; Otherwise, a -errno is returned.
 */
int fence_merge(char *const name, int fd1, int fd2);

/**
 * fence_wait() - wait for a fence
 * @fence:      fence pointer to a fence obj
 * @timeout:    how much time we wait at most
 *
 * The fence_wait() function waits for up to @timeout milliseconds
 * for the fence to signal.
 * A timeout of TIMEOUT_NEVER may be used to indicate that
 * the call should wait indefinitely for the fence to signal.
 */
inline int fence_wait(struct sync_fence *fence, int timeout);

#endif	/* __KERNEL __ */

#endif	/* _MTK_SYNC_H */
