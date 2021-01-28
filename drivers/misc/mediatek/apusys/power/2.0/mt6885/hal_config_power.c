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
static int set_power_boot_up(enum DVFS_USER, void *param);
static int set_power_shut_down(enum DVFS_USER, void *param);
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

int hal_config_power(enum HAL_POWER_CMD cmd, enum DVFS_USER user, void *param)
{
	int ret = 0;

	LOG_INF("%s power command : %d, by user : %d\n", __func__, cmd, user);

	if (cmd != PWR_CMD_INIT_POWER && is_apu_power_initilized == 0) {
		LOG_ERR("%s apu power state : %d, force return!\n",
					__func__, is_apu_power_initilized);
		return -1;
	}

	switch (cmd) {
	case PWR_CMD_INIT_POWER:
		ret = init_power_resource(user, param);
		hw_init_setting();
		break;
	case PWR_CMD_SET_BOOT_UP:
		ret = set_power_boot_up(user, param);
		break;
	case PWR_CMD_SET_SHUT_DOWN:
		ret = set_power_shut_down(user, param);
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

// vcore voltage p to vcore opp
static enum vcore_opp volt_to_vcore_opp(int target_volt)
{
	int opp;

	for (opp = 0 ; opp < VCORE_OPP_NUM ; opp++)
		if (vcore_opp_mapping[opp] == target_volt)
			break;

	if (opp >= VCORE_OPP_NUM) {
		LOG_ERR("%s failed, force to set default opp\n", __func__);
		return PM_QOS_VCORE_OPP_DEFAULT_VALUE;
	}

	LOG_DBG("%s opp = %d\n", __func__, opp);
	return (enum vcore_opp)opp;
}

static void prepare_apu_regulator(struct device *dev, int prepare)
{
	if (prepare) {
		prepare_regulator(VCORE_BUCK, dev);
		prepare_regulator(SRAM_BUCK, dev);
		prepare_regulator(VPU_BUCK, dev);
		prepare_regulator(MDLA_BUCK, dev);

		// register pm_qos notifier here,
		// vcore need to use pm_qos for voltage voting
		pm_qos_register();
	} else {
		unprepare_regulator(MDLA_BUCK);
		unprepare_regulator(VPU_BUCK);
		unprepare_regulator(SRAM_BUCK);
		unprepare_regulator(VCORE_BUCK);

		// unregister pm_qos notifier here,
		pm_qos_unregister();
	}
}

/******************************************
 * hal cmd corresponding platform function
 ******************************************/

static void hw_init_setting(void)
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
	regValue = DRV_Reg32(APU_RPC_TOP_SEL);
	regValue |= 0x9E;
	DRV_WriteReg32(APU_RPC_TOP_SEL, regValue);
}


static int init_power_resource(enum DVFS_USER user, void *param)
{
	struct hal_param_init_power *init_data = NULL;
	struct device *dev = NULL;

	init_data = (struct hal_param_init_power *)param;

	dev = init_data->dev;
	g_APU_RPCTOP_BASE = init_data->rpc_base_addr;
	g_APU_PCUTOP_BASE = init_data->pcu_base_addr;

	if (!is_apu_power_initilized) {
		prepare_apu_regulator(dev, 1);
#ifndef MTK_FPGA_PORTING
		prepare_apu_clock(dev);
#endif
		is_apu_power_initilized = 1;
	}

	return 0;
}

static int set_power_voltage(enum DVFS_USER user, void *param)
{
	enum DVFS_BUCK buck = 0;
	int target_volt = 0;
	int ret = 0;

	buck = ((struct hal_param_volt *)param)->target_buck;
	target_volt = ((struct hal_param_volt *)param)->target_volt;

	if (buck < APUSYS_BUCK_NUM) {
		if (buck != VCORE_BUCK) {
			LOG_DBG("%s set buck %d to %d\n", __func__,
							buck, target_volt);

			if (target_volt >= 0) {
				ret = config_normal_regulator(
						buck, target_volt);
			}

		} else {
			ret = config_vcore(user,
					volt_to_vcore_opp(target_volt));
		}
	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
	}

	return ret;
}

static int set_power_regulator_mode(enum DVFS_USER user, void *param)
{
	enum DVFS_BUCK buck = 0;
	int is_normal = 0;
	int ret = 0;

	buck = ((struct hal_param_regulator_mode *)param)->target_buck;
	is_normal = ((struct hal_param_regulator_mode *)param)->target_mode;

	ret = config_regulator_mode(buck, is_normal);
	return ret;
}


static void rpc_fifo_check(void)
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


static void rpc_power_status_check(int domain_idx, unsigned int enable)
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


static int set_power_mtcmos(enum DVFS_USER user, void *param)
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
		if (DRV_Reg32(APU_RPC_INTF_PWR_RDY) == 0) {
			LOG_WRN("%s enable wakeup signal\n", __func__);
			DRV_SetBitReg32(APU_RPC_TOP_CON, REG_WAKEUP_SET);
		}

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

static int set_power_clock(enum DVFS_USER user, void *param)
{
#ifndef MTK_FPGA_PORTING
	int enable = ((struct hal_param_clk *)param)->enable;

	LOG_INF("%s , user: %d , enable: %d\n", __func__, user, enable);

	if (enable)
		enable_apu_clock(user);
	else
		disable_apu_clock(user);
#endif
	return 0;
}

static int set_power_frequency(enum DVFS_USER user, void *param)
{
	enum DVFS_VOLTAGE_DOMAIN domain = 0;
	enum DVFS_FREQ freq = 0;
	int ret = 0;

	freq = ((struct hal_param_freq *)param)->target_freq;
	domain = ((struct hal_param_freq *)param)->target_volt_domain;

	if (domain < APUSYS_BUCK_DOMAIN_NUM)
		ret = set_apu_clock_source(freq, domain);
	else
		LOG_ERR("%s not support power domain : %d\n", __func__, domain);

	return ret;
}

static void get_current_power_info(enum DVFS_USER user, void *param)
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

static int uninit_power_resource(enum DVFS_USER user, void *param)
{
	if (is_apu_power_initilized) {
#ifndef MTK_FPGA_PORTING
		unprepare_apu_clock();
#endif
		prepare_apu_regulator(NULL, 0);
		is_apu_power_initilized = 0;
	}

	return 0;
}

static int set_power_boot_up(enum DVFS_USER user, void *param)
{
	struct hal_param_mtcmos mtcmos_data;
	struct hal_param_clk clk_data;
	struct hal_param_volt vpu_volt_data;
	struct hal_param_volt mdla_volt_data;
	struct hal_param_volt vcore_volt_data;
	struct hal_param_volt sram_volt_data;
	uint8_t power_bit_mask = 0;
	int ret = 0;

	power_bit_mask = ((struct hal_param_pwr_mask *)param)->power_bit_mask;

	if (power_bit_mask == 0) {
		vcore_volt_data.target_buck = VCORE_BUCK;
		vcore_volt_data.target_volt = VCORE_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&vcore_volt_data);

		sram_volt_data.target_buck = SRAM_BUCK;
		sram_volt_data.target_volt = VSRAM_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&sram_volt_data);

		vpu_volt_data.target_buck = VPU_BUCK;
		vpu_volt_data.target_volt = VVPU_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&vpu_volt_data);

		mdla_volt_data.target_buck = MDLA_BUCK;
		mdla_volt_data.target_volt = VMDLA_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&mdla_volt_data);
	}

