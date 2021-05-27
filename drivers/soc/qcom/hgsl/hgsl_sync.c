// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/types.h>
#include <linux/dma-buf.h>
#include <linux/dma-fence.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/regmap.h>
#include <linux/uaccess.h>

#include "hgsl.h"

static const struct dma_fence_ops hgsl_hsync_fence_ops;
static const struct dma_fence_ops hgsl_isync_fence_ops;


int hgsl_hsync_fence_create_fd(struct hgsl_context *context,
				uint32_t ts)
{
	int fence_fd;
	struct hgsl_hsync_fence *fence;

	fence_fd = get_unused_fd_flags(0);
	if (fence_fd < 0)
		return fence_fd;

	fence = hgsl_hsync_fence_create(context, ts);
	if (fence == NULL) {
		put_unused_fd(fence_fd);
		return -ENOMEM;
	}

	fd_install(fence_fd, fence->sync_file->file);

	return fence_fd;
}

struct hgsl_hsync_fence *hgsl_hsync_fence_create(
					struct hgsl_context *context,
					uint32_t ts)
{
	unsigned long flags;
	struct hgsl_hsync_timeline *timeline = context->timeline;
	struct hgsl_hsync_fence *fence;

	if (!kref_get_unless_zero(&timeline->kref))
		return NULL;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (fence == NULL) {
		hgsl_hsync_timeline_put(timeline);
		return NULL;
	}

	fence->timeline = timeline;
	fence->ts = ts;

	dma_fence_init(&fence->fence, &hgsl_hsync_fence_ops,
			&timeline->lock, timeline->fence_context, ts);

	fence->sync_file = sync_file_create(&fence->fence);
	dma_fence_put(&fence->fence);
	if (fence->sync_file == NULL) {
		hgsl_hsync_timeline_put(timeline);
		kfree(fence);
		return NULL;
	}

	spin_lock_irqsave(&timeline->lock, flags);
	list_add_tail(&fence->child_list, &timeline->fence_list);
	spin_unlock_irqrestore(&timeline->lock, flags);

	return fence;
}

void hgsl_hsync_timeline_signal(struct hgsl_hsync_timeline *timeline,
						unsigned int ts)
{
	struct hgsl_hsync_fence *cur, *next;
	unsigned long flags;

	if (!kref_get_unless_zero(&timeline->kref))
		return;

	if (hgsl_ts_ge(timeline->last_ts, ts))
		return;

	spin_lock_irqsave(&timeline->lock, flags);
	timeline->last_ts = ts;
	list_for_each_entry_safe(cur, next, &timeline->fence_list,
					child_list) {
		if (dma_fence_is_signaled_locked(&cur->fence))
			list_del_init(&cur->child_list);
	}
	spin_unlock_irqrestore(&timeline->lock, flags);

	hgsl_hsync_timeline_put(timeline);
}

int hgsl_hsync_timeline_create(struct hgsl_context *context)
{
	struct hgsl_hsync_timeline *timeline;

	timeline = kzalloc(sizeof(*timeline), GFP_KERNEL);
	if (!timeline)
		return -ENOMEM;

	snprintf(timeline->name, HGSL_TIMELINE_NAME_LEN,
		"timeline_%s_%d",
		current->comm, current->pid);

	kref_init(&timeline->kref);
	timeline->fence_context = dma_fence_context_alloc(1);
	INIT_LIST_HEAD(&timeline->fence_list);
	spin_lock_init(&timeline->lock);
	timeline->context = context;

	context->timeline = timeline;

	return 0;
}

static void hgsl_hsync_timeline_destroy(struct kref *kref)
{
	struct hgsl_hsync_timeline *timeline =
		container_of(kref, struct hgsl_hsync_timeline, kref);

	kfree(timeline);
}

void hgsl_hsync_timeline_put(struct hgsl_hsync_timeline *timeline)
{
	if (timeline)
		kref_put(&timeline->kref, hgsl_hsync_timeline_destroy);
}

static const char *hgsl_hsync_get_driver_name(struct dma_fence *base)
{
	return "hgsl-timeline";
}

