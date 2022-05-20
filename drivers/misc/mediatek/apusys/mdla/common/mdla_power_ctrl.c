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
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <apusys_power.h>

#include <common/mdla_device.h>
#include <common/mdla_power_ctrl.h>

#include <utilities/mdla_util.h>
#include <utilities/mdla_debug.h>
#include <utilities/mdla_profile.h>

#define get_pwr_id(core_id) (MDLA0 + core_id)

struct mdla_pwr_ctrl {
	int mdla_id;
	struct mutex lock;
	struct wakeup_source *wakeup;
	struct timer_list power_off_timer;
	struct work_struct power_off_work;
	void (*power_off_cb_func)(struct work_struct *work);
};

static int mdla_pwr_dummy_on(u32 core_id, bool force)
{
	mdla_pwr_ops_get()->hw_reset(core_id,
				mdla_dbg_get_reason_str(REASON_SIMULATOR));
	return 0;
}
static int mdla_pwr_dummy_off(u32 core_id, int suspend, bool force)
{
	return 0;
}
static void mdla_pwr_dummy_hw_reset(u32 core_id, const char *str) {}
static void mdla_pwr_dummy_opp(u32 a0, int a1) {}
static void mdla_pwr_dummy_lock(u32 a0) {}
static void mdla_pwr_dummy_ops(u32 a0) {}

static struct mdla_pwr_ops mdla_power = {
	.on                 = mdla_pwr_dummy_on,
	.off                = mdla_pwr_dummy_off,
	.off_timer_start    = mdla_pwr_dummy_ops,
	.off_timer_cancel   = mdla_pwr_dummy_ops,
	.set_opp            = mdla_pwr_dummy_opp,
	.set_opp_by_boost   = mdla_pwr_dummy_opp,
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

static void mdla_pwr_off_timer_start(u32 core_id)
{
	u32 poweroff_time = mdla_dbg_read_u32(FS_POWEROFF_TIME);
	struct mdla_dev *mdla_device = mdla_get_device(core_id);
	struct mdla_pwr_ctrl *pwr_ctrl = mdla_device->power;

	mutex_lock_nested(&pwr_ctrl->lock, core_id);

	if (mdla_device->cmd_list_cnt > 0)
		mdla_device->cmd_list_cnt--;

	if (mdla_device->cmd_list_cnt == 0)
		mdla_prof_stop(core_id, 1);

	if (poweroff_time) {
		mdla_pwr_debug("%s: MDLA %d start power_timer\n",
				__func__, core_id);
		mod_timer(&pwr_ctrl->power_off_timer,
			jiffies + msecs_to_jiffies(poweroff_time));
	}

	mutex_unlock(&pwr_ctrl->lock);
}

static void mdla_pwr_off_timer_cancel(u32 core_id)
{
	struct mdla_pwr_ctrl *pwr_ctrl = mdla_get_device(core_id)->power;

	if (timer_pending(&pwr_ctrl->power_off_timer))
		del_timer(&pwr_ctrl->power_off_timer);
}

static void mdla_pwr_set_opp(u32 core_id, int opp)
{
	apu_device_set_opp(get_pwr_id(core_id), opp);
}

static void mdla_pwr_set_opp_by_boost(u32 core_id, int boost_val)
{
	unsigned char mdla_opp = 0;

	mdla_opp = apusys_boost_value_to_opp(get_pwr_id(core_id), (unsigned char)boost_val);
	apu_device_set_opp(get_pwr_id(core_id), mdla_opp);
	mdla_pwr_debug("core: %d, opp: %d\n", core_id, mdla_opp);
}

static void mdla_pwr_switch_off_on(u32 core_id)
{
	struct mdla_pwr_ctrl *pwr_ctrl = mdla_get_device(core_id)->power;

	mutex_lock_nested(&pwr_ctrl->lock, core_id);
	if (apu_device_power_off(get_pwr_id(core_id))
			|| apu_device_power_on(get_pwr_id(core_id)))
		mdla_err("%s: fail\n", __func__);
	mutex_unlock(&pwr_ctrl->lock);
}

static void mdla_pwr_lock(u32 core_id)
{
	mutex_lock_nested(&mdla_get_device(core_id)->power->lock, core_id);
}
static void mdla_pwr_unlock(u32 core_id)
{
	mutex_unlock(&mdla_get_device(core_id)->power->lock);
}

static void mdla_pwr_wake_lock(u32 core_id)
{
	__pm_stay_awake(mdla_get_device(core_id)->power->wakeup);
}

static void mdla_pwr_wake_unlock(u32 core_id)
{
	__pm_relax(mdla_get_device(core_id)->power->wakeup);
}

static int mdla_dbg_pwr_show(struct seq_file *s, void *data)
{
	int i;
	struct mdla_dev *mdla_device;

	for_each_mdla_core(i) {
		mdla_device = mdla_get_device(i);

		seq_printf(s, "---- core %d ----\n", i);
		seq_printf(s, "apu power status = %d\n", apu_get_power_on_status(get_pwr_id(i)));
		seq_printf(s, "power_is_on      = %d\n", mdla_device->power_is_on);
		seq_printf(s, "sw_power_is_on   = %d\n", mdla_device->sw_power_is_on);
		seq_printf(s, "cmd_list_cnt     = %d\n", mdla_device->cmd_list_cnt);
	}
	seq_printf(s, "\nRandom opp test: %u\n", mdla_dbg_read_u32(FS_DVFS_RAND));

	seq_puts(s, "\n==== usage ====\n");
	seq_printf(s, "echo [param] > /d/mdla/%s\n", DBGFS_PWR_NAME);
	seq_puts(s, "\tparam:\n");
	seq_puts(s, "\t  0: force all core power off\n");
	seq_puts(s, "\t  1: force all core power on\n");
	seq_printf(s, "echo [0|1] > /d/mdla/%s\n", mdla_dbg_get_u32_node_str(FS_DVFS_RAND));
	seq_puts(s, "\tEnable/Disable random opp test\n");

	return 0;
}

static int mdla_dbg_pwr_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdla_dbg_pwr_show, inode->i_private);
}

