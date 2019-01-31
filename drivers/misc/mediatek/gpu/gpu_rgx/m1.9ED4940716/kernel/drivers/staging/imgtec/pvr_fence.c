/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Title          PowerVR Linux fence interface
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
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "pvr_fence.h"
#include "services_kernel_client.h"

#include "kernel_compatibility.h"

#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
#define	PVR_FENCE_CONTEXT_DESTROY_INITAL_WAIT_MS	100
#define	PVR_FENCE_CONTEXT_DESTROY_RETRIES		5
#endif

#if defined(MTK_DEBUG_PROC_PRINT)
/* MTK: sync log */
#include "mtk_pp.h"
#endif

#define PVR_DUMPDEBUG_LOG(pfnDumpDebugPrintf, pvDumpDebugFile, fmt, ...) \
	do {                                                             \
		if (pfnDumpDebugPrintf)                                  \
			pfnDumpDebugPrintf(pvDumpDebugFile, fmt,         \
					   ## __VA_ARGS__);              \
		else                                                     \
			pr_err(fmt "\n", ## __VA_ARGS__);                \
	} while (0)

static inline void
pvr_fence_sync_init(struct pvr_fence *pvr_fence)
{
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	SyncPrimSet(pvr_fence->sync, PVR_FENCE_SYNC_VAL_INIT);
#endif
}

static inline void
pvr_fence_sync_deinit(struct pvr_fence *pvr_fence)
{
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	SyncPrimSet(pvr_fence->sync, PVR_FENCE_SYNC_VAL_DONE);
#endif
}

static inline void
pvr_fence_sync_signal(struct pvr_fence *pvr_fence)
{
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	SyncPrimSet(pvr_fence->sync, PVR_FENCE_SYNC_VAL_SIGNALED);
#else
	SyncCheckpointSignal(pvr_fence->sync_checkpoint);
#endif
}

static inline bool
pvr_fence_sync_is_signaled(struct pvr_fence *pvr_fence)
{
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	return !!(*pvr_fence->sync->pui32LinAddr ==
		  PVR_FENCE_SYNC_VAL_SIGNALED);
#else
	return SyncCheckpointIsSignalled(pvr_fence->sync_checkpoint);
#endif
}

static inline u32
pvr_fence_sync_value(struct pvr_fence *pvr_fence)
{
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	return *pvr_fence->sync->pui32LinAddr;
#else
	if (SyncCheckpointIsErrored(pvr_fence->sync_checkpoint))
		return PVRSRV_SYNC_CHECKPOINT_ERRORED;
	else if (SyncCheckpointIsSignalled(pvr_fence->sync_checkpoint))
		return PVRSRV_SYNC_CHECKPOINT_SIGNALLED;
	else
		return PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED;
#endif
}

static void
pvr_fence_context_check_status(struct work_struct *data)
{
	PVRSRVCheckStatus(NULL);
}

static void
pvr_context_value_str(struct pvr_fence_context *fctx, char *str, int size)
{
	snprintf(str, size,
		 "%u ctx=%llu",
		 atomic_read(&fctx->fence_seqno),
		 fctx->fence_context);
}

static void
pvr_fence_context_fences_dump(struct pvr_fence_context *fctx,
			      DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
			      void *pvDumpDebugFile)
{
	struct pvr_fence *pvr_fence;
	unsigned long flags;
	char value[128];

	spin_lock_irqsave(&fctx->list_lock, flags);
	pvr_context_value_str(fctx, value, sizeof(value));
#if defined(MTK_DEBUG_PROC_PRINT)
	MTKPP_LOG(MTKPP_ID_SYNC, "hw: %s @%s", fctx->name, value);
#else
	PVR_DUMPDEBUG_LOG(pfnDumpDebugPrintf, pvDumpDebugFile,
			 "hw: %s @%s", fctx->name, value);
#endif
	list_for_each_entry(pvr_fence, &fctx->fence_list, fence_head) {
		if (is_pvr_fence(pvr_fence->fence)) {
			pvr_fence->fence->ops->fence_value_str(pvr_fence->fence,
				value, sizeof(value));
#if defined(MTK_DEBUG_PROC_PRINT)
			MTKPP_LOG(MTKPP_ID_SYNC, " @%s", value);
#else
			PVR_DUMPDEBUG_LOG(pfnDumpDebugPrintf, pvDumpDebugFile,
				" @%s", value);
#endif
		} else {
			/* Try to query as much info possible from foreign
			 * fences
			 */
			if (pvr_fence->fence->ops->get_timeline_name &&
			    pvr_fence->fence->ops->fence_value_str) {
				pvr_fence->fence->ops->fence_value_str(
					pvr_fence->fence,
					value, sizeof(value));
#if defined(MTK_DEBUG_PROC_PRINT)
				MTKPP_LOG(MTKPP_ID_SYNC, " %s@%s (foreign)",
					pvr_fence->fence->ops->
						get_timeline_name(
							pvr_fence->fence),
					value);
#else
				PVR_DUMPDEBUG_LOG(pfnDumpDebugPrintf,
					pvDumpDebugFile,
					" %s@%s (foreign)",
					pvr_fence->fence->ops->
						get_timeline_name(
							pvr_fence->fence),
					value);
#endif
			} else if (pvr_fence->fence->ops->fence_value_str) {
				pvr_fence->fence->ops->fence_value_str(
					pvr_fence->fence,
					value, sizeof(value));
#if defined(MTK_DEBUG_PROC_PRINT)
				MTKPP_LOG(MTKPP_ID_SYNC, " @%s (foreign)",
					value);
#else
				PVR_DUMPDEBUG_LOG(pfnDumpDebugPrintf,
					pvDumpDebugFile,
					" @%s (foreign)",
					value);
#endif
			}
		}
	}
	spin_unlock_irqrestore(&fctx->list_lock, flags);
}

static void
pvr_fence_context_queue_signal_work(void *data)
{
	struct pvr_fence_context *fctx = (struct pvr_fence_context *)data;

	queue_work(fctx->fence_wq, &fctx->signal_work);
}

static inline unsigned int
pvr_fence_context_seqno_next(struct pvr_fence_context *fctx)
{
	return atomic_inc_return(&fctx->fence_seqno) - 1;
}

static inline void
pvr_fence_context_free_deferred(struct pvr_fence_context *fctx)
{
	struct pvr_fence *pvr_fence, *tmp;
	LIST_HEAD(deferred_free_list);
	unsigned long flags;

	spin_lock_irqsave(&fctx->list_lock, flags);
	list_for_each_entry_safe(pvr_fence, tmp,
				 &fctx->deferred_free_list,
				 fence_head)
		list_move(&pvr_fence->fence_head, &deferred_free_list);
	spin_unlock_irqrestore(&fctx->list_lock, flags);

	list_for_each_entry_safe(pvr_fence, tmp,
				 &deferred_free_list,
				 fence_head) {
		list_del(&pvr_fence->fence_head);
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
		SyncPrimFree(pvr_fence->sync);
#else
		SyncCheckpointFree(pvr_fence->sync_checkpoint);
#endif
		dma_fence_free(&pvr_fence->base);
	}
}

static void
pvr_fence_context_signal_fences(struct work_struct *data)
{
	struct pvr_fence_context *fctx =
		container_of(data, struct pvr_fence_context, signal_work);
	struct pvr_fence *pvr_fence, *tmp;
	unsigned long flags;
	LIST_HEAD(signal_list);

	/*
	 * We can't call fence_signal while holding the lock as we can end up
	 * in a situation whereby pvr_fence_foreign_signal_sync, which also
	 * takes the list lock, ends up being called as a result of the
	 * fence_signal below, i.e. fence_signal(fence) -> fence->callback()
	 *  -> fence_signal(foreign_fence) -> foreign_fence->callback() where
	 * the foreign_fence callback is pvr_fence_foreign_signal_sync.
	 *
	 * So extract the items we intend to signal and add them to their own
	 * queue.
	 */
	spin_lock_irqsave(&fctx->list_lock, flags);
	list_for_each_entry_safe(pvr_fence, tmp, &fctx->signal_list,
				 signal_head) {
		if (pvr_fence_sync_is_signaled(pvr_fence))
			list_move(&pvr_fence->signal_head, &signal_list);
	}
	spin_unlock_irqrestore(&fctx->list_lock, flags);

	list_for_each_entry_safe(pvr_fence, tmp, &signal_list, signal_head) {

		PVR_FENCE_TRACE(&pvr_fence->base, "signalled fence (%s)\n",
				pvr_fence->name);
		list_del(&pvr_fence->signal_head);
		dma_fence_signal(pvr_fence->fence);
		dma_fence_put(pvr_fence->fence);
	}

	/*
	 * Take this opportunity to free up any fence objects we
	 * have deferred freeing.
	 */
	pvr_fence_context_free_deferred(fctx);
}

static void
pvr_fence_context_destroy_work(struct work_struct *data)
{
	struct delayed_work *dwork =
		container_of(data, struct delayed_work, work);
	struct pvr_fence_context *fctx =
		container_of(dwork, struct pvr_fence_context, destroy_work);
#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	PVRSRV_ERROR srv_err;
#endif

	PVR_FENCE_CTX_TRACE(fctx, "destroyed fence context (%s)\n", fctx->name);

	pvr_fence_context_free_deferred(fctx);

#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	/*
	 * Ensure any outstanding calls to SyncCheckpointFree have completed
	 * on other workqueues before calling SyncCheckpointContextDestroy,
	 * to avoid an unnecessary retry.
	 */
	flush_workqueue(fctx->fence_wq);

	srv_err = SyncCheckpointContextDestroy(fctx->sync_checkpoint_context);
	if (srv_err != PVRSRV_OK) {
		if (fctx->module_got && fctx->destroy_retries_left) {
			unsigned long destroy_delay_jiffies =
				msecs_to_jiffies(fctx->destroy_delay_ms);

			pr_debug("%s: SyncCheckpointContextDestroy of %p failed, retrying in %ums\n",
				 __func__, fctx->sync_checkpoint_context,
				 fctx->destroy_delay_ms);

			fctx->destroy_retries_left--;
			fctx->destroy_delay_ms *= 2;

			schedule_delayed_work(&fctx->destroy_work,
						destroy_delay_jiffies);
			return;
		}
	}
#endif

	if (WARN_ON(!list_empty_careful(&fctx->fence_list)))
		pvr_fence_context_fences_dump(fctx, NULL, NULL);

	PVRSRVUnregisterDbgRequestNotify(fctx->dbg_request_handle);
	PVRSRVUnregisterCmdCompleteNotify(fctx->cmd_complete_handle);

	destroy_workqueue(fctx->fence_wq);

#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	SyncPrimContextDestroy(fctx->sync_prim_context);
#else
	if (fctx->module_got) {
		if (srv_err == PVRSRV_OK) {
			unsigned int retries =
				PVR_FENCE_CONTEXT_DESTROY_RETRIES -
				fctx->destroy_retries_left;

			if (retries)
				pr_debug("%s: SyncCheckpointContextDestroy of %p successful, after %u %s\n",
					 __func__,
					 fctx->sync_checkpoint_context,
					retries,
					(retries == 1) ? "retry" : "retries");

			module_put(THIS_MODULE);
		} else {
			pr_err("%s: SyncCheckpointContextDestroy of %p failed, module unloadable\n",
			       __func__, fctx->sync_checkpoint_context);
		}
	} else {
		if (srv_err != PVRSRV_OK)
			pr_err("%s: SyncCheckpointContextDestroy of %p failed, context may be leaked\n",
			       __func__, fctx->sync_checkpoint_context);
	}
#endif

	kfree(fctx);
}

static void
pvr_fence_context_debug_request(void *data, u32 verbosity,
				DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
				void *pvDumpDebugFile)
{
	struct pvr_fence_context *fctx = (struct pvr_fence_context *)data;

	if (verbosity == DEBUG_REQUEST_VERBOSITY_MEDIUM)
		pvr_fence_context_fences_dump(fctx, pfnDumpDebugPrintf,
					      pvDumpDebugFile);
}

/**
 * pvr_fence_context_create - creates a PVR fence context
 * @dev_cookie: services device cookie
 * @name: context name (used for debugging)
 *
 * Creates a PVR fence context that can be used to create PVR fences or to
 * create PVR fences from an existing fence.
 *
 * pvr_fence_context_destroy should be called to clean up the fence context.
 *
 * Returns NULL if a context cannot be created.
 */
struct pvr_fence_context *
pvr_fence_context_create(void *dev_cookie,
			 const char *name)
{
	struct pvr_fence_context *fctx;
	PVRSRV_ERROR srv_err;

	fctx = kzalloc(sizeof(*fctx), GFP_KERNEL);
	if (!fctx)
		return NULL;

	spin_lock_init(&fctx->lock);
	atomic_set(&fctx->fence_seqno, 0);
	INIT_WORK(&fctx->check_status_work, pvr_fence_context_check_status);
	INIT_WORK(&fctx->signal_work, pvr_fence_context_signal_fences);
	INIT_DELAYED_WORK(&fctx->destroy_work, pvr_fence_context_destroy_work);
	spin_lock_init(&fctx->list_lock);
	INIT_LIST_HEAD(&fctx->signal_list);
	INIT_LIST_HEAD(&fctx->fence_list);
	INIT_LIST_HEAD(&fctx->deferred_free_list);

#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	fctx->destroy_retries_left = PVR_FENCE_CONTEXT_DESTROY_RETRIES;
	fctx->destroy_delay_ms = PVR_FENCE_CONTEXT_DESTROY_INITAL_WAIT_MS;
#endif

	fctx->fence_context = dma_fence_context_alloc(1);
	strncpy(fctx->name, name, sizeof(fctx->name));
	fctx->name[sizeof(fctx->name) - 1] = '\0';

	fctx->fence_wq =
		create_freezable_workqueue("pvr_fence_sync_workqueue");
	if (!fctx->fence_wq) {
		pr_err("%s: failed to create fence workqueue\n", __func__);
		goto err_free_fctx;
	}

#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	srv_err = SyncPrimContextCreate(dev_cookie, &fctx->sync_prim_context);
	if (srv_err != PVRSRV_OK) {
		pr_err("%s: failed to create sync prim context (%s)\n",
		       __func__, PVRSRVGetErrorStringKM(srv_err));
		goto err_destroy_workqueue;
	}
#else
	srv_err = SyncCheckpointContextCreate(dev_cookie,
				&fctx->sync_checkpoint_context);
	if (srv_err != PVRSRV_OK) {
		pr_err("%s: failed to create sync checkpoint context (%s)\n",
		       __func__, PVRSRVGetErrorStringKM(srv_err));
		goto err_destroy_workqueue;
	}
#endif

	srv_err = PVRSRVRegisterCmdCompleteNotify(&fctx->cmd_complete_handle,
				pvr_fence_context_queue_signal_work,
				fctx);
	if (srv_err != PVRSRV_OK) {
		pr_err("%s: failed to register command complete callback (%s)\n",
		       __func__, PVRSRVGetErrorStringKM(srv_err));
		goto err_sync_prim_context_destroy;
	}

	srv_err = PVRSRVRegisterDbgRequestNotify(&fctx->dbg_request_handle,
				dev_cookie,
				pvr_fence_context_debug_request,
				DEBUG_REQUEST_LINUXFENCE,
				fctx);
	if (srv_err != PVRSRV_OK) {
		pr_err("%s: failed to register debug request callback (%s)\n",
		       __func__, PVRSRVGetErrorStringKM(srv_err));
		goto err_unregister_cmd_complete_notify;
	}

	kref_init(&fctx->kref);

	PVR_FENCE_CTX_TRACE(fctx, "created fence context (%s)\n", name);

	return fctx;

err_unregister_cmd_complete_notify:
	PVRSRVUnregisterCmdCompleteNotify(fctx->cmd_complete_handle);
err_sync_prim_context_destroy:
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	SyncPrimContextDestroy(fctx->sync_prim_context);
#else
	SyncCheckpointContextDestroy(fctx->sync_checkpoint_context);
#endif
err_destroy_workqueue:
	destroy_workqueue(fctx->fence_wq);
err_free_fctx:
	kfree(fctx);
	return NULL;
}

static void pvr_fence_context_destroy_kref(struct kref *kref)
{
	struct pvr_fence_context *fctx =
		container_of(kref, struct pvr_fence_context, kref);

	PVR_FENCE_CTX_TRACE(fctx,
			    "scheduling destruction of fence context (%s)\n",
			    fctx->name);

	schedule_delayed_work(&fctx->destroy_work, 0);
}

/**
 * pvr_fence_context_destroy - destroys a context
 * @fctx: PVR fence context to destroy
 *
 * Destroys a PVR fence context with the expectation that all fences have been
 * destroyed.
 */
void
pvr_fence_context_destroy(struct pvr_fence_context *fctx)
{
#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	fctx->module_got = try_module_get(THIS_MODULE);
#endif

	kref_put(&fctx->kref, pvr_fence_context_destroy_kref);
}

static const char *
pvr_fence_get_driver_name(struct dma_fence *fence)
{
	return PVR_LDM_DRIVER_REGISTRATION_NAME;
}

static const char *
pvr_fence_get_timeline_name(struct dma_fence *fence)
{
	struct pvr_fence *pvr_fence = to_pvr_fence(fence);

	if (pvr_fence)
		return pvr_fence->fctx->name;
	return NULL;
}

static
void pvr_fence_fence_value_str(struct dma_fence *fence, char *str, int size)
{
	struct pvr_fence *pvr_fence = to_pvr_fence(fence);

	if (pvr_fence) {
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
		u32 sync_addr;
		(void)SyncPrimGetFirmwareAddr(pvr_fence->sync, &sync_addr);
		snprintf(str, size,
			 "%u: (%s%s) refs=%u, fwaddr=%#08x, cur=%#08x, nxt=%#08x, %s %s",
			 pvr_fence->fence->seqno,
			 test_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
				  &pvr_fence->fence->flags) ? "+" : "-",
			 test_bit(DMA_FENCE_FLAG_SIGNALED_BIT,
				  &pvr_fence->fence->flags) ? "+" : "-",
			 refcount_read(&pvr_fence->fence->refcount.refcount),
			 sync_addr,
			 pvr_fence_sync_value(pvr_fence),
			 PVR_FENCE_SYNC_VAL_SIGNALED,
			 pvr_fence->name,
			 (&pvr_fence->base != pvr_fence->fence) ?
				 "(foreign)" : "");
#else
		snprintf(str, size,
			 "%u: (%s%s) refs=%u, fwaddr=%#08x, cur=%#08x, nxt=%#08x, %s %s",
			 pvr_fence->fence->seqno,
			 test_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
				  &pvr_fence->fence->flags) ? "+" : "-",
			 test_bit(DMA_FENCE_FLAG_SIGNALED_BIT,
				  &pvr_fence->fence->flags) ? "+" : "-",
			 refcount_read(&pvr_fence->fence->refcount.refcount),
			 SyncCheckpointGetFirmwareAddr(
				pvr_fence->sync_checkpoint),
			 pvr_fence_sync_value(pvr_fence),
			 PVRSRV_SYNC_CHECKPOINT_SIGNALLED,
			 pvr_fence->name,
			 (&pvr_fence->base != pvr_fence->fence) ?
				 "(foreign)" : "");
#endif
	}
}

