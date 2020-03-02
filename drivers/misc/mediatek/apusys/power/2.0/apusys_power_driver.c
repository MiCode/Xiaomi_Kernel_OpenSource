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

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/kthread.h>


#include "apu_log.h"
#include "apusys_power_ctl.h"
#include "apusys_power_cust.h"
#include "apusys_power_debug.h"
#include "hal_config_power.h"


#define POWER_ON_DELAY	(100)

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
static int apu_power_counter;
static int power_callback_counter;
static struct task_struct *power_task_handle;
static uint64_t timestamp;

struct hal_param_init_power init_power_data;


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
			LOG_INF("%s call DVFS handler, id:%lu\n",
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

static void power_callback_caller(int power_on)
{
	struct power_callback_device *pwr_dev = NULL;

	LOG_DBG("%s begin (%d)\n", __func__, power_on);
	mutex_lock(&power_device_list_mtx);

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

	mutex_unlock(&power_device_list_mtx);
	LOG_DBG("%s end (%d)\n", __func__, power_on);
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


static int apu_power_probe(struct platform_device *pdev)
{
	int err = 0;
	struct resource *apusys_rpc_res = NULL;
	struct resource *apusys_pcu_res = NULL;
	struct device *apusys_dev = &pdev->dev;

	LOG_INF("%s pdev id = %d name = %s, name = %s\n", __func__,
						pdev->id, pdev->name,
						pdev->dev.of_node->name);
	init_power_data.dev = apusys_dev;

	apusys_rpc_res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "apusys_rpc");
	init_power_data.rpc_base_addr = devm_ioremap_resource(
						apusys_dev, apusys_rpc_res);

	if (IS_ERR((void *)init_power_data.rpc_base_addr)) {
		LOG_ERR("Unable to ioremap apusys_rpc\n");
		goto err_exit;
	}

	LOG_INF("%s apusys_rpc = 0x%x, size = %d\n", __func__,
				init_power_data.rpc_base_addr,
				(unsigned int)resource_size(apusys_rpc_res));

	apusys_pcu_res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "apusys_pcu");
	init_power_data.pcu_base_addr = devm_ioremap_resource(
						apusys_dev, apusys_pcu_res);

	if (IS_ERR((void *)init_power_data.pcu_base_addr)) {
		LOG_ERR("Unable to ioremap apusys_pcu\n");
		goto err_exit;
	}

	LOG_INF("%s apusys_pcu = 0x%x, size = %d\n", __func__,
				init_power_data.pcu_base_addr,
				(unsigned int)resource_size(apusys_pcu_res));

	power_task_handle = kthread_create(apusys_power_task,
						(void *)NULL, "apusys_power");
	if (IS_ERR(power_task_handle)) {
		power_task_handle = NULL;
		LOG_ERR("%s create power task fail\n");
		goto err_exit;
	}

	wake_up_process(power_task_handle);
	mutex_init(&power_device_list_mtx);
	mutex_init(&power_opp_mtx);

	apusys_power_debugfs_init();
	return 0;

err_exit:
	init_power_data.rpc_base_addr = NULL;
	init_power_data.pcu_base_addr = NULL;
	return err;
}


static int apu_power_remove(struct platform_device *pdev)
{
	apusys_power_debugfs_exit();

	if (power_task_handle)
		kthread_stop(power_task_handle);

	mutex_destroy(&power_opp_mtx);
	mutex_destroy(&power_device_list_mtx);

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

late_initcall(apu_power_drv_init)

static void __exit apu_power_drv_exit(void)
{
	platform_driver_unregister(&apu_power_driver);
}
module_exit(apu_power_drv_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("apu power driver");

