/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/sync.h>
#include <linux/oneshot_sync.h>

/**
 * struct oneshot_sync_timeline - a userspace signaled, out of order, timeline
 * @obj: base sync timeline
 * @lock: spinlock to guard other members
 * @state_list: list of oneshot_sync_states.
 * @id: next id for points creating oneshot_sync_pts
 */
struct oneshot_sync_timeline {
	struct sync_timeline obj;
	spinlock_t lock;
	struct list_head state_list;
	unsigned int id;
};

#define to_oneshot_timeline(_p) \
	container_of((_p), struct oneshot_sync_timeline, obj)

/**
 * struct oneshot_sync_state - signal state for a group of oneshot points
 * @refcount: reference count for this structure.
 * @signaled: is this signaled or not?
 * @id: identifier for this state
 * @orig_fence: fence used to create this state, no is reference count held.
 * @timeline: back pointer to the timeline.
 */
struct oneshot_sync_state {
	struct kref refcount;
	struct list_head node;
	bool signaled;
	unsigned int id;
	struct sync_fence *orig_fence;
	struct oneshot_sync_timeline *timeline;
};

/**
 * struct oneshot_sync_pt
 * @sync_pt: base sync point structure
 * @state: reference counted pointer to the state of this pt
 */
struct oneshot_sync_pt {
	struct sync_pt sync_pt;
	struct oneshot_sync_state *state;
	bool dup;
};
#define to_oneshot_pt(_p) container_of((_p), struct oneshot_sync_pt, sync_pt)

static void oneshot_state_destroy(struct kref *ref)
{
	struct oneshot_sync_state *state =
		container_of(ref, struct oneshot_sync_state, refcount);

	spin_lock(&state->timeline->lock);
	list_del(&state->node);
	spin_unlock(&state->timeline->lock);

	kfree(state);
}

static void oneshot_state_put(struct oneshot_sync_state *state)
{
	kref_put(&state->refcount, oneshot_state_destroy);
}

static struct oneshot_sync_pt *
oneshot_pt_create(struct oneshot_sync_timeline *timeline)
{
	struct oneshot_sync_pt *pt = NULL;

	pt = (struct oneshot_sync_pt *)sync_pt_create(&timeline->obj,
						     sizeof(*pt));
	if (pt == NULL)
		return NULL;

	pt->state = kzalloc(sizeof(struct oneshot_sync_state), GFP_KERNEL);
	if (pt->state == NULL)
		goto error;

	kref_init(&pt->state->refcount);
	pt->state->signaled = false;
	pt->state->timeline = timeline;

	spin_lock(&timeline->lock);
	/* assign an id to the state, which could be shared by several pts. */
	pt->state->id = ++(timeline->id);
	/* add this pt to the list of pts that can be signaled by userspace */
	list_add_tail(&pt->state->node, &timeline->state_list);
	spin_unlock(&timeline->lock);

	return pt;
error:
	if (pt)
		sync_pt_free(&pt->sync_pt);
	return NULL;
}

static struct sync_pt *oneshot_pt_dup(struct sync_pt *sync_pt)
{
	struct oneshot_sync_pt *out_pt;
	struct oneshot_sync_pt *pt = to_oneshot_pt(sync_pt);

	if (!kref_get_unless_zero(&pt->state->refcount))
		return NULL;

	out_pt = (struct oneshot_sync_pt *)
		sync_pt_create(sync_pt->parent, sizeof(*out_pt));

	if (out_pt == NULL) {
		oneshot_state_put(pt->state);
		return NULL;
	}
	out_pt->state = pt->state;
	out_pt->dup = true;

	return &out_pt->sync_pt;
}

static int oneshot_pt_has_signaled(struct sync_pt *sync_pt)
{
	struct oneshot_sync_pt *pt = to_oneshot_pt(sync_pt);

	return pt->state->signaled;
}