static ssize_t mdla_dbg_pwr_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *buf;
	u32 param;
	int i, ret;

	buf = kzalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = copy_from_user(buf, buffer, count);
	if (ret)
		goto out;

	buf[count] = '\0';

	if (kstrtouint(buf, 10, &param) != 0) {
		ret = -EINVAL;
		goto out;
	}

	for_each_mdla_core(i) {
		if (param == 0)
			mdla_pwr_ops_get()->off(i, 0, true);
		else if (param == 1)
			mdla_pwr_ops_get()->on(i, true);
	}

out:
	kfree(buf);
	return count;
}

static const struct file_operations mdla_dbg_pwr_fops = {
	.open = mdla_dbg_pwr_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mdla_dbg_pwr_write,
};


static void mdla_pwr_fs_init(void)
{
	struct dentry *d = mdla_dbg_get_fs_root();

	if (d)
		debugfs_create_file(DBGFS_PWR_NAME, 0644, d, NULL,
				&mdla_dbg_pwr_fops);
}

const struct mdla_pwr_ops *mdla_pwr_ops_get(void)
{
	return &mdla_power;
}

int mdla_pwr_get_random_boost_val(void)
{
	int val;

	/**
	 * Get opp 0 only when boost_val is 100
	 * Using division of 128 to increase probabiliy of getting opp 0
	 */
	val = (get_random_int() & 0x7F) + 1;

	return val > 100 ? 100 : val;
}

bool mdla_pwr_apusys_disabled(void)
{
	return !apusys_power_check();
}

void mdla_pwr_reset_setup(void (*hw_reset)(u32 core_id, const char *str))
{
	if (hw_reset)
		mdla_power.hw_reset = hw_reset;
}

