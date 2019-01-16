
#ifndef __KREE_SYS_IPC__
#define __KREE_SYS_IPC__

#include <linux/types.h>
#include "trustzone/tz_cross/trustzone.h"
#include "trustzone/tz_cross/ree_service.h"

/* Mutex
*/
TZ_RESULT KREE_ServMutexCreate (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServMutexDestroy (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServMutexLock (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServMutexUnlock (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServMutexTrylock (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServMutexIslock (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

/* Semaphore
*/
TZ_RESULT KREE_ServSemaphoreCreate (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServSemaphoreDestroy (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServSemaphoreDown (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServSemaphoreDownTimeout (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServSemaphoreDowntrylock (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServSemaphoreUp (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

#if 0
/* wait queue
*/
TZ_RESULT KREE_ServWaitqCreate (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServWaitqDestroy (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServWaitqWaitevent (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServWaitqWaiteventTimeout (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

TZ_RESULT KREE_ServWaitqWakeup (u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);
#endif
#endif /* __KREE_SYS_IPC__ */