static int oneshot_pt_compare(struct sync_pt *a, struct sync_pt *b)
{
	struct oneshot_sync_pt *pt_a = to_oneshot_pt(a);
	struct oneshot_sync_pt *pt_b = to_oneshot_pt(b);
	/*
	 * since oneshot sync points are order-independent,
	 * return an arbitrary order which just happens to
	 * prevent sync.c from collapsing the points.
	 */
	return (pt_a->state == pt_b->state) ? 0 : 1;
}

static void oneshot_pt_free(struct sync_pt *sync_pt)
{
	struct oneshot_sync_pt *pt = to_oneshot_pt(sync_pt);

	struct oneshot_sync_timeline *timeline = sync_pt->parent ?
		to_oneshot_timeline(sync_pt->parent) : NULL;

	if (timeline != NULL) {
		spin_lock(&timeline->lock);
		/*
		 * If this is the original pt (and fence), signal to avoid
		 * deadlock. Unfornately, we can't signal the timeline here
		 * safely, so there could be a delay until the pt's
		 * state change is noticed.
		 */

		if (pt->dup == false) {
			/*
			 * If the original pt goes away, force it signaled to
			 * avoid deadlock.
			 */
			if (!pt->state->signaled) {
				pr_debug("id %d: fence closed before signal.\n",
						pt->state->id);
				pt->state->signaled = true;
			}
		}
		spin_unlock(&timeline->lock);
	}
	oneshot_state_put(pt->state);
}

static void oneshot_pt_value_str(struct sync_pt *sync_pt, char *str, int size)
{
	struct oneshot_sync_pt *pt = to_oneshot_pt(sync_pt);

	snprintf(str, size, "%u", pt->state->id);
}

static struct sync_timeline_ops oneshot_timeline_ops = {
	.driver_name = "oneshot",
	.dup = oneshot_pt_dup,
	.has_signaled = oneshot_pt_has_signaled,
	.compare = oneshot_pt_compare,
	.free_pt = oneshot_pt_free,
	.pt_value_str = oneshot_pt_value_str,
};

struct oneshot_sync_timeline *oneshot_timeline_create(const char *name)
{
	struct oneshot_sync_timeline *timeline = NULL;
	static const char *default_name = "oneshot-timeline";

	if (name == NULL)
		name = default_name;

	timeline = (struct oneshot_sync_timeline *)
			sync_timeline_create(&oneshot_timeline_ops,
					     sizeof(*timeline),
					     name);

	if (timeline == NULL)
		return NULL;

	INIT_LIST_HEAD(&timeline->state_list);
	spin_lock_init(&timeline->lock);

	return timeline;
}
EXPORT_SYMBOL(oneshot_timeline_create);

void oneshot_timeline_destroy(struct oneshot_sync_timeline *timeline)
{
	if (timeline)
		sync_timeline_destroy(&timeline->obj);
}
EXPORT_SYMBOL(oneshot_timeline_destroy);

struct sync_fence *oneshot_fence_create(struct oneshot_sync_timeline *timeline,
					const char *name)
{
	struct sync_fence *fence = NULL;
	struct oneshot_sync_pt *pt = NULL;

	pt = oneshot_pt_create(timeline);
	if (pt == NULL)
		return NULL;

	fence = sync_fence_create(name, &pt->sync_pt);
	if (fence == NULL) {
		sync_pt_free(&pt->sync_pt);
		return NULL;
	}

	pt->state->orig_fence = fence;

	return fence;
}
EXPORT_SYMBOL(oneshot_fence_create);

int oneshot_fence_signal(struct oneshot_sync_timeline *timeline,
			struct sync_fence *fence)
{
	int ret = -EINVAL;
	struct oneshot_sync_state *state = NULL;
	bool signaled = false;

	if (timeline == NULL || fence == NULL)
		return -EINVAL;

