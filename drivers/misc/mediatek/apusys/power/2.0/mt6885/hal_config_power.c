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
#include <linux/sched/clock.h>

#include "hal_config_power.h"
#include "apu_power_api.h"
#include "apusys_power_ctl.h"
#include "apusys_power_cust.h"
#include "apusys_power_reg.h"
#include "apu_log.h"
#include "apusys_power_rule_check.h"
//#include <helio-dvfsrc-opp.h>

#define CREATE_TRACE_POINTS
#include "apu_power_events.h"
#include "mtk_devinfo.h"

#if SUPPORT_VCORE_TO_IPUIF
#define NUM_OF_IPUIF_OPP VCORE_OPP_NUM
#endif

static int is_apu_power_initilized;
static int force_pwr_on = 1;
static int force_pwr_off;
static int buck_already_on;
static int power_on_counter;
static int hal_cmd_status[APUSYS_POWER_USER_NUM];
int conn_mtcmos_on;

struct apu_power_info_record power_fail_record;

void *g_APU_RPCTOP_BASE;
void *g_APU_PCUTOP_BASE;
void *g_APU_VCORE_BASE;
void *g_APU_INFRACFG_AO_BASE;
void *g_APU_INFRA_BCRM_BASE;
void *g_APU_CONN_BASE;
void *g_APU_VPU0_BASE;
void *g_APU_VPU1_BASE;
void *g_APU_VPU2_BASE;
void *g_APU_MDLA0_BASE;
void *g_APU_MDLA1_BASE;
void *g_APU_SPM_BASE;

/************************************
 * platform related power APIs
 ************************************/

static int init_power_resource(void *param);
static int set_power_boot_up(enum DVFS_USER, void *param);
static int set_power_shut_down(enum DVFS_USER, void *param);
static int set_power_voltage(enum DVFS_USER, void *param);
static int set_power_regulator_mode(void *param);
static int set_power_mtcmos(enum DVFS_USER, void *param);
static int set_power_clock(enum DVFS_USER, void *param);
static int set_power_frequency(void *param);
static void get_current_power_info(void *param, int force);
static int uninit_power_resource(void);
static int apusys_power_reg_dump(struct apu_power_info *info, int force);
static void power_debug_func(void *param);
static void hw_init_setting(void);
static int buck_control(enum DVFS_USER user, int level);
static int rpc_power_status_check(int domain_idx, unsigned int enable);
static int apu_pm_handler(void *param);
static int segment_user_support_check(void *param);
static void recording_power_fail_state(void);
static void dump_fail_state(void);
static int binning_support_check(void);

/************************************
 * common power hal command
 ************************************/

