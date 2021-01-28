/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef __KREE_SYS_IPC__
#define __KREE_SYS_IPC__

#include <linux/types.h>
#include "tz_cross/trustzone.h"
#include "tz_cross/ree_service.h"

/* Mutex
 */
int KREE_ServMutexCreate(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServMutexDestroy(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServMutexLock(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServMutexUnlock(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServMutexTrylock(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServMutexIslock(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

/* Semaphore
 */
int KREE_ServSemaphoreCreate(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServSemaphoreDestroy(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServSemaphoreDown(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServSemaphoreDownInterruptible(u32 op,
					u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServSemaphoreDownTimeout(u32 op,
					u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServSemaphoreDowntrylock(u32 op,
					u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServSemaphoreUp(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

#if 0
/* wait queue
 */
int KREE_ServWaitqCreate(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServWaitqDestroy(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServWaitqWaitevent(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServWaitqWaiteventTimeout(u32 op,
					u8 param[REE_SERVICE_BUFFER_SIZE]);

int KREE_ServWaitqWakeup(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);
#endif
#endif				/* __KREE_SYS_IPC__ */