static
void pvr_fence_timeline_value_str(struct dma_fence *fence, char *str, int size)
{
	struct pvr_fence *pvr_fence = to_pvr_fence(fence);

	if (pvr_fence)
		pvr_context_value_str(pvr_fence->fctx, str, size);
}

static bool
pvr_fence_enable_signaling(struct dma_fence *fence)
{
	struct pvr_fence *pvr_fence = to_pvr_fence(fence);
	unsigned long flags;

	if (!pvr_fence)
		return false;

	WARN_ON_SMP(!spin_is_locked(&pvr_fence->fctx->lock));

	if (pvr_fence_sync_is_signaled(pvr_fence))
		return false;

	dma_fence_get(&pvr_fence->base);

	spin_lock_irqsave(&pvr_fence->fctx->list_lock, flags);
	list_add_tail(&pvr_fence->signal_head, &pvr_fence->fctx->signal_list);
	spin_unlock_irqrestore(&pvr_fence->fctx->list_lock, flags);

	PVR_FENCE_TRACE(&pvr_fence->base, "signalling enabled (%s)\n",
			pvr_fence->name);

	return true;
}

static bool
pvr_fence_is_signaled(struct dma_fence *fence)
{
	struct pvr_fence *pvr_fence = to_pvr_fence(fence);

	if (pvr_fence)
		return pvr_fence_sync_is_signaled(pvr_fence);
	return false;
}