int hal_config_power(enum HAL_POWER_CMD cmd, enum DVFS_USER user, void *param)
{
	int ret = 0;

	LOG_DBG("%s power command : %d, by user : %d\n", __func__, cmd, user);

	if (cmd != PWR_CMD_INIT_POWER && cmd != PWR_CMD_SEGMENT_CHECK &&
		cmd != PWR_CMD_BINNING_CHECK && is_apu_power_initilized == 0) {
		LOG_ERR("%s apu power state : %d, force return!\n",
					__func__, is_apu_power_initilized);
		return -1;
	}

	switch (cmd) {
	case PWR_CMD_INIT_POWER:
		ret = init_power_resource(param);
		break;
	case PWR_CMD_SET_BOOT_UP:
		ret = set_power_boot_up(user, param);
		break;
	case PWR_CMD_SET_SHUT_DOWN:
		ret = set_power_shut_down(user, param);
		break;
	case PWR_CMD_SET_VOLT:
		hal_cmd_status[user] = PWR_CMD_SET_VOLT;
		ret = set_power_voltage(user, param);
		hal_cmd_status[user] = 0;
		break;
	case PWR_CMD_SET_REGULATOR_MODE:
		ret = set_power_regulator_mode(param);
		break;
	case PWR_CMD_SET_FREQ:
		hal_cmd_status[user] = PWR_CMD_SET_FREQ;
		ret = set_power_frequency(param);
		hal_cmd_status[user] = 0;
		break;
	case PWR_CMD_PM_HANDLER:
		ret = apu_pm_handler(param);
		break;
	case PWR_CMD_GET_POWER_INFO:
		get_current_power_info(param, 0);
		break;
	case PWR_CMD_REG_DUMP:
		apusys_power_reg_dump(NULL, 0);
		break;
	case PWR_CMD_UNINIT_POWER:
		ret = uninit_power_resource();
		break;
	case PWR_CMD_DEBUG_FUNC:
		power_debug_func(param);
		break;
	case PWR_CMD_SEGMENT_CHECK:
		segment_user_support_check(param);
		break;
	case PWR_CMD_DUMP_FAIL_STATE:
		dump_fail_state();
		break;
	case PWR_CMD_BINNING_CHECK:
		binning_support_check();
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

static void recording_power_fail_state(void)
{
	uint64_t time = 0;
	uint32_t nanosec = 0;

	time = sched_clock();
	nanosec = do_div(time, 1000000000);

	power_fail_record.time_sec = (unsigned long)time;
	power_fail_record.time_nsec = (unsigned long)nanosec / 1000;
	power_fail_record.pwr_info.id = 0;
	power_fail_record.pwr_info.force_print = 1;
	power_fail_record.pwr_info.type = 1;

	get_current_power_info(&power_fail_record.pwr_info, 1);
}

static void dump_fail_state(void)
{
	char log_str[128];
	int ret = 0;

	ret = snprintf(log_str, sizeof(log_str),
		"v[%u,%u,%u,%u]f[%u,%u,%u,%u,%u,%u,%u]r[%x,%x,%x,%x,%x,%x,%x,%x,%x]t[%lu.%06lu]",
		power_fail_record.pwr_info.vvpu,
		power_fail_record.pwr_info.vmdla,
		power_fail_record.pwr_info.vcore,
		power_fail_record.pwr_info.vsram,
		power_fail_record.pwr_info.dsp_freq,
		power_fail_record.pwr_info.dsp1_freq,
		power_fail_record.pwr_info.dsp2_freq,
		power_fail_record.pwr_info.dsp3_freq,
		power_fail_record.pwr_info.dsp6_freq,
		power_fail_record.pwr_info.dsp7_freq,
		power_fail_record.pwr_info.ipuif_freq,
		power_fail_record.pwr_info.spm_wakeup,
		power_fail_record.pwr_info.rpc_intf_rdy,
		power_fail_record.pwr_info.vcore_cg_stat,
		power_fail_record.pwr_info.conn_cg_stat,
		power_fail_record.pwr_info.vpu0_cg_stat,
		power_fail_record.pwr_info.vpu1_cg_stat,
		power_fail_record.pwr_info.vpu2_cg_stat,
		power_fail_record.pwr_info.mdla0_cg_stat,
		power_fail_record.pwr_info.mdla1_cg_stat,
		power_fail_record.time_sec, power_fail_record.time_nsec);

	if (ret >= 0) {
		LOG_ERR("APUPWR err %s\n", log_str);
		LOG_DUMP("APUPWR err %s\n", log_str); // debug ring buffer
	}
}

// vcore voltage p to vcore opp
enum vcore_opp volt_to_vcore_opp(int target_volt)
{
	int opp;

	for (opp = 0 ; opp < VCORE_OPP_NUM ; opp++)
		if (vcore_opp_mapping[opp] == target_volt)
			break;

	if (opp >= VCORE_OPP_NUM) {
		LOG_ERR("%s failed, force to set opp 0\n", __func__);
		return VCORE_OPP_0;
	}

	LOG_DBG("%s opp = %d\n", __func__, opp);
	return (enum vcore_opp)opp;
}

#if SUPPORT_VCORE_TO_IPUIF
// vcore voltage p to ipuif freq
enum DVFS_FREQ volt_to_ipuif_freq(int target_volt)
{
	int opp;

	for (opp = 0 ; opp < NUM_OF_IPUIF_OPP ; opp++)
		if (g_ipuif_opp_table[opp].ipuif_vcore == target_volt)
			break;

	if (opp >= NUM_OF_IPUIF_OPP) {
		LOG_ERR("%s failed, force to set min opp\n", __func__);
		return g_ipuif_opp_table[NUM_OF_IPUIF_OPP - 1].ipuif_khz;
	}

	LOG_DBG("%s freq = %d\n", __func__, g_ipuif_opp_table[opp].ipuif_khz);
	return g_ipuif_opp_table[opp].ipuif_khz;
}
#endif

static void prepare_apu_regulator(struct device *dev, int prepare)
{
	if (prepare) {
		// obtain regulator handle
		prepare_regulator(VCORE_BUCK, dev);
		prepare_regulator(SRAM_BUCK, dev);
		prepare_regulator(VPU_BUCK, dev);
		prepare_regulator(MDLA_BUCK, dev);

		// register pm_qos notifier here,
		// vcore need to use pm_qos for voltage voting
		pm_qos_register();
	} else {
		// release regulator handle
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

	/*
	 * set memory type to PD or sleep group
	 * sw_type register for each memory group, set to PD mode default
	 */
	DRV_WriteReg32(APU_RPC_SW_TYPE0, 0xFF);	// APUTOP
	DRV_WriteReg32(APU_RPC_SW_TYPE2, 0x7);	// VPU0
	DRV_WriteReg32(APU_RPC_SW_TYPE3, 0x7);	// VPU1
	DRV_WriteReg32(APU_RPC_SW_TYPE4, 0x7);	// VPU2
	DRV_WriteReg32(APU_RPC_SW_TYPE6, 0x3);	// MDLA0
	DRV_WriteReg32(APU_RPC_SW_TYPE7, 0x3);	// MDLA1

	// subsys mtcmos domain (power switch) force ON
	DRV_SetBitReg32(APU_RPC_PWR_CON2, BIT(1)); // VPU0
	DRV_SetBitReg32(APU_RPC_PWR_CON3, BIT(1)); // VPU1
	DRV_SetBitReg32(APU_RPC_PWR_CON4, BIT(1)); // VPU2
	DRV_SetBitReg32(APU_RPC_PWR_CON6, BIT(1)); // MDLA0
	DRV_SetBitReg32(APU_RPC_PWR_CON7, BIT(1)); // MDLA1

	// mask RPC IRQ and bypass WFI
	regValue = DRV_Reg32(APU_RPC_TOP_SEL);
	regValue |= 0x9E;
	regValue |= BIT(10);
	DRV_WriteReg32(APU_RPC_TOP_SEL, regValue);

	udelay(100);

#if !BYPASS_POWER_OFF
	// sleep request enable
	regValue = DRV_Reg32(APU_RPC_TOP_CON);
	regValue |= 0x1;
	DRV_WriteReg32(APU_RPC_TOP_CON, regValue);

	rpc_power_status_check(0, 0);
	LOG_WRN("%s done and request to enter sleep\n", __func__);
#else
	LOG_WRN("%s done\n", __func__);
#endif
}

static int init_power_resource(void *param)
{
	struct hal_param_init_power *init_data = NULL;
	struct device *dev = NULL;

	init_data = (struct hal_param_init_power *)param;

	dev = init_data->dev;
	g_APU_RPCTOP_BASE = init_data->rpc_base_addr;
	g_APU_PCUTOP_BASE = init_data->pcu_base_addr;
	g_APU_VCORE_BASE = init_data->vcore_base_addr;
	g_APU_INFRACFG_AO_BASE = init_data->infracfg_ao_base_addr;
	g_APU_INFRA_BCRM_BASE = init_data->infra_bcrm_base_addr;
	g_APU_SPM_BASE = init_data->spm_base_addr;

	g_APU_CONN_BASE = init_data->conn_base_addr;
	g_APU_VPU0_BASE = init_data->vpu0_base_addr;
	g_APU_VPU1_BASE = init_data->vpu1_base_addr;
	g_APU_VPU2_BASE = init_data->vpu2_base_addr;
	g_APU_MDLA0_BASE = init_data->mdla0_base_addr;
	g_APU_MDLA1_BASE = init_data->mdla1_base_addr;

	LOG_DBG("%s , g_APU_RPCTOP_BASE 0x%p\n", __func__, g_APU_RPCTOP_BASE);
	LOG_DBG("%s , g_APU_PCUTOP_BASE 0x%p\n", __func__, g_APU_PCUTOP_BASE);
	LOG_DBG("%s , g_APU_VCORE_BASE 0x%p\n", __func__, g_APU_VCORE_BASE);
	LOG_DBG("%s , g_APU_INFRACFG_AO_BASE 0x%p\n", __func__,
						g_APU_INFRACFG_AO_BASE);
	LOG_DBG("%s , g_APU_INFRA_BCRM_BASE 0x%p\n", __func__,
						g_APU_INFRA_BCRM_BASE);

	LOG_DBG("%s , g_APU_CONN_BASE 0x%p\n", __func__, g_APU_CONN_BASE);
	LOG_DBG("%s , g_APU_VPU0_BASE 0x%p\n", __func__, g_APU_VPU0_BASE);
	LOG_DBG("%s , g_APU_VPU1_BASE 0x%p\n", __func__, g_APU_VPU1_BASE);
	LOG_DBG("%s , g_APU_VPU2_BASE 0x%p\n", __func__, g_APU_VPU2_BASE);
	LOG_DBG("%s , g_APU_MDLA0_BASE 0x%p\n", __func__, g_APU_MDLA0_BASE);
	LOG_DBG("%s , g_APU_MDLA1_BASE 0x%p\n", __func__, g_APU_MDLA1_BASE);
	LOG_DBG("%s , g_APU_SPM_BASE 0x%p\n", __func__, g_APU_SPM_BASE);

	if (!is_apu_power_initilized) {
		prepare_apu_regulator(dev, 1);
#ifndef MTK_FPGA_PORTING
		prepare_apu_clock(dev);
#endif
		is_apu_power_initilized = 1;
	}
	enable_apu_vcore_clksrc();
	enable_apu_conn_clksrc();
	hw_init_setting();
	set_apu_clock_source(VCORE_OFF_FREQ, V_VCORE);
	disable_apu_conn_clksrc();

	buck_control(VPU0, 3); // buck on
	udelay(100);
	buck_control(VPU0, 0); // buck off

	return 0;
}

static int segment_user_support_check(void *param)
{
	uint32_t val = 0;
	struct hal_param_seg_support *seg_info =
		(struct hal_param_seg_support *)param;

	seg_info->support = true;
	seg_info->seg = SEGMENT_1;

#if defined(CONFIG_MACH_MT6893)
	val = get_devinfo_with_index(30);
	if (val == 0x10) {
		seg_info->seg = SEGMENT_0;
		pr_info("%s segment:0x%x\n", __func__, (unsigned int)val);
	}
#else
	val = get_devinfo_with_index(30);
	if (val == 0x1) {
		seg_info->seg = SEGMENT_0;
		if (seg_info->user == VPU2 || seg_info->user == MDLA1)
			seg_info->support = false;
	} else if (val == 0x10)
		seg_info->seg = SEGMENT_2;
#endif
	if (seg_info->support == false)
		LOG_INF("%s user=%d, support=%d\n", __func__,
		seg_info->user, seg_info->support);

	return 0;
}

#if defined(CONFIG_MACH_MT6893)
static int binning_init;
#if BINNING_UT
static int global_test_efuse_bin;
static int global_test_efuse_raise;
static struct apusys_dvfs_steps opps_backup[
		APUSYS_MAX_NUM_OPPS][APUSYS_BUCK_DOMAIN_NUM];
#endif

// set upper bound of clk_path_max_vol for valid dvfs user
static void update_clk_path_max_vol(enum DVFS_USER user, enum DVFS_VOLTAGE volt)
{
	// CAUTION: have to ensure VPU0 has already run the func before MDLA0/1

	if (user == MDLA0 || user == MDLA1) {
		dvfs_clk_path_max_vol[user][0] = volt;
		// CONN & IOMMU & VPU share vvpu buck, so the max volt
		// is different with vmdla
		dvfs_clk_path_max_vol[user][1] = dvfs_clk_path_max_vol[VPU0][1];
		dvfs_clk_path_max_vol[user][2] = dvfs_clk_path_max_vol[VPU0][2];

	} else {

		dvfs_clk_path_max_vol[user][0] = volt;

		/*
		 * Index[1] is APUCONN and APUCONN use vvpu as well.
		 * That is why also modify index[1]'s upper bound here.
		 */
		dvfs_clk_path_max_vol[user][1] = volt;

		/*
		 * Index[2] is APU_IOMMU and APU_IOMMU use vvpu as well.
		 * That is why also modify index[2]'s upper bound here.
		 */
		dvfs_clk_path_max_vol[user][2] = volt;
	}
}

#ifndef AGING_MARGIN
static void aging_support_check(int opp, enum DVFS_VOLTAGE_DOMAIN bk_dmn) {}
#else
/*
 * aging_support_check() - Brief description of aging_support_check.
 * @opp: opp to check
 * @bk_dmn: buck domain to check
 *
 * Comparing whether freq of opp on the buck domain matches aging freq.
 * If yes, voltage of opp on the buck domain will minus aging voltage.
 * (so far only support vpu buck domains)
 *
 * Return void.
 */
static void aging_support_check(int opp, enum DVFS_VOLTAGE_DOMAIN bk_dmn)
{
	enum DVFS_USER user = 0;
	enum DVFS_FREQ ag_freq = 0;
	enum DVFS_FREQ seg_freq = 0;
	int seg_volt = 0;
	int ag_volt = 0;
	int ag_opp_idx = 0;

	struct apusys_dvfs_constraint *dvfs_ctrn = NULL;
	int idx = 0;
	enum DVFS_BUCK buck;

	/* only support VPU for aging */
	if (bk_dmn > V_VCORE)
		LOG_ERR("%s %s opp %d not support aging volt\n",
				__func__, buck_domain_str[bk_dmn], opp);

	user = apusys_buck_domain_to_user[bk_dmn];
	seg_freq = apusys_opps.opps[opp][bk_dmn].freq;
	seg_volt = apusys_opps.opps[opp][bk_dmn].voltage;
	buck = apusys_buck_domain_to_buck[bk_dmn];

	/*
	 * Brute-force searching whether seg_freq meet
	 * any aging freq in aging_tbl array
	 * note: bypass last opp to prevent voltage be reduced twice
	 */
	for (ag_opp_idx = 0; ag_opp_idx < APUSYS_MAX_NUM_OPPS - 1
		; ag_opp_idx++) {

		if (get_devinfo_with_index(30) == 0x10) {
			// SEGMENT_0
			ag_freq = aging_tbl_b0[ag_opp_idx][bk_dmn].freq;
			ag_volt = aging_tbl_b0[ag_opp_idx][bk_dmn].volt;
		} else {
			// SEGMENT_1
			ag_freq = aging_tbl_b1[ag_opp_idx][bk_dmn].freq;
			ag_volt = aging_tbl_b1[ag_opp_idx][bk_dmn].volt;
		}

		/*
		 * if setment freqs matchs aging freq,
		 * minus aging voltage and break
		 */
		if (ag_freq == seg_freq) {
			apusys_opps.opps[opp][bk_dmn].voltage -= ag_volt;
			LOG_DBG("%s %s opp%d(%d, %d) hit ag(%d,%d) end v %d\n",
				__func__, buck_domain_str[bk_dmn], opp,
				seg_freq, seg_volt, ag_freq, ag_volt,
				apusys_opps.opps[opp][bk_dmn].voltage);

			if (opp == 0 && user < APUSYS_DVFS_USER_NUM) {
				// set upper bound of clk_path_max_vol
				// for valid dvfs user
				update_clk_path_max_vol(user,
					apusys_opps.opps[opp][bk_dmn].voltage);
			}

			for (idx = 0; idx < APUSYS_DVFS_CONSTRAINT_NUM; idx++) {
				dvfs_ctrn = &dvfs_constraint_table[idx];
				if (dvfs_ctrn->buck0 == buck) {
					/* minus aging volt */
					if (dvfs_ctrn->voltage0 == seg_volt)
						dvfs_ctrn->voltage0 -= ag_volt;
				}
				if (dvfs_ctrn->buck1 == buck) {
					/* minus aging volt */
					if (dvfs_ctrn->voltage1 == seg_volt)
						dvfs_ctrn->voltage1 -= ag_volt;
				}
			}
			break;
		}
	}
}
#endif

#if BINNING_VOLTAGE_SUPPORT || VOLTAGE_RAISE_UP

/**
 * get_bin_raise_voltage() - calculate binning/raising voltage
 * @bin_efuse: binning efuse value
 * @raise_efuse: raising efuse value
 * @bin_mv: return binning voltage
 * @raise_mv: return raising voltage
 *
 * Based on binning/raising efuse, return voltage upper bound, bin_mv,
 * and lower bound, raise_mv, to caller.
 */
static void get_bin_raise_voltage(enum DVFS_BUCK buck, int bin_efuse,
	int raise_efuse, enum DVFS_VOLTAGE *bin_mv, enum DVFS_VOLTAGE *raise_mv)
{
	if (buck == VPU_BUCK) {
		/* Binning voltage check */
		if (bin_efuse == 4)
			*bin_mv = DVFS_VOLT_00_775000_V;
		else if (bin_efuse == 5)
			*bin_mv = DVFS_VOLT_00_762500_V;
		else if (bin_efuse == 6)
			*bin_mv = DVFS_VOLT_00_750000_V;

		/* Raising voltage check */
		if (raise_efuse == 1)
			*raise_mv = DVFS_VOLT_00_600000_V; // 0.575 + 25mV
		else if (raise_efuse == 2)
			*raise_mv = DVFS_VOLT_00_625000_V; // 0.575 + 50mV

	} else if (buck == MDLA_BUCK) {
		/* Binning voltage check */
		if (bin_efuse == 4)
			*bin_mv = DVFS_VOLT_00_800000_V;
		else if (bin_efuse == 5)
			*bin_mv = DVFS_VOLT_00_787500_V;
		else if (bin_efuse == 6)
			*bin_mv = DVFS_VOLT_00_775000_V;

		/* Raising voltage check */
		if (raise_efuse == 1)
			*raise_mv = DVFS_VOLT_00_600000_V; // 0.575 + 25mV
		else if (raise_efuse == 2)
			*raise_mv = DVFS_VOLT_00_625000_V; // 0.575 + 50mV

	} else {
		LOG_ERR("%s invalid buck : %d\n", __func__, buck);
	}

	pr_info("%s bin_mv:%d, raise_mv:%d\n", __func__, *bin_mv, *raise_mv);
}

/*
 * <Input> i1: low freq, b1: low volt, i2: high freq, b2: high volt, i: mid freq
 * <Return> corresponding mid voltage of mid freq
 * <Example> interpolation_volt(275000, 575000, 832000, 800000, 728000);
 */
static int interpolation_volt(int i1, int b1, int i2, int b2, int i)
{
	int ret;
	int scaling_ratio = 1000;
	int normalize = 6250; // 0.00625
	int tmp1, tmp2;

	tmp1 = DIV_ROUND_CLOSEST((i - i1) * scaling_ratio, i2 - i1);
	tmp2 = ((b2 - b1) * tmp1) / scaling_ratio + b1;
	ret = DIV_ROUND_UP(tmp2, normalize) * normalize;

	return ret;
}

static enum DVFS_VOLTAGE cal_suitable_bin_volt(
		int opp, enum DVFS_VOLTAGE_DOMAIN bk_domain,
		enum DVFS_VOLTAGE bin_volt, enum DVFS_VOLTAGE raise_volt,
		int low_bound)
{
	int ret;
	int vp6_volt;
	int iommu_volt;
	int top_volt;

	if (bk_domain == V_VPU0 || bk_domain == V_VPU1 || bk_domain == V_VPU2
		|| bk_domain == V_APU_CONN || bk_domain == V_TOP_IOMMU) {

		vp6_volt = interpolation_volt(
			apusys_opps.opps[
			APUSYS_MAX_NUM_OPPS - low_bound][V_VPU0].freq,
			raise_volt,
			apusys_opps.opps[0][V_VPU0].freq,
			bin_volt,
			apusys_opps.opps[opp][V_VPU0].freq);

		top_volt = interpolation_volt(
			apusys_opps.opps[
			APUSYS_MAX_NUM_OPPS - low_bound][V_APU_CONN].freq,
			raise_volt,
			apusys_opps.opps[0][V_APU_CONN].freq,
			bin_volt,
			apusys_opps.opps[opp][V_APU_CONN].freq);

		iommu_volt = interpolation_volt(
			apusys_opps.opps[
			APUSYS_MAX_NUM_OPPS - low_bound][V_TOP_IOMMU].freq,
			raise_volt,
			apusys_opps.opps[0][V_TOP_IOMMU].freq,
			bin_volt,
			apusys_opps.opps[opp][V_TOP_IOMMU].freq);

		ret = MAX(MAX(vp6_volt, top_volt), iommu_volt);

	} else if (bk_domain == V_MDLA0 || bk_domain == V_MDLA1) {
		ret = interpolation_volt(
			apusys_opps.opps[
			APUSYS_MAX_NUM_OPPS - low_bound][V_MDLA0].freq,
			raise_volt,
			apusys_opps.opps[0][V_MDLA0].freq,
			bin_volt,
			apusys_opps.opps[opp][V_MDLA0].freq);

	} else {
		LOG_ERR("%s, invalid bk_domain : %d\n", __func__, bk_domain);
		ret = 0;
	}

	return ret;
}

/**
 * change_constrain_volt() - change constrains voltage upper/lower bound
 * @bk_domain: which buck's opp need to modify
 * @bin_mv: upper bound voltage
 * @raise_mv: lower bound voltage
 *
 * Modify upper/lower voltage bound of constrain's opp.
 */
static void change_constrain_volt(enum DVFS_BUCK buck,
				  enum DVFS_VOLTAGE *bin_mv,
				  enum DVFS_VOLTAGE *raise_mv)
{
	int idx = 0;
	struct apusys_dvfs_constraint *dvfs_ctrn = NULL;

	for (idx = 0; idx < APUSYS_DVFS_CONSTRAINT_NUM; idx++) {
		dvfs_ctrn = &dvfs_constraint_table[idx];
		if (dvfs_ctrn->buck0 == buck) {
			/* set upper bound as binning voltage */
			if (dvfs_ctrn->voltage0 > *bin_mv)
				dvfs_ctrn->voltage0 = *bin_mv;

			/* set lower bound as raising voltage */
			if (dvfs_ctrn->voltage0 < *raise_mv)
				dvfs_ctrn->voltage0 = *raise_mv;
		}
		if (dvfs_ctrn->buck1 == buck) {
			/* set upper bound as binning voltage */
			if (dvfs_ctrn->voltage1 > *bin_mv)
				dvfs_ctrn->voltage1 = *bin_mv;

			/* set lower bound as raising voltage */
			if (dvfs_ctrn->voltage1 < *raise_mv)
				dvfs_ctrn->voltage1 = *raise_mv;
		}
	}
}

/**
 * change_opp_voltage() - change opp's voltage upper/lower bound
 * @bk_domain: which buck domain's opp need to modify
 * @bin_mv: upper bound voltage
 * @raise_mv: lower bound voltage
 *
 * Modify upper/lower voltage bound of buck domain's opp.
 */
static void change_opp_voltage(enum DVFS_VOLTAGE_DOMAIN bk_domain,
			enum DVFS_VOLTAGE *bin_mv, enum DVFS_VOLTAGE *raise_mv)
{
	int opp = 0;
	int tmp1 = 0, tmp2 = 0, tmp3 = 0, final_min_volt = 0;
	enum DVFS_USER user;
	enum DVFS_VOLTAGE check1 = DVFS_VOLT_NOT_SUPPORT;
	enum DVFS_VOLTAGE check2 = DVFS_VOLT_00_750000_V;
	enum DVFS_VOLTAGE check3 = DVFS_VOLT_00_700000_V;
	enum DVFS_VOLTAGE check_cmp = DVFS_VOLT_00_650000_V;

	// config raise volt first no matter binning or not
	apusys_opps.opps[APUSYS_MAX_NUM_OPPS - 1][bk_domain].voltage =
								*raise_mv;
	// no binning, no need to update opp table
	if (apusys_opps.opps[0][bk_domain].voltage == *bin_mv)
		return;

	user = apusys_buck_domain_to_user[bk_domain];

	/* set upper bound of clk_path_max_vol for valid dvfs user*/
	if (user < APUSYS_DVFS_USER_NUM)
		update_clk_path_max_vol(user, *bin_mv);

	if (bk_domain == V_MDLA0 || bk_domain == V_MDLA1)
		check1 = DVFS_VOLT_00_800000_V;

	for (opp = 0; opp < APUSYS_MAX_NUM_OPPS; opp++) {

		// implicit we have to compare with 0.65v
		if (apusys_opps.opps[opp][bk_domain].voltage == check1
			|| apusys_opps.opps[opp][bk_domain].voltage == check2
			|| apusys_opps.opps[opp][bk_domain].voltage == check3)
			tmp3 = 1;

		// sign-off corresponding volt
		tmp1 = apusys_opps.opps[opp][bk_domain].voltage;

		// do interpolation and find MIN of buck related domain
		// low_bound is counterwise 1
		tmp2 = cal_suitable_bin_volt(
				opp, bk_domain, *bin_mv, *raise_mv, 1);

		final_min_volt = MIN(tmp1, tmp2);

		if (tmp3 == 1) {
			// 0.65v as low bound to do interpolation
			// low_bound is counterwise 2
			tmp3 = cal_suitable_bin_volt(
					opp, bk_domain, *bin_mv, check_cmp, 2);

			final_min_volt = MIN(final_min_volt, tmp3);
		}

		apusys_opps.opps[opp][bk_domain].voltage = final_min_volt;

		pr_info("%s Update Volt!!, bk:%d opp-%d vol=%d (%d, %d, %d)\n",
				__func__, bk_domain, opp,
				apusys_opps.opps[opp][bk_domain].voltage,
				tmp1, tmp2, tmp3);

		/* set upper bound as binning voltage to this buck domain */
		if (apusys_opps.opps[opp][bk_domain].voltage > *bin_mv) {
			apusys_opps.opps[opp][bk_domain].voltage = *bin_mv;
			pr_info("%s Align BinningVolt!!, bk:%d opp-%d vol=%d\n",
				__func__, bk_domain, opp,
				apusys_opps.opps[opp][bk_domain].voltage);
		}

		/* set lower bound as raising voltage to this buck domain */
		if (apusys_opps.opps[opp][bk_domain].voltage < *raise_mv) {
			apusys_opps.opps[opp][bk_domain].voltage = *raise_mv;
			pr_info("%s Align RaisingVolt!!, bk:%d opp-%d vol=%d\n",
				__func__, bk_domain, opp,
				apusys_opps.opps[opp][bk_domain].voltage);
		}
	}
}
#endif

static int binning_support_check(void)
{
	int opp = 0;
#if BINNING_VOLTAGE_SUPPORT || VOLTAGE_RAISE_UP
	unsigned int vpu_efuse_bin = 0;
	unsigned int vpu_efuse_raise = 0;
	unsigned int mdla_efuse_bin = 0;
	unsigned int mdla_efuse_raise = 0;
	enum DVFS_VOLTAGE bin_mv = 0;
	enum DVFS_VOLTAGE raise_mv = 0;
#endif
	/* opp table only need to be aging/bining/raise once */
	if (binning_init)
		goto out;

#if BINNING_VOLTAGE_SUPPORT || VOLTAGE_RAISE_UP
	vpu_efuse_bin =
		GET_BITS_VAL(14:12, get_devinfo_with_index(EFUSE_BIN));
	LOG_DBG("Vol bin: vpu_efuse=%d, efuse: 0x%x\n",
			vpu_efuse_bin, get_devinfo_with_index(EFUSE_BIN));

	vpu_efuse_raise =
		GET_BITS_VAL(2:0, get_devinfo_with_index(EFUSE_RAISE));
	LOG_DBG("Raise bin: vpu_efuse=%d, efuse: 0x%x\n",
			vpu_efuse_raise, get_devinfo_with_index(EFUSE_RAISE));

	mdla_efuse_bin =
		GET_BITS_VAL(17:15, get_devinfo_with_index(EFUSE_BIN));
	LOG_DBG("Vol bin: mdla_efuse=%d, efuse: 0x%x\n",
			mdla_efuse_bin, get_devinfo_with_index(EFUSE_BIN));

	mdla_efuse_raise =
		GET_BITS_VAL(2:0, get_devinfo_with_index(EFUSE_RAISE));
	LOG_DBG("Raise bin: mdla_efuse=%d, efuse: 0x%x\n",
			mdla_efuse_raise, get_devinfo_with_index(EFUSE_RAISE));

#if BINNING_UT
	vpu_efuse_bin = global_test_efuse_bin;
	vpu_efuse_raise = global_test_efuse_raise;
#endif
	// sign-off voltage will be treated as default value first
	bin_mv = apusys_opps.opps[0][V_VPU0].voltage;
	raise_mv = apusys_opps.opps[APUSYS_MAX_NUM_OPPS - 1][V_VPU0].voltage;
	get_bin_raise_voltage(VPU_BUCK, vpu_efuse_bin, vpu_efuse_raise,
							&bin_mv, &raise_mv);

	if (vpu_efuse_bin > 3 || vpu_efuse_raise > 0) {
		if (vpu_efuse_bin > 3)
			LOG_ERR("Vol bin: vpu_efuse=%d\n", vpu_efuse_bin);
		if (vpu_efuse_raise > 0)
			LOG_ERR("Raise bin: vpu_efuse=%d\n", vpu_efuse_raise);

		change_opp_voltage(V_VPU0, &bin_mv, &raise_mv);
		change_opp_voltage(V_VPU1, &bin_mv, &raise_mv);
		change_opp_voltage(V_VPU2, &bin_mv, &raise_mv);

		/* APU_CONN & APU_IOMMU share vvpu with VPU0/1/2 */
		change_opp_voltage(V_APU_CONN, &bin_mv, &raise_mv);
		change_opp_voltage(V_TOP_IOMMU, &bin_mv, &raise_mv);
		/* binning and raise constrain VPU buck */
		change_constrain_volt(VPU_BUCK, &bin_mv, &raise_mv);
	}

#if BINNING_UT
	mdla_efuse_bin = global_test_efuse_bin;
	mdla_efuse_raise = global_test_efuse_raise;
#endif
	// sign-off voltage will be treated as default value first
	bin_mv = apusys_opps.opps[0][V_MDLA0].voltage;
	raise_mv = apusys_opps.opps[APUSYS_MAX_NUM_OPPS - 1][V_MDLA0].voltage;
	get_bin_raise_voltage(MDLA_BUCK, mdla_efuse_bin, mdla_efuse_raise,
							&bin_mv, &raise_mv);

	if (mdla_efuse_bin > 3 || mdla_efuse_raise > 0) {
		if (mdla_efuse_bin > 3)
			LOG_ERR("Vol bin: mdla_efuse=%d\n", mdla_efuse_bin);
		if (mdla_efuse_raise > 0)
			LOG_ERR("Raise bin: mdla_efuse=%d\n", mdla_efuse_raise);

		change_opp_voltage(V_MDLA0, &bin_mv, &raise_mv);
		change_opp_voltage(V_MDLA1, &bin_mv, &raise_mv);
		change_constrain_volt(MDLA_BUCK, &bin_mv, &raise_mv);
	}
#endif

	// bypass last opp to prevent voltage be reduced twice
	for (opp = 0; opp < APUSYS_MAX_NUM_OPPS - 1; opp++) {
		/* Minus aging voltage if need */
		aging_support_check(opp, V_VPU0);
		aging_support_check(opp, V_VPU1);
		aging_support_check(opp, V_VPU2);
		aging_support_check(opp, V_MDLA0);
		aging_support_check(opp, V_MDLA1);
		aging_support_check(opp, V_APU_CONN);
		aging_support_check(opp, V_TOP_IOMMU);
	}

	/* initial done */
	binning_init = 1;
out:
	return 0;
}

#else
static int binning_support_check(void)
{
	pr_info("%s bypass\n", __func__);
	return 0;
}
#endif
static int apu_pm_handler(void *param)
{
	int suspend = ((struct hal_param_pm *)param)->is_suspend;

	if (suspend) {
		LOG_WRN("%s suspend begin\n", __func__);
		// TODO: do we have any action need to be handled in suspend?
	} else {
		// TODO: do we need to call init_power_resource again in resume?
#if 1
		enable_apu_vcore_clksrc();
		enable_apu_conn_clksrc();
		set_apu_clock_source(VCORE_OFF_FREQ, V_VCORE);
		disable_apu_conn_clksrc();
#endif
		LOG_WRN("%s resume end\n", __func__);
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

	if (ret)
		LOG_ERR("%s failed(%d), buck:%d, volt:%d\n",
					__func__, ret, buck, target_volt);

#ifdef ASSERTIOM_CHECK
	voltage_constraint_check();
#endif

	return ret;
}

static int set_power_regulator_mode(void *param)
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
#if 1
	unsigned int regValue = 0;
	unsigned int finished = 1;
	unsigned int check_round = 0;

	do {
		udelay(10);
		regValue = DRV_Reg32(APU_RPC_TOP_CON);
		finished = (regValue & BIT(31));

		if (++check_round >= REG_POLLING_TIMEOUT_ROUNDS) {
			recording_power_fail_state();
			LOG_ERR("%s timeout !\n", __func__);
			break;
		}
	} while (finished);
#else
	udelay(500);
#endif
}

static unsigned int check_spm_register(struct apu_power_info *info, int log)
{
	unsigned int spm_wake_bit = DRV_Reg32(SPM_CROSS_WAKE_M01_REQ);

	if (info != NULL) {
		info->spm_wakeup = spm_wake_bit;

	} else {
		if (log) {
			LOG_PM("APUREG, SPM SPM_CROSS_WAKE_M01_REQ = 0x%x\n",
								spm_wake_bit);
			LOG_PM("APUREG, SPM OTHER_PWR_STATUS = 0x%x\n",
						DRV_Reg32(OTHER_PWR_STATUS));
			LOG_PM("APUREG, SPM BUCK_ISOLATION = 0x%x\n",
						DRV_Reg32(BUCK_ISOLATION));
		}
	}

	if (spm_wake_bit == 0x1)
		return 0x1;
	else
		return 0x0;
}

static int check_if_rpc_alive(void)
{
	unsigned int regValue = 0x0;
	int bit_offset = 26; // [31:26] is reserved for debug

	regValue = DRV_Reg32(APU_RPC_TOP_SEL);
	LOG_PM("%s , before: APU_RPC_TOP_SEL = 0x%x\n", __func__, regValue);
	regValue |= (0x3a << bit_offset);
	DRV_WriteReg32(APU_RPC_TOP_SEL, regValue);

	regValue = 0x0;
	regValue = DRV_Reg32(APU_RPC_TOP_SEL);
	LOG_PM("%s , after: APU_RPC_TOP_SEL = 0x%x\n", __func__, regValue);

	DRV_ClearBitReg32(APU_RPC_TOP_SEL, (BIT(26) | BIT(27) | BIT(28)
					| BIT(29) | BIT(30) | BIT(31)));

	return ((regValue >> bit_offset) & 0x3f) == 0x3a ? 1 : 0;
}

/*
 * domain_idx : 0 (conn), 2 (vpu0), 3 (vpu1), 4 (vpu2), 6 (mdla0), 7 (mdla1)
 * mode : 0 (disable), 1 (enable), 2 (disable mid stage)
 * explain :
 *	conn enable - check SPM flag to 0x1 only
 *	conn disable mid stage - check SPM flag to 0x0 before sleep request
 *	conn disable - check APU_RPC_INTF_PWR_RDY after sleep request
 *	other devices enable/disable - check APU_RPC_INTF_PWR_RDY only
 */
static int rpc_power_status_check(int domain_idx, unsigned int mode)
{
	unsigned int spmValue = 0x0;
	unsigned int rpcValue = 0x0;
	unsigned int chkValue = 0x0;
	unsigned int finished = 0x0;
	unsigned int check_round = 0;
	int fail_type = 0;
	int rpc_alive = 0;

	// check SPM_CROSS_WAKE_M01_REQ
	spmValue = check_spm_register(NULL, 0);

	do {
		// check APU_RPC_INTF_PWR_RDY
		rpcValue = DRV_Reg32(APU_RPC_INTF_PWR_RDY);

		if (domain_idx == 0 && mode != 0)
			chkValue = spmValue;
		else
			chkValue = rpcValue;

		if (mode == 1)
			finished = !((chkValue >> domain_idx) & 0x1);
		else // mode equals to 0 (disable) or 2 (disable mid stage)
			finished = (chkValue >> domain_idx) & 0x1;

		if (++check_round >= REG_POLLING_TIMEOUT_ROUNDS) {

			recording_power_fail_state();
			check_spm_register(NULL, 1);
			rpc_alive = check_if_rpc_alive();
			if (domain_idx == 0 && mode != 0) {
				LOG_ERR(
				"%s fail SPM Wakeup = 0x%x, idx:%d, mode:%d, ra:%d, timeout !\n",
					__func__, spmValue, domain_idx, mode,
					rpc_alive);

				apu_aee_warn(
				"APUPWR_SPM_TIMEOUT",
				"SPM Wakeup:0x%x, idx:%d, mode:%d, ra:%d timeout\n",
				spmValue, domain_idx, mode, rpc_alive);

			} else {
				LOG_ERR(
				"%s fail APU_RPC_INTF_PWR_RDY = 0x%x, idx:%d, mode:%d, ra:%d, timeout !\n",
					__func__, rpcValue, domain_idx, mode,
					rpc_alive);

				apu_aee_warn(
				"APUPWR_RPC_TIMEOUT",
				"APU_RPC_INTF_PWR_RDY:0x%x, idx:%d, mode:%d, ra:%d timeout\n",
				rpcValue, domain_idx, mode, rpc_alive);
			}

			return -1;
		}

		if (finished)
			udelay(10);

	} while (finished);

	if (domain_idx == 0) {

		if (mode == 0 && rpcValue != 0x2)
			fail_type = 1;

		if (mode == 1 && rpcValue != 0x3)
			fail_type = 2;

		if (mode != 2 && spmValue != (rpcValue & 0x1))
			fail_type = 3;
	}

	if (chkValue == rpcValue && (rpcValue >> 8) != 0x0)
		fail_type = 4;

	if (fail_type > 0) {
		check_spm_register(NULL, 1);
		rpc_alive = check_if_rpc_alive();
		LOG_ERR(
		"%s fail conn ctl type:%d, mode:%d, spm:0x%x, rpc:0x%x, ra:%d\n",
		__func__, fail_type, mode, spmValue, rpcValue, rpc_alive);

		recording_power_fail_state();
		apu_aee_warn(
			"APUPWR_RPC_CHK_FAIL",
			"type:%d, mode:%d, spm:0x%x, rpc:0x%x, ra:%d\n",
			fail_type, mode, spmValue, rpcValue, rpc_alive);
#if 1
		return -1;
#endif
	}

	if (domain_idx == 0 && mode != 0)
		LOG_DBG("%s SPM Wakeup = 0x%x (idx:%d, mode:%d)\n",
					__func__, spmValue, domain_idx, mode);
	else
		LOG_DBG("%s APU_RPC_INTF_PWR_RDY = 0x%x (idx:%d, mode:%d)\n",
					__func__, rpcValue, domain_idx, mode);
	return 0;
}

static int set_domain_to_default_clk(int domain_idx)
{
	int ret = 0;

	if (domain_idx == 2)
		ret = set_apu_clock_source(
				BUCK_VVPU_DOMAIN_DEFAULT_FREQ, V_VPU0);
	else if (domain_idx == 3)
		ret = set_apu_clock_source(
				BUCK_VVPU_DOMAIN_DEFAULT_FREQ, V_VPU1);
	else if (domain_idx == 4)
		ret = set_apu_clock_source(
				BUCK_VVPU_DOMAIN_DEFAULT_FREQ, V_VPU2);
	else if (domain_idx == 6)
		ret = config_apupll(
				BUCK_VMDLA_DOMAIN_DEFAULT_FREQ, V_MDLA0);
	else if (domain_idx == 7)
		ret = config_apupll(
				BUCK_VMDLA_DOMAIN_DEFAULT_FREQ, V_MDLA1);
	else {
		ret = set_apu_clock_source(BUCK_VCONN_DOMAIN_DEFAULT_FREQ,
								V_APU_CONN);
		ret |= set_apu_clock_source(BUCK_VIOMMU_DOMAIN_DEFAULT_FREQ,
								V_TOP_IOMMU);
	}

	return ret;

}

static int set_power_mtcmos(enum DVFS_USER user, void *param)
{
	unsigned int enable = ((struct hal_param_mtcmos *)param)->enable;
	unsigned int domain_idx = 0;
	unsigned int regValue = 0;
	int retry = 0;
	int ret = 0;

	LOG_DBG("%s , user: %d , enable: %d\n", __func__, user, enable);

	if (user == EDMA || user == EDMA2 || user == REVISER)
		domain_idx = 0;
	else if (user == VPU0)
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
		LOG_WRN("%s not support user : %d\n", __func__, user);

	if (enable) {
		// call spm api to enable wake up signal for apu_conn/apu_vcore
		if (force_pwr_on) {
			LOG_DBG("%s enable wakeup signal\n", __func__);

			ret |= enable_apu_conn_clksrc();
			ret |= set_apu_clock_source(VCORE_ON_FREQ,
								V_VCORE);

			// CCF API assist to enable clock source of apu conn
			ret |= enable_apu_mtcmos(1);

			// wait for conn mtcmos enable ready
			ret |= rpc_power_status_check(0, 1);

			// clear inner dummy CG (true enable but bypass disable)
			ret |= enable_apu_conn_vcore_clock();

			force_pwr_on = 0;
			conn_mtcmos_on = 1;
		}

		// EDMA do not need to control mtcmos by rpc
		if (user < APUSYS_DVFS_USER_NUM && !ret) {
			// enable clock source of this device first
			ret |= enable_apu_device_clksrc(user);

			do {
				rpc_fifo_check();
				// BIT(4) to Power on
				DRV_WriteReg32(APU_RPC_SW_FIFO_WE,
					(domain_idx | BIT(4)));
				LOG_DBG("%s APU_RPC_SW_FIFO_WE write 0x%x\n",
					__func__,
					(unsigned int)(domain_idx | BIT(4)));

				if (retry >= 3) {
					LOG_ERR("%s fail (user:%d, mode:%d)\n",
							__func__, user, enable);
					disable_apu_device_clksrc(user);
					return -1;
				}
				retry++;
			} while (rpc_power_status_check(domain_idx, enable));
		}

	} else {

		// EDMA do not need to control mtcmos by rpc
		if (user < APUSYS_DVFS_USER_NUM) {
			do {
				rpc_fifo_check();
				DRV_WriteReg32(APU_RPC_SW_FIFO_WE, domain_idx);
				LOG_DBG("%s APU_RPC_SW_FIFO_WE write %u\n",
					__func__, domain_idx);

				if (retry >= 3) {
					LOG_ERR("%s fail (user:%d, mode:%d)\n",
							__func__, user, enable);
					return -1;
				}
				retry++;
			} while (rpc_power_status_check(domain_idx, enable));

			ret |= set_domain_to_default_clk(domain_idx);
			// disable clock source of this device
			disable_apu_device_clksrc(user);
		}

		// only remained apu_top is power on
		if (force_pwr_off) {
		/*
		 * call spm api to disable wake up signal
		 * for apu_conn/apu_vcore
		 */
			// inner dummy cg won't be gated when you call disable
			disable_apu_conn_vcore_clock();

			ret |= enable_apu_mtcmos(0);
			//udelay(100);

			// conn disable mid stage, checking SPM flag
			ret |= rpc_power_status_check(0, 2);

			// mask RPC IRQ and bypass WFI
			regValue = DRV_Reg32(APU_RPC_TOP_SEL);
			regValue |= 0x9E;
			DRV_WriteReg32(APU_RPC_TOP_SEL, regValue);

			// sleep request enable
			// CAUTION!! do NOT request sleep twice in succession
			// or system may crash (comments from DE)
			regValue = DRV_Reg32(APU_RPC_TOP_CON);
			regValue |= 0x1;
			DRV_WriteReg32(APU_RPC_TOP_CON, regValue);

			// conn disable, checking APU_RPC_INTF_PWR_RDY
			ret |= rpc_power_status_check(0, 0);

			ret |= set_apu_clock_source(VCORE_OFF_FREQ,
								V_VCORE);

			ret |= set_domain_to_default_clk(0);
			disable_apu_conn_clksrc();

			force_pwr_off = 0;
			conn_mtcmos_on = 0;
		}
	}

	return ret;
}

static int set_power_clock(enum DVFS_USER user, void *param)
{
	int ret = 0;
#ifndef MTK_FPGA_PORTING
	int enable = ((struct hal_param_clk *)param)->enable;

	LOG_DBG("%s , user: %d , enable: %d\n", __func__, user, enable);

	if (enable)
		ret = enable_apu_device_clock(user);
	else
		// inner dummy cg won't be gated when you call disable
		disable_apu_device_clock(user);
#endif
	return ret;
}

static int set_power_frequency(void *param)
{
	enum DVFS_VOLTAGE_DOMAIN domain = 0;
	enum DVFS_FREQ freq = 0;
	int ret = 0;

	freq = ((struct hal_param_freq *)param)->target_freq;
	domain = ((struct hal_param_freq *)param)->target_volt_domain;

	if (domain < APUSYS_BUCK_DOMAIN_NUM) {
		if (domain == V_MDLA0 || domain == V_MDLA1)
			ret = config_apupll(freq, domain);
		else
			ret = set_apu_clock_source(freq, domain);
	} else {
		LOG_ERR("%s not support power domain : %d\n", __func__, domain);
	}

	if (ret)
		LOG_ERR("%s failed(%d), domain:%d, freq:%d\n",
					__func__, ret, domain, freq);
	return ret;
}

static void get_current_power_info(void *param, int force)
{
	struct apu_power_info *info = ((struct apu_power_info *)param);
	char log_str[128];
	unsigned int mdla_0 = 0, mdla_1 = 0;
	unsigned long rem_nsec;
	int ret = 0;

	info->dump_div = 1000;

	// including APUsys related buck
	dump_voltage(info);

	// including APUsys related freq
	dump_frequency(info);

	mdla_0 = (apu_get_power_on_status(MDLA0)) ? info->dsp6_freq : 0;
	mdla_1 = (apu_get_power_on_status(MDLA1)) ? info->dsp6_freq : 0;

	/* calcluate the remainder nanosecond */
	rem_nsec = do_div(info->id, 1000000000);

	if (info->type == 1) {
		// including APUsys pwr related reg
		apusys_power_reg_dump(info, force);

		// including SPM related pwr reg
		check_spm_register(info, 0);

		ret = snprintf(log_str, sizeof(log_str),
			"v[%u,%u,%u,%u]f[%u,%u,%u,%u,%u,%u,%u]r[%x,%x,%x,%x,%x,%x,%x,%x,%x][%5lu.%06lu]",
			info->vvpu, info->vmdla, info->vcore, info->vsram,
			info->dsp_freq, info->dsp1_freq, info->dsp2_freq,
			info->dsp3_freq, info->dsp6_freq, info->dsp7_freq,
			info->ipuif_freq, info->spm_wakeup, info->rpc_intf_rdy,
			info->vcore_cg_stat, info->conn_cg_stat,
			info->vpu0_cg_stat, info->vpu1_cg_stat,
			info->vpu2_cg_stat, info->mdla0_cg_stat,
			info->mdla1_cg_stat,
			(unsigned long)info->id, rem_nsec / 1000);
	} else {
		ret = snprintf(log_str, sizeof(log_str),
			"v[%u,%u,%u,%u]f[%u,%u,%u,%u,%u,%u,%u][%5lu.%06lu]",
			info->vvpu, info->vmdla, info->vcore, info->vsram,
			info->dsp_freq, info->dsp1_freq, info->dsp2_freq,
			info->dsp3_freq, info->dsp6_freq, info->dsp7_freq,
			info->ipuif_freq,
			(unsigned long)info->id, rem_nsec / 1000);
	}

	trace_APUSYS_DFS(info, mdla_0, mdla_1);

	if (ret >= 0) {
		if (info->force_print)
			LOG_ERR("APUPWR %s\n", log_str);
		else
			LOG_PM("APUPWR %s\n", log_str);

		LOG_DUMP("APUPWR %s\n", log_str); // debug ring buffer
	}
}

static int uninit_power_resource(void)
{
	if (is_apu_power_initilized) {
		buck_control(VPU0, 0); // buck off
		buck_already_on = 0;
		udelay(100);
#ifndef MTK_FPGA_PORTING
		unprepare_apu_clock();
#endif
		prepare_apu_regulator(NULL, 0);
		is_apu_power_initilized = 0;
	}

	return 0;
}

/*
 * control buck to four different levels -
 *	level 3 : buck ON
 *	level 2 : buck to default voltage
 *	level 1 : buck to low voltage
 *	level 0 : buck OFF
 */
static int buck_control(enum DVFS_USER user, int level)
{
	struct hal_param_volt vpu_volt_data;
	struct hal_param_volt mdla_volt_data;
	struct hal_param_volt vcore_volt_data;
	struct hal_param_volt sram_volt_data;
	struct apu_power_info info = {0};
	int ret = 0;

	LOG_DBG("%s begin, level = %d\n", __func__, level);

	if (level == 3) { // buck ON
		// just turn on buck
		enable_regulator(VPU_BUCK);
		enable_regulator(MDLA_BUCK);
		enable_regulator(SRAM_BUCK);

		// release buck isolation
		DRV_ClearBitReg32(BUCK_ISOLATION, (BIT(0) | BIT(5)));

	} else if (level == 2) { // default voltage

		vcore_volt_data.target_buck = VCORE_BUCK;
		vcore_volt_data.target_volt = VCORE_DEFAULT_VOLT;
		ret |= set_power_voltage(VPU0, (void *)&vcore_volt_data);

		#if SUPPORT_VCORE_TO_IPUIF
		vcore_volt_data.target_buck = VCORE_BUCK;
		vcore_volt_data.target_volt = VCORE_DEFAULT_VOLT;
		ret |= set_power_voltage(MDLA0, (void *)&vcore_volt_data);
		apusys_opps.driver_apu_vcore = VCORE_DEFAULT_VOLT;
		apusys_opps.qos_apu_vcore = apusys_opps.driver_apu_vcore;
		#endif

		vpu_volt_data.target_buck = VPU_BUCK;
		vpu_volt_data.target_volt = VSRAM_TRANS_VOLT;
		ret |= set_power_voltage(user, (void *)&vpu_volt_data);

		mdla_volt_data.target_buck = MDLA_BUCK;
		mdla_volt_data.target_volt = VSRAM_TRANS_VOLT;
		ret |= set_power_voltage(user, (void *)&mdla_volt_data);

		sram_volt_data.target_buck = SRAM_BUCK;
		sram_volt_data.target_volt = VSRAM_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&sram_volt_data);

		vpu_volt_data.target_buck = VPU_BUCK;
		vpu_volt_data.target_volt = VVPU_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&vpu_volt_data);

		mdla_volt_data.target_buck = MDLA_BUCK;
		mdla_volt_data.target_volt = VMDLA_DEFAULT_VOLT;
		ret |= set_power_voltage(user, (void *)&mdla_volt_data);

	} else {

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
		sram_volt_data.target_volt = VSRAM_TRANS_VOLT;
		ret |= set_power_voltage(user, (void *)&sram_volt_data);

		vcore_volt_data.target_buck = VCORE_BUCK;
		vcore_volt_data.target_volt = VCORE_SHUTDOWN_VOLT;
		ret |= set_power_voltage(VPU0,
				(void *)&vcore_volt_data);

		#if SUPPORT_VCORE_TO_IPUIF
		vcore_volt_data.target_buck = VCORE_BUCK;
		vcore_volt_data.target_volt = VCORE_SHUTDOWN_VOLT;
		ret |= set_power_voltage(MDLA0,
				(void *)&vcore_volt_data);
		apusys_opps.driver_apu_vcore = VCORE_SHUTDOWN_VOLT;
		apusys_opps.qos_apu_vcore = apusys_opps.driver_apu_vcore;
		#endif

		if (level == 1) { // buck adjust to low voltage
			/*
			 * then adjust vmdla/vvpu again to real default voltage
			 */
			mdla_volt_data.target_buck = MDLA_BUCK;
			mdla_volt_data.target_volt = VMDLA_SHUTDOWN_VOLT;
			ret |= set_power_voltage(user, (void *)&mdla_volt_data);

			vpu_volt_data.target_buck = VPU_BUCK;
			vpu_volt_data.target_volt = VVPU_SHUTDOWN_VOLT;
			ret |= set_power_voltage(user, (void *)&vpu_volt_data);

		} else { // buck OFF
			// enable buck isolation
			DRV_SetBitReg32(BUCK_ISOLATION, (BIT(0) | BIT(5)));

			// just turn off buck and don't release regulator handle
			disable_regulator(SRAM_BUCK);
			disable_regulator(VPU_BUCK);
			disable_regulator(MDLA_BUCK);
		}
	}

	info.dump_div = 1000;
	info.id = 0;
	dump_voltage(&info);

	LOG_DBG("%s end, level = %d\n", __func__, level);
	return ret;
}

static int set_power_boot_up(enum DVFS_USER user, void *param)
{
	struct hal_param_mtcmos mtcmos_data;
	struct hal_param_clk clk_data;
	int ret = 0;

	if (!buck_already_on) {
		buck_control(user, 3); // buck on
		buck_already_on = 1;
		udelay(100);
	}

	if (power_on_counter == 0) {

		buck_control(user, 2); // default voltage

		force_pwr_on = 1;
	}

	// Set mtcmos enable
	mtcmos_data.enable = 1;
	ret = set_power_mtcmos(user, (void *)&mtcmos_data);

	if (!ret && user < APUSYS_DVFS_USER_NUM) {
		// Set cg enable
		clk_data.enable = 1;
		ret |= set_power_clock(user, (void *)&clk_data);
	}

	if (ret)
		LOG_ERR("%s fail, ret = %d\n", __func__, ret);
	else
		LOG_DBG("%s pass, ret = %d\n", __func__, ret);

	power_on_counter++;
	return ret;
}


static int set_power_shut_down(enum DVFS_USER user, void *param)
{
	struct hal_param_mtcmos mtcmos_data;
	struct hal_param_clk clk_data;
	int ret = 0;
	int timeout_round = 0;

	if (user < APUSYS_DVFS_USER_NUM) {

		// power off should be later until DVFS completed
		while (hal_cmd_status[user]) {
			if (timeout_round >= 50) {
				LOG_ERR(
				"%s, user:%d wait for hal_cmd:%d finish timeout !",
				__func__, user, hal_cmd_status[user]);
				break;
			}

			udelay(100);
			timeout_round++;
		}

		// inner dummy cg won't be gated when you call disable
		clk_data.enable = 0;
		ret = set_power_clock(user, (void *)&clk_data);
	}

	if (power_on_counter == 1)
		force_pwr_off = 1;

	// Set mtcmos disable
	mtcmos_data.enable = 0;
	ret |= set_power_mtcmos(user, (void *)&mtcmos_data);

	if (power_on_counter == 1 && buck_already_on) {
		buck_control(user, 0); // buck off
		buck_already_on = 0;
	}

	if (ret)
		LOG_ERR("%s fail, ret = %d\n", __func__, ret);
	else
		LOG_DBG("%s pass, ret = %d\n", __func__, ret);

	power_on_counter--;
	return ret;
}

static int apusys_power_reg_dump(struct apu_power_info *info, int force)
{
	unsigned int regVal = 0x0;
	unsigned int tmpVal = 0x0;

	// FIXME: remove this code if 26MHz always on is ready after resume
#if 1
	if (force == 0 && conn_mtcmos_on == 0) {
		LOG_WRN("APUREG dump bypass (conn mtcmos off)\n");
		if (info != NULL) {
			info->rpc_intf_rdy = 0xdb;
			info->vcore_cg_stat = 0xdb;
			info->conn_cg_stat = 0xdb;
			info->vpu0_cg_stat = 0xdb;
			info->vpu1_cg_stat = 0xdb;
			info->vpu2_cg_stat = 0xdb;
			info->mdla0_cg_stat = 0xdb;
			info->mdla1_cg_stat = 0xdb;
		}
		return -1;
	}
#else
	// keep 26M vcore clk make we can dump reg directly
#endif
	// dump mtcmos status
	regVal = DRV_Reg32(APU_RPC_INTF_PWR_RDY);
	if (info != NULL)
		info->rpc_intf_rdy = regVal;
	else
		LOG_WRN(
		"APUREG APU_RPC_INTF_PWR_RDY = 0x%x, conn_mtcmos_on = %d\n",
							regVal, conn_mtcmos_on);

	if (((regVal & BIT(0))) == 0x1) {
		tmpVal = DRV_Reg32(APU_VCORE_CG_CON);
		if (info != NULL)
			info->vcore_cg_stat = tmpVal;
		else
			LOG_WRN("APUREG APU_VCORE_CG_CON = 0x%x\n", tmpVal);

		tmpVal = DRV_Reg32(APU_CONN_CG_CON);
		if (info != NULL)
			info->conn_cg_stat = tmpVal;
		else
			LOG_WRN("APUREG APU_CONN_CG_CON = 0x%x\n", tmpVal);

	} else {
		if (info != NULL) {
			info->vcore_cg_stat = 0xdb;
			info->conn_cg_stat = 0xdb;
		} else {
			LOG_WRN(
			"APUREG conn_vcore mtcmos not ready, bypass CG dump\n");
		}
		return -1;
	}

	if (((regVal & BIT(2)) >> 2) == 0x1) {
		tmpVal = DRV_Reg32(APU0_APU_CG_CON);
		if (info != NULL)
			info->vpu0_cg_stat = tmpVal;
		else
			LOG_WRN("APUREG APU0_APU_CG_CON = 0x%x\n", tmpVal);

	} else {
		if (info != NULL)
			info->vpu0_cg_stat = 0xdb;
		else
			LOG_WRN(
			"APUREG vpu0 mtcmos not ready, bypass CG dump\n");
	}

	if (((regVal & BIT(3)) >> 3) == 0x1) {
		tmpVal = DRV_Reg32(APU1_APU_CG_CON);
		if (info != NULL)
			info->vpu1_cg_stat = tmpVal;
		else
			LOG_WRN("APUREG APU1_APU_CG_CON = 0x%x\n", tmpVal);

	} else {
		if (info != NULL)
			info->vpu1_cg_stat = 0xdb;
		else
			LOG_WRN(
			"APUREG vpu1 mtcmos not ready, bypass CG dump\n");
	}

	if (((regVal & BIT(4)) >> 4) == 0x1) {
		tmpVal = DRV_Reg32(APU2_APU_CG_CON);
		if (info != NULL)
			info->vpu2_cg_stat = tmpVal;
		else
			LOG_WRN("APUREG APU2_APU_CG_CON = 0x%x\n", tmpVal);

	} else {
		if (info != NULL)
			info->vpu2_cg_stat = 0xdb;
		else
			LOG_WRN(
			"APUREG vpu2 mtcmos not ready, bypass CG dump\n");
	}

	if (((regVal & BIT(6)) >> 6) == 0x1) {
		tmpVal = DRV_Reg32(APU_MDLA0_APU_MDLA_CG_CON);
		if (info != NULL)
			info->mdla0_cg_stat = tmpVal;
		else
			LOG_WRN("APUREG APU_MDLA0_APU_MDLA_CG_CON = 0x%x\n",
									tmpVal);

	} else {
		if (info != NULL)
			info->mdla0_cg_stat = 0xdb;
		else
			LOG_WRN(
			"APUREG mdla0 mtcmos not ready, bypass CG dump\n");
	}

	if (((regVal & BIT(7)) >> 7) == 0x1) {
		tmpVal = DRV_Reg32(APU_MDLA1_APU_MDLA_CG_CON);
		if (info != NULL)
			info->mdla1_cg_stat = tmpVal;
		else
			LOG_WRN("APUREG APU_MDLA1_APU_MDLA_CG_CON = 0x%x\n",
									tmpVal);
	} else {
		if (info != NULL)
			info->mdla1_cg_stat = 0xdb;
		else
			LOG_WRN(
			"APUREG mdla1 mtcmos not ready, bypass CG dump\n");
	}

	return 0;
}

static void power_debug_func(void *param)
{
#if BINNING_UT
#if defined(CONFIG_MACH_MT6893)
	static int backup_done;
#endif
#endif
	LOG_WRN("%s begin +++\n", __func__);

#if BINNING_UT
#if defined(CONFIG_MACH_MT6893)
	if (!backup_done) {
		memcpy(opps_backup, dvfs_table_b1, sizeof(opps_backup));
		backup_done = 1;
	}

	global_test_efuse_bin = (*((uint32_t *)param) & 0xFFFF) >> 8;
	global_test_efuse_raise = *((uint32_t *)param) & 0xF;
	pr_info("%s test_binning:%d, test_raising:%d\n",
			__func__,
			global_test_efuse_bin,
			global_test_efuse_raise);

	memcpy(dvfs_table_b1, opps_backup, sizeof(dvfs_table_b1));
	apusys_opps.opps = dvfs_table_b1;
	binning_init = 0;
	binning_support_check();
#endif
#endif
	LOG_WRN("%s end ---\n", __func__);
}
