#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <asm/atomic.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#include <linux/sync.h>
#else
#include <../drivers/staging/android/sync.h>
#endif

#include <linux/mtk_gpu_utility.h>
#include "ged_monitor_3D_fence.h"
#include "ged_log.h"
#include "ged_base.h"

static atomic_t g_i32Count = ATOMIC_INIT(0);

#ifdef GED_DEBUG_MONITOR_3D_FENCE
extern GED_LOG_BUF_HANDLE ghLogBuf_GED;
#endif

typedef struct GED_MONITOR_3D_FENCE_TAG
{
    struct sync_fence_waiter    sSyncWaiter;
	struct work_struct          sWork;
    struct sync_fence*          psSyncFence;
} GED_MONITOR_3D_FENCE;

static void ged_sync_cb(struct sync_fence *fence, struct sync_fence_waiter *waiter)
{
	GED_MONITOR_3D_FENCE *psMonitor;
	psMonitor = GED_CONTAINER_OF(waiter, GED_MONITOR_3D_FENCE, sSyncWaiter);
    schedule_work(&psMonitor->sWork);
}

static void ged_monitor_3D_fence_work_cb(struct work_struct *psWork)
{
	GED_MONITOR_3D_FENCE *psMonitor;

#ifdef GED_DEBUG_MONITOR_3D_FENCE
    ged_log_buf_print(ghLogBuf_GED, "ged_monitor_3D_fence_work_cb");
#endif

    if (atomic_sub_return(1, &g_i32Count) < 1)
    {
#ifdef GED_DEBUG_MONITOR_3D_FENCE
        ged_log_buf_print(ghLogBuf_GED, "mtk_set_bottom_gpu_freq(0)");
#endif
        mtk_set_bottom_gpu_freq(0);
    }

	psMonitor = GED_CONTAINER_OF(psWork, GED_MONITOR_3D_FENCE, sWork);
    sync_fence_put(psMonitor->psSyncFence);
    ged_free(psMonitor, sizeof(GED_MONITOR_3D_FENCE));
}

GED_ERROR ged_monitor_3D_fence_add(int fence_fd)
{
    int err;
    GED_MONITOR_3D_FENCE* psMonitor = (GED_MONITOR_3D_FENCE*)ged_alloc(sizeof(GED_MONITOR_3D_FENCE));

#ifdef GED_DEBUG_MONITOR_3D_FENCE
    ged_log_buf_print(ghLogBuf_GED, "[+]ged_monitor_3D_fence_add");
#endif

    if (!psMonitor)
    {
        return GED_ERROR_OOM;
    }

    sync_fence_waiter_init(&psMonitor->sSyncWaiter, ged_sync_cb);
    INIT_WORK(&psMonitor->sWork, ged_monitor_3D_fence_work_cb);
    psMonitor->psSyncFence = sync_fence_fdget(fence_fd);
    if (NULL == psMonitor->psSyncFence)
    {
        ged_free(psMonitor, sizeof(GED_MONITOR_3D_FENCE));
        return GED_ERROR_INVALID_PARAMS;
    }

#ifdef GED_DEBUG_MONITOR_3D_FENCE
    ged_log_buf_print(ghLogBuf_GED, "[+]sync_fence_wait_async");
#endif

    err = sync_fence_wait_async(psMonitor->psSyncFence, &psMonitor->sSyncWaiter);

#ifdef GED_DEBUG_MONITOR_3D_FENCE
    ged_log_buf_print(ghLogBuf_GED, "[-]sync_fence_wait_async, err = %d", err);
#endif

    if ((1 == err) || (0 > err))
    {
        sync_fence_put(psMonitor->psSyncFence);
        ged_free(psMonitor, sizeof(GED_MONITOR_3D_FENCE));
    }
    else if (0 == err)
    {
        int iCount = atomic_add_return (1, &g_i32Count);
        if (iCount > 1)
        {
            //mtk_set_bottom_gpu_freq(iCount + 1);
            mtk_set_bottom_gpu_freq(4);
        }
    }

#ifdef GED_DEBUG_MONITOR_3D_FENCE
    ged_log_buf_print(ghLogBuf_GED, "[-]ged_monitor_3D_fence_add, count = %d", atomic_read(&g_i32Count));
#endif

    return GED_OK;
}

