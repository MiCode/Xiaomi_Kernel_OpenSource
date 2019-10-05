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

#define APUSYS_POWER_ENABLE	(1)
#define FOR_BRING_UP		(0)
#define SUPPORT_DVFS		(1)

#if APUSYS_POWER_ENABLE
static struct hal_param_init_power init_power_data;
#endif

static int apu_power_counter;

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

#if FOR_BRING_UP

int apu_power_device_register(enum DVFS_USER user, struct platform_device *dev)
{ apu_power_counter++; return 0; }
EXPORT_SYMBOL(apu_power_device_register);

void apu_power_device_unregister(enum DVFS_USER user)
{ apu_power_counter--; }
EXPORT_SYMBOL(apu_power_device_unregister);

int apu_device_power_on(enum DVFS_USER user)
{ return 0; }
EXPORT_SYMBOL(apu_device_power_on);

int apu_device_power_off(enum DVFS_USER user)
{ return 0; }
EXPORT_SYMBOL(apu_device_power_off);

void apu_device_set_opp(enum DVFS_USER user, uint8_t opp)
{}
EXPORT_SYMBOL(apu_device_set_opp);

bool apu_get_power_on_status(enum DVFS_USER user)
{ return 1; }
EXPORT_SYMBOL(apu_get_power_on_status);

void apu_power_on_callback(void)
{}
EXPORT_SYMBOL(apu_power_on_callback);

int apu_power_callback_device_register(enum POWER_CALLBACK_USER user,
				void (*power_on_callback)(void *para),
				void (*power_off_callback)(void *para))
{ return 0; }
EXPORT_SYMBOL(apu_power_callback_device_register);

void apu_power_callback_device_unregister(enum POWER_CALLBACK_USER user)
{}
EXPORT_SYMBOL(apu_power_callback_device_unregister);

void apu_power_reg_dump(void)
{
	hal_config_power(PWR_CMD_REG_DUMP, VPU0, NULL);
}
EXPORT_SYMBOL(apu_power_reg_dump);

void apu_get_power_info(void)
{
	struct hal_param_pwr_info info;

	info.id = 0;
	hal_config_power(PWR_CMD_GET_POWER_INFO, VPU0, &info);
}
EXPORT_SYMBOL(apu_get_power_info);

#else

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
static struct mutex power_opp_mtx;
static int power_callback_counter;
static struct task_struct *power_task_handle;
static uint64_t timestamp;

uint64_t get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);
}

static int apusys_power_task(void *arg)
{
	int keep_loop = 0;

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
		} else {
			LOG_INF("%s enter sleep\n", __func__);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
		}

	}

	LOG_INF("%s task stop\n", __func__);
	return 0;
}

void apu_get_power_info(void)
{
	struct hal_param_pwr_info info;

	info.id = timestamp;

	mutex_lock(&power_device_list_mtx);

	if (apu_power_counter != 0)
		hal_config_power(PWR_CMD_GET_POWER_INFO, VPU0, &info);
	else
		LOG_WRN("%s apu_power_counter = 0 , bypss\n", __func__);

	mutex_unlock(&power_device_list_mtx);
}
EXPORT_SYMBOL(apu_get_power_info);


void apu_power_reg_dump(void)
{
	mutex_lock(&power_device_list_mtx);

	if (apu_power_counter != 0)
		hal_config_power(PWR_CMD_REG_DUMP, VPU0, NULL);
	else
		LOG_WRN("%s apu_power_counter = 0 , bypss\n", __func__);

	mutex_unlock(&power_device_list_mtx);
}
EXPORT_SYMBOL(apu_power_reg_dump);


static struct power_device *find_out_device_by_user(enum DVFS_USER user)
{
	struct power_device *pwr_dev = NULL;

	mutex_lock(&power_device_list_mtx);

	if (!list_empty(&power_device_list)) {

		list_for_each_entry(pwr_dev,
				&power_device_list, list) {

			if (pwr_dev && pwr_dev->dev_usr == user) {
				mutex_unlock(&power_device_list_mtx);
				return pwr_dev;
			}
		}

	} else {
		LOG_ERR("%s empty list\n", __func__);
	}

	mutex_unlock(&power_device_list_mtx);
	return NULL;
}


bool apu_get_power_on_status(enum DVFS_USER user)
{
	bool power_on_status;
	struct power_device *pwr_dev = find_out_device_by_user(user);

	mutex_lock(&power_device_list_mtx);

	power_on_status = pwr_dev->is_power_on;

	mutex_unlock(&power_device_list_mtx);

	return power_on_status;
}
EXPORT_SYMBOL(apu_get_power_on_status);

