// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-fence-array.h>
#include "mtk_vcodec_fence.h"

#define FENCE_NAME_LEN 32
#define FENCE_LOG_ERR 0
#define FENCE_LOG_INFO 1
#define FENCE_LOG_DEBUG 2

static const struct dma_fence_ops mtk_vcodec_fence_ops;

static int fence_debug;
module_param(fence_debug, int, 0644);

#define dprintk(level, fmt, args...)					\
	do {								\
		if (fence_debug >= level)				\
			pr_info("[mtk_vcodec_fence] %s(),%d: " fmt,	\
			__func__, __LINE__, ##args);			\
	} while (0)

struct mtk_vcodec_fence {
	struct dma_fence fence;
	spinlock_t lock;
	char timeline_name[FENCE_NAME_LEN];
};

static struct mtk_vcodec_fence *fence_to_mtk_fence(struct dma_fence *fence)
{
	if (fence->ops != &mtk_vcodec_fence_ops) {
		WARN_ON(1);
		return NULL;
	}
	return container_of(fence, struct mtk_vcodec_fence, fence);
}

static const char *mtk_vcodec_fence_get_driver_name(struct dma_fence *fence)
{
	return "mtk-vcodec";
}

static const char *mtk_vcodec_fence_get_timeline_name(struct dma_fence *fence)
{
	struct mtk_vcodec_fence *mtk_fence = fence_to_mtk_fence(fence);

	return mtk_fence->timeline_name;
}

static void mtk_vcodec_fence_release(struct dma_fence *f)
{
	struct mtk_vcodec_fence *mtk_fence = fence_to_mtk_fence(f);

	/* Unconditionally signal the fence. The process is getting
	 * terminated.
	 */
	if (WARN_ON(!mtk_fence))
		return; /* Not an mtk_vcodec_fence */

	dprintk(FENCE_LOG_DEBUG, "release fence %s\n", mtk_fence->timeline_name);
	kfree_rcu(f, rcu);
}

static const struct dma_fence_ops mtk_vcodec_fence_ops = {
	.get_driver_name = mtk_vcodec_fence_get_driver_name,
	.get_timeline_name = mtk_vcodec_fence_get_timeline_name,
	.release = mtk_vcodec_fence_release,
};

static bool is_mtk_vcodec_fence(struct dma_fence *fence)
{
	return fence->ops == &mtk_vcodec_fence_ops;
}

void mtk_vcodec_fence_signal(struct dma_fence *fence, int index)
{
	struct dma_fence_array *fence_array;
	struct dma_fence *slice_fence;

	if (!fence) {
		dprintk(FENCE_LOG_INFO, "fence is NULL\n");
		return;
	}

	fence_array = to_dma_fence_array(fence);
	if (!fence_array) {
		dprintk(FENCE_LOG_ERR, "fence %s is not fence array\n",
			fence->ops->get_timeline_name(fence));
		return;
	}

	if (index >= fence_array->num_fences) {
		dprintk(FENCE_LOG_ERR, "max slice done count %d already reached\n",
			fence_array->num_fences);
		return;
	}

	slice_fence = fence_array->fences[index];
	if (!is_mtk_vcodec_fence(slice_fence)) {
		dprintk(FENCE_LOG_ERR, "fence %s at index %d is not a mtk_vcodec fence\n",
			slice_fence->ops->get_timeline_name(slice_fence), index);
		return;
	}

	dprintk(FENCE_LOG_DEBUG, "signal index %d fence %s\n",
		index, slice_fence->ops->get_timeline_name(slice_fence));
	dma_fence_signal(slice_fence);
}
EXPORT_SYMBOL(mtk_vcodec_fence_signal);

struct dma_fence *mtk_vcodec_create_fence(int fence_count)
{
	struct dma_fence_array *fence_array = NULL;
	struct dma_fence **fences = NULL;
	u64 context;
	int i, len;
	int created_fence_count = 0;

	// no need to use fence
	if (fence_count <= 0)
		goto CREATE_FENCE_FAIL;

	fences = kcalloc(fence_count, sizeof(*fences), GFP_KERNEL);
	if (fences == NULL) {
		dprintk(FENCE_LOG_ERR, "alloc %d fences fail\n", fence_count);
		goto CREATE_FENCE_FAIL;
	}
	context = dma_fence_context_alloc(1);
	for (i = 0; i < fence_count; i++) {
		struct mtk_vcodec_fence *mtk_fence;

		mtk_fence = kzalloc(sizeof(*mtk_fence), GFP_KERNEL);
		if (mtk_fence == NULL) {
			dprintk(FENCE_LOG_ERR, "alloc fence index %d fail\n", i);
			goto CREATE_FENCE_FAIL;
		}
		spin_lock_init(&mtk_fence->lock);
		len = snprintf(mtk_fence->timeline_name, sizeof(mtk_fence->timeline_name),
			"mtk_vcodec_fence%lld-slice%d", context, i);
		if (len < 0)
			dprintk(FENCE_LOG_ERR, "print timeline_name fails with ret %d\n", len);
		dma_fence_init(&mtk_fence->fence, &mtk_vcodec_fence_ops, &mtk_fence->lock,
			       context, 0);
		fences[i] = &mtk_fence->fence;
		created_fence_count++;
	}

	fence_array = dma_fence_array_create(fence_count, fences, context, 0, false);
	if (fence_array == NULL) {
		dprintk(FENCE_LOG_ERR, "create fences array fail\n");
		goto CREATE_FENCE_FAIL;
	}

	dprintk(FENCE_LOG_DEBUG, "create fence%lld slice count %d\n", context, fence_count);

	return &fence_array->base;

CREATE_FENCE_FAIL:

	if (fences) {
		for (i = 0; i < created_fence_count; i++)
			kfree(fences[i]);
		kfree(fences);
	}

	return NULL;
}
EXPORT_SYMBOL(mtk_vcodec_create_fence);
