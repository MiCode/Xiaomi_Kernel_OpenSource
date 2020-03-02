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

#include <linux/delay.h>
#include <linux/io.h>
#include "hal_config_power.h"
#include "apu_power_api.h"
#include "apusys_power_cust.h"
#include "apusys_power_reg.h"
#include "apu_log.h"
#include <helio-dvfsrc-opp.h>

static int is_apu_power_initilized;

/************************************
 * platform related power APIs
 ************************************/
static int init_power_resource(enum DVFS_USER, void *param);
static int set_power_voltage(enum DVFS_USER, void *param);
static int set_power_regulator_mode(enum DVFS_USER, void *param);
static int set_power_mtcmos(enum DVFS_USER, void *param);
static int set_power_clock(enum DVFS_USER, void *param);
static int set_power_frequency(enum DVFS_USER, void *param);
static void get_current_power_info(enum DVFS_USER user, void *param);
static int uninit_power_resource(enum DVFS_USER, void *param);
static void hw_init_setting(void);

/************************************
 * common power hal command
 ************************************/

static void DRV_WriteReg32(u32 addr, u32 value)
{
	iowrite32(value, addr);
}

static u32 DRV_Reg32(u32 addr)
{
	return ioread32(addr);
}

int hal_config_power(enum HAL_POWER_CMD cmd, enum DVFS_USER user, void *param)
{
	int ret = 0;

	LOG_INF("%s power command : %d, by user : %d\n", __func__, cmd, user);

	switch (cmd) {
	case PWR_CMD_INIT_POWER:
		ret = init_power_resource(user, param);
		hw_init_setting();
		break;
	case PWR_CMD_SET_VOLT:
		ret = set_power_voltage(user, param);
		break;
	case PWR_CMD_SET_REGULATOR_MODE:
		ret = set_power_regulator_mode(user, param);
		break;
	case PWR_CMD_SET_MTCMOS:
		ret = set_power_mtcmos(user, param);
		break;
	case PWR_CMD_SET_CLK:
		ret = set_power_clock(user, param);
		break;
	case PWR_CMD_SET_FREQ:
		ret = set_power_frequency(user, param);
		break;
	case PWR_CMD_GET_POWER_INFO:
		get_current_power_info(user, param);
		break;
	case PWR_CMD_UNINIT_POWER:
		ret = uninit_power_resource(user, param);
		break;
	default:
		LOG_ERR("%s unknown power command : %d\n", __func__, cmd);
		return -1;
	}

	return ret;
}


/************************************
 * utility function
 ************************************/

// normal opp to frequency
int opp_to_frequency(int target_opp, enum DVFS_VOLTAGE_DOMAIN domain)
{
	int frequency = 0;

	frequency = dvfs_table[target_opp][domain].freq;

	if (frequency >= DVFS_FREQ_MAX) {
		LOG_ERR("%s failed, force change freq to %d\n",
						__func__, frequency);
		return -1;
	}

	return frequency;
}

// normal opp to voltage
int opp_to_voltage(int target_opp, enum DVFS_VOLTAGE_DOMAIN domain)
{
	int voltage = 0;

	voltage = dvfs_table[target_opp][domain].voltage;

	if (voltage >= DVFS_VOLT_MAX) {
		LOG_ERR("%s failed, force change voltage to %d\n",
							__func__, voltage);
		return -1;
	}

	return voltage;
}

// normal opp to vcore opp
int opp_to_vcore_opp(int target_opp)
{
	int vcore_opp = 0;

	vcore_opp = vcore_opp_mapping[target_opp];

	if (vcore_opp >= VCORE_OPP_NUM) {
		LOG_ERR("%s failed, force change vcore_opp to %d\n",
							__func__, vcore_opp);
		return -1;
	}

	return vcore_opp;
}

void prepare_apu_regulator(struct device *dev, int prepare)
{
	if (prepare) {
		prepare_regulator(VCORE_BUCK, dev);
		prepare_regulator(VPU_BUCK, dev);
		prepare_regulator(MDLA_BUCK, dev);

		// register pm_qos notifier here,
		// vcore need to use pm_qos for voltage voting
		pm_qos_register();
	} else {
		unprepare_regulator(MDLA_BUCK);
		unprepare_regulator(VPU_BUCK);
		unprepare_regulator(VCORE_BUCK);

		// unregister pm_qos notifier here,
		pm_qos_unregister();
	}
}

