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


int mdla_pwr_on_v2_0(u32 core_id, bool force)
{
	int ret = 0;
	u64 poweron_t;
	struct mdla_dev *mdla_device = mdla_get_device(core_id);
	enum DVFS_USER user_mdla = MDLA0 + core_id;

	if (unlikely(!mdla_device))
		return -1;

	mdla_pwr_ops_get()->lock(core_id);
	if (force == false) {
		mdla_device->cmd_list_cnt++;
		if (mdla_device->cmd_list_cnt > 1)
			goto power_on_done;
	}
	mdla_device->sw_power_is_on = true;

	mdla_pwr_ops_get()->off_timer_cancel(core_id);

	if (mdla_device->power_is_on)
		goto power_on_done;

	poweron_t = sched_clock();
	ret = apu_device_power_on(user_mdla);

	if (!ret) {
		mdla_pwr_debug("%s power on device %d (mdla core %d) success\n",
					__func__, user_mdla, core_id);
		mdla_device->power_is_on = true;
	} else {
		mdla_err("%s power on device %d (mdla core %d) fail(%d)\n",
					__func__, user_mdla, core_id, ret);
		if (force == false)
			mdla_device->cmd_list_cnt--;

		goto power_on_done;
	}

	mdla_pwr_debug("mdla %d: power on info: apu_device_power_on_time: %llu\n",
			core_id, sched_clock() - poweron_t);

	mdla_pwr_ops_get()->hw_reset(core_id,
				mdla_dbg_get_reason_str(REASON_POWERON));
power_on_done:
	mdla_pwr_ops_get()->unlock(core_id);

	return 0;
}

int mdla_pwr_off_v2_0(u32 core_id,
				int suspend, bool force)
{
	int ret = 0;
	struct mdla_dev *mdla_device = mdla_get_device(core_id);

	if (unlikely(!mdla_device))
		return -1;

	mdla_pwr_ops_get()->lock(core_id);

	if (force == true)
		mdla_device->cmd_list_cnt = 0;
	if (mdla_device->cmd_list_cnt == 0)
		mdla_device->sw_power_is_on = false;

	if (mdla_device->power_is_on == false)
		goto power_off_done;

	mdla_pwr_ops_get()->off_timer_cancel(core_id);

	if (mdla_get_device(core_id)->cmd_list_cnt >= 1)
		goto power_off_done;

	if (mdla_get_device(core_id)->sw_power_is_on)
		goto power_off_done;

	ret = apu_device_power_suspend(MDLA0 + core_id, suspend);
	if (!ret) {
		mdla_pwr_debug("%s power off device %d (mdla-%d) success\n",
					__func__, MDLA0 + core_id, core_id);
		mdla_get_device(core_id)->power_is_on = false;
	} else {
		mdla_err("%s power off device %d (mdla-%d) fail(%d)\n",
					__func__, MDLA0 + core_id, core_id, ret);
	}

power_off_done:
	mdla_pwr_ops_get()->unlock(core_id);

	return ret;
}

