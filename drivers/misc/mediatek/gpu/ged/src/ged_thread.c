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

#include <linux/sched.h>
#include <linux/kthread.h>
#include "ged_base.h"
#include "ged_thread.h"

struct GED_THREAD_DATA {
	struct task_struct *psThread;
	void (*pFunc)(void *f);
	void *pvData;
};

static int ged_thread_run(void *pvData)
{
	struct GED_THREAD_DATA *psThreadData = (struct GED_THREAD_DATA *)pvData;

	if (psThreadData == NULL)
		return 0;

	psThreadData->pFunc(psThreadData->pvData);

	while (!kthread_should_stop())
		schedule();

	return 0;
}

GED_ERROR ged_thread_create(GED_THREAD_HANDLE *phThread,
	const char *szThreadName, void (*pFunc)(void *), void *pvData)
{
	struct GED_THREAD_DATA *psThreadData;

	if (phThread == NULL)
		return GED_ERROR_INVALID_PARAMS;

	psThreadData =
	(struct GED_THREAD_DATA *)ged_alloc(sizeof(struct GED_THREAD_DATA));
	if (psThreadData == NULL)
		return GED_ERROR_OOM;

	psThreadData->pFunc = pFunc;
	psThreadData->pvData = pvData;
	psThreadData->psThread =
		kthread_run(ged_thread_run, psThreadData, szThreadName);

	if (IS_ERR(psThreadData->psThread)) {
		ged_free(psThreadData, sizeof(struct GED_THREAD_DATA));
		return GED_ERROR_OOM;
	}

	*phThread = (GED_THREAD_HANDLE)psThreadData;

	return GED_OK;
}

GED_ERROR ged_thread_destroy(GED_THREAD_HANDLE hThread)
{
	struct GED_THREAD_DATA *psThreadData =
		(struct GED_THREAD_DATA *)hThread;

	if (psThreadData == NULL)
		return GED_ERROR_INVALID_PARAMS;

	kthread_stop(psThreadData->psThread);
	ged_free(psThreadData, sizeof(struct GED_THREAD_DATA));
	return GED_OK;
}
