// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/of_device.h>
#include <linux/pm_wakeup.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>

#include <apusys_power.h>

#include <common/mdla_device.h>
#include <common/mdla_power_ctrl.h>

#include <utilities/mdla_util.h>
#include <utilities/mdla_debug.h>


struct mdla_pwr_ctrl {
	int mdla_id;
	struct mutex lock;
	struct mutex wake_lock;
	struct wakeup_source *wakeup;
	struct timer_list power_off_timer;
	struct work_struct power_off_work;
	void (*power_off_cb_func)(struct work_struct *work);
};

static int mdla_pwr_dummy_on(int core_id, bool force)
{
	return 0;
}
static int mdla_pwr_dummy_off(int core_id, int suspend, bool force)
{
	return 0;
}
static void mdla_pwr_dummy_hw_reset(int core_id, const char *str) {}
static void mdla_pwr_dummy_opp(int a0, int a1) {}
static void mdla_pwr_dummy_lock(int a0) {}
static void mdla_pwr_dummy_ops(int a0) {}

static struct mdla_pwr_ops mdla_power = {
	.on                 = mdla_pwr_dummy_on,
	.off                = mdla_pwr_dummy_off,
	.off_timer_start    = mdla_pwr_dummy_ops,
	.off_timer_cancel   = mdla_pwr_dummy_ops,
	.set_opp            = mdla_pwr_dummy_opp,
	.set_opp_by_bootst  = mdla_pwr_dummy_opp,
	.switch_off_on      = mdla_pwr_dummy_ops,
	.hw_reset           = mdla_pwr_dummy_hw_reset,
	.lock               = mdla_pwr_dummy_lock,
	.unlock             = mdla_pwr_dummy_lock,
	.wake_lock          = mdla_pwr_dummy_lock,
	.wake_unlock        = mdla_pwr_dummy_lock,
};

static void mdla_pwr_timeup(struct timer_list *timer)
{
	struct mdla_pwr_ctrl *pwr_ctrl;

	pwr_ctrl = container_of(timer, struct mdla_pwr_ctrl, power_off_timer);
	schedule_work(&pwr_ctrl->power_off_work);
}

static void mdla_pwr_off(struct work_struct *work)
{
	struct mdla_pwr_ctrl *pwr_ctrl;

	pwr_ctrl = container_of(work, struct mdla_pwr_ctrl, power_off_work);
	mdla_power.off(pwr_ctrl->mdla_id, 0, false);
}

static void mdla_pwr_off_timer_start(int core_id)
{
	unsigned int poweroff_time = mdla_dbg_read_u32(FS_POWEROFF_TIME);
	struct mdla_dev *mdla_device = mdla_get_device(core_id);
	struct mdla_pwr_ctrl *pwr_ctrl = mdla_device->power;

	mutex_lock(&pwr_ctrl->lock);

	if (mdla_device->cmd_list_cnt > 0)
		mdla_device->cmd_list_cnt--;

	if (poweroff_time) {
		mdla_drv_debug("%s: MDLA %d start power_timer\n",
				__func__, core_id);
		mod_timer(&pwr_ctrl->power_off_timer,
			jiffies + msecs_to_jiffies(poweroff_time));
	}

	mutex_unlock(&pwr_ctrl->lock);
}

static void mdla_pwr_off_timer_cancel(int core_id)
{
	struct mdla_pwr_ctrl *pwr_ctrl = mdla_get_device(core_id)->power;

	if (timer_pending(&pwr_ctrl->power_off_timer))
		del_timer(&pwr_ctrl->power_off_timer);
}

static void mdla_pwr_set_opp(int core_id, int opp)
{
	apu_device_set_opp(MDLA0 + core_id, opp);
}

static void mdla_pwr_set_opp_by_bootst(int core_id, int bootst_val)
{
	uint8_t mdla_opp = 0;
	enum DVFS_USER user_mdla = MDLA0 + core_id;

	mdla_opp = apusys_boost_value_to_opp(user_mdla, bootst_val);
	apu_device_set_opp(user_mdla, mdla_opp);
}

static void mdla_pwr_switch_off_on(int core_id)
{
	struct mdla_pwr_ctrl *pwr_ctrl = mdla_get_device(core_id)->power;
	enum DVFS_USER user_mdla = MDLA0 + core_id;

	mutex_lock(&pwr_ctrl->lock);
	apu_device_power_off(user_mdla);
	apu_device_power_on(user_mdla);
	mutex_unlock(&pwr_ctrl->lock);
}

static void mdla_pwr_lock(int core_id)
{
	mutex_lock(&mdla_get_device(core_id)->power->lock);
}
static void mdla_pwr_unlock(int core_id)
{
	mutex_unlock(&mdla_get_device(core_id)->power->lock);
}