// FIXME
#if 0
	struct hal_param_regulator_mode mode_data;

	// Set regulator mode
	vcore_mode_data.target_buck = apusys_user_to_buck[user];
	vcore_mode_data.target_mode = 1;
	hal_config_power(PWR_CMD_SET_REGULATOR_MODE, user, (void *)&mode_data);
	udelay(POWER_ON_DELAY);
#endif

	// Set mtcmos enable
	mtcmos_data.enable = 1;
	ret |= set_power_mtcmos(user, (void *)&mtcmos_data);

	// Set cg enable
	clk_data.enable = 1;
	ret |= set_power_clock(user, (void *)&clk_data);

	return ret;
}


static int set_power_shut_down(enum DVFS_USER user, void *param)
{
	struct hal_param_mtcmos mtcmos_data;
	struct hal_param_clk clk_data;
	struct hal_param_volt vpu_volt_data;
	struct hal_param_volt mdla_volt_data;
	struct hal_param_volt vcore_volt_data;
	struct hal_param_volt sram_volt_data;
	uint8_t power_bit_mask = 0;
	int ret = 0;

	power_bit_mask = ((struct hal_param_pwr_mask *)param)->power_bit_mask;

// FIXME
#if 0
	struct hal_param_regulator_mode mode_data;

	// Set regulator voltage
	vcore_mode_data.target_buck = apusys_user_to_buck[user];
	vcore_mode_data.target_mode = 0;
	hal_config_power(PWR_CMD_SET_REGULATOR_MODE, user, (void *)&mode_data);
	udelay(POWER_ON_DELAY);
#endif
	// Set mtcmos disable
	mtcmos_data.enable = 0;
	ret |= set_power_mtcmos(user, (void *)&mtcmos_data);

	// Set cg disable
	clk_data.enable = 0;
	ret |= set_power_clock(user, (void *)&clk_data);

	if (power_bit_mask == 0) {
		/*
		 * to avoid vmdla/vvpu constraint,
		 * adjust to transition voltage first.
		 */
		mdla_volt_data.target_buck = MDLA_BUCK;
		mdla_volt_data.target_volt = VSRAM_TRANS_VOLT;
		ret |= set_power_voltage(user, (void *)&mdla_volt_data);

		vpu_volt_data.target_buck = VPU_BUCK;
		vpu_volt_data.target_volt = VSRAM_TRANS_VOLT;
		ret |= set_power_voltage(user, (void *)&vpu_volt_data);

		sram_volt_data.target_buck = SRAM_BUCK;
		sram_volt_data.target_volt = VSRAM_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&sram_volt_data);

		/*
		 * then adjust vmdla/vvpu again to real default voltage
		 */
		mdla_volt_data.target_buck = MDLA_BUCK;
		mdla_volt_data.target_volt = VMDLA_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&mdla_volt_data);

		vpu_volt_data.target_buck = VPU_BUCK;
		vpu_volt_data.target_volt = VVPU_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&vpu_volt_data);

		vcore_volt_data.target_buck = VCORE_BUCK;
		vcore_volt_data.target_volt = VCORE_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&vcore_volt_data);
	}

	return ret;
}