	spin_lock(&timeline->lock);
	list_for_each_entry(state, &timeline->state_list, node) {
		/*
		 * If we have the point from this fence on our list,
		 * this is is the original fence we created, so signal it.
		 */
		if (state->orig_fence == fence) {
			/* ignore attempts to signal multiple times */
			if (!state->signaled) {
				state->signaled = true;
				signaled = true;
			}
			ret = 0;
			break;
		}
	}
	spin_unlock(&timeline->lock);
	if (ret == -EINVAL)
		pr_debug("fence: %p not from this timeline\n", fence);

	if (signaled)
		sync_timeline_signal(&timeline->obj);
	return ret;
}
EXPORT_SYMBOL(oneshot_fence_signal);

#ifdef CONFIG_ONESHOT_SYNC_USER

static int oneshot_open(struct inode *inode, struct file *file)
{
	struct oneshot_sync_timeline *timeline = NULL;
	char name[32];
	char task_comm[TASK_COMM_LEN];

	get_task_comm(task_comm, current);
	snprintf(name, sizeof(name), "%s-oneshot", task_comm);

	timeline = oneshot_timeline_create(name);
	if (timeline == NULL)
		return -ENOMEM;

	file->private_data = timeline;
	return 0;
}

static int oneshot_release(struct inode *inode, struct file *file)
{
	struct oneshot_sync_timeline *timeline = file->private_data;

	oneshot_timeline_destroy(timeline);

	return 0;
}

static long oneshot_ioctl_fence_create(struct oneshot_sync_timeline *timeline,
				 unsigned long arg)
{
	struct oneshot_sync_create_fence param;
	int ret = -ENOMEM;
	struct sync_fence *fence = NULL;
	int fd = get_unused_fd();

	if (fd < 0)
		return fd;

	if (copy_from_user(&param, (void __user *)arg, sizeof(param))) {
		ret = -EFAULT;
		goto out;
	}

	fence = oneshot_fence_create(timeline, param.name);
	if (fence == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	param.fence_fd = fd;

	if (copy_to_user((void __user *)arg, &param, sizeof(param))) {
		ret = -EFAULT;
		goto out;
	}

	sync_fence_install(fence, fd);
	ret = 0;
out:
	if (ret) {
		if (fence)
			sync_fence_put(fence);
		put_unused_fd(fd);
	}
	return ret;
}



static long oneshot_ioctl_fence_signal(struct oneshot_sync_timeline *timeline,
				 unsigned long arg)
{
	int ret = -EINVAL;
	int fd = -1;
	struct sync_fence *fence = NULL;

	if (get_user(fd, (int __user *)arg))
		return -EFAULT;

	fence = sync_fence_fdget(fd);
	if (fence == NULL)
		return -EBADF;

	ret = oneshot_fence_signal(timeline, fence);
	sync_fence_put(fence);

	return ret;
}

static long oneshot_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	struct oneshot_sync_timeline *timeline = file->private_data;

	switch (cmd) {
	case ONESHOT_SYNC_IOC_CREATE_FENCE:
		return oneshot_ioctl_fence_create(timeline, arg);

	case ONESHOT_SYNC_IOC_SIGNAL_FENCE:
		return oneshot_ioctl_fence_signal(timeline, arg);

	default:
		return -ENOTTY;
	}
}

static const struct file_operations oneshot_fops = {
	.owner = THIS_MODULE,
	.open = oneshot_open,
	.release = oneshot_release,
	.unlocked_ioctl = oneshot_ioctl,
	.compat_ioctl = oneshot_ioctl,
};
static struct miscdevice oneshot_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "oneshot_sync",
	.fops	= &oneshot_fops,
};

static int __init oneshot_init(void)
{
	return misc_register(&oneshot_dev);
}

static void __exit oneshot_remove(void)
{
	misc_deregister(&oneshot_dev);
}

module_init(oneshot_init);
module_exit(oneshot_remove);

#endif /* CONFIG_ONESHOT_SYNC_USER */
MODULE_LICENSE("GPL v2");

