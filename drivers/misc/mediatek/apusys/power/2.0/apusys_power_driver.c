/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/spinlock_types.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>

#include "apu_log.h"
#include "apusys_power_ctl.h"
#include "apusys_power_cust.h"
#include "apusys_power_debug.h"
#include "apu_platform_resource.h"
#include "hal_config_power.h"
#include "apu_power_api.h"
#include "pmic_api_buck.h"
#include "apusys_power_rule_check.h"
#include "apusys_dbg.h"
#include "apusys_power.h"
#ifdef APUPWR_TAG_TP
#include "apu_power_tag.h"
#endif

int g_pwr_log_level = APUSYS_PWR_LOG_ERR;
int g_pm_procedure;
int power_on_off_stress;
static int apu_power_counter;
static int apusys_power_broken;
static uint64_t power_info_id;
static uint8_t power_info_force_print;

bool apusys_power_check(void)
{
#if defined(CONFIG_MACH_MT6885)
	char *pwr_ptr;
	bool pwr_status = true;

	pwr_ptr = strstr(saved_command_line,
				"apusys_status=normal");
	if (pwr_ptr == 0) {
		pwr_status = false;
		LOG_ERR("apusys power disable !!, pwr_status=%d\n",
			pwr_status);
	}
	LOG_INF("apusys power check, pwr_status=%d\n",
			pwr_status);
	return pwr_status;
#else
	return true;
#endif
}
EXPORT_SYMBOL(apusys_power_check);

struct power_device {
	enum DVFS_USER dev_usr;
	int is_power_on;
	struct platform_device *pdev;
	struct list_head list;
};

struct power_callback_device {
	enum POWER_CALLBACK_USER power_callback_usr;
	void (*power_on_callback)(void *para);
	void (*power_off_callback)(void *para);
	struct list_head list;
};

static LIST_HEAD(power_device_list);
static LIST_HEAD(power_callback_device_list);
static struct mutex power_device_list_mtx;
static struct mutex power_ctl_mtx;
static struct mutex power_opp_mtx;
static spinlock_t power_info_lock;
static int power_callback_counter;
static struct task_struct *power_task_handle;
static uint64_t timestamp;
static struct hal_param_init_power init_power_data;

static struct workqueue_struct *wq;
static void d_work_power_info_func(struct work_struct *work);
static void d_work_power_init_func(struct work_struct *work);
static DECLARE_WORK(d_work_power_info, d_work_power_info_func);
static DECLARE_WORK(d_work_power_init, d_work_power_init_func);
struct delayed_work d_work_power;

#ifdef CONFIG_PM_SLEEP
struct wakeup_source pwr_wake_lock;
#endif

static void apu_pwr_wake_lock(void)
{
#ifdef CONFIG_PM_SLEEP
	__pm_stay_awake(&pwr_wake_lock);
#endif
}

static void apu_pwr_wake_unlock(void)
{
#ifdef CONFIG_PM_SLEEP
	__pm_relax(&pwr_wake_lock);
#endif
}

static void apu_pwr_wake_init(void)
{
#ifdef CONFIG_PM_SLEEP
	wakeup_source_init(&pwr_wake_lock, "apupwr_wakelock");
#endif
}

/**
 * print_time() - Brief description of print_time.
 * @ts: nanosecond
 * @buf: buffer to put [%5lu.%06lu]
 *
 * Transfer nanoseconds to format as [second.micro_second].
 *
 * Return: length of valid string
 **/
static size_t print_time(u64 ts, char *buf)
{
	unsigned long rem_nsec;

	rem_nsec = do_div(ts, 1000000000);

	if (!buf)
		return snprintf(NULL, 0, "[%5lu.000000] ", (unsigned long)ts);

	return sprintf(buf, "[%5lu.%06lu] ",
		       (unsigned long)ts, rem_nsec / 1000);
}

