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

#include "apu_log.h"
#include "apusys_power_ctl.h"
#include "apusys_power_cust.h"
#include "apusys_power_debug.h"
#include "apu_platform_resource.h"
#include "hal_config_power.h"
#include "apu_power_api.h"
#include "../../../pmic/include/pmic_api_buck.h"

int g_pwr_log_level = APUSYS_PWR_LOG_INFO;
int g_pm_procedure;
static int apu_power_counter;
static int apusys_power_broken;
static int power_on_off_stress;
static uint64_t power_info_id;

bool apusys_power_check(void)
{
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

#ifdef CONFIG_PM_WAKELOCKS
struct wakeup_source pwr_wake_lock;
#else
struct wake_lock pwr_wake_lock;
#endif

static void apu_pwr_wake_lock(void)
{
#ifdef CONFIG_PM_WAKELOCKS
	__pm_stay_awake(&pwr_wake_lock);
#else
	wake_lock(&pwr_wake_lock);
#endif
}

static void apu_pwr_wake_unlock(void)
{
#ifdef CONFIG_PM_WAKELOCKS
	__pm_relax(&pwr_wake_lock);
#else
	wake_unlock(&pwr_wake_lock);
#endif
}

static void apu_pwr_wake_init(void)
{
#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_init(&pwr_wake_lock, "apupwr_wakelock");
#else
	wake_lock_init(&pwr_wake_lock, WAKE_LOCK_SUSPEND, "apupwr_wakelock");
#endif
}

uint64_t get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);
}