/******************************************
 * hal cmd corresponding platform function
 ******************************************/
void hw_init_setting(void)
{
	uint32_t regValue = 0;

	/* set memory type to PD or sleep */

	// MD32 sleep type
	DRV_WriteReg32(APU_RPC_SW_TYPE0, 0x6F);

	// IMEM_ICACHE sleep type for VPU0
	DRV_WriteReg32(APU_RPC_SW_TYPE2, 0x2);

	// IMEM_ICACHE sleep type for VPU1
	DRV_WriteReg32(APU_RPC_SW_TYPE3, 0x2);

	// IMEM_ICACHE sleep type for VPU2
	DRV_WriteReg32(APU_RPC_SW_TYPE4, 0x2);

	// mask RPC IRQ and bypass WFI
	regValue = DRV_Reg32(APU_RPC_SW_FIFO_WE);
	regValue |= 0x9E;
	DRV_WriteReg32(APU_RPC_SW_FIFO_WE, regValue);
}


int init_power_resource(enum DVFS_USER user, void *param)
{
	struct device *dev = ((struct hal_param_init_power *)param)->dev;

	if (!is_apu_power_initilized) {
		prepare_apu_regulator(dev, 1);
		vpu_prepare_clock(dev);
		mdla_prepare_clock(dev);
		is_apu_power_initilized = 1;
	}

	return 0;
}

void vsram_check(enum DVFS_BUCK buck, enum DVFS_VOLTAGE voltage_mV)
{
// FIXME: implement this function
/*
 *	// read vsarm_volatge
 *	if (vsarm_voltage == 0.85V) {
 *
 *	} else {
 *
 *	}
 */
}

int set_power_voltage(enum DVFS_USER user, void *param)
{
	enum DVFS_VOLTAGE_DOMAIN domain = 0;
	enum DVFS_BUCK buck = 0;
	int target_opp = 0;
	int val = 0;
	int ret = 0;

	domain = ((struct hal_param_volt *)param)->target_volt_domain;
	buck = ((struct hal_param_volt *)param)->target_buck;
	target_opp = ((struct hal_param_volt *)param)->target_opp;

	if (buck < APUSYS_BUCK_NUM && domain < APUSYS_BUCK_DOMAIN_NUM) {
		if (buck != VCORE_BUCK) {
			val = opp_to_voltage(target_opp, domain);
			LOG_DBG("%s set buck %d to %d\n", __func__, buck, val);

			if (val >= 0)
				ret = config_normal_regulator(buck, val);

		} else {
			// vcore opp range : 0 ~ 2
			val = opp_to_vcore_opp(target_opp);
			LOG_DBG("%s set vcore to opp %d\n", __func__, val);
			if (val >= 0)
				ret = config_vcore(user, val);
		}
	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
	}

	return ret;
}

int set_power_regulator_mode(enum DVFS_USER user, void *param)
{
	enum DVFS_BUCK buck = 0;
	int is_normal = 0;
	int ret = 0;

	buck = ((struct hal_param_regulator_mode *)param)->target_buck;
	is_normal = ((struct hal_param_regulator_mode *)param)->target_mode;

	ret = config_regulator_mode(buck, is_normal);
	return ret;
}


void rpc_fifo_check(void)
{
	unsigned int regValue = 0;
	unsigned int finished = 0;

	finished = 1;

	do {
		udelay(10);
		regValue = DRV_Reg32(APU_RPC_TOP_CON);
		finished = (regValue & (1 << 31));
	} while (finished);
}


void rpc_power_status_check(int domain_idx, unsigned int enable)
{
	unsigned int regValue = 0;
	unsigned int finished = 0;

	do {
		udelay(10);
		regValue = DRV_Reg32(APU_RPC_INTF_PWR_RDY);

		if (enable)
			finished = !((regValue >> domain_idx) & 0x1);
		else
			finished = (regValue >> domain_idx) & 0x1;
	} while (finished);
}