uint64_t apu_get_power_info(int force)
{
	uint64_t ret = 0;

	spin_lock(&power_info_lock);
	power_info_id = sched_clock();
	ret = power_info_id;
	power_info_force_print = force;
	spin_unlock(&power_info_lock);

	queue_work(wq, &d_work_power_info);

	return ret;
}
EXPORT_SYMBOL(apu_get_power_info);

void apu_power_reg_dump(void)
{
	mutex_lock(&power_ctl_mtx);
	// keep 26M vcore clk make we can dump reg directly
	hal_config_power(PWR_CMD_REG_DUMP, VPU0, NULL);
	mutex_unlock(&power_ctl_mtx);
}
EXPORT_SYMBOL(apu_power_reg_dump);


void apu_set_vcore_boost(bool enable)
{

}
EXPORT_SYMBOL(apu_set_vcore_boost);

void apu_qos_set_vcore(int target_volt)
{
#ifdef MTK_FPGA_PORTING
		LOG_WRN("%s FPGA porting bypass DVFS\n", __func__);
#else

#if !BYPASS_DVFS_CTL
		mutex_lock(&power_opp_mtx);

#if SUPPORT_VCORE_TO_IPUIF
		// set opp value
		apusys_set_apu_vcore(target_volt);
#endif

		// change regulator and clock if need
		wake_up_process(power_task_handle);

		mutex_unlock(&power_opp_mtx);
#else
		LOG_WRN("%s bypass since not support DVFS\n", __func__);
#endif

#endif
}
EXPORT_SYMBOL(apu_qos_set_vcore);

static struct power_device *find_out_device_by_user(enum DVFS_USER user)
{
	struct power_device *pwr_dev = NULL;

	if (!list_empty(&power_device_list)) {

		list_for_each_entry(pwr_dev,
				&power_device_list, list) {

			if (pwr_dev && pwr_dev->dev_usr == user)
				return pwr_dev;
		}

	} else {
		LOG_ERR("%s empty list\n", __func__);
	}

	return NULL;
}

bool apu_get_power_on_status(enum DVFS_USER user)
{
	bool power_on_status;
	struct power_device *pwr_dev = find_out_device_by_user(user);

	if (pwr_dev == NULL)
		return false;

	power_on_status = pwr_dev->is_power_on;

	return power_on_status;
}
EXPORT_SYMBOL(apu_get_power_on_status);

#if !BYPASS_POWER_CTL
static void power_callback_caller(int power_on)
{
	struct power_callback_device *pwr_dev = NULL;

	LOG_DBG("%s begin (%d)\n", __func__, power_on);

	if (!list_empty(&power_callback_device_list)) {
		list_for_each_entry(pwr_dev,
			&power_callback_device_list, list) {

			LOG_DBG("%s calling %d in state %d\n", __func__,
					pwr_dev->power_callback_usr, power_on);

			if (power_on) {
				if (pwr_dev && pwr_dev->power_on_callback)
					pwr_dev->power_on_callback(NULL);
			} else {
				if (pwr_dev && pwr_dev->power_off_callback)
					pwr_dev->power_off_callback(NULL);
			}
		}
	}

	LOG_DBG("%s end (%d)\n", __func__, power_on);
}
#endif

static struct power_callback_device*
find_out_callback_device_by_user(enum POWER_CALLBACK_USER user)
{
	struct power_callback_device *pwr_dev = NULL;

	if (!list_empty(&power_callback_device_list)) {

		list_for_each_entry(pwr_dev,
				&power_callback_device_list, list) {

			if (pwr_dev && pwr_dev->power_callback_usr == user)
				return pwr_dev;
		}

	} else {
		LOG_ERR("%s empty list\n", __func__);
	}

	return NULL;
}

