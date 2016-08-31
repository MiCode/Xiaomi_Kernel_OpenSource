/*
 * drivers/video/tegra/host/nvhost_sync.c
 *
 * Tegra Graphics Host Syncpoint Integration to linux/sync Framework
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/nvhost_ioctl.h>

#include "nvhost_sync.h"
#include "nvhost_syncpt.h"
#include "nvhost_intr.h"
#include "nvhost_acm.h"
#include "dev.h"
#include "chip_support.h"

struct nvhost_sync_timeline {
	struct sync_timeline		obj;
	struct nvhost_syncpt		*sp;
	u32				id;
};

/**
 * The sync framework dups pts when merging fences. We share a single
 * refcounted nvhost_sync_pt for each duped pt.
 */
struct nvhost_sync_pt {
	struct kref			refcount;
	u32				thresh;
	bool				has_intr;
	struct nvhost_sync_timeline	*obj;
};

struct nvhost_sync_pt_inst {
	struct sync_pt			pt;
	struct nvhost_sync_pt		*shared;
};

struct nvhost_sync_pt *to_nvhost_sync_pt(struct sync_pt *pt)
{
	struct nvhost_sync_pt_inst *pti =
			container_of(pt, struct nvhost_sync_pt_inst, pt);
	return pti->shared;
}

static void nvhost_sync_pt_free_shared(struct kref *ref)
{
	struct nvhost_sync_pt *pt =
		container_of(ref, struct nvhost_sync_pt, refcount);

	/* Host should have been idled in nvhost_sync_pt_signal. */
	BUG_ON(pt->has_intr);
	kfree(pt);
}

/* Request an interrupt to signal the timeline on thresh. */
static int nvhost_sync_pt_set_intr(struct nvhost_sync_pt *pt)
{
	int err;
	void *waiter;

	/**
	 * When this syncpoint expires, we must call sync_timeline_signal.
	 * That requires us to schedule an interrupt at this point, even
	 * though we might never end up doing a CPU wait on the syncpoint.
	 * Most of the time this does not hurt us since we have already set
	 * an interrupt for SUBMIT_COMPLETE on the same syncpt value.
	 */

	/* Get a ref for the interrupt handler, keep host alive. */
	kref_get(&pt->refcount);
	pt->has_intr = true;
	nvhost_module_busy(syncpt_to_dev(pt->obj->sp)->dev);

	waiter = nvhost_intr_alloc_waiter();
	err = nvhost_intr_add_action(&(syncpt_to_dev(pt->obj->sp)->intr),
				pt->obj->id, pt->thresh,
				NVHOST_INTR_ACTION_SIGNAL_SYNC_PT, pt,
				waiter, NULL);
	if (err) {
		nvhost_module_idle(syncpt_to_dev(pt->obj->sp)->dev);
		pt->has_intr = false;
		kref_put(&pt->refcount, nvhost_sync_pt_free_shared);
		return err;
	}
	return 0;
}

static struct nvhost_sync_pt *nvhost_sync_pt_create_shared(
		struct nvhost_sync_timeline *obj, u32 thresh)
{
	struct nvhost_sync_pt *shared;

	shared = kzalloc(sizeof(*shared), GFP_KERNEL);
	if (!shared)
		return NULL;

	kref_init(&shared->refcount);
	shared->obj = obj;
	shared->thresh = thresh;
	shared->has_intr = false;

	if ((obj->id != NVSYNCPT_INVALID) &&
	    !nvhost_syncpt_is_expired(obj->sp, obj->id, thresh)) {
		if (nvhost_sync_pt_set_intr(shared)) {
			kfree(shared);
			return NULL;
		}
	}

	return shared;
}

static struct sync_pt *nvhost_sync_pt_create_inst(
		struct nvhost_sync_timeline *obj, u32 thresh)
{
	struct nvhost_sync_pt_inst *pti;

	pti = (struct nvhost_sync_pt_inst *)
		sync_pt_create(&obj->obj, sizeof(*pti));
	if (!pti)
		return NULL;

	pti->shared = nvhost_sync_pt_create_shared(obj, thresh);
	if (!pti->shared) {
		sync_pt_free(&pti->pt);
		return NULL;
	}
	return &pti->pt;
}

