/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Title          PowerVR Linux software "counting" timeline fence implementation
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/kref.h>

#include "pvr_counting_timeline.h"
#include "pvr_sw_fence.h"

struct pvr_counting_fence_timeline {
	char name[32];
	struct pvr_sw_fence_context *context;

	spinlock_t active_fences_lock;
	u64 current_value; /* guarded by active_fences_lock */
	struct list_head active_fences;

	struct kref kref;
};

struct pvr_counting_fence {
	u64 value;
	struct dma_fence *fence;
	struct list_head active_list_entry;
};

struct pvr_counting_fence_timeline *pvr_counting_fence_timeline_create(
	const char *name)
{
	struct pvr_counting_fence_timeline *timeline =
		kmalloc(sizeof(*timeline), GFP_KERNEL);

	if (!timeline)
		goto err_out;

	strlcpy(timeline->name, name, sizeof(timeline->name));

	timeline->context = pvr_sw_fence_context_create(timeline->name,
		"pvr_sw_sync");
	if (!timeline->context)
		goto err_free_timeline;

	timeline->current_value = 0;
	kref_init(&timeline->kref);
	spin_lock_init(&timeline->active_fences_lock);
	INIT_LIST_HEAD(&timeline->active_fences);

err_out:
	return timeline;

err_free_timeline:
	kfree(timeline);
	timeline = NULL;
	goto err_out;
}

void pvr_counting_fence_timeline_force_complete(
	struct pvr_counting_fence_timeline *timeline)
{
	struct list_head *entry, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&timeline->active_fences_lock, flags);

	list_for_each_safe(entry, tmp, &timeline->active_fences) {
		struct pvr_counting_fence *fence =
			list_entry(entry, struct pvr_counting_fence,
			active_list_entry);
		dma_fence_signal(fence->fence);
		dma_fence_put(fence->fence);
		fence->fence = NULL;
		list_del(&fence->active_list_entry);
		kfree(fence);
	}
	spin_unlock_irqrestore(&timeline->active_fences_lock, flags);
}

static void pvr_counting_fence_timeline_destroy(
	struct kref *kref)
{
	struct pvr_counting_fence_timeline *timeline =
		container_of(kref, struct pvr_counting_fence_timeline, kref);

	WARN_ON(!list_empty(&timeline->active_fences));

	pvr_sw_fence_context_destroy(timeline->context);
	kfree(timeline);
}

void pvr_counting_fence_timeline_put(
	struct pvr_counting_fence_timeline *timeline)
{
	kref_put(&timeline->kref, pvr_counting_fence_timeline_destroy);
}

struct pvr_counting_fence_timeline *pvr_counting_fence_timeline_get(
	struct pvr_counting_fence_timeline *timeline)
{
	if (!timeline)
		return NULL;
	kref_get(&timeline->kref);
	return timeline;
}

struct dma_fence *pvr_counting_fence_create(
	struct pvr_counting_fence_timeline *timeline, u64 value)
{
	unsigned long flags;
	struct dma_fence *sw_fence;
	struct pvr_counting_fence *fence = kmalloc(sizeof(*fence), GFP_KERNEL);

	if (!fence)
		return NULL;

	sw_fence = pvr_sw_fence_create(timeline->context);
	if (!sw_fence)
		goto err_free_fence;

	fence->fence = dma_fence_get(sw_fence);
	fence->value = value;

	spin_lock_irqsave(&timeline->active_fences_lock, flags);

	list_add_tail(&fence->active_list_entry, &timeline->active_fences);

	spin_unlock_irqrestore(&timeline->active_fences_lock, flags);

	return sw_fence;

err_free_fence:
	kfree(fence);
	return NULL;
}

void pvr_counting_fence_timeline_inc(
	struct pvr_counting_fence_timeline *timeline, u64 value)
{
	struct list_head *entry, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&timeline->active_fences_lock, flags);

	timeline->current_value += value;

	list_for_each_safe(entry, tmp, &timeline->active_fences) {
		struct pvr_counting_fence *fence =
			list_entry(entry, struct pvr_counting_fence,
			active_list_entry);
		if (fence->value <= timeline->current_value) {
			dma_fence_signal(fence->fence);
			dma_fence_put(fence->fence);
			fence->fence = NULL;
			list_del(&fence->active_list_entry);
			kfree(fence);
		}
	}

	spin_unlock_irqrestore(&timeline->active_fences_lock, flags);
}