int apu_device_power_suspend(enum DVFS_USER user, int is_suspend)
{
	int ret = 0;
#if !BYPASS_POWER_OFF
	char time_stmp[32];
	struct power_device *pwr_dev = NULL;
#if TIME_PROFILING
	struct profiling_timestamp power_profiling;

	power_profiling.begin = sched_clock();
#endif
	pwr_dev = find_out_device_by_user(user);
	if (pwr_dev == NULL) {
		LOG_ERR("%s fail, dev of user %d is NULL\n", __func__, user);
		return -1;
	}

	LOG_DBG("%s waiting for lock, user = %d\n", __func__, user);
	mutex_lock(&power_ctl_mtx);

	if (is_suspend)
		g_pm_procedure = 1;

	if (apusys_power_broken) {
		mutex_unlock(&power_ctl_mtx);
		hal_config_power(PWR_CMD_DUMP_FAIL_STATE, VPU0, NULL);
		LOG_ERR(
		"APUPWR_BROKEN, user:%d fail to pwr off, is_suspend:%d\n",
							user, is_suspend);
		return -ENODEV;
	}

	if (pwr_dev->is_power_on == 0) {
		mutex_unlock(&power_ctl_mtx);
		LOG_ERR(
		"APUPWR_OFF_FAIL, not allow user:%d to pwr off twice, is_suspend:%d\n",
							user, is_suspend);
		return -ECANCELED;
	}

	LOG_DBG("%s for user:%d, cnt:%d, is_suspend:%d\n", __func__,
				user, power_callback_counter, is_suspend);

	if (!g_pm_procedure)
		apu_pwr_wake_lock();

	power_callback_counter--;
	if (power_callback_counter == 0) {
		// call passive power off function list
#if !BYPASS_POWER_CTL
		power_callback_caller(0);
#endif
	}

	// for debug
	// dump_stack();
#if !BYPASS_POWER_CTL
	ret = apusys_power_off(user);
#endif
	if (!ret) {
		pwr_dev->is_power_on = 0;
	} else {
		apusys_power_broken = 1;
		power_callback_counter++;
	}

	if (!g_pm_procedure)
		apu_pwr_wake_unlock();

	mutex_unlock(&power_ctl_mtx);

	// for pwr saving, get pwr info in case pwr ctl fail or enable debug log
	if (g_pwr_log_level >= APUSYS_PWR_LOG_WARN || ret) {
		/* prepare time stamp format */
		memset(time_stmp, 0, sizeof(time_stmp));
		print_time(apu_get_power_info(0), time_stmp);
		LOG_PM(
		"%s for user:%d, ret:%d, cnt:%d, is_suspend:%d, info_id: %s\n",
				__func__, user, ret,
				power_callback_counter, is_suspend,
				ret ? 0 : time_stmp);
	}
#else
	LOG_WRN("%s by user:%d bypass\n", __func__, user);
#endif // BYPASS_POWER_OFF

	if (ret) {
		hal_config_power(PWR_CMD_DUMP_FAIL_STATE, VPU0, NULL);
#ifndef APUSYS_POWER_BRINGUP
		apusys_reg_dump();
#endif
		apu_aee_warn("APUPWR_OFF_FAIL", "user:%d, is_suspend:%d\n",
							user, is_suspend);
		return -ENODEV;
	} else {
#if TIME_PROFILING
		power_profiling.end = sched_clock();
		apu_profiling(&power_profiling, __func__);
#endif
		return 0;
	}
}
EXPORT_SYMBOL(apu_device_power_suspend);

int apu_device_power_off(enum DVFS_USER user)
{
	return apu_device_power_suspend(user, 0);
}
EXPORT_SYMBOL(apu_device_power_off);