static void nvhost_sync_pt_free_inst(struct sync_pt *sync_pt)
{
	struct nvhost_sync_pt *pt = to_nvhost_sync_pt(sync_pt);
	if (pt)
		kref_put(&pt->refcount, nvhost_sync_pt_free_shared);
}

static struct sync_pt *nvhost_sync_pt_dup_inst(struct sync_pt *sync_pt)
{
	struct nvhost_sync_pt_inst *pti;
	struct nvhost_sync_pt *pt = to_nvhost_sync_pt(sync_pt);

	pti = (struct nvhost_sync_pt_inst *)
		sync_pt_create(&pt->obj->obj, sizeof(*pti));
	if (!pti)
		return NULL;
	pti->shared = pt;
	kref_get(&pt->refcount);
	return &pti->pt;
}

static int nvhost_sync_pt_has_signaled(struct sync_pt *sync_pt)
{
	struct nvhost_sync_pt *pt = to_nvhost_sync_pt(sync_pt);
	struct nvhost_sync_timeline *obj = pt->obj;

	if (obj->id != NVSYNCPT_INVALID)
		/* No need to update min */
		return nvhost_syncpt_is_expired(obj->sp, obj->id, pt->thresh);
	else
		return 1;
}

static int nvhost_sync_pt_compare(struct sync_pt *a, struct sync_pt *b)
{
	struct nvhost_sync_pt *pt_a = to_nvhost_sync_pt(a);
	struct nvhost_sync_pt *pt_b = to_nvhost_sync_pt(b);
	struct nvhost_sync_timeline *obj = pt_a->obj;
	BUG_ON(pt_a->obj != pt_b->obj);

	if (obj->id != NVSYNCPT_INVALID)
		/* No need to update min */
		return nvhost_syncpt_compare(obj->sp, obj->id,
					     pt_a->thresh, pt_b->thresh);
	else
		return 0;
}

static u32 nvhost_sync_timeline_current(struct nvhost_sync_timeline *obj)
{
	if (obj->id != NVSYNCPT_INVALID)
		return nvhost_syncpt_read_min(obj->sp, obj->id);
	else
		return 0;
}

static void nvhost_sync_timeline_value_str(struct sync_timeline *timeline,
		char *str, int size)
{
	struct nvhost_sync_timeline *obj =
		(struct nvhost_sync_timeline *)timeline;
	snprintf(str, size, "%d", nvhost_sync_timeline_current(obj));
}

static void nvhost_sync_pt_value_str(struct sync_pt *sync_pt, char *str,
		int size)
{
	struct nvhost_sync_pt *pt = to_nvhost_sync_pt(sync_pt);
	struct nvhost_sync_timeline *obj = pt->obj;

	if (obj->id != NVSYNCPT_INVALID)
		snprintf(str, size, "%d", pt->thresh);
	else
		snprintf(str, size, "0");
}

static int nvhost_sync_fill_driver_data(struct sync_pt *sync_pt,
		void *data, int size)
{
	struct nvhost_sync_pt *pt = to_nvhost_sync_pt(sync_pt);
	struct nvhost_ctrl_sync_fence_info info;

	if (size < sizeof(info))
		return -ENOMEM;

	info.id = pt->obj->id;
	info.thresh = pt->thresh;
	memcpy(data, &info, sizeof(info));

	return sizeof(info);
}

static const struct sync_timeline_ops nvhost_sync_timeline_ops = {
	.driver_name = "nvhost_sync",
	.dup = nvhost_sync_pt_dup_inst,
	.has_signaled = nvhost_sync_pt_has_signaled,
	.compare = nvhost_sync_pt_compare,
	.free_pt = nvhost_sync_pt_free_inst,
	.fill_driver_data = nvhost_sync_fill_driver_data,
	.timeline_value_str = nvhost_sync_timeline_value_str,
	.pt_value_str = nvhost_sync_pt_value_str,
};

struct sync_fence *nvhost_sync_fdget(int fd)
{
	struct sync_fence *fence = sync_fence_fdget(fd);
	struct list_head *pos;

	if (!fence)
		return fence;

	list_for_each(pos, &fence->pt_list_head) {
		struct sync_pt *pt =
			container_of(pos, struct sync_pt, pt_list);
		struct sync_timeline *timeline = pt->parent;

		if (timeline->ops != &nvhost_sync_timeline_ops) {
			sync_fence_put(fence);
			return ERR_PTR(-EINVAL);
		}
	}

