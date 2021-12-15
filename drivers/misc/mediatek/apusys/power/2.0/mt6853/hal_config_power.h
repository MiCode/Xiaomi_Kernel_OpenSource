// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _HAL_CONFIG_POWER_H_
#define _HAL_CONFIG_POWER_H_

#include "apusys_power_cust.h"
#include <helio-dvfsrc-opp.h>

extern int conn_mtcmos_on;

extern bool apu_get_power_on_status(enum DVFS_USER user);
extern enum vcore_opp volt_to_vcore_opp(int target_volt);
extern enum DVFS_FREQ volt_to_ipuif_freq(int target_volt);

/************************************
 * command base hal interface
 ************************************/
enum HAL_POWER_CMD {
	PWR_CMD_INIT_POWER,		// 0
	PWR_CMD_SET_BOOT_UP,		// 1
	PWR_CMD_SET_SHUT_DOWN,		// 2
	PWR_CMD_SET_VOLT,		// 3
	PWR_CMD_SET_REGULATOR_MODE,	// 4
	PWR_CMD_SET_FREQ,		// 5
	PWR_CMD_PM_HANDLER,		// 6
	PWR_CMD_GET_POWER_INFO,		// 7
	PWR_CMD_REG_DUMP,		// 8
	PWR_CMD_UNINIT_POWER,		// 9
	PWR_CMD_DEBUG_FUNC,		//10
	PWR_CMD_SEGMENT_CHECK,		//11
	PWR_CMD_DUMP_FAIL_STATE,	//12
	PWR_CMD_BINNING_CHECK,		//13
};


/************************************
 * command base hal param struct
 ************************************/

struct hal_param_init_power {
	struct device *dev;
	void *rpc_base_addr;
	void *pcu_base_addr;
	void *vcore_base_addr;
	void *infracfg_ao_base_addr;
	void *infra_bcrm_base_addr;
	void *spm_base_addr;
	void *conn_base_addr;
	void *vpu0_base_addr;
	void *vpu1_base_addr;
	void *apmixed_base_addr;
};

// regulator only
struct hal_param_volt {
	enum DVFS_BUCK target_buck;
	enum DVFS_VOLTAGE target_volt;
};

// regulator only, target_mode range : 0 and 1
struct hal_param_regulator_mode {
	enum DVFS_BUCK target_buck;
	int target_mode;
};

// mtcmos only
struct hal_param_mtcmos {
	int enable;
};

// cg and clk
struct hal_param_clk {
	int enable;
};

// freq only
struct hal_param_freq {
	enum DVFS_VOLTAGE_DOMAIN target_volt_domain;
	enum DVFS_FREQ target_freq;
};

struct hal_param_pwr_info {
	uint64_t id;
};

struct hal_param_pwr_mask {
	uint8_t power_bit_mask;
};

struct hal_param_seg_support {
	enum DVFS_USER user;
	bool support;
	enum SEGMENT_INFO seg;
};

// suspend, resume only
struct hal_param_pm {
	uint8_t is_suspend;
};

/************************************
 * common power config function
 ************************************/
int hal_config_power(enum HAL_POWER_CMD, enum DVFS_USER, void *param);

#endif // _HAL_CONFIG_POWER_H_