static void
pvr_fence_release(struct dma_fence *fence)
{
	struct pvr_fence *pvr_fence = to_pvr_fence(fence);
	unsigned long flags;

	if (pvr_fence) {
		struct pvr_fence_context *fctx = pvr_fence->fctx;

		PVR_FENCE_TRACE(&pvr_fence->base, "released fence (%s)\n",
				pvr_fence->name);

		pvr_fence_sync_deinit(pvr_fence);

		spin_lock_irqsave(&fctx->list_lock, flags);
		list_move(&pvr_fence->fence_head,
			  &fctx->deferred_free_list);
		spin_unlock_irqrestore(&fctx->list_lock, flags);

		kref_put(&fctx->kref, pvr_fence_context_destroy_kref);
	}
}

const struct dma_fence_ops pvr_fence_ops = {
	.get_driver_name = pvr_fence_get_driver_name,
	.get_timeline_name = pvr_fence_get_timeline_name,
	.fence_value_str = pvr_fence_fence_value_str,
	.timeline_value_str = pvr_fence_timeline_value_str,
	.enable_signaling = pvr_fence_enable_signaling,
	.signaled = pvr_fence_is_signaled,
	.wait = dma_fence_default_wait,
	.release = pvr_fence_release,
};

/**
 * pvr_fence_create - creates a PVR fence
 * @fctx: PVR fence context on which the PVR fence should be created
 * @name: PVR fence name (used for debugging)
 *
 * Creates a PVR fence.
 *
 * Once the fence is finished with, pvr_fence_destroy should be called.
 *
 * Returns NULL if a PVR fence cannot be created.
 */