static const char *hgsl_hsync_get_timeline_name(struct dma_fence *base)
{
	struct hgsl_hsync_fence *fence =
			container_of(base, struct hgsl_hsync_fence, fence);
	struct hgsl_hsync_timeline *timeline = fence->timeline;

	return timeline->name;
}

static bool hgsl_hsync_enable_signaling(struct dma_fence *base)
{
	return true;
}

static bool hgsl_hsync_has_signaled(struct dma_fence *base)
{
	struct hgsl_hsync_fence *fence =
			container_of(base, struct hgsl_hsync_fence, fence);
	struct hgsl_hsync_timeline *timeline = fence->timeline;

	return hgsl_ts_ge(timeline->last_ts, fence->ts);
}

static void hgsl_hsync_fence_release(struct dma_fence *base)
{
	struct hgsl_hsync_fence *fence =
			container_of(base, struct hgsl_hsync_fence, fence);
	struct hgsl_hsync_timeline *timeline = fence->timeline;

	if (timeline) {
		spin_lock(&timeline->lock);
		list_del_init(&fence->child_list);
		spin_unlock(&timeline->lock);
		hgsl_hsync_timeline_put(timeline);
	}
	kfree(fence);
}

static void hgsl_hsync_fence_value_str(struct dma_fence *base,
				      char *str, int size)
{
	struct hgsl_hsync_fence *fence =
			container_of(base, struct hgsl_hsync_fence, fence);

	snprintf(str, size, "%u", fence->ts);
}

static void hgsl_hsync_timeline_value_str(struct dma_fence *base,
				  char *str, int size)
{
	struct hgsl_hsync_fence *fence =
			container_of(base, struct hgsl_hsync_fence, fence);
	struct hgsl_hsync_timeline *timeline = fence->timeline;

	if (!kref_get_unless_zero(&timeline->kref))
		return;

	snprintf(str, size, "Last retired TS:%u", timeline->last_ts);

	hgsl_hsync_timeline_put(timeline);
}

static const struct dma_fence_ops hgsl_hsync_fence_ops = {
	.get_driver_name = hgsl_hsync_get_driver_name,
	.get_timeline_name = hgsl_hsync_get_timeline_name,
	.enable_signaling = hgsl_hsync_enable_signaling,
	.signaled = hgsl_hsync_has_signaled,
	.wait = dma_fence_default_wait,
	.release = hgsl_hsync_fence_release,

	.fence_value_str = hgsl_hsync_fence_value_str,
	.timeline_value_str = hgsl_hsync_timeline_value_str,
};

static void hgsl_isync_timeline_release(struct kref *kref)
{
	struct hgsl_isync_timeline *timeline = container_of(kref,
					struct hgsl_isync_timeline,
					kref);

	kfree(timeline);
}

static struct hgsl_isync_timeline *
hgsl_isync_timeline_get(struct hgsl_priv *priv, int id)
{
	int ret = 0;
	struct hgsl_isync_timeline *timeline = NULL;

	spin_lock(&priv->isync_timeline_lock);
	timeline = idr_find(&priv->isync_timeline_idr, id);
	spin_unlock(&priv->isync_timeline_lock);

	if (timeline)
		ret = kref_get_unless_zero(&timeline->kref);


	if (!ret)
		timeline = NULL;

	return timeline;
}

static void hgsl_isync_timeline_put(struct hgsl_isync_timeline *timeline)
{
	if (timeline)
		kref_put(&timeline->kref, hgsl_isync_timeline_release);
}

int hgsl_isync_timeline_create(struct hgsl_priv *priv,
				    uint32_t *timeline_id)
{
	struct hgsl_isync_timeline *timeline;
	int ret = -EINVAL;
	uint32_t idr;

	if (timeline_id == NULL)
		return -EINVAL;

	timeline = kzalloc(sizeof(*timeline), GFP_KERNEL);
	if (timeline == NULL)
		return -ENOMEM;

	kref_init(&timeline->kref);
	timeline->context = dma_fence_context_alloc(1);
	INIT_LIST_HEAD(&timeline->fence_list);
	spin_lock_init(&timeline->lock);
	timeline->priv = priv;
	timeline->last_ts = 0;

	idr_preload(GFP_KERNEL);
	spin_lock(&priv->isync_timeline_lock);
	idr = idr_alloc(&priv->isync_timeline_idr, timeline, 1, 0, GFP_NOWAIT);
	spin_unlock(&priv->isync_timeline_lock);
	idr_preload_end();

	if (idr > 0) {
		timeline->id = idr;
		*timeline_id = idr;
		ret = 0;
	} else
		kfree(timeline);

	return ret;
}

