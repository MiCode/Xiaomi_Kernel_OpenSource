/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sync_file.h>
#include <linux/fence.h>
#include "msm_drv.h"
#include "sde_kms.h"
#include "sde_fence.h"

#define TIMELINE_VAL_LENGTH		128

void *sde_sync_get(uint64_t fd)
{
	/* force signed compare, fdget accepts an int argument */
	return (signed int)fd >= 0 ? sync_file_get_fence(fd) : NULL;
}

void sde_sync_put(void *fence)
{
	if (fence)
		fence_put(fence);
}

signed long sde_sync_wait(void *fnc, long timeout_ms)
{
	struct fence *fence = fnc;
	int rc;
	char timeline_str[TIMELINE_VAL_LENGTH];

	if (!fence)
		return -EINVAL;
	else if (fence_is_signaled(fence))
		return timeout_ms ? msecs_to_jiffies(timeout_ms) : 1;

	rc = fence_wait_timeout(fence, true,
				msecs_to_jiffies(timeout_ms));
	if (!rc || (rc == -EINVAL)) {
		if (fence->ops->timeline_value_str)
			fence->ops->timeline_value_str(fence,
					timeline_str, TIMELINE_VAL_LENGTH);

		SDE_ERROR(
			"fence driver name:%s timeline name:%s seqno:0x%x timeline:%s signaled:0x%x\n",
			fence->ops->get_driver_name(fence),
			fence->ops->get_timeline_name(fence),
			fence->seqno, timeline_str,
			fence->ops->signaled ?
				fence->ops->signaled(fence) : 0xffffffff);
	}

	return rc;
}

uint32_t sde_sync_get_name_prefix(void *fence)
{
	const char *name;
	uint32_t i, prefix;
	struct fence *f = fence;

	if (!fence)
		return 0;

	name = f->ops->get_driver_name(f);
	if (!name)
		return 0;

	prefix = 0x0;
	for (i = 0; i < sizeof(uint32_t) && name[i]; ++i)
		prefix = (prefix << CHAR_BIT) | name[i];

	return prefix;
}

/**
 * struct sde_fence - release/retire fence structure
 * @fence: base fence structure
 * @name: name of each fence- it is fence timeline + commit_count
 * @fence_list: list to associated this fence on timeline/context
 * @fd: fd attached to this fence - debugging purpose.
 */
struct sde_fence {
	struct fence base;
	struct sde_fence_context *ctx;
	char name[SDE_FENCE_NAME_SIZE];
	struct list_head	fence_list;
	int fd;
};

static void sde_fence_destroy(struct kref *kref)
{
}

static inline struct sde_fence *to_sde_fence(struct fence *fence)
{
	return container_of(fence, struct sde_fence, base);
}

static const char *sde_fence_get_driver_name(struct fence *fence)
{
	struct sde_fence *f = to_sde_fence(fence);

	return f->name;
}

static const char *sde_fence_get_timeline_name(struct fence *fence)
{
	struct sde_fence *f = to_sde_fence(fence);

	return f->ctx->name;
}

static bool sde_fence_enable_signaling(struct fence *fence)
{
	return true;
}

static bool sde_fence_signaled(struct fence *fence)
{
	struct sde_fence *f = to_sde_fence(fence);
	bool status;

	status = (int)(fence->seqno - f->ctx->done_count) <= 0 ? true : false;
	SDE_DEBUG("status:%d fence seq:%d and timeline:%d\n",
			status, fence->seqno, f->ctx->done_count);
	return status;
}

static void sde_fence_release(struct fence *fence)
{
	struct sde_fence *f = to_sde_fence(fence);
	struct sde_fence *fc, *next;
	struct sde_fence_context *ctx = f->ctx;
	unsigned long flags;
	bool release_kref = false;

	spin_lock_irqsave(&ctx->lock, flags);
	list_for_each_entry_safe(fc, next, &ctx->fence_list_head,
				 fence_list) {
		/* fence release called before signal */
		if (f == fc) {
			list_del_init(&fc->fence_list);
			release_kref = true;
			break;
		}
	}
	spin_unlock_irqrestore(&ctx->lock, flags);

	/* keep kput outside spin_lock because it may release ctx */
	if (release_kref)
		kref_put(&ctx->kref, sde_fence_destroy);
	kfree_rcu(f, base.rcu);
}

static void sde_fence_value_str(struct fence *fence,
				    char *str, int size)
{
	snprintf(str, size, "%d", fence->seqno);
}

static void sde_fence_timeline_value_str(struct fence *fence,
					     char *str, int size)
{
	struct sde_fence *f = to_sde_fence(fence);

	snprintf(str, size, "%d", f->ctx->done_count);
}

static struct fence_ops sde_fence_ops = {
	.get_driver_name = sde_fence_get_driver_name,
	.get_timeline_name = sde_fence_get_timeline_name,
	.enable_signaling = sde_fence_enable_signaling,
	.signaled = sde_fence_signaled,
	.wait = fence_default_wait,
	.release = sde_fence_release,
	.fence_value_str = sde_fence_value_str,
	.timeline_value_str = sde_fence_timeline_value_str,
};

/**
 * _sde_fence_create_fd - create fence object and return an fd for it
 * This function is NOT thread-safe.
 * @timeline: Timeline to associate with fence
 * @val: Timeline value at which to signal the fence
 * Return: File descriptor on success, or error code on error
 */