struct pvr_fence *
pvr_fence_create(struct pvr_fence_context *fctx, const char *name)
{
	struct pvr_fence *pvr_fence;
	unsigned int seqno;
	unsigned long flags;
	PVRSRV_ERROR srv_err;

	pvr_fence = kzalloc(sizeof(*pvr_fence), GFP_KERNEL);
	if (!pvr_fence)
		return NULL;

#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	srv_err = SyncPrimAlloc(fctx->sync_prim_context, &pvr_fence->sync,
				name);
#else
	srv_err = SyncCheckpointAlloc(fctx->sync_checkpoint_context, -1,
				      name, &pvr_fence->sync_checkpoint);
#endif
	if (srv_err != PVRSRV_OK)
		goto err_free_fence;

	pvr_fence_sync_init(pvr_fence);

	INIT_LIST_HEAD(&pvr_fence->fence_head);
	INIT_LIST_HEAD(&pvr_fence->signal_head);
	pvr_fence->fctx = fctx;
	seqno = pvr_fence_context_seqno_next(fctx);
	/* Add the seqno to the fence name for easier debugging */
	snprintf(pvr_fence->name, sizeof(pvr_fence->name), "%d-%s",
		 seqno, name);
	pvr_fence->fence = &pvr_fence->base;

	dma_fence_init(&pvr_fence->base, &pvr_fence_ops, &fctx->lock,
		       fctx->fence_context, seqno);

	spin_lock_irqsave(&fctx->list_lock, flags);
	list_add_tail(&pvr_fence->fence_head, &fctx->fence_list);
	spin_unlock_irqrestore(&fctx->list_lock, flags);

	kref_get(&fctx->kref);

	PVR_FENCE_TRACE(&pvr_fence->base, "created fence (%s)\n", name);

	return pvr_fence;