int mdla_pwr_device_register(struct platform_device *pdev,
			int (*on)(u32 core_id, bool force),
			int (*off)(u32 core_id, int suspend, bool force))
{
	char ws_str[16] = {0};
	int i, ret = 0;
	enum DVFS_USER user_mdla;
	struct mdla_dev *mdla_device;
	struct mdla_pwr_ctrl *pwr_ctrl;
	struct apu_dev_power_data *pwr_data;

	mdla_drv_debug("probe 0, pdev id = %d name = %s, name = %s\n",
						pdev->id, pdev->name,
						pdev->dev.of_node->name);

	pwr_data = kzalloc(sizeof(struct apu_dev_power_data), GFP_KERNEL);
	if (!pwr_data)
		return -1;

	/* Only one platform_device */
	pwr_data->dev_type = MDLA0;
	pwr_data->dev_core = 0;
	platform_set_drvdata(pdev, pwr_data);

	for_each_mdla_core(i) {
		user_mdla = get_pwr_id(i);
		ret = apu_power_device_register(user_mdla, pdev);
		if (!ret) {
			mdla_drv_debug("%s register power device %d success\n",
							__func__,
							user_mdla);
		} else {
			mdla_err("%s register power device %d fail\n",
							__func__,
							user_mdla);
			goto reg_pwr_err;
		}
	}

	if ((on == NULL) && (off == NULL))
		return 0;

	for_each_mdla_core(i) {

		pwr_ctrl = kzalloc(sizeof(struct mdla_pwr_ctrl), GFP_KERNEL);
		if (!pwr_ctrl)
			goto alloc_err;

		pwr_ctrl->mdla_id = i;
		mutex_init(&pwr_ctrl->lock);
		timer_setup(&pwr_ctrl->power_off_timer, mdla_pwr_timeup, 0);

		pwr_ctrl->power_off_cb_func = mdla_pwr_off;
		INIT_WORK(&pwr_ctrl->power_off_work,
				pwr_ctrl->power_off_cb_func);

		if (snprintf(ws_str, sizeof(ws_str), "mdla_%d", i) > 0) {
			pwr_ctrl->wakeup = wakeup_source_register(NULL, ws_str);

			if (!pwr_ctrl->wakeup)
				mdla_err("mdla%d wakelock register fail!\n", i);
		}

		mdla_device = mdla_get_device(i);
		mdla_device->power          = pwr_ctrl;
		mdla_device->power_is_on    = false;
		mdla_device->sw_power_is_on = false;
	}

	if (on)
		mdla_power.on = on;
	if (off)
		mdla_power.off = off;

	mdla_power.set_opp              = mdla_pwr_set_opp;
	mdla_power.set_opp_by_boost     = mdla_pwr_set_opp_by_boost;
	mdla_power.switch_off_on        = mdla_pwr_switch_off_on;
	mdla_power.off_timer_start      = mdla_pwr_off_timer_start;
	mdla_power.off_timer_cancel     = mdla_pwr_off_timer_cancel;
	mdla_power.lock                 = mdla_pwr_lock;
	mdla_power.unlock               = mdla_pwr_unlock;
	mdla_power.wake_lock            = mdla_pwr_wake_lock;
	mdla_power.wake_unlock          = mdla_pwr_wake_unlock;

	mdla_pwr_fs_init();

	return 0;

alloc_err:
	for (i = i - 1;  i >= 0; i--) {
		pwr_ctrl = mdla_get_device(i)->power;
		wakeup_source_unregister(pwr_ctrl->wakeup);
		mutex_destroy(&pwr_ctrl->lock);
		kfree(pwr_ctrl);
		mdla_get_device(i)->power = NULL;
	}
	i = mdla_util_get_core_num();

reg_pwr_err:
	for (i = i - 1;  i >= 0; i--)
		apu_power_device_unregister(get_pwr_id(i));

	kfree(pwr_data);

	return -1;
}

int mdla_pwr_device_unregister(struct platform_device *pdev)
{
	struct mdla_pwr_ctrl *pwr_ctrl;
	int i;

	for_each_mdla_core(i)
		mdla_power.off(i, 0, true);

	for_each_mdla_core(i) {
		apu_power_device_unregister(get_pwr_id(i));

		pwr_ctrl = mdla_get_device(i)->power;
		if (!pwr_ctrl)
			continue;
		wakeup_source_unregister(pwr_ctrl->wakeup);
		mutex_destroy(&pwr_ctrl->lock);
		kfree(pwr_ctrl);
	}

	kfree(platform_get_drvdata(pdev));

	return 0;
}