uint64_t apu_get_power_info(void)
{
	spin_lock(&power_info_lock);
	power_info_id = get_current_time_us();
	spin_unlock(&power_info_lock);

	queue_work(wq, &d_work_power_info);

	return power_info_id;
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
	struct power_device *pwr_dev = find_out_device_by_user(user);

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

	udelay(100);
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
	LOG_PM("%s for user:%d, ret:%d, cnt:%d, is_suspend:%d, info_id: %llu\n",
					__func__, user, ret,
					power_callback_counter, is_suspend,
					apu_get_power_info());
#else
	LOG_WRN("%s by user:%d bypass\n", __func__, user);
#endif // BYPASS_POWER_OFF

	if (ret) {
		apu_aee_warn("APUPWR_OFF_FAIL", "user:%d, is_suspend:%d\n",
							user, is_suspend);
		return -ENODEV;
	} else {
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
	struct power_device *pwr_dev = find_out_device_by_user(user);
	int ret = 0;

	if (pwr_dev == NULL) {
		LOG_ERR("%s fail, dev of user %d is NULL\n", __func__, user);
		return -1;
	}

	LOG_DBG("%s waiting for lock, user:%d\n", __func__, user);
	mutex_lock(&power_ctl_mtx);

	g_pm_procedure = 0;

	if (apusys_power_broken) {
		mutex_unlock(&power_ctl_mtx);
		LOG_ERR("APUPWR_BROKEN, user:%d fail to pwr on\n", user);
		return -ENODEV;
	}

	if (pwr_dev->is_power_on == 1) {
		mutex_unlock(&power_ctl_mtx);
		LOG_ERR("APUPWR_ON_FAIL, not allow user:%d to pwr on twice\n",
									user);
		return -ECANCELED;
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
	LOG_PM("%s for user:%d, ret:%d, cnt:%d, info_id: %llu\n",
				__func__, user, ret, power_callback_counter,
				apu_get_power_info());

	if (ret) {
		apu_aee_warn("APUPWR_ON_FAIL", "user:%d\n", user);
		return -ENODEV;
	} else {
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
	struct apu_power_info info;

	spin_lock(&power_info_lock);
	info.id = power_info_id;
	spin_unlock(&power_info_lock);

	info.type = 1;
	hal_config_power(PWR_CMD_GET_POWER_INFO, VPU0, &info);
}

static void d_work_power_init_func(struct work_struct *work)
{
	apusys_power_init(VPU0, (void *)&init_power_data);

#if DEFAULT_POWER_ON
	default_power_on_func();
#endif // DEFAULT_POWER_ON
}

static void default_power_on_func(void)
{
	LOG_WRN("### apusys power on all device ###\n");
	apusys_power_on(VPU0);
	udelay(200);
	apusys_power_on(VPU1);
	udelay(200);
	apusys_power_on(VPU2);
	udelay(200);
	apusys_power_on(MDLA0);
	udelay(200);
	apusys_power_on(MDLA1);
	udelay(200);
	apu_power_counter++;

	apu_get_power_info();
}

static bool apu_get_conn_power_on_status(void)
{
	if (apu_get_power_on_status(VPU0) == true ||
		apu_get_power_on_status(VPU1) == true ||
		apu_get_power_on_status(VPU2) == true ||
		apu_get_power_on_status(MDLA0) == true ||
		apu_get_power_on_status(MDLA1) == true)
		return true;
	return false;
}

static void apu_power_assert_check(struct apu_power_info *info)
{
	int dsp_freq;
	int dsp1_freq;
	int dsp2_freq;
	int dsp3_freq;
	int dsp5_freq;
	int dsp6_freq;
	int dsp7_freq;
	//int ipuif_freq = apusys_get_dvfs_freq(V_VCORE)/1000;

	int vvpu = info->vvpu * info->dump_div;
	int vmdla = info->vmdla * info->dump_div;
	int vsram = info->vsram * info->dump_div;
	int vcore = info->vcore * info->dump_div;

	if (apu_get_power_on_status(VPU0) == true && info->dsp1_freq != 0) {
		dsp1_freq = apusys_get_dvfs_freq(V_VPU0)/info->dump_div;
		if ((abs(dsp1_freq - info->dsp1_freq) * 100) >
			dsp1_freq * ASSERTION_PERCENTAGE) {
			LOG_WRN("ASSERT dsp1_freq=%d, info->dsp1_freq=%d\n",
				dsp1_freq, info->dsp1_freq);
		}
	}

	if (apu_get_power_on_status(VPU1) == true && info->dsp2_freq != 0) {
		dsp2_freq = apusys_get_dvfs_freq(V_VPU1)/info->dump_div;
		if ((abs(dsp2_freq - info->dsp2_freq) * 100) >
			dsp2_freq * ASSERTION_PERCENTAGE) {
			LOG_WRN("ASSERT dsp2_freq=%d, info->dsp2_freq=%d\n",
				dsp2_freq, info->dsp2_freq);
		}
	}

	if (dvfs_user_support(VPU2) && apu_get_power_on_status(VPU2) == true &&
		info->dsp3_freq != 0) {
		dsp3_freq = apusys_get_dvfs_freq(V_VPU2)/info->dump_div;
		if ((abs(dsp3_freq - info->dsp3_freq) * 100) >
			dsp3_freq * ASSERTION_PERCENTAGE) {
			LOG_WRN("ASSERT dsp3_freq=%d, info->dsp3_freq=%d\n",
				dsp3_freq, info->dsp3_freq);
		}
	}

	if ((apu_get_power_on_status(MDLA0) == true ||
		(dvfs_user_support(MDLA1) &&
		apu_get_power_on_status(MDLA1) == true)) &&
		info->dsp6_freq != 0) {
		dsp5_freq = apusys_get_dvfs_freq(V_MDLA0)/info->dump_div;
		dsp6_freq = apusys_get_dvfs_freq(V_MDLA1)/info->dump_div;
		if (((abs(dsp5_freq - info->dsp6_freq) * 100) >
			dsp5_freq * ASSERTION_PERCENTAGE) &&
			((abs(dsp6_freq - info->dsp6_freq) * 100) >
			dsp6_freq * ASSERTION_PERCENTAGE)) {
			LOG_WRN("ASSERT dsp5=%d, dsp6=%d, info->dsp6_freq=%d\n",
				dsp5_freq, dsp6_freq, info->dsp6_freq);
		}
	}


if (apu_get_conn_power_on_status() == true) {
	if (info->dsp_freq != 0) {
		dsp_freq = apusys_get_dvfs_freq(V_APU_CONN)/info->dump_div;
		if ((abs(dsp_freq - info->dsp_freq) * 100) >
			dsp_freq * ASSERTION_PERCENTAGE) {
			LOG_WRN("ASSERT dsp_freq=%d, info->dsp_freq=%d\n",
				dsp_freq, info->dsp_freq);
		}
	}

	if (info->dsp7_freq != 0) {
		dsp7_freq = apusys_get_dvfs_freq(V_TOP_IOMMU)/info->dump_div;
		if ((abs(dsp7_freq - info->dsp7_freq) * 100) >
			dsp7_freq * ASSERTION_PERCENTAGE) {
			LOG_WRN("ASSERT dsp7_freq=%d, info->dsp7_freq=%d\n",
				dsp7_freq, info->dsp7_freq);
		}
	}
}


#if 0	// dvfs don't use vcore
		if (abs(ipuif_freq - info->ipuif_freq) >
			ipuif_freq * ASSERTION_PERCENTAGE) {
			apu_aee_warn("VCORE",
				"ipuif_freq=%d, info->ipuif_freq=%d\n",
				ipuif_freq, info->ipuif_freq)
		}
#endif

	if (vvpu == DVFS_VOLT_00_575000_V && vmdla >= DVFS_VOLT_00_800000_V) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d\n",
			vvpu, vmdla);
	}

	if (vmdla == DVFS_VOLT_00_575000_V && vvpu >= DVFS_VOLT_00_800000_V) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d\n",
			vvpu, vmdla);
	}

	if (vcore == DVFS_VOLT_00_575000_V && vvpu >= DVFS_VOLT_00_800000_V) {
		LOG_WRN("ASSERT vvpu=%d, vcore=%d\n",
			vvpu, vcore);
	}

	if ((vvpu > VSRAM_TRANS_VOLT || vmdla > VSRAM_TRANS_VOLT)
		&& vsram == VSRAM_LOW_VOLT) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d, vsram=%d\n",
			vvpu, vmdla, vsram);
	}

	if ((vvpu < VSRAM_TRANS_VOLT && vmdla < VSRAM_TRANS_VOLT)
		&& vsram == VSRAM_HIGH_VOLT) {
		LOG_WRN("ASSERT vvpu=%d, vmdla=%d, vsram=%d\n",
			vvpu, vmdla, vsram);
	}
}