err_free_fence:
	kfree(pvr_fence);
	return NULL;
}

static const char *
pvr_fence_foreign_get_driver_name(struct dma_fence *fence)
{
	return "unknown";
}

static const char *
pvr_fence_foreign_get_timeline_name(struct dma_fence *fence)
{
	return "unknown";
}

static bool
pvr_fence_foreign_enable_signaling(struct dma_fence *fence)
{
	WARN_ON("cannot enable signalling on foreign fence");
	return false;
}

static signed long
pvr_fence_foreign_wait(struct dma_fence *fence, bool intr, signed long timeout)
{
	WARN_ON("cannot wait on foreign fence");
	return 0;
}

static void
pvr_fence_foreign_release(struct dma_fence *fence)
{
	struct pvr_fence *pvr_fence = to_pvr_fence(fence);
	unsigned long flags;

	if (pvr_fence) {
		PVR_FENCE_TRACE(&pvr_fence->base,
				"released fence for foreign fence %llu#%d (%s)\n",
				(u64) pvr_fence->fence->context,
				pvr_fence->fence->seqno, pvr_fence->name);

		pvr_fence_sync_deinit(pvr_fence);

		spin_lock_irqsave(&pvr_fence->fctx->list_lock, flags);
		list_move(&pvr_fence->fence_head,
			  &pvr_fence->fctx->deferred_free_list);
		spin_unlock_irqrestore(&pvr_fence->fctx->list_lock, flags);

		dma_fence_put(pvr_fence->fence);
		kref_put(&pvr_fence->fctx->kref,
			 pvr_fence_context_destroy_kref);
	}
}

