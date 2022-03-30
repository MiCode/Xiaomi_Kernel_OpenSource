// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include "adsp_core.h"
#include "adsp_feature_define.h"
#include "adsp_reserved_mem.h"
#include "adsp_platform.h"
#include "adsp_platform_driver.h"

#define WAIT_MS                     (1000)
#define AP_AWAKE_LOCK_BIT           (0)
#define AP_AWAKE_UNLOCK_BIT         (1)
#define AP_AWAKE_LOCK_MASK          (0x1 << AP_AWAKE_LOCK_BIT)
#define AP_AWAKE_UNLOCK_MASK        (0x1 << AP_AWAKE_UNLOCK_BIT)


/*
 * acquire adsp lock flag, keep adsp awake
 * @param adsp_id: adsp core id
 * return 0     : get lock success
 *        non-0 : get lock fail
 */
int adsp_awake_lock(u32 cid)
{
	int msg = AP_AWAKE_LOCK_MASK;
	int ret = ADSP_IPI_BUSY;

	if (cid >= get_adsp_core_total()) {
		ret = -EINVAL;
		goto ERROR;
	}

	if (!is_adsp_ready(cid) || !adsp_feature_is_active(cid)) {
		ret = -ENODEV;
		goto ERROR;
	}

	ret = adsp_push_message(ADSP_IPI_DVFS_WAKE, &msg, sizeof(u32), WAIT_MS, cid);

	if (ret)
		goto ERROR;

	return ADSP_IPI_DONE;

ERROR:
	pr_info("%s, lock fail, ret = %d", __func__, ret);
	return ret;
}

/*
 * release adsp awake lock flag
 * @param adsp_id: adsp core id
 * return 0     : release lock success
 *        non-0 : release lock fail
 */
int adsp_awake_unlock(u32 cid)
{
	int msg = AP_AWAKE_UNLOCK_MASK;
	int ret = ADSP_IPI_BUSY;

	if (cid >= get_adsp_core_total()) {
		ret = -EINVAL;
		goto ERROR;
	}

	if (!is_adsp_ready(cid) || !adsp_feature_is_active(cid)) {
		ret = -ENODEV;
		goto ERROR;
	}

	ret = adsp_push_message(ADSP_IPI_DVFS_WAKE, &msg, sizeof(u32), WAIT_MS, cid);

	if (ret)
		goto ERROR;

	return ADSP_IPI_DONE;

ERROR:
	pr_info("%s, unlock fail, ret = %d", __func__, ret);
	return ret;
}

