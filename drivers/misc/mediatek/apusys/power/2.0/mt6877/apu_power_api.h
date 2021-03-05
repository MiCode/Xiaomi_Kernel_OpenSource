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

#ifndef _APU_POWER_API_H_
#define _APU_POWER_API_H_

#include "apusys_power_cust.h"

struct apu_power_info {
	unsigned int dump_div;
	unsigned int vvpu;
	unsigned int vcore;
	unsigned int vsram;
	unsigned int conn_freq;		// conn
	unsigned int vpu0_freq;		// vpu core0
	unsigned int vpu1_freq;		// vpu core1
	unsigned int mdla0_freq;		// mdla core0
	unsigned int iommu_freq;	// iommu
	unsigned int apupll_freq;		// apupll for mdla usage
	unsigned int npupll_freq;		// npupll for vpu usage
	unsigned int apupll1_freq;	// apupll for mnoc usage
	unsigned int apupll2_freq;	// apupll for iommu usage
	unsigned int acc_status[8];
	unsigned int spm_wakeup;
	unsigned int rpc_intf_rdy;
	unsigned int vcore_cg_stat;
	unsigned int conn_cg_stat;
	unsigned int conn1_cg_stat;
	unsigned int vpu0_cg_stat;
	unsigned int vpu1_cg_stat;
	unsigned int mdla0_cg_stat;
	unsigned int max_opp_limit;
	unsigned int min_opp_limit;
	unsigned int thermal_cond;
	unsigned int power_lock;
	unsigned int type;
	unsigned int force_print;
	unsigned long long id;
};

struct apu_power_info_record {
	struct apu_power_info pwr_info;
	unsigned long time_sec;
	unsigned long time_nsec;
};

//APU
void pm_qos_register(void);
void pm_qos_unregister(void);

int prepare_regulator(enum DVFS_BUCK buck, struct device *dev);
int enable_regulator(enum DVFS_BUCK buck);
int disable_regulator(enum DVFS_BUCK buck);
int unprepare_regulator(enum DVFS_BUCK buck);
int config_normal_regulator(enum DVFS_BUCK buck, enum DVFS_VOLTAGE voltage_mV);
int config_regulator_mode(enum DVFS_BUCK buck, int is_normal);
int config_vcore(enum DVFS_USER user, int vcore_opp);

int prepare_apu_clock(struct device *dev);
void unprepare_apu_clock(void);

int enable_apu_mtcmos(int enable);
int enable_apu_vcore_clksrc(void);
int enable_apu_conn_clksrc(void);
int enable_apu_device_clksrc(enum DVFS_USER user);
int enable_apu_conn_vcore_clock(void);
int enable_apu_device_clock(enum DVFS_USER user);

void disable_apu_conn_vcore_clock(void);
void disable_apu_device_clock(enum DVFS_USER user);
void disable_apu_conn_clksrc(void);
void disable_apu_device_clksrc(enum DVFS_USER user);

int set_apu_clock_source(enum DVFS_FREQ freq, enum DVFS_VOLTAGE_DOMAIN domain);
int config_apupll_freq(enum DVFS_FREQ freq, enum DVFS_VOLTAGE_DOMAIN domain);
void acc_init(void);

void dump_voltage(struct apu_power_info *info);
void dump_frequency(struct apu_power_info *info);

bool dvfs_user_support(enum DVFS_USER user);
bool dvfs_power_domain_support(enum DVFS_VOLTAGE_DOMAIN domain);


#endif // _APU_POWER_API_H_