const struct dma_fence_ops pvr_fence_foreign_ops = {
	.get_driver_name = pvr_fence_foreign_get_driver_name,
	.get_timeline_name = pvr_fence_foreign_get_timeline_name,
	.enable_signaling = pvr_fence_foreign_enable_signaling,
	.wait = pvr_fence_foreign_wait,
	.release = pvr_fence_foreign_release,
};

static void
pvr_fence_foreign_signal_sync(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct pvr_fence *pvr_fence = container_of(cb, struct pvr_fence, cb);
	struct pvr_fence_context *fctx = pvr_fence->fctx;

	WARN_ON_ONCE(is_pvr_fence(fence));

	pvr_fence_sync_signal(pvr_fence);

	queue_work(fctx->fence_wq, &fctx->check_status_work);

	PVR_FENCE_TRACE(&pvr_fence->base,
			"foreign fence %llu#%d signalled (%s)\n",
			(u64) pvr_fence->fence->context,
			pvr_fence->fence->seqno, pvr_fence->name);

	/* Drop the reference on the base fence */
	dma_fence_put(&pvr_fence->base);
}

/**
 * pvr_fence_create_from_fence - creates a PVR fence from a fence
 * @fctx: PVR fence context on which the PVR fence should be created
 * @fence: fence from which the PVR fence should be created
 * @name: PVR fence name (used for debugging)
 *
 * Creates a PVR fence from an existing fence. If the fence is a foreign fence,
 * i.e. one that doesn't originate from a PVR fence context, then a new PVR
 * fence will be created. Otherwise, a reference will be taken on the underlying
 * fence and the PVR fence will be returned.
 *
 * Once the fence is finished with, pvr_fence_destroy should be called.
 *
 * Returns NULL if a PVR fence cannot be created.
 */