int apu_device_power_on(enum DVFS_USER user)
{
	struct power_device *pwr_dev = NULL;
	int ret = 0;
	char time_stmp[32];
#if TIME_PROFILING
	struct profiling_timestamp power_profiling;

	power_profiling.begin = sched_clock();
#endif
	pwr_dev = find_out_device_by_user(user);
	if (pwr_dev == NULL) {
		LOG_ERR("%s fail, dev of user %d is NULL\n", __func__, user);
		return -1;
	}

	LOG_DBG("%s waiting for lock, user:%d\n", __func__, user);
	mutex_lock(&power_ctl_mtx);

	g_pm_procedure = 0;

	if (apusys_power_broken) {
		mutex_unlock(&power_ctl_mtx);
		hal_config_power(PWR_CMD_DUMP_FAIL_STATE, VPU0, NULL);
		LOG_ERR("APUPWR_BROKEN, user:%d fail to pwr on\n", user);
		return -ENODEV;
	}

	if (pwr_dev->is_power_on == 1) {
		mutex_unlock(&power_ctl_mtx);
#if !BYPASS_POWER_OFF
		LOG_ERR("APUPWR_ON_FAIL, not allow user:%d to pwr on twice\n",
									user);
		return -ECANCELED;
#else
		LOG_WRN("%s by user:%d bypass\n", __func__, user);
		return 0;
#endif
	}

	LOG_DBG("%s for user:%d, cnt:%d\n", __func__,
						user, power_callback_counter);

	if (!g_pm_procedure)
		apu_pwr_wake_lock();
	// for debug
	// dump_stack();
#if !BYPASS_POWER_CTL
	ret = apusys_power_on(user);
#endif
	if (!ret) {
		pwr_dev->is_power_on = 1;
		power_callback_counter++;
	} else {
		apusys_power_broken = 1;
	}

	if (power_callback_counter == 1) {
		// call passive power on function list
#if !BYPASS_POWER_CTL
		power_callback_caller(1);
#endif
	}

	if (!g_pm_procedure)
		apu_pwr_wake_unlock();

	mutex_unlock(&power_ctl_mtx);

	// for pwr saving, get pwr info in case pwr ctl fail or enable debug log
	if (g_pwr_log_level >= APUSYS_PWR_LOG_WARN || ret) {
		/* prepare time stamp format */
		memset(time_stmp, 0, sizeof(time_stmp));
		print_time(apu_get_power_info(0), time_stmp);
		LOG_PM("%s for user:%d, ret:%d, cnt:%d, info_id: %s\n",
				__func__, user, ret, power_callback_counter,
				ret ? 0 : time_stmp);
	}

	if (ret) {
		hal_config_power(PWR_CMD_DUMP_FAIL_STATE, VPU0, NULL);
#ifndef APUSYS_POWER_BRINGUP
		apusys_reg_dump();
#endif
		apu_aee_warn("APUPWR_ON_FAIL", "user:%d\n", user);
		return -ENODEV;
	} else {
#if TIME_PROFILING
		power_profiling.end = sched_clock();
		apu_profiling(&power_profiling, __func__);
#endif
		return 0;
	}
}
EXPORT_SYMBOL(apu_device_power_on);

int apu_power_device_register(enum DVFS_USER user, struct platform_device *pdev)
{
	struct power_device *pwr_dev = NULL;

	pwr_dev = kzalloc(sizeof(struct power_device), GFP_KERNEL);

	if (pwr_dev == NULL) {
		LOG_ERR("%s fail in dvfs user %d\n", __func__, user);
		return -1;
	}

	pwr_dev->dev_usr = user;
	pwr_dev->is_power_on = 0;
	pwr_dev->pdev = pdev;

	/* add to device link list */
	mutex_lock(&power_device_list_mtx);

	list_add_tail(&pwr_dev->list, &power_device_list);
// call apusys_power_init in probe
#if 0
	if (apu_power_counter == 0) {
		// prepare clock and get regulator handle
		apusys_power_init(user, (void *)&init_power_data);
	}
#endif
	apu_power_counter++;

	mutex_unlock(&power_device_list_mtx);

	LOG_INF("%s add dvfs user %d success\n", __func__, user);

	return 0;
}
EXPORT_SYMBOL(apu_power_device_register);

