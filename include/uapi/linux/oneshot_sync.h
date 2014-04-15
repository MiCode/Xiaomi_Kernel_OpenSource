#ifndef ONESHOT_SYNC_H
#define ONESHOT_SYNC_H

/**
 * DOC: Oneshot sync Userspace API
 *
 * Opening a file descriptor from /dev/oneshot_sync creates a * sync timeline
 * for userspace signaled fences. Userspace may create new fences from a
 * /dev/oneshot_sync file descriptor and then signal them by passing the fence
 * file descriptor in an ioctl() call on the fd used to create the fence.
 * Unlike most sync timelines, there is no ordering on a oneshot timeline.
 * Each fence may be signaled in any order without affecting the state of other
 * fences on the timeline.
 */

#define ONESHOT_SYNC_IOC_MAGIC '1'

/**
 * struct oneshot_sync_create_fence - argument to create fence ioctl
 * @name: name of the new fence, to aid debugging.
 * @fence_fd: returned sync_fence file descriptor
 */
struct oneshot_sync_create_fence {
	char name[32];
	int fence_fd;
};

/**
 * DOC: ONESHOT_SYNC_IOC_CREATE_FENCE - create a userspace signaled fence
 *
 * Create a fence that may be signaled by userspace by calling
 * ONESHOT_SYNC_IOC_SIGNAL_FENCE. There are no order dependencies between
 * these fences, but otherwise they behave like normal sync fences.
 * Argument is struct oneshot_sync_create_fence.
 */
#define ONESHOT_SYNC_IOC_CREATE_FENCE _IOWR(ONESHOT_SYNC_IOC_MAGIC, 1,\
		struct oneshot_sync_create_fence)

/**
 * DOC: ONESHOT_SYNC_IOC_SIGNAL_FENCE - signal a fence
 *
 * Signal a fence that was created by a ONESHOT_SYNC_IOC_CREATE_FENCE
 * call on the same file descriptor. This allows a fence to be shared
 * to other processes but only signaled by the process owning the fd
 * used to create the fence.  Argument is the fence file descriptor.
 */
#define ONESHOT_SYNC_IOC_SIGNAL_FENCE _IOWR(ONESHOT_SYNC_IOC_MAGIC, 2,\
		int)
#endif