struct pvr_fence *
pvr_fence_create_from_fence(struct pvr_fence_context *fctx,
			    struct dma_fence *fence,
			    const char *name)
{
	struct pvr_fence *pvr_fence = to_pvr_fence(fence);
	unsigned int seqno;
	unsigned long flags;
	PVRSRV_ERROR srv_err;
	int err;

	if (pvr_fence) {
		if (WARN_ON(fence->ops == &pvr_fence_foreign_ops))
			return NULL;
		dma_fence_get(fence);

		PVR_FENCE_TRACE(fence, "created fence from PVR fence (%s)\n",
				name);
		return pvr_fence;
	}

	pvr_fence = kzalloc(sizeof(*pvr_fence), GFP_KERNEL);
	if (!pvr_fence)
		return NULL;

#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	srv_err = SyncPrimAlloc(fctx->sync_prim_context, &pvr_fence->sync,
				name);
#else
	srv_err = SyncCheckpointAlloc(fctx->sync_checkpoint_context, -1,
				      name, &pvr_fence->sync_checkpoint);
#endif
	if (srv_err != PVRSRV_OK)
		goto err_free_pvr_fence;

	pvr_fence_sync_init(pvr_fence);

	INIT_LIST_HEAD(&pvr_fence->fence_head);
	INIT_LIST_HEAD(&pvr_fence->signal_head);
	pvr_fence->fctx = fctx;
	pvr_fence->fence = dma_fence_get(fence);
	strlcpy(pvr_fence->name, name, sizeof(pvr_fence->name));
	/*
	 * We use the base fence to refcount the PVR fence and to do the
	 * necessary clean up once the refcount drops to 0.
	 */
	seqno = pvr_fence_context_seqno_next(fctx);
	dma_fence_init(&pvr_fence->base, &pvr_fence_foreign_ops, &fctx->lock,
		       fctx->fence_context, seqno);

	/*
	 * Take an extra reference on the base fence that gets dropped when the
	 * foreign fence is signalled.
	 */
	dma_fence_get(&pvr_fence->base);

	spin_lock_irqsave(&fctx->list_lock, flags);
	list_add_tail(&pvr_fence->fence_head, &fctx->fence_list);
	spin_unlock_irqrestore(&fctx->list_lock, flags);
	kref_get(&fctx->kref);

	PVR_FENCE_TRACE(&pvr_fence->base,
			"created fence from foreign fence %llu#%d (%s)\n",
			(u64) pvr_fence->fence->context,
			pvr_fence->fence->seqno, name);

	err = dma_fence_add_callback(fence, &pvr_fence->cb,
				     pvr_fence_foreign_signal_sync);
	if (err) {
		if (err != -ENOENT)
			goto err_put_ref;

		/*
		 * The fence has already signalled so set the sync as signalled.
		 */
		pvr_fence_sync_signal(pvr_fence);
		PVR_FENCE_TRACE(&pvr_fence->base,
				"foreign fence %llu#%d already signaled (%s)\n",
				(u64) pvr_fence->fence->context,
				pvr_fence->fence->seqno,
				name);
		dma_fence_put(&pvr_fence->base);
	}


	return pvr_fence;

