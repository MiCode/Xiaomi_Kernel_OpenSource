/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef _VPU_POWER_API_H_
#define _VPU_POWER_API_H_

#include "apusys_power_cust.h"

// FIXME: should match config of 6885
struct apu_power_info {
	unsigned int dump_div;
	unsigned int vvpu;
	unsigned int vmdla;
	unsigned int vcore;
	unsigned int vsram;
	unsigned int dsp_freq;		// dsp conn
	unsigned int dsp1_freq;		// vpu core0
	unsigned int dsp2_freq;		// vpu core1
	unsigned int dsp3_freq;		// mdla core
	unsigned int ipuif_freq;	// ipu intf.
	unsigned int max_opp_limit;
	unsigned int min_opp_limit;
	unsigned int thermal_cond;
	unsigned int power_lock;
	unsigned long long id;
};

//APU
void pm_qos_register(void);
void pm_qos_unregister(void);
int prepare_regulator(enum DVFS_BUCK buck, struct device *dev);
int unprepare_regulator(enum DVFS_BUCK buck);
int config_normal_regulator(enum DVFS_BUCK buck, enum DVFS_VOLTAGE voltage_mV);
int config_regulator_mode(enum DVFS_BUCK buck, int is_normal);
int config_vcore(enum DVFS_USER user, int vcore_opp);
void dump_voltage(struct apu_power_info *info);
void dump_frequency(struct apu_power_info *info);
int set_if_clock_source(int target_opp, enum DVFS_VOLTAGE_DOMAIN domain);

//VPU
int vpu_set_clock_source(int target_opp, enum DVFS_VOLTAGE_DOMAIN domain);
int vpu_prepare_clock(struct device *dev);
void vpu_unprepare_clock(void);
void vpu_disable_clock(int core);
void vpu_enable_clock(int core);

//MDLA
int mdla_set_clock_source(int target_opp, enum DVFS_VOLTAGE_DOMAIN domain);
int mdla_prepare_clock(struct device *dev);
void mdla_unprepare_clock(void);
void mdla_disable_clock(int core);
void mdla_enable_clock(int core);

#endif // _VPU_POWER_API_H_