static void power_callback_caller(int power_on)
{
	struct power_callback_device *pwr_dev = NULL;

	LOG_WRN("%s begin (%d)\n", __func__, power_on);

	if (!list_empty(&power_callback_device_list)) {
		list_for_each_entry(pwr_dev,
			&power_callback_device_list, list) {

			LOG_WRN("%s calling %d in state %d\n", __func__,
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

	LOG_WRN("%s end (%d)\n", __func__, power_on);
}

static struct power_callback_device*
find_out_callback_device_by_user(enum POWER_CALLBACK_USER user)
{
	struct power_callback_device *pwr_dev = NULL;

	mutex_lock(&power_device_list_mtx);

	if (!list_empty(&power_callback_device_list)) {

		list_for_each_entry(pwr_dev,
				&power_callback_device_list, list) {

			if (pwr_dev && pwr_dev->power_callback_usr == user) {
				mutex_unlock(&power_device_list_mtx);
				return pwr_dev;
			}
		}

	} else {
		LOG_ERR("%s empty list\n", __func__);
	}

	mutex_unlock(&power_device_list_mtx);
	return NULL;
}

int apu_device_power_off(enum DVFS_USER user)
{
	struct power_device *pwr_dev = find_out_device_by_user(user);

	if (pwr_dev == NULL) {
		LOG_ERR("%s fail, dev of user %d is NULL\n", __func__, user);
		return -1;
	}

	mutex_lock(&power_device_list_mtx);

	if (pwr_dev->is_power_on) {
		LOG_INF("%s for user : %d, cnt : %d\n", __func__,
						user, apu_power_counter);
		power_callback_counter--;
		if (power_callback_counter == 0) {
			// call passive power off function list
			power_callback_caller(0);
		}

		// for debug
		dump_stack();
		// disable clock and set regulator mode to idle (lowest volt)
		apusys_power_off(user);
		pwr_dev->is_power_on = 0;

	} else {
		LOG_INF("%s user %d has already power off, bypass this time!\n",
							__func__, user);
	}

	mutex_unlock(&power_device_list_mtx);

	return 0;
}
EXPORT_SYMBOL(apu_device_power_off);

int apu_device_power_on(enum DVFS_USER user)
{
	struct power_device *pwr_dev = find_out_device_by_user(user);

	if (pwr_dev == NULL) {
		LOG_ERR("%s fail, dev of user %d is NULL\n", __func__, user);
		return -1;
	}

	mutex_lock(&power_device_list_mtx);

	if (!pwr_dev->is_power_on) {
		LOG_INF("%s for user : %d, cnt : %d\n", __func__,
						user, apu_power_counter);
		// for debug
		dump_stack();
		// enable clock and set regulator mode to normal
		apusys_power_on(user);
		pwr_dev->is_power_on = 1;

		if (power_callback_counter == 0) {
			// call passive power on function list
			power_callback_caller(1);
		}
		power_callback_counter++;

	} else {
		LOG_INF("%s user %d has already power on, bypass this time!\n",
								__func__, user);
	}
	mutex_unlock(&power_device_list_mtx);

	return 0;
}
EXPORT_SYMBOL(apu_device_power_on);


void apu_device_set_opp(enum DVFS_USER user, uint8_t opp)
{
#ifdef MTK_FPGA_PORTING
	LOG_WRN("%s FPGA porting bypass DVFS\n", __func__);
#else

#if SUPPORT_DVFS
	if (user >= 0 && user < APUSYS_DVFS_USER_NUM
		&& opp < APUSYS_MAX_NUM_OPPS) {

		// check power on state
		apu_device_power_on(user);

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

	if (apu_power_counter == 0) {
		// prepare clock and get regulator handle
		apusys_power_init(user, (void *)&init_power_data);
	}
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

#endif // FOR_BRING_UP

#if 0
static void d_work_func(void)
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

	apu_power_reg_dump();
}
#endif

static int apu_power_probe(struct platform_device *pdev)
{
#if APUSYS_POWER_ENABLE
	int ret = 0;
	int err;

	if (!apusys_power_check())
		return 0;

	ret = init_platform_resource(pdev, &init_power_data);

	if (ret)
		goto err_exit;

//	apusys_power_init(VPU0, (void *)&init_power_data);
//	d_work_func();

#if !FOR_BRING_UP
	power_task_handle = kthread_create(apusys_power_task,
						(void *)NULL, "apusys_power");
	if (IS_ERR(power_task_handle)) {
		LOG_ERR("%s create power task fail\n", __func__);
		goto err_exit;
	}

	wake_up_process(power_task_handle);
	mutex_init(&power_device_list_mtx);
	mutex_init(&power_opp_mtx);

	apu_power_reg_dump();
#endif // !FOR_BRING_UP

	apusys_power_debugfs_init();

	return 0;

err_exit:
#if !FOR_BRING_UP
	if (power_task_handle != NULL) {
		kfree(power_task_handle);
		power_task_handle = NULL;
	}
#endif // !FOR_BRING_UP
	return err;
#else
	LOG_WRN("%s bypass #########################\n", __func__);
#endif // APUSYS_POWER_ENABLE

	return 0;
}

int apu_power_power_stress(int type, int device, int opp)
{
	static struct mutex power_stress_mtx;
	static int power_stress_dev_pwr_stat[APUSYS_DVFS_USER_NUM] = {0};
	int id = 0;

	mutex_init(&power_stress_mtx);

	LOG_WRN("%s begin with type %d +++\n", __func__, type);

	if (type < 0 || type >= 10) {
		LOG_ERR("%s err with type = %d\n", __func__, type);
		return -1;
	}

	if (device != 9 && (device < 0 || device >= APUSYS_DVFS_USER_NUM)) {
		LOG_ERR("%s err with device = %d\n", __func__, device);
		return -1;
	}

	if (opp < 0 || opp >= APUSYS_MAX_NUM_OPPS) {
		LOG_ERR("%s err with opp = %d\n", __func__, opp);
		return -1;
	}

	mutex_lock(&power_stress_mtx);

	if (apu_power_counter == 0) {
		// prepare clock and get regulator handle
		apusys_power_init(VPU0, (void *)&init_power_data);
		apu_power_counter++;
	}

	switch (type) {
	case 0: // config opp
		if (device == 9) { // all devices
			for (id = 0 ; id < APUSYS_DVFS_USER_NUM ; id++) {
				if (dvfs_power_domain_support(id) == false)
					continue;
				apusys_set_opp(id, opp);
			}
		} else {
			apusys_set_opp(device, opp);
		}

		udelay(100);

		apusys_dvfs_policy(0);
		break;

	case 1: // config power on

		if (device == 9) { // all devices
			for (id = 0 ; id < APUSYS_DVFS_USER_NUM ; id++) {
				if (dvfs_power_domain_support(id) == false)
					continue;
				if (!power_stress_dev_pwr_stat[id]) {
					apusys_power_on(id);
					power_stress_dev_pwr_stat[id] = 1;
				}
			}
		} else {
			if (!power_stress_dev_pwr_stat[device]) {
				apusys_power_on(device);
				power_stress_dev_pwr_stat[device] = 1;
			}
		}
		break;

	case 2: // config power off

		if (device == 9) { // all devices
			for (id = 0 ; id < APUSYS_DVFS_USER_NUM ; id++) {
				if (dvfs_power_domain_support(id) == false)
					continue;
				if (power_stress_dev_pwr_stat[id]) {
					apusys_power_off(id);
					power_stress_dev_pwr_stat[id] = 0;
				}
			}
		} else {
			if (power_stress_dev_pwr_stat[device]) {
				apusys_power_off(device);
				power_stress_dev_pwr_stat[device] = 0;
			}
		}
		break;

	default:
		LOG_WRN("%s invalid type %d !\n", __func__, type);
	}

	apu_get_power_info();

	mutex_unlock(&power_stress_mtx);

	LOG_WRN("%s end with type %d ---\n", __func__, type);

	return 0;
}


static int apu_power_remove(struct platform_device *pdev)
{
	apusys_power_debugfs_exit();

#if !FOR_BRING_UP
	if (power_task_handle)
		kthread_stop(power_task_handle);

	mutex_destroy(&power_opp_mtx);
	mutex_destroy(&power_device_list_mtx);
#endif

	return 0;
}


static const struct of_device_id apu_power_of_match[] = {
	{ .compatible = "mediatek,apusys_power" },
	{ /* end of list */},
};


static struct platform_driver apu_power_driver = {
	.probe	= apu_power_probe,
	.remove	= apu_power_remove,
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

