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

#ifndef _HAL_CONFIG_POWER_H_
#define _HAL_CONFIG_POWER_H_

//#include <helio-dvfsrc-opp.h>
#include "apusys_power_cust.h"

/************************************
 * command base hal interface
 ************************************/
enum HAL_POWER_CMD {
	PWR_CMD_INIT_POWER,
	PWR_CMD_SET_BOOT_UP,
	PWR_CMD_SET_SHUT_DOWN,
	PWR_CMD_SET_VOLT,
	PWR_CMD_SET_REGULATOR_MODE,
	PWR_CMD_SET_MTCMOS,
	PWR_CMD_SET_CLK,
	PWR_CMD_SET_FREQ,
	PWR_CMD_GET_POWER_INFO,
	PWR_CMD_UNINIT_POWER,
};


/************************************
 * command base hal param struct
 ************************************/

struct hal_param_init_power {
	struct device *dev;
	void __iomem *rpc_base_addr;
	void __iomem *pcu_base_addr;
};

// regulator only, target_opp range : 0 ~ 15
struct hal_param_volt {
	enum DVFS_VOLTAGE_DOMAIN target_volt_domain;
	enum DVFS_BUCK target_buck;
	int target_opp;
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

// freq only, target_opp range : 0 ~ 15
struct hal_param_freq {
	enum DVFS_VOLTAGE_DOMAIN target_volt_domain;
	int target_opp;
};

struct hal_param_pwr_info {
	uint64_t id;
};

struct hal_param_pwr_mask {
	uint8_t power_bit_mask;
};

/************************************
 * common power config function
 ************************************/
int hal_config_power(enum HAL_POWER_CMD, enum DVFS_USER, void *param);

#endif // _HAL_CONFIG_POWER_H_