int apu_power_callback_device_register(enum POWER_CALLBACK_USER user,
void (*power_on_callback)(void *para), void (*power_off_callback)(void *para))
{
	struct power_callback_device *pwr_dev = NULL;

	pwr_dev = kzalloc(sizeof(struct power_callback_device), GFP_KERNEL);

	if (pwr_dev == NULL) {
		LOG_ERR("%s fail in power callback user %d\n", __func__, user);
		return -1;
	}

	pwr_dev->power_callback_usr = user;
	pwr_dev->power_on_callback = power_on_callback;
	pwr_dev->power_off_callback = power_off_callback;

	/* add to device link list */
	mutex_lock(&power_device_list_mtx);

	list_add_tail(&pwr_dev->list, &power_callback_device_list);

	mutex_unlock(&power_device_list_mtx);

	LOG_INF("%s add power callback user %d success\n", __func__, user);

	return 0;
}
EXPORT_SYMBOL(apu_power_callback_device_register);

void apu_power_device_unregister(enum DVFS_USER user)
{
	struct power_device *pwr_dev = find_out_device_by_user(user);

	mutex_lock(&power_device_list_mtx);

	apu_power_counter--;
	if (apu_power_counter == 0)
		apusys_power_uninit(user);

	/* remove from device link list */
	list_del_init(&pwr_dev->list);
	kfree(pwr_dev);

	mutex_unlock(&power_device_list_mtx);

	LOG_INF("%s remove dvfs user %d success\n", __func__, user);
}
EXPORT_SYMBOL(apu_power_device_unregister);

void apu_power_callback_device_unregister(enum POWER_CALLBACK_USER user)
{
	struct power_callback_device *pwr_dev
			= find_out_callback_device_by_user(user);

	mutex_lock(&power_device_list_mtx);

	/* remove from device link list */
	list_del_init(&pwr_dev->list);
	kfree(pwr_dev);

	mutex_unlock(&power_device_list_mtx);

	LOG_INF("%s remove power callback user %d success\n", __func__, user);
}
EXPORT_SYMBOL(apu_power_callback_device_unregister);

static void d_work_power_info_func(struct work_struct *work)
{
	struct apu_power_info info = {0};

	spin_lock(&power_info_lock);
	info.id = power_info_id;
	info.force_print = power_info_force_print;
	spin_unlock(&power_info_lock);

	info.type = 1;
	hal_config_power(PWR_CMD_GET_POWER_INFO, VPU0, &info);
}

#if DEFAULT_POWER_ON
static void default_power_on_func(void)
{
	int dev_id = 0;

	LOG_ERR("### apusys power on all device ###\n");

	for (dev_id = 0 ; dev_id < APUSYS_DVFS_USER_NUM ; dev_id++) {
		apu_power_counter++;
		apusys_power_on(dev_id);
		udelay(200);
	}

	apu_get_power_info(1);
}
#endif

static void d_work_power_init_func(struct work_struct *work)
{
	int ret = 0;

	ret = apusys_power_init(VPU0, (void *)&init_power_data);
	if (ret == -EPROBE_DEFER) {
		INIT_DEFERRABLE_WORK(&d_work_power, d_work_power_init_func);
		queue_delayed_work(wq, &d_work_power,
				   msecs_to_jiffies(100));
		return;
	}
#if DEFAULT_POWER_ON
	default_power_on_func();
#endif // DEFAULT_POWER_ON
}