static int _sde_fence_create_fd(void *fence_ctx, uint32_t val)
{
	struct sde_fence *sde_fence;
	struct sync_file *sync_file;
	signed int fd = -EINVAL;
	struct sde_fence_context *ctx = fence_ctx;
	unsigned long flags;

	if (!ctx) {
		SDE_ERROR("invalid context\n");
		goto exit;
	}

	sde_fence = kzalloc(sizeof(*sde_fence), GFP_KERNEL);
	if (!sde_fence)
		return -ENOMEM;

	snprintf(sde_fence->name, SDE_FENCE_NAME_SIZE, "fence%u", val);

	fence_init(&sde_fence->base, &sde_fence_ops, &ctx->lock,
		ctx->context, val);

	/* create fd */
	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		fence_put(&sde_fence->base);
		SDE_ERROR("failed to get_unused_fd_flags(), %s\n",
							sde_fence->name);
		goto exit;
	}

	/* create fence */
	sync_file = sync_file_create(&sde_fence->base);
	if (sync_file == NULL) {
		put_unused_fd(fd);
		fence_put(&sde_fence->base);
		SDE_ERROR("couldn't create fence, %s\n", sde_fence->name);
		goto exit;
	}

	fd_install(fd, sync_file->file);

	spin_lock_irqsave(&ctx->lock, flags);
	sde_fence->ctx = fence_ctx;
	sde_fence->fd = fd;
	list_add_tail(&sde_fence->fence_list, &ctx->fence_list_head);
	kref_get(&ctx->kref);
	spin_unlock_irqrestore(&ctx->lock, flags);
exit:
	return fd;
}

int sde_fence_init(struct sde_fence_context *ctx,
		const char *name,
		uint32_t drm_id)
{
	if (!ctx) {
		SDE_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}
	memset(ctx, 0, sizeof(*ctx));

	strlcpy(ctx->name, name, SDE_FENCE_NAME_SIZE);
	ctx->drm_id = drm_id;
	kref_init(&ctx->kref);
	ctx->context = fence_context_alloc(1);

	spin_lock_init(&ctx->lock);
	INIT_LIST_HEAD(&ctx->fence_list_head);

	return 0;
}

void sde_fence_deinit(struct sde_fence_context *ctx)
{
	if (!ctx) {
		SDE_ERROR("invalid fence\n");
		return;
	}

	kref_put(&ctx->kref, sde_fence_destroy);
}

void sde_fence_prepare(struct sde_fence_context *ctx)
{
	unsigned long flags;

	if (!ctx) {
		SDE_ERROR("invalid argument(s), fence %pK\n", ctx);
	} else {
		spin_lock_irqsave(&ctx->lock, flags);
		++ctx->commit_count;
		spin_unlock_irqrestore(&ctx->lock, flags);
	}
}

int sde_fence_create(struct sde_fence_context *ctx, uint64_t *val,
							uint32_t offset)
{
	uint32_t trigger_value;
	int fd, rc = -EINVAL;
	unsigned long flags;

	if (!ctx || !val) {
		SDE_ERROR("invalid argument(s), fence %d, pval %d\n",
				ctx != NULL, val != NULL);
		return rc;
	}

	/*
	 * Allow created fences to have a constant offset with respect
	 * to the timeline. This allows us to delay the fence signalling
	 * w.r.t. the commit completion (e.g., an offset of +1 would
	 * cause fences returned during a particular commit to signal
	 * after an additional delay of one commit, rather than at the
	 * end of the current one.
	 */
	spin_lock_irqsave(&ctx->lock, flags);
	trigger_value = ctx->commit_count + offset;

	spin_unlock_irqrestore(&ctx->lock, flags);

	fd = _sde_fence_create_fd(ctx, trigger_value);
	*val = fd;
	SDE_DEBUG("fence_create::fd:%d trigger:%d commit:%d offset:%d\n",
				fd, trigger_value, ctx->commit_count, offset);

	SDE_EVT32(ctx->drm_id, trigger_value, fd);

	if (fd >= 0)
		rc = 0;
	else
		rc = fd;

	return rc;
}

void sde_fence_signal(struct sde_fence_context *ctx, bool is_error)
{
	unsigned long flags;
	struct sde_fence *fc, *next;
	uint32_t count = 0;

	if (!ctx) {
		SDE_ERROR("invalid ctx, %pK\n", ctx);
		return;
	} else if (is_error) {
		return;
	}

	spin_lock_irqsave(&ctx->lock, flags);
	if ((int)(ctx->done_count - ctx->commit_count) < 0) {
		++ctx->done_count;
	} else {
		SDE_ERROR("extra signal attempt! done count:%d commit:%d\n",
					ctx->done_count, ctx->commit_count);
		goto end;
	}

	if (list_empty(&ctx->fence_list_head)) {
		SDE_DEBUG("nothing to trigger!-no get_prop call\n");
		goto end;
	}

	SDE_DEBUG("fence_signal:done count:%d commit count:%d\n",
					ctx->commit_count, ctx->done_count);

	list_for_each_entry_safe(fc, next, &ctx->fence_list_head,
				 fence_list) {
		if (fence_is_signaled_locked(&fc->base)) {
			list_del_init(&fc->fence_list);
			count++;
		}
	}

	SDE_EVT32(ctx->drm_id, ctx->done_count);

end:
	spin_unlock_irqrestore(&ctx->lock, flags);

	/* keep this outside spin_lock because same ctx may be released */
	for (; count > 0; count--)
		kref_put(&ctx->kref, sde_fence_destroy);
}