err_put_ref:
	kref_put(&fctx->kref, pvr_fence_context_destroy_kref);
	spin_lock_irqsave(&fctx->list_lock, flags);
	list_del(&pvr_fence->fence_head);
	spin_unlock_irqrestore(&fctx->list_lock, flags);
#if !defined(PVRSRV_USE_SYNC_CHECKPOINTS)
	SyncPrimFree(pvr_fence->sync);
#else
	SyncCheckpointFree(pvr_fence->sync_checkpoint);
#endif
err_free_pvr_fence:
	kfree(pvr_fence);
	return NULL;
}

/**
 * pvr_fence_destroy - destroys a PVR fence
 * @pvr_fence: PVR fence to destroy
 *
 * Destroys a PVR fence. Upon return, the PVR fence may still exist if something
 * else still references the underlying fence, e.g. a reservation object, or if
 * software signalling has been enabled and the fence hasn't yet been signalled.
 */
void
pvr_fence_destroy(struct pvr_fence *pvr_fence)
{
	PVR_FENCE_TRACE(&pvr_fence->base, "destroyed fence (%s)\n",
			pvr_fence->name);

	dma_fence_put(&pvr_fence->base);
}

/**
 * pvr_fence_sw_signal - signals a PVR fence sync
 * @pvr_fence: PVR fence to signal
 *
 * Sets the PVR fence sync value to signalled.
 *
 * Returns -EINVAL if the PVR fence represents a foreign fence.
 */
int
pvr_fence_sw_signal(struct pvr_fence *pvr_fence)
{
	if (!is_our_fence(pvr_fence->fctx, &pvr_fence->base))
		return -EINVAL;

	pvr_fence_sync_signal(pvr_fence);

	queue_work(pvr_fence->fctx->fence_wq,
		   &pvr_fence->fctx->check_status_work);

	PVR_FENCE_TRACE(&pvr_fence->base, "sw set fence sync signalled (%s)\n",
			pvr_fence->name);

	return 0;
}

#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
int
pvr_fence_get_checkpoints(struct pvr_fence **pvr_fences, u32 nr_fences,
			  PSYNC_CHECKPOINT *fence_checkpoints)
{
	PSYNC_CHECKPOINT *next_fence_checkpoint = fence_checkpoints;
	struct pvr_fence **next_pvr_fence = pvr_fences;
	int fence_checkpoint_idx;

	if (nr_fences > 0) {
		struct pvr_fence *next_fence = *next_pvr_fence++;

		for (fence_checkpoint_idx = 0; fence_checkpoint_idx < nr_fences;
		     fence_checkpoint_idx++) {
			*next_fence_checkpoint++ = next_fence->sync_checkpoint;
			/* Take reference on sync checkpoint (will be dropped
			 * later by kick code)
			 */
			SyncCheckpointTakeRef(next_fence->sync_checkpoint);
		}
	}

	return 0;
}

PSYNC_CHECKPOINT
pvr_fence_get_checkpoint(struct pvr_fence *update_fence)
{
	return update_fence->sync_checkpoint;
}
#endif