	return fence;
}

int nvhost_sync_num_pts(struct sync_fence *fence)
{
	int num = 0;
	struct list_head *pos;

	list_for_each(pos, &fence->pt_list_head) {
		struct sync_pt *_pt =
			container_of(pos, struct sync_pt, pt_list);
		struct nvhost_sync_pt *pt = to_nvhost_sync_pt(_pt);
		struct nvhost_sync_timeline *obj = pt->obj;

		u32 id = nvhost_sync_pt_id(pt);
		u32 thresh = nvhost_sync_pt_thresh(pt);

		if (!nvhost_syncpt_is_expired(obj->sp, id, thresh))
			num++;
	}

	return num;
}

u32 nvhost_sync_pt_id(struct nvhost_sync_pt *pt)
{
	return pt->obj->id;
}

u32 nvhost_sync_pt_thresh(struct nvhost_sync_pt *pt)
{
	return pt->thresh;
}

/* Public API */

struct nvhost_sync_timeline *nvhost_sync_timeline_create(
		struct nvhost_syncpt *sp, int id)
{
	struct nvhost_sync_timeline *obj;
	char name[30];
	const char *syncpt_name = NULL;

	if (id != NVSYNCPT_INVALID)
		syncpt_name = syncpt_op().name(sp, id);

	if (syncpt_name && strlen(syncpt_name))
		snprintf(name, sizeof(name), "%d_%s", id, syncpt_name);
	else
		snprintf(name, sizeof(name), "%d", id);

	obj = (struct nvhost_sync_timeline *)
		sync_timeline_create(&nvhost_sync_timeline_ops,
				     sizeof(struct nvhost_sync_timeline),
				     name);
	if (!obj)
		return NULL;

	obj->sp = sp;
	obj->id = id;

	return obj;
}

void nvhost_sync_pt_signal(struct nvhost_sync_pt *pt)
{
	/* At this point the fence (and its sync_pt's) might already be gone if
	 * the user has closed its fd's. The nvhost_sync_pt object still exists
	 * since we took a ref while scheduling the interrupt. */
	struct nvhost_sync_timeline *obj = pt->obj;
	if (pt->has_intr) {
		nvhost_module_idle(syncpt_to_dev(pt->obj->sp)->dev);
		pt->has_intr = false;
		kref_put(&pt->refcount, nvhost_sync_pt_free_shared);
	}
	sync_timeline_signal(&obj->obj);
}

int nvhost_sync_create_fence(struct nvhost_syncpt *sp,
		struct nvhost_ctrl_sync_fence_info *pts,
		u32 num_pts, const char *name, int *fence_fd)
{
	int fd;
	int err;
	u32 i;
	struct sync_fence *fence = NULL;

	for (i = 0; i < num_pts; i++) {
		struct nvhost_sync_timeline *obj;
		struct sync_pt *pt;
		struct sync_fence *f, *f2;
		u32 id = pts[i].id;
		u32 thresh = pts[i].thresh;

		BUG_ON(id >= nvhost_syncpt_nb_pts(sp) &&
				(id != NVSYNCPT_INVALID));
		obj = nvhost_syncpt_timeline(sp, id);
		pt = nvhost_sync_pt_create_inst(obj, thresh);
		if (pt == NULL) {
			err = -ENOMEM;
			goto err;
		}

		f = sync_fence_create(name, pt);
		if (f == NULL) {
			sync_pt_free(pt);
			err = -ENOMEM;
			goto err;
		}

		if (fence == NULL) {
			fence = f;
		} else {
			f2 = sync_fence_merge(name, fence, f);
			sync_fence_put(f);
			sync_fence_put(fence);
			fence = f2;
			if (!fence) {
				err = -ENOMEM;
				goto err;
			}
		}
	}

	if (fence == NULL) {
		err = -EINVAL;
		goto err;
	}

	fd = get_unused_fd();
	if (fd < 0) {
		err = fd;
		goto err;
	}

	*fence_fd = fd;
	sync_fence_install(fence, fd);
	return 0;

err:
	if (fence)
		sync_fence_put(fence);

	return err;
}
