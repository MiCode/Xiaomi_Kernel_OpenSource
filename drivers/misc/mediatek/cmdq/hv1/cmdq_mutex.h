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

#ifndef __CMDQ_MUTEX_H__
#define __CMDQ_MUTEX_H__

#include <linux/mutex.h>
#include <ddp_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

	int32_t cmdqMutexInitialize(void);

	int32_t cmdqMutexAcquire(void);

	int32_t cmdqMutexRelease(int32_t mutex);

	void cmdqMutexDeInitialize(void);

	bool cmdqMDPMutexInUse(int index);

	pid_t cmdqMDPMutexOwnerPid(int index);


#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_MUTEX_H__ */
