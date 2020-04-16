/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <asm/mman.h>
#include <linux/dmapool.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/sched/clock.h>

#include "mdla.h"
#include "mdla_pmu.h"
#include "mdla_trace.h"
#include "mdla_debug.h"
#include "mdla_util.h"
#include "mdla_power_ctrl.h"
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
#include "apusys_power.h"
#endif

/* power on/off warpper function, protected by cmd_list_lock */
enum MDLA_POWER_STAT {
	PWR_OFF = 0,
	PWR_ON = 1,
};


int get_power_on_status(int core_id)
{
	return mdla_devices[core_id].mdla_power_status == PWR_ON ? 1:0;
}

int mdla_pwr_on(int core_id)
{

	int ret = 0;
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	#ifdef __APUSYS_MDLA_UT__
	u64 poweron_t;   /*power on start time */
	#endif

	enum DVFS_USER register_user = MDLA;

	mutex_lock(&mdla_devices[core_id].power_lock);

	if (timer_pending(&mdla_devices[core_id].power_timer))
		del_timer(&mdla_devices[core_id].power_timer);

	if (get_power_on_status(core_id) == PWR_ON)
		goto power_on_done;

	#ifdef __APUSYS_MDLA_UT__
	poweron_t = sched_clock();
	#endif

	ret = apu_device_power_on(register_user);
	if (!ret) {
		mdla_cmd_debug("%s power on device %d success\n",
						__func__, register_user);
		mdla_devices[core_id].mdla_power_status = PWR_ON;
	} else {
		mdla_cmd_debug("%s power on device %d fail\n",
						__func__, register_user);
	}

	#ifdef __APUSYS_MDLA_UT__
		mdla_perf_debug("mdla %d: power on info: apu_device_power_on_time: %u\n",
				core_id, sched_clock()-poweron_t);
	#endif

	mdla_reset_lock(core_id, REASON_DRVINIT);

power_on_done:
	mdla_profile_start();
	mutex_unlock(&mdla_devices[core_id].power_lock);
#else//CONFIG_FPGA_EARLY_PORTING

	mdla_reset_lock(core_id, REASON_DRVINIT);
	mdla_profile_start();

#endif
	return ret;
}

int mdla_pwr_off(int core_id)
{
	int ret = 0;
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	enum DVFS_USER register_user = MDLA;

	mutex_lock(&mdla_devices[core_id].power_lock);

	if (get_power_on_status(core_id) == PWR_OFF)
		goto power_off_done;

	/*pdn directly by user request*/
	if (timer_pending(&mdla_devices[core_id].power_timer))
		del_timer(&mdla_devices[core_id].power_timer);

	pmu_reg_save();

	ret = apu_device_power_off(register_user);
	if (!ret) {
		mdla_cmd_debug("%s power off device %d success\n",
						__func__, register_user);
		mdla_devices[core_id].mdla_power_status = PWR_OFF;
	} else {
		mdla_cmd_debug("%s power off device %d fail\n",
						__func__, register_user);
	}

power_off_done:
	mutex_unlock(&mdla_devices[core_id].power_lock);
#endif
	return ret;
}

void mdla0_start_power_off(struct work_struct *work)
{
	mdla_start_power_off(0);
}

void mdla1_start_power_off(struct work_struct *work)
{
	mdla_start_power_off(1);
}

void mdla_start_power_off(int core_id)
{
	mutex_lock(&mdla_devices[core_id].cmd_lock);
	mdla_pwr_off(core_id);
	mutex_unlock(&mdla_devices[core_id].cmd_lock);
}

void mdla_power_timeup(unsigned long data)
{
	schedule_work(&mdla_devices[data].power_off_work);
}

void mdla_setup_power_down(int core_id)
{
	/*
	 * XXX: this function might be reentrant,
	 * so we should check the timer status here
	 */
	if (mdla_poweroff_time) {
		mdla_drv_debug("%s: MDLA %d start power_timer\n",
				__func__, core_id);
		mod_timer(&mdla_devices[core_id].power_timer,
			jiffies + msecs_to_jiffies(mdla_poweroff_time));
	}
}

int mdla_register_power(struct platform_device *pdev)
{
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	int ret = 0;
	enum DVFS_USER register_user = MDLA;

	mdla_cmd_debug("probe 0, pdev id = %d name = %s, name = %s\n",
						pdev->id, pdev->name,
						pdev->dev.of_node->name);

	ret = apu_power_device_register(register_user, pdev);
	if (!ret) {
		mdla_cmd_debug("%s register power device %d success\n",
						__func__, register_user);
	} else {
		mdla_cmd_debug("%s register power device %d fail\n",
						__func__, register_user);
		return -1;
	}
#endif
	return 0;
}


int mdla_unregister_power(struct platform_device *pdev)
{
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	enum DVFS_USER register_user = MDLA;
	int i;

	for (i = 0; i < mdla_max_num_core; i++)
		mdla_start_power_off(i);

	apu_power_device_unregister(register_user);
#endif
	return 0;

}