int hgsl_isync_fence_create(struct hgsl_priv *priv, uint32_t timeline_id,
						uint32_t ts, int *fence_fd)
{
	struct hgsl_isync_timeline *timeline = NULL;
	struct hgsl_isync_fence *fence = NULL;
	struct sync_file *sync_file = NULL;
	int ret = 0;

	if (fence_fd == NULL)
		return -EINVAL;

	timeline = hgsl_isync_timeline_get(priv, timeline_id);
	if (timeline == NULL) {
		ret = -EINVAL;
		goto out;
	}

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (fence == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	fence->timeline = timeline;

	/* set a minimal ts if user don't set it */
	if (ts == 0)
		ts = 1;

	fence->ts = ts;

	dma_fence_init(&fence->fence, &hgsl_isync_fence_ops,
						&timeline->lock,
						timeline->context,
						ts);

	sync_file = sync_file_create(&fence->fence);

	if (sync_file == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	dma_fence_put(&fence->fence);

	*fence_fd = get_unused_fd_flags(0);
	if (*fence_fd < 0) {
		ret = -EBADF;
		goto out;
	}

	fd_install(*fence_fd, sync_file->file);

	spin_lock(&timeline->lock);
	list_add_tail(&fence->child_list, &timeline->fence_list);
	spin_unlock(&timeline->lock);

out:
	if (ret) {
		if (sync_file)
			fput(sync_file->file);

		if (timeline)
			hgsl_isync_timeline_put(timeline);
	}

	return ret;
}

static int hgsl_isync_timeline_destruct(struct hgsl_priv *priv,
				struct hgsl_isync_timeline *timeline)
{
	struct hgsl_isync_fence *cur, *next;

	if (timeline == NULL)
		return -EINVAL;

	spin_lock(&timeline->lock);
	list_for_each_entry_safe(cur, next, &timeline->fence_list,
				 child_list) {
		dma_fence_signal_locked(&cur->fence);
		list_del_init(&cur->child_list);
	}
	spin_unlock(&timeline->lock);

	hgsl_isync_timeline_put(timeline);

	return 0;
}

int hgsl_isync_timeline_destroy(struct hgsl_priv *priv, uint32_t id)
{
	struct hgsl_isync_timeline *timeline;

	spin_lock(&priv->isync_timeline_lock);
	timeline = idr_find(&priv->isync_timeline_idr, id);
	spin_unlock(&priv->isync_timeline_lock);

	if (timeline == NULL)
		return 0;

	if (timeline->id > 0) {
		idr_remove(&priv->isync_timeline_idr, timeline->id);
		timeline->id = 0;
	}

	return hgsl_isync_timeline_destruct(priv, timeline);
}

void hgsl_isync_fini(struct hgsl_priv *priv)
{
	LIST_HEAD(flist);
	struct hgsl_isync_timeline *cur, *t;
	uint32_t idr;

	spin_lock(&priv->isync_timeline_lock);
	idr_for_each_entry(&priv->isync_timeline_idr,
					cur, idr) {
		idr_remove(&priv->isync_timeline_idr, idr);
		list_add(&cur->free_list, &flist);
	}
	spin_unlock(&priv->isync_timeline_lock);

	list_for_each_entry_safe(cur, t, &flist, free_list) {
		list_del(&cur->free_list);
		hgsl_isync_timeline_destruct(priv, cur);
	}

	idr_destroy(&priv->isync_timeline_idr);
}

static int _isync_timeline_signal(
				struct hgsl_isync_timeline *timeline,
				struct dma_fence *fence)
{
	int ret = -EINVAL;
	struct hgsl_isync_fence *cur, *next;

	spin_lock(&timeline->lock);
	list_for_each_entry_safe(cur, next, &timeline->fence_list,
						child_list) {
		if (fence == &cur->fence) {
			dma_fence_signal_locked(fence);
			list_del_init(&cur->child_list);
			ret = 0;
			break;
		}
	}
	spin_unlock(&timeline->lock);

	return ret;
}

int hgsl_isync_fence_signal(struct hgsl_priv *priv, uint32_t timeline_id,
							int fence_fd)
{
	struct hgsl_isync_timeline *timeline;
	struct dma_fence *fence = NULL;
	int ret = -EINVAL;

	timeline = hgsl_isync_timeline_get(priv, timeline_id);
	if (timeline == NULL) {
		ret = -EINVAL;
		goto out;
	}

	fence = sync_file_get_fence(fence_fd);
	if (fence == NULL) {
		ret = -EBADF;
		goto out;
	}

	ret = _isync_timeline_signal(timeline, fence);
out:
	if (fence)
		dma_fence_put(fence);

	if (timeline)
		hgsl_isync_timeline_put(timeline);
	return ret;
}

int hgsl_isync_forward(struct hgsl_priv *priv, uint32_t timeline_id,
							uint32_t ts)
{
	struct hgsl_isync_timeline *timeline;
	struct hgsl_isync_fence *cur, *next;

	timeline = hgsl_isync_timeline_get(priv, timeline_id);
	if (timeline == NULL)
		return -EINVAL;

	if (hgsl_ts_ge(timeline->last_ts, ts))
		goto out;

	spin_lock(&timeline->lock);
	timeline->last_ts = ts;
	list_for_each_entry_safe(cur, next, &timeline->fence_list,
				 child_list) {
		if (hgsl_ts_ge(ts, cur->ts)) {
			dma_fence_signal_locked(&cur->fence);
			list_del_init(&cur->child_list);
		}
	}
	spin_unlock(&timeline->lock);
out:
	if (timeline)
		hgsl_isync_timeline_put(timeline);
	return 0;
}

static const char *hgsl_isync_get_driver_name(struct dma_fence *base)
{
	return "hgsl-isync-timeline";
}

static const char *hgsl_isync_get_timeline_name(struct dma_fence *base)
{
	struct hgsl_isync_fence *fence =
				container_of(base,
					     struct hgsl_isync_fence,
					     fence);

	struct hgsl_isync_timeline *timeline = fence->timeline;

	return timeline->name;
}

static bool hgsl_isync_enable_signaling(struct dma_fence *base)
{
	return true;
}

static bool hgsl_isync_has_signaled(struct dma_fence *base)
{
	struct hgsl_isync_fence *fence = NULL;
	struct hgsl_isync_timeline *timeline = NULL;

	if (base) {
		fence = container_of(base, struct hgsl_isync_fence, fence);
		timeline = fence->timeline;
		if (timeline && timeline->last_ts > 0)
			return hgsl_ts_ge(timeline->last_ts, fence->ts);
	}

	return false;
}

static void hgsl_isync_fence_release(struct dma_fence *base)
{
	struct hgsl_isync_fence *fence = container_of(base,
				    struct hgsl_isync_fence,
				    fence);

	_isync_timeline_signal(fence->timeline, base);
	hgsl_isync_timeline_put(fence->timeline);

	kfree(fence);
}

static void hgsl_isync_fence_value_str(struct dma_fence *base,
				      char *str, int size)
{
	snprintf(str, size, "%llu", base->context);
}

static const struct dma_fence_ops hgsl_isync_fence_ops = {
	.get_driver_name = hgsl_isync_get_driver_name,
	.get_timeline_name = hgsl_isync_get_timeline_name,
	.enable_signaling = hgsl_isync_enable_signaling,
	.signaled = hgsl_isync_has_signaled,
	.wait = dma_fence_default_wait,
	.release = hgsl_isync_fence_release,

	.fence_value_str = hgsl_isync_fence_value_str,
};

