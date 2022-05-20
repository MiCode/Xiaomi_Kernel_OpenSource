// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/sched/clock.h>

#include "edma_driver.h"
#include "edma_cmd_hnd.h"

#include "apusys_power.h"
#include "edma_dbgfs.h"
#include "edma_plat_internal.h"

#define EDMA_POWEROFF_TIME_DEFAULT 2000

static inline void lock_command(struct edma_sub *edma_sub)
{
	mutex_lock(&edma_sub->cmd_mutex);
	edma_sub->is_cmd_done = false;
}

static inline int wait_command(struct edma_sub *edma_sub, u32 timeout)
{
	return (wait_event_interruptible_timeout(edma_sub->cmd_wait,
						 edma_sub->is_cmd_done,
						 msecs_to_jiffies
						 (timeout)) > 0)
	    ? 0 : -ETIMEDOUT;
}

static inline void unlock_command(struct edma_sub *edma_sub)
{
	mutex_unlock(&edma_sub->cmd_mutex);
}


/**
 * @brief excute request by each edma_sub
 *  parameters:
 *    edma_sub: include each edma base address
 */
int edma_ext_by_sub(struct edma_sub *edma_sub, struct edma_request *req)
{
	int ret = 0;
	unsigned long long get_port_time = 0;
	struct edma_plat_drv *drv = (struct edma_plat_drv *)edma_sub->plat_drv;

	ret = edma_power_on(edma_sub);
	if (ret != 0)
		return ret;

	get_port_time = sched_clock();

	lock_command(edma_sub);

	if (drv)
		ret = drv->exe_sub(edma_sub, req);

	ret = wait_command(edma_sub, CMD_WAIT_TIME_MS);
	if (ret) {
		pr_notice
		    ("%s:timeout\n", __func__);
		if (drv)
			drv->prt_error(edma_sub, req);
	}

	get_port_time = sched_clock() - get_port_time; //ns

	edma_sub->ip_time = get_port_time/1000;

	unlock_command(edma_sub);
	edma_power_off(edma_sub, 0);

	return ret;
}

bool edma_is_all_power_off(struct edma_device *edma_device)
{
	int i;

	for (i = 0; i < edma_device->edma_sub_num; i++)
		if (edma_device->edma_sub[i]->power_state == EDMA_POWER_ON)
			return false;

	return true;
}

int edma_power_on(struct edma_sub *edma_sub)
{
	struct edma_device *edma_device;
	int ret = 0;

	edma_device = edma_sub->edma_device;
	mutex_lock(&edma_device->power_mutex);
	if (edma_is_all_power_off(edma_device))	{
		if (timer_pending(&edma_device->power_timer)) {
			del_timer(&edma_device->power_timer);
			LOG_DBG("%s deltimer no pwr on, state = %d\n",
					__func__, edma_device->power_state);
		} else if (edma_device->power_state ==
			EDMA_POWER_ON) {
			LOG_ERR("%s power on twice\n", __func__);
		} else {
			ret = apu_device_power_on(EDMA);
			if (!ret) {
				LOG_DBG("%s power on success\n", __func__);
				edma_device->power_state = EDMA_POWER_ON;
			} else {
				LOG_ERR("%s power on fail\n",
								__func__);
				mutex_unlock(&edma_device->power_mutex);
				return ret;
			}
		}
	}
	edma_sub->power_state = EDMA_POWER_ON;
	mutex_unlock(&edma_device->power_mutex);
	return ret;
}

void edma_start_power_off(struct work_struct *work)
{
	int ret = 0;
	struct edma_device *edmaDev = NULL;

	edmaDev =
	    container_of(work, struct edma_device, power_off_work);

	LOG_DBG("%s: contain power_state = %d!!\n", __func__,
		edmaDev->power_state);

	if (edmaDev->power_state == EDMA_POWER_OFF)
		LOG_ERR("%s pwr off twice\n",
						__func__);

	mutex_lock(&edmaDev->power_mutex);

	ret = apu_device_power_off(EDMA);
	if (ret != 0) {
		LOG_ERR("%s power off fail\n", __func__);
	} else {
		pr_notice("%s: power off done!!\n", __func__);
		edmaDev->power_state = EDMA_POWER_OFF;
	}
	mutex_unlock(&edmaDev->power_mutex);

}

void edma_power_time_up(struct timer_list *tlist)
{
	//struct edma_device *edma_device = (struct edma_device *)data;

	struct edma_device *edma_device =
	    container_of(tlist, struct edma_device, power_timer);

	pr_notice("%s: !!\n", __func__);
	//use kwork job to prevent power off at irq
	schedule_work(&edma_device->power_off_work);
}

int edma_power_off(struct edma_sub *edma_sub, u8 force)
{
	struct edma_device *edma_device =
		edma_sub->edma_device;
	int ret = 0;

	if (edma_device->dbg_cfg
		& EDMA_DBG_DISABLE_PWR_OFF) {

		pr_notice("%s:no power off!!\n", __func__);

		return 0;
	}

	mutex_lock(&edma_device->power_mutex);
	edma_sub->power_state = EDMA_POWER_OFF;
	if (edma_is_all_power_off(edma_device)) {

		if (timer_pending(&edma_device->power_timer))
			del_timer(&edma_device->power_timer);

		if (force == 1) {

			if (edma_device->power_state != EDMA_POWER_OFF) {
				ret = apu_device_power_suspend(EDMA, 1);

				pr_notice("%s: force power off!!\n", __func__);
				if (!ret) {
					LOG_INF("%s power off success\n",
							__func__);
					edma_device->power_state =
						EDMA_POWER_OFF;
				} else {
					LOG_ERR("%s power off fail\n",
						__func__);
				}

			} else
				LOG_INF("%s force power off skip\n",
						__func__);

		} else {
			edma_device->power_timer.expires = jiffies +
			msecs_to_jiffies(EDMA_POWEROFF_TIME_DEFAULT);
			add_timer(&edma_device->power_timer);
		}
	}
	mutex_unlock(&edma_device->power_mutex);

	return ret;

}
#ifndef EDMA_IOCTRL
void edma_setup_ext_mode_request(struct edma_request *req,
			       struct edma_ext *edma_ext,
			       unsigned int type)
{
	req->handle = (u64) req;
	req->cmd = type;
	req->ext_reg_addr = edma_ext->reg_addr;
	req->ext_count = edma_ext->count;
	req->fill_value = edma_ext->fill_value;
	req->desp_iommu_en = edma_ext->desp_iommu_en;
	req->cmd_result = 0;
}

#endif


int edma_execute(struct edma_sub *edma_sub, struct edma_ext *edma_ext)
{
	int ret = 0;
	struct edma_request req = {0};
#ifdef DEBUG
	uint32_t t1, t2;
	uint32_t exe_time;

	t1 = ktime_get_ns();
#endif
	edma_setup_ext_mode_request(&req, edma_ext, EDMA_PROC_EXT_MODE);

	ret = edma_ext_by_sub(edma_sub, &req);

#ifdef DEBUG
	t2 = ktime_get_ns();
	exe_time = (t2 - t1)/1000;

	//pr_notice("%s:ip time = %d\n", __func__, edma_sub->ip_time);
	LOG_DBG("%s:func done[%d], exe_time = %d, ip time = %d\n",
		__func__, ret, exe_time, edma_sub->ip_time);
#endif
	return ret;
}