static int apusys_power_task(void *arg)
{
	int keep_loop = 0;
	struct	apu_power_info info;

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
			timestamp = get_current_time_us();
			LOG_INF("%s call DVFS handler, id:%llu\n",
							__func__, timestamp);
			// call dvfs API and bring timestamp to id
			apusys_dvfs_policy(timestamp);
			info.id = timestamp;
			info.type = 0;
			hal_config_power(PWR_CMD_GET_POWER_INFO, VPU0, &info);
			#if DVFS_ASSERTION_CHECK
			apu_power_assert_check(&info);
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

static int apu_power_probe(struct platform_device *pdev)
{
	int ret = 0;
	int err;

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

	queue_work(wq, &d_work_power_init);
	queue_work(wq, &d_work_power_info);

	power_task_handle = kthread_create(apusys_power_task,
						(void *)NULL, "apu_pwr_policy");
	if (IS_ERR(power_task_handle)) {
		LOG_ERR("%s create power task fail\n", __func__);
		goto err_exit;
	}

	wake_up_process(power_task_handle);
	mutex_init(&power_device_list_mtx);
	mutex_init(&power_ctl_mtx);
	mutex_init(&power_opp_mtx);
	spin_lock_init(&power_info_lock);

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

	apu_pwr_wake_init();
	apusys_power_debugfs_init();

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

int apu_power_power_stress(int type, int device, int opp)
{
	int id = 0;
	int i = 0, j = 0;
	int m = 0, n = 0, o = 0;
	int count = 0;
	int loop = opp; // for type = 5
	struct	apu_power_info info;

	LOG_WRN("%s begin with type %d +++\n", __func__, type);

	if (type < 0 || type >= 10) {
		LOG_ERR("%s err with type = %d\n", __func__, type);
		return -1;
	}

	if (device != 9 && (device < 0 || device >= APUSYS_DVFS_USER_NUM)) {
		LOG_ERR("%s err with device = %d\n", __func__, device);
		return -1;
	}

	if (type != 5 && (opp < 0 || opp >= APUSYS_MAX_NUM_OPPS)) {
		LOG_ERR("%s err with opp = %d\n", __func__, opp);
		return -1;
	}

	if (apu_power_counter == 0) {
		// call apusys_power_init in probe
		// prepare clock and get regulator handle
		// apusys_power_init(VPU0, (void *)&init_power_data);
		apu_power_counter++;
		LOG_WRN("%s apu_power_counter++ for debug\n", __func__);
	} else {
		LOG_WRN("%s we have %d devices for debug\n",
					__func__, apu_power_counter);
	}

	switch (type) {
	case 0: // config opp
		if (device == 9) { // all devices
			for (id = 0 ; id < APUSYS_DVFS_USER_NUM ; id++) {
				if (dvfs_power_domain_support(id) == false)
					continue;
				apu_device_set_opp(id, opp);
			}
		} else {
			apu_device_set_opp(device, opp);
		}

		udelay(100);

		break;

	case 1: // config power on

		if (device == 9) { // all devices
			for (id = 0 ; id < APUSYS_DVFS_USER_NUM ; id++) {
				if (dvfs_power_domain_support(id) == false)
					continue;

				apu_device_power_on(id);
			}
		} else {
			apu_device_power_on(device);
		}
		break;

	case 2: // config power off
		if (device == 9) { // all devices
			for (id = 0 ; id < APUSYS_DVFS_USER_NUM ; id++) {
				if (dvfs_power_domain_support(id) == false)
					continue;

				apu_device_power_off(id);
			}
		} else {
			apu_device_power_off(device);
		}
		break;
	case 4: // power driver debug func
		hal_config_power(PWR_CMD_DEBUG_FUNC, VPU0, NULL);
		break;
	case 5:	// dvfs all combination test , opp = run count
for (loop = 0; loop < count; loop++) {
	for (i = 0 ; i < APUSYS_MAX_NUM_OPPS ; i++) {
		apusys_set_opp(VPU0, i);
		for (j = 0 ; j < APUSYS_MAX_NUM_OPPS; j++) {
			apusys_set_opp(VPU1, j);
			for (m = 0 ; m < APUSYS_MAX_NUM_OPPS; m++) {
				apusys_set_opp(VPU2, m);
				for (n = 0 ; n < APUSYS_MAX_NUM_OPPS; n++) {
					apusys_set_opp(MDLA0, n);
				for (o = 0 ; o < APUSYS_MAX_NUM_OPPS; o++) {
					count++;
			LOG_WRN("## dvfs conbinational test, count = %d ##\n",
						count);
					apusys_set_opp(MDLA1, o);
					timestamp = get_current_time_us();
					info.id = timestamp;
					info.type = 0;

					apusys_dvfs_policy(timestamp);

					hal_config_power(PWR_CMD_GET_POWER_INFO,
						VPU0, &info);
					apu_power_assert_check(&info);
					udelay(100);
					}
				}
			}
		}
	}
}
		break;
	case 7: // power on/off suspend stress
		if (power_on_off_stress == 0)
			power_on_off_stress = 1;
		else
			power_on_off_stress = 0;
		break;
	case 8: // dump power info and options
		LOG_WRN("%s, BYPASS_POWER_OFF : %d\n",
					__func__, BYPASS_POWER_OFF);
		LOG_WRN("%s, BYPASS_POWER_CTL : %d\n",
					__func__, BYPASS_POWER_CTL);
		LOG_WRN("%s, BYPASS_DVFS_CTL : %d\n",
					__func__, BYPASS_DVFS_CTL);
		LOG_WRN("%s, DEFAULT_POWER_ON : %d\n",
					__func__, DEFAULT_POWER_ON);
		LOG_WRN("%s, AUTO_BUCK_OFF_SUSPEND : %d\n",
					__func__, AUTO_BUCK_OFF_SUSPEND);
		LOG_WRN("%s, AUTO_BUCK_OFF_DEEPIDLE : %d\n",
					__func__, AUTO_BUCK_OFF_DEEPIDLE);
		LOG_WRN("%s, VCORE_DVFS_SUPPORT : %d\n",
					__func__, VCORE_DVFS_SUPPORT);
		LOG_WRN("%s, ASSERTION_PERCENTAGE : %d\n",
					__func__, ASSERTION_PERCENTAGE);
#ifdef AGING_MARGIN
		LOG_WRN("%s, AGING_MARGIN : %d\n",
					__func__, AGING_MARGIN);
#endif
		LOG_WRN("%s, BINNING_VOLTAGE_SUPPORT : %d\n",
					__func__, BINNING_VOLTAGE_SUPPORT);
		LOG_WRN("%s, g_pwr_log_level : %d\n",
					__func__, g_pwr_log_level);
		LOG_WRN("%s, power_on_off_stress : %d\n",
					__func__, power_on_off_stress);
		apu_get_power_info();
		break;
	case 9: // config to force power on
		default_power_on_func();
		break;
	default:
		LOG_WRN("%s invalid type %d !\n", __func__, type);
	}

	LOG_WRN("%s end with type %d ---\n", __func__, type);

	return 0;
}

static int apu_power_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct hal_param_pm pm;

	LOG_PM("%s begin\n", __func__);

	if (power_on_off_stress) {
		LOG_PM("%s, power_on_off_stress: %d\n",
				__func__, power_on_off_stress);
		apu_device_power_suspend(0, 1);
		apu_device_power_suspend(1, 1);
		apu_device_power_suspend(2, 1);
		apu_device_power_suspend(3, 1);
		apu_device_power_suspend(4, 1);
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

	LOG_PM("%s begin\n", __func__);

	mutex_lock(&power_ctl_mtx);
	pm.is_suspend = 0;
	hal_config_power(PWR_CMD_PM_HANDLER, VPU0, &pm);
	g_pm_procedure = 0;
	mutex_unlock(&power_ctl_mtx);

	if (power_on_off_stress) {
		LOG_PM("%s, power_on_off_stress: %d\n",
				__func__, power_on_off_stress);
		apu_device_power_on(0);
		apu_device_power_on(1);
		apu_device_power_on(2);
		apu_device_power_on(3);
		apu_device_power_on(4);
	}

	LOG_PM("%s end\n", __func__);
	return 0;
}

static int apu_power_remove(struct platform_device *pdev)
{
	apusys_power_debugfs_exit();

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

