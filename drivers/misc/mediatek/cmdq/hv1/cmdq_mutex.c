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

#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/string.h>
/* #include <mach/mt_clkmgr.h> */

#include "cmdq_def.h"
#include "cmdq_mutex.h"
#include "cmdq_core.h"

static spinlock_t gMutexLock;
static uint32_t gMutexUsed[DISP_MUTEX_MDP_COUNT];
static pid_t gMutexUserPid[DISP_MUTEX_MDP_COUNT];
static int gMDPMutexCount;

int32_t cmdqMutexInitialize(void)
{
	unsigned long flags;

	spin_lock_init(&gMutexLock);

	spin_lock_irqsave(&gMutexLock, flags);

	gMDPMutexCount = 0;

	memset(gMutexUsed, 0x0, sizeof(gMutexUsed));

	memset(gMutexUserPid, 0x0, sizeof(gMutexUsed));

	spin_unlock_irqrestore(&gMutexLock, flags);

	return 0;
}

bool cmdqMDPMutexInUse(int index)
{
	if (gMutexUsed[index] != 0)
		return true;
	else
		return false;

}

pid_t cmdqMDPMutexOwnerPid(int index)
{
	if (gMutexUsed[index] != 0)
		return gMutexUserPid[index];
	else
		return 0;

}

int32_t cmdqMutexAcquire(void)
{
	unsigned long flags;
	int32_t mutex;
	int32_t index;

	mutex = -1;

	spin_lock_irqsave(&gMutexLock, flags);

	for (index = 0; index < DISP_MUTEX_MDP_COUNT; index++) {
		if (0 == gMutexUsed[index]) {
			/* Record the mutex */
			mutex = index;

			/* Set to use state */
			gMutexUsed[index] = 1;
			gMutexUserPid[index] = current->pid;

			/* note: although we tracks Mutex usage count, */
			/* we do not enable/disable its clock. */
			/* this is because some system process like */
			/* MMComposerThread may keep mutex for a long time, */
			/* even across suspend/resume calls. */
			gMDPMutexCount++;

			break;
		}
	}
	spin_unlock_irqrestore(&gMutexLock, flags);

	if (mutex == -1) {
		CMDQ_ERR("cmdqMutexAcquire failed\n");
		return mutex;
	}
	/* note that we have an offset, */
	/* the mutex id does NOT start from 0! */
	mutex += DISP_MUTEX_MDP_FIRST;

	CMDQ_VERBOSE("[MUTEX] acquire mutex %d\n", mutex);

	return mutex;
}


int32_t cmdqMutexRelease(int32_t mutex)
{
	unsigned long flags;

	CMDQ_VERBOSE("[MUTEX] release mutex %d\n", mutex);

	mutex -= DISP_MUTEX_MDP_FIRST;

	if ((mutex < 0) || (mutex >= DISP_MUTEX_MDP_COUNT)) {
		CMDQ_ERR("[MUTEX]wrong mutex offset %d\n", mutex);
		return -EFAULT;
	}

	spin_lock_irqsave(&gMutexLock, flags);

	if (1 == gMutexUsed[mutex]) {
		/* OK we release a Mutex - clock OFF if no more MUTEX */
		gMDPMutexCount--;
		/* note: although we tracks Mutex usage count, */
		/* we do not enable/disable its clock. */
		/* this is because some system process like */
		/* MMComposerThread may keep mutex for a long time, */
		/* even across suspend/resume calls. */
	}

	gMutexUsed[mutex] = 0;
	spin_unlock_irqrestore(&gMutexLock, flags);

	return 0;
}


void cmdqMutexDeInitialize(void)
{
	/* Do nothing */
}
