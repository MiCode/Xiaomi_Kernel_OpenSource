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
#include <linux/timer.h>

#ifndef __MDLA_POWER_CTL_H__
#define __MDLA_POWER_CTL_H__


int get_power_on_status(int core_id);
int mdla_pwr_on(int core_id);
void mdla_setup_power_down(int core_id);
int mdla_register_power(struct platform_device *pdev);
int mdla_unregister_power(struct platform_device *pdev);
void mdla_power_timeup(unsigned long data);
void mdla_start_power_off(int core_id);
int mdla_pwr_off(int core_id);
void mdla0_start_power_off(struct work_struct *work);
void mdla1_start_power_off(struct work_struct *work);
//int __weak mdla_cmd_qos_start(int core);
//int __weak mdla_qos_counter_end(unsigned int core);
//int __weak mdla_qos_counter_start(unsigned int core);

#endif
