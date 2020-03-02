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

#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <../drivers/staging/android/sync.h>

#include "ged_frr.h"
#include "ged_base.h"
#include "ged_type.h"
/*#include "primary_display.h"*/
#include "dfrc.h"
#include "dfrc_drv.h"

#ifdef GED_LOGE
#undef GED_LOGE
#define GED_LOGE pr_debug
#endif

/* These module params are for developers to set specific fps and debug. */
static char *ged_frr_debug_fence2context_table;
module_param(ged_frr_debug_fence2context_table, charp, S_IRUGO|S_IWUSR);

/* Fence2Context Table */
typedef struct GED_FRR_FENCE2CONTEXT_TABLE_TYPE {
	int			pid;
	uint64_t	cid;
	void		*fid;
	uint64_t	createTime;
} GED_FRR_FENCE2CONTEXT_TABLE;

static GED_FRR_FENCE2CONTEXT_TABLE *fence2ContextTable;
static int fence2ContextTableSize;

static struct mutex fence2ContextTableLock;

static void ged_frr_fence2context_table_dump(void)
{
	int i;
	static char buf[1024];
	char temp[128];

	memset(buf, '\0', 1024);

	for (i = 0; i < fence2ContextTableSize; i++) {
		snprintf(temp, sizeof(temp),
				"%d,%llu,%p,%llu\n",
				fence2ContextTable[i].pid,
				fence2ContextTable[i].cid,
				fence2ContextTable[i].fid,
				fence2ContextTable[i].createTime);

		strncat(buf, temp, 1023);
	}

	ged_frr_debug_fence2context_table = buf;
}

static GED_ERROR ged_frr_fence2context_table_create(void)
{
	int i;

	mutex_init(&fence2ContextTableLock);

	fence2ContextTableSize = GED_FRR_FENCE2CONTEXT_TABLE_SIZE;
	fence2ContextTable =
		(GED_FRR_FENCE2CONTEXT_TABLE *)ged_alloc(fence2ContextTableSize * sizeof(GED_FRR_FENCE2CONTEXT_TABLE));

	if (!fence2ContextTable) {
		GED_LOGE("[FRR] fence2ContextTable is NULL\n");
		return GED_ERROR_OOM;
	}

	for (i = 0; i < fence2ContextTableSize; i++) {
		fence2ContextTable[i].pid = 0;
		fence2ContextTable[i].cid = 0;
		fence2ContextTable[i].fid = 0;
		fence2ContextTable[i].createTime = 0;
	}

	ged_frr_fence2context_table_dump();

	return GED_OK;
}

static GED_ERROR ged_frr_fence2context_table_release(void)
{
	mutex_destroy(&fence2ContextTableLock);

	if (fence2ContextTable)
		ged_free(fence2ContextTable, fence2ContextTableSize * sizeof(GED_FRR_FENCE2CONTEXT_TABLE));

	return GED_OK;
}

static GED_ERROR ged_frr_fence2context_table_add_item(int targetPid, uint64_t targetCid, void *targetFid)
{
	int i;
	int targetIndex;
	unsigned long long leastCreateTime;

	GED_LOGE("[FRR] [+] add item: pid(%d), cid(%llu), fid(%p)\n"
		, targetPid, targetCid, targetFid);

	if (!fence2ContextTable) {
		GED_LOGE("[FRR] [-] add item: fence2ContextTable is NULL\n");
		return GED_ERROR_FAIL;
	}

	targetIndex = 0;
	leastCreateTime = fence2ContextTable[0].createTime;

	for (i = 0; i < fence2ContextTableSize; i++) {
		if (fence2ContextTable[i].pid == 0) {
			targetIndex = i;
			break;
		}
		if (fence2ContextTable[i].createTime < leastCreateTime) {
			leastCreateTime = fence2ContextTable[i].createTime;
			targetIndex = i;
		}
	}

	fence2ContextTable[targetIndex].pid = targetPid;
	fence2ContextTable[targetIndex].cid = targetCid;
	fence2ContextTable[targetIndex].fid = targetFid;
	fence2ContextTable[targetIndex].createTime = ged_get_time();

	GED_LOGE("[FRR] [-] add item, targetIndex(%d)\n", targetIndex);

	return GED_OK;
}

GED_ERROR ged_frr_system_init(void)
{
	return ged_frr_fence2context_table_create();
}

GED_ERROR ged_frr_system_exit(void)
{
	return ged_frr_fence2context_table_release();
}

GED_ERROR ged_frr_fence2context_table_update(int pid, uint64_t cid, int fenceFd)
{
	int i;
	int ret = GED_ERROR_FAIL;
	void *fence;

	if (!fence2ContextTable) {
		GED_LOGE("[FRR] fence2ContextTable is NULL\n");
		return GED_ERROR_FAIL;
	}

	if (fenceFd < 0) {
		/* GED_LOGE("[FRR] fenceFd < 0\n");*/
		return GED_ERROR_INVALID_PARAMS;
	}

	fence = (void *)sync_fence_fdget(fenceFd);

	mutex_lock(&fence2ContextTableLock);
	for (i = 0; i < fence2ContextTableSize; i++) {
		if (fence2ContextTable[i].pid == pid && fence2ContextTable[i].cid == cid) {
			fence2ContextTable[i].fid = fence;
			ged_frr_fence2context_table_dump();
			ret = GED_OK;
			break;
		}
	}

	if (ret != GED_OK) {
		ret = ged_frr_fence2context_table_add_item(pid, cid, fence);
		ged_frr_fence2context_table_dump();
	}
	mutex_unlock(&fence2ContextTableLock);

	sync_fence_put(fence);
	return ret;
}

GED_ERROR ged_frr_fence2context_table_get_cid(int pid, void *fid, uint64_t *cid)
{
	int i;

	if (!fence2ContextTable) {
		GED_LOGE("[FRR] fence2ContextTable is NULL\n");
		return GED_ERROR_FAIL;
	}

	for (i = 0; i < fence2ContextTableSize; i++) {
		if ((fence2ContextTable[i].pid == pid) && (fence2ContextTable[i].fid == fid)) {
			*cid = fence2ContextTable[i].cid;
			return GED_OK;
		}
	}

	return GED_ERROR_FAIL;
}

int ged_frr_get_fps(int targetPid, uint64_t targetCid)
{
	int fps = -1;
#ifdef CONFIG_MTK_DYNAMIC_FPS_FRAMEWORK_SUPPORT
	int mode;

	dfrc_get_frr_setting(targetPid, targetCid, &fps, &mode);

	if (mode != DFRC_DRV_MODE_FRR)
		fps = 60;
#endif
	return fps;
}

GED_ERROR ged_frr_wait_hw_vsync(void)
{
	int ret = 0;
	/*struct disp_session_vsync_config vsync_config;*/

	/*ret = primary_display_wait_for_vsync(&vsync_config);*/
	if (ret < 0) {
		GED_LOGE("[FRR] ged_frr_wait_hw_vsync, ret(%d)\n", ret);
		return GED_ERROR_FAIL;
	}
	return GED_OK;
}

