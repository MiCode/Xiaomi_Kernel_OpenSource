// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/timer.h>

#ifndef __MDLA_POWER_CTL_H__
#define __MDLA_POWER_CTL_H__


int get_power_on_status(unsigned int core_id);
int mdla_pwr_on(unsigned int core_id, bool force);
void mdla_set_opp(unsigned int core_id, int bootst_val);
void mdla_setup_power_down(unsigned int core_id);
int mdla_register_power(struct platform_device *pdev);
int mdla_unregister_power(struct platform_device *pdev);
void mdla_power_timeup(struct timer_list *timer);
int mdla_start_power_off(unsigned int core_id, int suspend, bool force);
int mdla_pwr_off(unsigned int core_id, int suspend, bool force);
void mdla0_start_power_off(struct work_struct *work);
void mdla1_start_power_off(struct work_struct *work);


#endif