static void mdla_pwr_wake_lock(int core_id)
{
	struct mdla_pwr_ctrl *pwr_ctrl = mdla_get_device(core_id)->power;

	mutex_lock(&pwr_ctrl->wake_lock);
	__pm_stay_awake(pwr_ctrl->wakeup);
	mutex_unlock(&pwr_ctrl->wake_lock);
}

static void mdla_pwr_wake_unlock(int core_id)
{
	struct mdla_pwr_ctrl *pwr_ctrl = mdla_get_device(core_id)->power;

	mutex_lock(&pwr_ctrl->wake_lock);
	__pm_relax(pwr_ctrl->wakeup);
	mutex_unlock(&pwr_ctrl->wake_lock);
}

const struct mdla_pwr_ops *mdla_pwr_ops_get(void)
{
	return &mdla_power;
}

bool mdla_power_check(void)
{
	return apusys_power_check();
}

int mdla_pwr_device_register(struct platform_device *pdev,
			int (*on)(int core_id, bool force),
			int (*off)(int core_id, int suspend, bool force),
			void (*hw_reset)(int core_id, const char *str))
{
	int i, ret = 0;
	enum DVFS_USER user_mdla;
	struct mdla_dev *mdla_device;
	struct mdla_pwr_ctrl *pwr_ctrl;

	if (hw_reset)
		mdla_power.hw_reset = hw_reset;

	if (!on || !off)
		return 0;

	mdla_cmd_debug("probe 0, pdev id = %d name = %s, name = %s\n",
						pdev->id, pdev->name,
						pdev->dev.of_node->name);

	for_each_mdla_core(i) {

		pwr_ctrl = kzalloc(sizeof(struct mdla_pwr_ctrl),
					GFP_KERNEL);
		if (!pwr_ctrl)
			goto out;

		pwr_ctrl->mdla_id = i;
		mutex_init(&pwr_ctrl->lock);
		mutex_init(&pwr_ctrl->wake_lock);
		timer_setup(&pwr_ctrl->power_off_timer, mdla_pwr_timeup, 0);

		pwr_ctrl->power_off_cb_func = mdla_pwr_off;
		INIT_WORK(&pwr_ctrl->power_off_work,
				pwr_ctrl->power_off_cb_func);

		user_mdla = MDLA0 + i;
		ret = apu_power_device_register(user_mdla, pdev);
		if (!ret) {
			mdla_cmd_debug("%s register power device %d success\n",
							__func__,
							user_mdla);
		} else {
			mdla_err("%s register power device %d fail\n",
							__func__,
							user_mdla);
			kfree(pwr_ctrl);
			goto out;
		}

		pwr_ctrl->wakeup = wakeup_source_register(NULL, "mdla");

		if (!pwr_ctrl->wakeup)
			mdla_err("mdla wakelock register fail!\n");

		mdla_device = mdla_get_device(i);
		mdla_device->power          = pwr_ctrl;
		mdla_device->power_is_on    = false;
		mdla_device->sw_power_is_on = false;
	}

	mdla_power.on                   = on;
	mdla_power.off                  = off;
	mdla_power.set_opp              = mdla_pwr_set_opp;
	mdla_power.set_opp_by_bootst    = mdla_pwr_set_opp_by_bootst;
	mdla_power.switch_off_on        = mdla_pwr_switch_off_on;
	mdla_power.off_timer_start      = mdla_pwr_off_timer_start;
	mdla_power.off_timer_cancel     = mdla_pwr_off_timer_cancel;
	mdla_power.lock                 = mdla_pwr_lock;
	mdla_power.unlock               = mdla_pwr_unlock;
	mdla_power.wake_lock            = mdla_pwr_wake_lock;
	mdla_power.wake_unlock          = mdla_pwr_wake_unlock;

	return 0;

out:
	for (i = i - 1;  i >= 0; i--) {
		pwr_ctrl = mdla_get_device(i)->power;
		wakeup_source_unregister(pwr_ctrl->wakeup);
		mutex_destroy(&pwr_ctrl->wake_lock);
		mutex_destroy(&pwr_ctrl->lock);
		kfree(pwr_ctrl);
		mdla_get_device(i)->power = NULL;
		apu_power_device_unregister(MDLA0 + i);
	}

	return -1;
}

int mdla_pwr_device_unregister(struct platform_device *pdev)
{
	struct mdla_pwr_ctrl *pwr_ctrl;
	enum DVFS_USER user_mdla;
	int i;

	for_each_mdla_core(i)
		mdla_power.off(i, 0, true);

	for_each_mdla_core(i) {
		user_mdla = MDLA0 + i;
		apu_power_device_unregister(user_mdla);

		pwr_ctrl = mdla_get_device(i)->power;

		if (!pwr_ctrl)
			continue;

		wakeup_source_unregister(pwr_ctrl->wakeup);
		mutex_destroy(&pwr_ctrl->wake_lock);
		mutex_destroy(&pwr_ctrl->lock);
		kfree(pwr_ctrl);
	}
	return 0;
}




