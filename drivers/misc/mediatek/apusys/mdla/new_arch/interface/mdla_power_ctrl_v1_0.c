// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/of_device.h>
#include <linux/pm_wakeup.h>
#include <linux/sched/clock.h>

#include <apusys_power.h>

#include <common/mdla_device.h>
#include <common/mdla_power_ctrl.h>

#include <utilities/mdla_util.h>
#include <utilities/mdla_profile.h>
#include <utilities/mdla_debug.h>


int mdla_pwr_on_v1_0(u32 core_id, bool force)
{
	int ret = 0;
	struct mdla_dev *mdla_device = mdla_get_device(core_id);
	enum DVFS_USER user_mdla = MDLA0 + core_id;

	mdla_pwr_ops_get()->lock(core_id);

	mdla_pwr_ops_get()->off_timer_cancel(core_id);

	if (mdla_device->power_is_on) {
		mdla_pwr_debug("%s: already on.\n", __func__);
		goto power_on_done;
	}

	ret = apu_device_power_on(user_mdla);

	if (!ret) {
		mdla_pwr_debug("%s power on device %d success\n",
						__func__, user_mdla);
		mdla_device->power_is_on = true;
	} else {
		mdla_err("%s power on device %d fail\n",
						__func__, user_mdla);
	}

	mdla_pwr_ops_get()->hw_reset(core_id,
				mdla_dbg_get_reason_str(REASON_POWERON));

power_on_done:
	mdla_pwr_ops_get()->unlock(core_id);

	return ret;
}

int mdla_pwr_off_v1_0(u32 core_id,
				int suspend, bool force)
{
	int ret = 0;
	struct mdla_dev *mdla_device = mdla_get_device(core_id);
	enum DVFS_USER user_mdla = MDLA0 + core_id;

	if (unlikely(!mdla_device))
		return -1;

	mutex_lock(&mdla_device->cmd_lock);
	mdla_pwr_ops_get()->lock(core_id);

	if (mdla_device->power_is_on == false)
		goto power_off_done;

	ret = apu_device_power_off(user_mdla);

	if (!ret) {
		mdla_pwr_debug("%s power off device %d success\n",
						__func__, user_mdla);
		mdla_device->power_is_on = false;
	} else {
		mdla_err("%s power off device %d fail\n",
						__func__, user_mdla);
	}

power_off_done:
	mdla_pwr_ops_get()->unlock(core_id);
	mutex_unlock(&mdla_device->cmd_lock);

	return ret;
}