int set_power_mtcmos(enum DVFS_USER user, void *param)
{
	unsigned int enable = ((struct hal_param_mtcmos *)param)->enable;
	unsigned int domain_idx;
	unsigned int regValue = 0;

	LOG_INF("%s , user: %d , enable: %d\n", __func__, user, enable);

	if (user == VPU0)
		domain_idx = 2;
	else if (user == VPU1)
		domain_idx = 3;
	else if (user == VPU2)
		domain_idx = 4;
	else if (user == MDLA0)
		domain_idx = 6;
	else if (user == MDLA1)
		domain_idx = 7;
	else
		LOG_ERR("%s not support user : %d\n", __func__, user);

	if (enable) {
		// call spm api to enable wake up signal for apu_conn/apu_vcore
		if (DRV_Reg32(APU_RPC_INTF_PWR_RDY) == 0)
			LOG_WRN("%s enable wakeup signal\n", __func__);
			// FIXME: implement code

		rpc_fifo_check();
		DRV_WriteReg32(APU_RPC_SW_FIFO_WE, (domain_idx | (1 << 4)));
		rpc_power_status_check(domain_idx, enable);
	} else {
		DRV_WriteReg32(APU_RPC_SW_FIFO_WE, domain_idx);
		rpc_power_status_check(domain_idx, enable);

		// only apu_top power on
		if (DRV_Reg32(APU_RPC_INTF_PWR_RDY) == 0x1) {
		/*
		 * call spm api to disable wake up signal
		 * for apu_conn/apu_vcore
		 */
		}
		// sleep request enable
		regValue = DRV_Reg32(APU_RPC_TOP_CON);
		regValue |= 0x1;
		DRV_WriteReg32(APU_RPC_TOP_CON, regValue);
	}

	return 0;
}

int set_power_clock(enum DVFS_USER user, void *param)
{
	int enable = ((struct hal_param_clk *)param)->enable;

	LOG_INF("%s , user: %d , enable: %d\n", __func__, user, enable);

	if (enable) {
		if (VPU0 == user || VPU1 == user || VPU2 == user)
			vpu_enable_clock(user);
		else if (MDLA0 == user || MDLA1 == user)
			mdla_enable_clock(user);
		else
			LOG_ERR("%s not support user : %d\n", __func__, user);
	} else {
		if (VPU0 == user || VPU1 == user || VPU2 == user)
			vpu_disable_clock(user);
		else if (MDLA0 == user || MDLA1 == user)
			mdla_disable_clock(user);
		else
			LOG_ERR("%s not support user : %d\n", __func__, user);
	}

	return 0;
}

int set_power_frequency(enum DVFS_USER user, void *param)
{
	enum DVFS_VOLTAGE_DOMAIN domain = 0;
	int target_opp = 0;
	int ret = 0;

	domain = ((struct hal_param_freq *)param)->target_volt_domain;
	target_opp = ((struct hal_param_freq *)param)->target_opp;

	if (domain < APUSYS_BUCK_DOMAIN_NUM) {
		switch (domain) {
		case V_VPU0:
		case V_VPU1:
		case V_VPU2:
			ret = vpu_set_clock_source(target_opp, domain);
			break;
		case V_MDLA0:
		case V_MDLA1:
			ret = mdla_set_clock_source(target_opp, domain);
			break;
		case V_APU_CONN:
		case V_VCORE:
			ret = set_if_clock_source(target_opp, domain);
			break;
		default:
			LOG_ERR("%s not support power domain : %d\n",
							__func__, domain);
		}
	} else {
		LOG_ERR("%s not support power domain : %d\n", __func__, domain);
	}

	return ret;
}

void get_current_power_info(enum DVFS_USER user, void *param)
{
	struct apu_power_info info;
	char log_str[60];

	info.dump_div = 1000;
	info.id = ((struct hal_param_pwr_info *)param)->id;

	// including APUsys related buck
	dump_voltage(&info);

	// including APUsys related freq
	dump_frequency(&info);

	// FIXME: should match config of 6885
	snprintf(log_str, sizeof(log_str),
				"v[%u,%u,%u,%u]f[%u,%u,%u,%u,%u]%llu",
				info.vvpu, info.vmdla, info.vcore, info.vsram,
				info.dsp_freq, info.dsp1_freq, info.dsp2_freq,
				info.dsp3_freq, info.ipuif_freq, info.id);

	// TODO: return value to MET

	LOG_INF("%s\n", log_str);
}

int uninit_power_resource(enum DVFS_USER user, void *param)
{
	if (is_apu_power_initilized) {
		vpu_unprepare_clock();
		mdla_unprepare_clock();
		prepare_apu_regulator(NULL, 0);
		is_apu_power_initilized = 0;
	}

	return 0;
}
