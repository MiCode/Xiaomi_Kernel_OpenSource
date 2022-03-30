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

#define AP_AWAKE_LOCK_BIT           (0)
#define AP_AWAKE_UNLOCK_BIT         (1)
#define AP_AWAKE_LOCK_MASK          (0x1 << AP_AWAKE_LOCK_BIT)
#define AP_AWAKE_UNLOCK_MASK        (0x1 << AP_AWAKE_UNLOCK_BIT)

struct adsp_sysevent_ctrl {
	struct adsp_priv *pdata;
	struct mutex lock;
};

static struct adsp_sysevent_ctrl sysevent_ctrls[ADSP_CORE_TOTAL];

static int adsp_send_sys_event(struct adsp_sysevent_ctrl *ctrl,
				u32 event, bool wait)
{
	ktime_t start_time;
	s64     time_ipc_us;

	if (mutex_trylock(&ctrl->lock) == 0) {
		pr_info("%s(), mutex_trylock busy", __func__);
		return ADSP_IPI_BUSY;
	}

	if (adsp_mt_check_swirq(ctrl->pdata->id)) {
		mutex_unlock(&ctrl->lock);
		return ADSP_IPI_BUSY;
	}

	adsp_copy_to_sharedmem(ctrl->pdata, ADSP_SHAREDMEM_WAKELOCK,
				&event, sizeof(event));

	adsp_mt_set_swirq(ctrl->pdata->id);

	if (wait) {
		start_time = ktime_get();
		while (adsp_mt_check_swirq(ctrl->pdata->id)) {
			time_ipc_us = ktime_us_delta(ktime_get(), start_time);
			if (time_ipc_us > 1000) /* 1 ms */
				break;
		}
	}

	mutex_unlock(&ctrl->lock);
	return ADSP_IPI_DONE;
}

int adsp_awake_init(struct adsp_priv *pdata)
{
	struct adsp_sysevent_ctrl *ctrl;

	if (!pdata)
		return -EINVAL;

	ctrl = &sysevent_ctrls[pdata->id];
	ctrl->pdata = pdata;
	mutex_init(&ctrl->lock);

	return 0;
}

/*
 * acquire adsp lock flag, keep adsp awake
 * @param adsp_id: adsp core id
 * return 0     : get lock success
 *        non-0 : get lock fail
 */
int adsp_awake_lock(u32 cid)
{
	int ret = -1;

	if (cid >= get_adsp_core_total())
		return -EINVAL;

	if (!is_adsp_ready(cid) || !adsp_feature_is_active(cid))
		return -ENODEV;

	ret = adsp_send_sys_event(&sysevent_ctrls[cid],
				  AP_AWAKE_LOCK_MASK, true);

	if (ret)
		pr_info("%s, lock fail", __func__);

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
	int ret = -1;

	if (cid >= get_adsp_core_total())
		return -EINVAL;

	if (!is_adsp_ready(cid) || !adsp_feature_is_active(cid))
		return -ENODEV;

	ret = adsp_send_sys_event(&sysevent_ctrls[cid],
				  AP_AWAKE_UNLOCK_MASK, true);

	if (ret)
		pr_info("%s, unlock fail", __func__);

	return ret;
}