static int apusys_power_task(void *arg)
{
	int keep_loop = 0;
	struct apu_power_info info = {0};
	unsigned long rem_nsec;

	set_current_state(TASK_INTERRUPTIBLE);

	LOG_INF("%s first time wakeup and enter sleep now\n", __func__);
	schedule();

	for (;;) {

		if (kthread_should_stop())
			break;
#if 1
		mutex_lock(&power_opp_mtx);
		keep_loop = apusys_check_opp_change();
		mutex_unlock(&power_opp_mtx);
#else
		keep_loop = 1;
		mdelay(1000);
#endif
		if (keep_loop) {
			timestamp = sched_clock();
			info.id = timestamp;
			info.type = 0;
			// call dvfs API and bring timestamp to id
			rem_nsec = do_div(timestamp, 1000000000);
			LOG_INF("%s call DVFS handler, id:[%5lu.%06lu]\n",
				__func__,
				(unsigned long)timestamp, rem_nsec / 1000);

			apusys_dvfs_policy(info.id);
			hal_config_power(PWR_CMD_GET_POWER_INFO, VPU0, &info);
			#if SUPPORT_VCORE_TO_IPUIF
			apusys_ipuif_opp_change();
			#endif
			#if DVFS_ASSERTION_CHECK
			apu_power_assert_check(&info);
			#endif
			#ifdef APUPWR_TASK_DEBOUNCE
			task_debounce();
			#endif
		} else {
			LOG_DBG("%s enter sleep\n", __func__);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
		}

	}

	LOG_WRN("%s task stop\n", __func__);
	return 0;
}

void apu_device_set_opp(enum DVFS_USER user, uint8_t opp)
{
#ifdef MTK_FPGA_PORTING
	LOG_WRN("%s FPGA porting bypass DVFS\n", __func__);
#else

#if !BYPASS_DVFS_CTL
	if (user >= 0 && user < APUSYS_DVFS_USER_NUM
		&& opp < APUSYS_MAX_NUM_OPPS) {

		mutex_lock(&power_opp_mtx);

		// set opp value
		apusys_set_opp(user, opp);

		// change regulator and clock if need
		wake_up_process(power_task_handle);

		mutex_unlock(&power_opp_mtx);
	}
#else
	LOG_WRN("%s bypass since not support DVFS\n", __func__);
#endif

#endif
}
EXPORT_SYMBOL(apu_device_set_opp);

/**
 * apu_profiling() - Brief description of apu_profiling.
 * @profile: struct profiling_timestamp with begin/end timestamp.
 * @tag: the string need to print prefix.
 *
 * Calcualte abs(end-begin) and show result in unit of us with "tag" prefix.
 *
 * Return: void
 **/
void apu_profiling(struct profiling_timestamp *profile, const char *tag)
{
	u64 nanosec = 0;
	u64 time = 0;

	time = abs(profile->end - profile->begin),
	nanosec = do_div(time, 1000000000);
	pr_info("%s: %s take %lu (us)\n", __func__, tag,
		((unsigned long)nanosec / 1000));
}
EXPORT_SYMBOL(apu_profiling);

void event_trigger_dvfs_policy(void)
{
	wake_up_process(power_task_handle);
}

static int apu_power_probe(struct platform_device *pdev)
{
	int ret = 0;
	int err = 0;

	if (!apusys_power_check())
		return 0;

	ret = init_platform_resource(pdev, &init_power_data);

	if (ret)
		goto err_exit;

	wq = create_workqueue("apu_pwr_drv_wq");
	if (IS_ERR(wq)) {
		LOG_ERR("%s create power driver wq fail\n", __func__);
		goto err_exit;
	}

	power_task_handle = kthread_create(apusys_power_task,
						(void *)NULL, "apu_pwr_policy");
	if (IS_ERR(power_task_handle)) {
		LOG_ERR("%s create power task fail\n", __func__);
		goto err_exit;
	}

	mutex_init(&power_device_list_mtx);
	mutex_init(&power_ctl_mtx);
	mutex_init(&power_opp_mtx);
	spin_lock_init(&power_info_lock);
	apu_pwr_wake_init();

	queue_work(wq, &d_work_power_init);
	queue_work(wq, &d_work_power_info);
	wake_up_process(power_task_handle);

	#if AUTO_BUCK_OFF_SUSPEND
	// buck auto power off in suspend
	pmic_buck_vproc1_lp(SRCLKEN0, 1, 1, HW_OFF);
	pmic_buck_vproc2_lp(SRCLKEN0, 1, 1, HW_OFF);
	pmic_ldo_vsram_md_lp(SRCLKEN0, 1, 1, HW_OFF);
	#endif

	#if AUTO_BUCK_OFF_DEEPIDLE
	// buck auto power off in deeep idle
	pmic_buck_vproc1_lp(SRCLKEN2, 1, 1, HW_OFF);
	pmic_buck_vproc2_lp(SRCLKEN2, 1, 1, HW_OFF);
	pmic_ldo_vsram_md_lp(SRCLKEN2, 1, 1, HW_OFF);
	#endif

	apusys_power_debugfs_init();
	#if defined(CONFIG_MACH_MT6877)
	apusys_power_create_procfs();
	#endif
	#ifdef APUPWR_TAG_TP
	apupwr_init_drv_tags();
	#endif

	return 0;

err_exit:
	if (power_task_handle != NULL) {
		kfree(power_task_handle);
		power_task_handle = NULL;
	}

	if (wq != NULL) {
		flush_workqueue(wq);
		destroy_workqueue(wq);
		kfree(wq);
		wq = NULL;
	}

	return err;
}

static int apu_power_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct hal_param_pm pm;
	enum DVFS_USER user;

	LOG_PM("%s begin\n", __func__);

	if (power_on_off_stress) {
		LOG_PM("%s, power_on_off_stress: %d\n",
				__func__, power_on_off_stress);
		for (user = VPU0; user < APUSYS_DVFS_USER_NUM; user++)
			apu_device_power_suspend(user, 1);
	}

	mutex_lock(&power_ctl_mtx);
	pm.is_suspend = 1;
	hal_config_power(PWR_CMD_PM_HANDLER, VPU0, &pm);
	mutex_unlock(&power_ctl_mtx);

	LOG_PM("%s end\n", __func__);
	return 0;
}

static int apu_power_resume(struct platform_device *pdev)
{
	struct hal_param_pm pm;
	enum DVFS_USER user;

	LOG_PM("%s begin\n", __func__);

	mutex_lock(&power_ctl_mtx);
	pm.is_suspend = 0;
	hal_config_power(PWR_CMD_PM_HANDLER, VPU0, &pm);
	g_pm_procedure = 0;
	mutex_unlock(&power_ctl_mtx);

	if (power_on_off_stress) {
		LOG_PM("%s, power_on_off_stress: %d\n",
				__func__, power_on_off_stress);
		for (user = VPU0; user < APUSYS_DVFS_USER_NUM; user++)
			apu_device_power_on(user);
	}

	LOG_PM("%s end\n", __func__);
	return 0;
}

static int apu_power_remove(struct platform_device *pdev)
{
	apusys_power_debugfs_exit();
	#ifdef APUPWR_TAG_TP
	apupwr_exit_drv_tags();
	#endif

	if (power_task_handle)
		kthread_stop(power_task_handle);

	mutex_destroy(&power_opp_mtx);
	mutex_destroy(&power_ctl_mtx);
	mutex_destroy(&power_device_list_mtx);

	if (wq) {
		flush_workqueue(wq);
		destroy_workqueue(wq);
		kfree(wq);
		wq = NULL;
	}

	return 0;
}


static const struct of_device_id apu_power_of_match[] = {
	{ .compatible = "mediatek,apusys_power" },
	{ /* end of list */},
};


static struct platform_driver apu_power_driver = {
	.probe	= apu_power_probe,
	.remove	= apu_power_remove,
	.suspend = apu_power_suspend,
	.resume = apu_power_resume,
	.driver = {
		.name = "apusys_power",
		.of_match_table = apu_power_of_match,
	},
};

static int __init apu_power_drv_init(void)
{
	return platform_driver_register(&apu_power_driver);
}

module_init(apu_power_drv_init)

static void __exit apu_power_drv_exit(void)
{
	platform_driver_unregister(&apu_power_driver);
}
module_exit(apu_power_drv_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("apu power driver");

