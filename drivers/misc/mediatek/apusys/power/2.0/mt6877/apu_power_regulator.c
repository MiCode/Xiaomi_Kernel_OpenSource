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
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "upmu_hw.h"
#include "apusys_power_reg.h"
#include "apu_power_api.h"
#include "apusys_power_ctl.h"
#include "apu_log.h"
#include "mtk_devinfo.h"

/* regulator id */
static struct regulator *vvpu_reg_id;
static struct regulator *vcore_reg_id;
static struct regulator *vsram_reg_id;

static int curr_vvpu_volt;
static int curr_vsram_volt;

#define MT6315VPU_UP_RATE       10000   /* 12.5mV/us - 20% */
#define MT6315VPU_DOWN_RATE     4800    /*    6mV/us - 20% */
#define MT6315VPU_CONSTRAIN     30      /* min interval between 2 cmds */

#define MT6359P_UP_RATE         11250   /* 12.5mV/us - 10% */
#define MT6359P_DOWN_RATE       4500    /*    5mV/us - 10% */
#define MT6359P_CONSTRAIN       0       /* min interval between 2 cmds */

#define VPU_BUCK_UP_RATE	MT6359P_UP_RATE
#define VPU_BUCK_DOWN_RATE	MT6359P_DOWN_RATE
#define VPU_BUCK_CONSTRAIN	MT6359P_CONSTRAIN
#define VPU_BUCK_NAME		"MT6359P"

#define SRAM_BUCK_UP_RATE        11250    /* 12.5mV/us - 10% */
#define SRAM_BUCK_DOWN_RATE      4500     /*    5mV/us - 10% */
#define BUCK_LATENCY             8        /* latency while programing pmic */

/* pm qos client */
static struct pm_qos_request pm_qos_vcore_request[APUSYS_DVFS_USER_NUM];


/***************************************************
 * The following functions are vpu common usage
 ****************************************************/
void pm_qos_register(void)
{
	int i;

	for (i = 0 ; i < APUSYS_DVFS_USER_NUM ; i++) {
		if (dvfs_user_support(i) == false)
			continue;
		pm_qos_add_request(&pm_qos_vcore_request[i],
						PM_QOS_VCORE_OPP,
						PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	}
}

void pm_qos_unregister(void)
{
	int i;

	for (i = 0 ; i < APUSYS_DVFS_USER_NUM ; i++) {
		if (dvfs_user_support(i) == false)
			continue;
		pm_qos_update_request(&pm_qos_vcore_request[i],
					PM_QOS_VCORE_OPP_DEFAULT_VALUE);
		pm_qos_remove_request(&pm_qos_vcore_request[i]);
	}
}

/*
 * regulator_get: vvpu, vcore, vsram
 */
int prepare_regulator(enum DVFS_BUCK buck, struct device *dev)
{
	int ret = 0;

	if (buck == VPU_BUCK) {
		vvpu_reg_id = regulator_get(dev, "vvpu");
		if (IS_ERR(vvpu_reg_id)) {
			ret = PTR_ERR(vvpu_reg_id);
			LOG_ERR("regulator_get vpu failed, ret: %d\n", ret);
			return ret;
		}
	} else if (buck == VCORE_BUCK) {
		vcore_reg_id = regulator_get(dev, "vcore");
		if (IS_ERR(vcore_reg_id)) {
			ret = PTR_ERR(vcore_reg_id);
			LOG_ERR("regulator_get vcore failed, ret: %d\n", ret);
			return ret;
		}
	} else if (buck == SRAM_BUCK) {
		vsram_reg_id = regulator_get(dev, "vsram_apu");
		if (IS_ERR(vsram_reg_id)) {
			ret = PTR_ERR(vsram_reg_id);
			LOG_ERR("regulator_get vsram failed, ret %d\n", ret);
			return ret;
		}
	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
		return -EINVAL;
	}

	return ret;
}

/*
 * regulator_enable: vvpu, vsram
 */
int enable_regulator(enum DVFS_BUCK buck)
{
	int ret = 0;

	if (buck == VPU_BUCK) {
		if (!vvpu_reg_id) {
			ret = -ENOENT;
			LOG_ERR("regulator_get vvpu_reg_id failed\n");
			return ret;
		}

		ret = regulator_enable(vvpu_reg_id);
		if (ret) {
			LOG_ERR("regulator_enable vvpu_reg_id failed\n");
			return ret;
		}
		LOG_DBG("enable vvpu success\n");

	} else if (buck == VCORE_BUCK) {
		if (!vcore_reg_id) {
			ret = -ENOENT;
			LOG_ERR("regulator_get vcore_reg_id failed\n");
			return ret;
		}


	} else if (buck == SRAM_BUCK) {
		if (!vsram_reg_id) {
			ret = -ENOENT;
			LOG_ERR("regulator_get vsram_reg_id failed\n");
			return ret;
		}

		ret = regulator_enable(vsram_reg_id);
		if (ret) {
			LOG_ERR("regulator_enable vsram_reg_id failed\n");
			return ret;
		}
		LOG_DBG("enable vsram success\n");

	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
		return -1;
	}

	udelay(200); // slew rate:rising10mV/us

	return ret;
}

/*
 * regulator_disable: vvpu, vsram
 */
int disable_regulator(enum DVFS_BUCK buck)
{
	int ret = 0;

	if (buck == VPU_BUCK) {
		if (!vvpu_reg_id) {
			ret = -ENOENT;
			LOG_ERR("vvpu_reg_id is invalid\n");
			return ret;
		}

		ret = regulator_disable(vvpu_reg_id);
		if (ret) {
			LOG_ERR("regulator_disable vvpu_reg_id failed\n");
			return ret;
		}
		LOG_DBG("disable vvpu success\n");

	} else if (buck == VCORE_BUCK) {
		if (!vcore_reg_id) {
			ret = -ENOENT;
			LOG_ERR("vcore_reg_id is invalid\n");
			return ret;
		}

	} else if (buck == SRAM_BUCK) {
		if (!vsram_reg_id) {
			ret = -ENOENT;
			LOG_ERR("vsram_reg_id is invalid\n");
			return ret;
		}

		ret = regulator_disable(vsram_reg_id);
		if (ret) {
			LOG_ERR("regulator_disable vsram_reg_id failed\n");
			return ret;
		}
		LOG_DBG("disable vsram success\n");

	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
		return -1;
	}

	udelay(200); // slew rate:rising10mV/us
	return ret;
}

/*
 * regulator_put: vvpu, vsram, vcore
 */
int unprepare_regulator(enum DVFS_BUCK buck)
{
	int ret = 0;

	if (buck == VPU_BUCK) {
		if (IS_ERR(vvpu_reg_id)) {
			ret = -ENOENT;
			LOG_ERR("vvpu_reg_id is invalid\n");
			return ret;
		}

		regulator_put(vvpu_reg_id);
		vvpu_reg_id = NULL;
		LOG_DBG("release vvpu_reg_id success\n");

	} else if (buck == VCORE_BUCK) {
		if (IS_ERR(vcore_reg_id)) {
			ret = -ENOENT;
			LOG_ERR("vcore_reg_id is invalid\n");
			return ret;
		}

		regulator_put(vcore_reg_id);
		vcore_reg_id = NULL;
		LOG_DBG("release vcore_reg_id success\n");

	} else if (buck == SRAM_BUCK) {
		if (IS_ERR(vsram_reg_id)) {
			ret = -ENOENT;
			LOG_ERR("vsram_reg_id is invalid\n");
			return ret;
		}

		regulator_put(vsram_reg_id);
		vsram_reg_id = NULL;
		LOG_DBG("release vsram_reg_id success\n");

	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
		return -1;
	}

	return ret;
}

static int get_slew_rate(int volt_diff, enum DVFS_BUCK buck)
{
	int slew_rate = 0;

	if (buck == VPU_BUCK) {
		if (volt_diff > 0)	/* VPU Rising */
			slew_rate = VPU_BUCK_UP_RATE;
		else if (volt_diff < 0)	/* VPU Falling */
			slew_rate = VPU_BUCK_DOWN_RATE;
	} else if (buck == SRAM_BUCK) {
		if (volt_diff > 0)	/* SRAM Rising */
			slew_rate = SRAM_BUCK_UP_RATE;
		else if (volt_diff < 0) /* SRAM Falling */
			slew_rate = SRAM_BUCK_DOWN_RATE;
	} else {
		LOG_WRN("no slew rate match buck %d\n", buck);
		return 0;
	}
	return slew_rate;
}

static int settle_time_check
(enum DVFS_BUCK buck, enum DVFS_VOLTAGE voltage_mV)
{
	int settle_time = 0;
	int volt_diff = 0;
	u32 volt_abs = 0;
	int slew_rate = 0;

	if (buck == VPU_BUCK) {
		volt_diff = voltage_mV - curr_vvpu_volt;
		curr_vvpu_volt = voltage_mV;
	} else if (buck == SRAM_BUCK) {
		volt_diff = voltage_mV - curr_vsram_volt;
		curr_vsram_volt = voltage_mV;
	} else {
		LOG_ERR("%s unsupport buck %d\n", __func__, buck);
		return -1;
	}

	/* save the absolute value of volt_diff for ceil */
	volt_abs = abs(volt_diff);

	/* voltage changed and find out settle_time */
	if (volt_diff) {
		slew_rate = get_slew_rate(volt_diff, buck);
		settle_time =
			DIV_ROUND_UP_ULL(volt_abs, slew_rate) + BUCK_LATENCY;
	} else {
		LOG_DBG("%s buck:%d voltage no change (%d)\n",
			__func__, buck, voltage_mV);
		return 0;
	}

	/* for vvpu, take the worst case of slew rate and constrain */
	if (buck == VPU_BUCK)
		settle_time = max(settle_time, VPU_BUCK_CONSTRAIN);

	LOG_DBG("%s buck:%d %s, settle:%d(us), %s %d(uv), slew:%d(uv/us)\n",
		__func__, buck,
		(buck == VPU_BUCK) ? VPU_BUCK_NAME : "",
		settle_time,
		(volt_diff > 0) ? "up" : "down",
		volt_diff,
		slew_rate);

	if (settle_time > 200)
		settle_time = 200;

#if APUSYS_SETTLE_TIME_TEST
	/* Here (buck + 1) due to index of Vsram = -1.*/
	apusys_opps.st[buck + 1].end = sched_clock();
	LOG_WRN("APUSYS_SETTLE_TIME_TEST bkid:%d %s,%s %d(uv),settle:%d(us)\n",
		buck, (buck == VPU_BUCK) ? VPU_BUCK_NAME : "",
		(volt_diff > 0) ? "up" : "down", volt_diff, settle_time);
#endif

	return settle_time;
}

// call regulat_set_voltage directly to config buck vvpu.
int config_normal_regulator(enum DVFS_BUCK buck, enum DVFS_VOLTAGE voltage_mV)
{
#if SUPPORT_HW_CONTROL_PMIC
	/*
	 * Bit[31:16]: Which address should be updated in PMIC
	 * Bit[15:0]: Which 16-bit value should be updateds in PMIC
	 */
	uint16_t buck_addr = 0x0;	// Bit[31:16]
	uint16_t volt_code = 0x0;	// Bit[15:0]
	uint32_t pmic_cmd = 0x0;
	uint32_t check_round = 0;
	uint32_t formula_param = 0;
#endif
	int ret = 0;
	int voltage_MAX = voltage_mV + 50000;
	int settle_time = 0;
#if VOLTAGE_RAISE_UP
	unsigned int vpu_efuse_raise = 0;
#endif
	struct regulator *reg_id = NULL;
#if VOLTAGE_CHECKER
	int check_volt = 0;
#endif

	/*
	 * Buck_control will use VVPU_DEFAULT_VOLT instead of opp table
	 * that is why add raising voltage check here.
	 */
#if VOLTAGE_RAISE_UP
	vpu_efuse_raise =
		GET_BITS_VAL(21:20, get_devinfo_with_index(EFUSE_BIN));
	LOG_DBG("Raise bin: vpu_efuse=%d, efuse: 0x%x\n",
		vpu_efuse_raise, get_devinfo_with_index(EFUSE_BIN));
	/* raising up Vvpu LV from 575mv to 625mv */
	if (vpu_efuse_raise == 1 &&
		buck == VPU_BUCK &&
		voltage_mV == DVFS_VOLT_00_575000_V)
		voltage_mV += 50000;

	/* raising up Vvpu LV from 575mv to 600mv */
	if (vpu_efuse_raise == 2 &&
		buck == VPU_BUCK &&
		voltage_mV == DVFS_VOLT_00_575000_V)
		voltage_mV += 25000;
#endif

	if (buck >= 0) /* bypass the case of SRAM_BUCK = -1 */
		LOG_DBG("%s try to config buck : %s to %d(max:%d)\n",
			__func__, buck_str[buck], voltage_mV, voltage_MAX);

	if (voltage_mV <= DVFS_VOLT_NOT_SUPPORT
		|| voltage_mV >= DVFS_VOLT_MAX) {
		LOG_ERR("%s voltage_mV over range : %d\n",
			__func__, voltage_mV);
		return -1;
	}

#if SUPPORT_HW_CONTROL_PMIC
	if (buck == VPU_BUCK) {
		buck_addr = PMIC_RG_BUCK_VPROC1_VOSEL_ADDR;
		formula_param = 400000; // 0.4 V
		reg_id = vvpu_reg_id;
	} else if (buck == SRAM_BUCK) {
		buck_addr = PMIC_RG_LDO_VSRAM_OTHERS_VOSEL_ADDR;
		formula_param = 500000; // 0.5 V
		reg_id = vsram_reg_id;
	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
		return -1;
	}

	// Vout = formula_param + 6.25 mV*code
	volt_code = (uint32_t)((voltage_mV - formula_param) / 6250);
	pmic_cmd = (buck_addr << 16) | volt_code;

	LOG_DBG("%s pmic_cmd = 0x%x\n", __func__, pmic_cmd);
	/*
	 * If last setting, snapshotted by PCU_PMIC_CUR_BUF,
	 * is the same as current value, pmic_cmd, driver will
	 * not setting again. Since there will be no interrupt
	 * happen once same value set again.
	 */
	if (pmic_cmd != DRV_Reg32(APU_PCU_PMIC_CUR_BUF)) {
		DRV_WriteReg32(APU_PCU_PMIC_TAR_BUF, pmic_cmd);

		while ((DRV_Reg32(APU_PCU_PMIC_STATUS) & 0x1) == 0) {
			udelay(50);
		if (++check_round >= REG_POLLING_TIMEOUT_ROUNDS) {
			LOG_DBG("%s wait APU_PCU_PMIC_STATUS timeout !\n",
								__func__);
			break;
			}
		}

		DRV_WriteReg32(APU_PCU_PMIC_STATUS, 0x1);
		LOG_DBG("read back from reg = 0x%x\n",
					DRV_Reg32(APU_PCU_PMIC_CUR_BUF));
	} else {
		LOG_DBG("%s same as last pmic_cmd = 0x%x\n",
				__func__, DRV_Reg32(APU_PCU_PMIC_CUR_BUF));
	}
#else
	if (buck == VPU_BUCK) {
		reg_id = vvpu_reg_id;
	} else if (buck == SRAM_BUCK) {
		reg_id = vsram_reg_id;
	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
		return -1;
	}

	ret = regulator_set_voltage(reg_id, voltage_mV, voltage_MAX);
	if (ret) {
		LOG_ERR("%s set buck %d %s failded, ret = %d\n",
			__func__,
			buck,
			(buck == VPU_BUCK) ? VPU_BUCK_NAME : "",
			ret);
		return ret;
	}

	/* check whether regulator driver implement slew rate delay */
	settle_time =
		regulator_set_voltage_time(reg_id, voltage_mV, voltage_MAX);
#endif
	/*
	 * (A) Regulator frame work return error
	 * (B) Regutlaor not implement regulator delay
	 * (C) Choose HW_CONTORL_PMIC flow.
	 *
	 * will get settle_time <= 0 and
	 * start calcuating settle_time and udelay that value.
	 */
	if (settle_time <= 0) {
		settle_time = settle_time_check(buck, voltage_mV);
		if (settle_time > 0)
			udelay(settle_time);
	} else
		LOG_DBG("%s buck:%d %s, OS use settletime:%d(us)\n",
			__func__, buck,
			(buck == VPU_BUCK) ? VPU_BUCK_NAME : "",
			settle_time);

#if VOLTAGE_CHECKER
	check_volt = regulator_get_voltage(reg_id);
	if (voltage_mV != check_volt) {
#if SUPPORT_HW_CONTROL_PMIC
		LOG_ERR("%s check fail, pcu reg tar_buf:0x%x, cur_buf:0x%x\n",
			__func__, pmic_cmd, DRV_Reg32(APU_PCU_PMIC_CUR_BUF));
#endif
		LOG_ERR("%s check fail, buck:%d, tar_volt:%d, chk_volt:%d\n",
					__func__, buck, voltage_mV, check_volt);

		return -1;
	}
	LOG_DBG("%s buck:%d, chk_volt:%d\n", __func__, buck, check_volt);
#endif
	return ret;
}

// buck vcore is shared resource so we need to notify pm_qos for voting.
int config_vcore(enum DVFS_USER user, int vcore_opp)
{
	int ret = 0;

	LOG_DBG("%s %s, vcore_opp:%d\n", __func__, user_str[user], vcore_opp);
	pm_qos_update_request(&pm_qos_vcore_request[user], vcore_opp);

	return ret;
}

/*
 * @ include/linux/regulator/consumer.h
 * REGULATOR_MODE_INVALID                  0x0
 * REGULATOR_MODE_FAST                     0x1	(force pwm)
 * REGULATOR_MODE_NORMAL                   0x2	(auto mode)
 * REGULATOR_MODE_IDLE                     0x4
 * REGULATOR_MODE_STANDBY                  0x8
 */
int config_regulator_mode(enum DVFS_BUCK buck, int is_normal)
{
	int ret = 0;

	LOG_DBG("%s try to config buck : %d to mode %d\n",
						__func__, buck, is_normal);

	if (is_normal < 0 || is_normal > 1) {
		LOG_ERR("%s mode over range : %d\n", __func__, is_normal);
		return -1;
	}

	if (buck == VPU_BUCK) {
		ret = regulator_set_mode(vvpu_reg_id, is_normal ?
				REGULATOR_MODE_NORMAL : REGULATOR_MODE_FAST);
		udelay(100); // slew rate:rising10mV/us

	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
		return -1;
	}

	return ret;
}

// dump related voltages of APUsys
void dump_voltage(struct apu_power_info *info)
{
	unsigned int vvpu = 0;
	unsigned int vcore = 0;
	unsigned int vsram = 0;
	unsigned int dump_div = 1;

	if (!IS_ERR(vvpu_reg_id))
		vvpu = regulator_get_voltage(vvpu_reg_id);

	if (!IS_ERR(vcore_reg_id))
		vcore = regulator_get_voltage(vcore_reg_id);

	if (!IS_ERR(vsram_reg_id))
		vsram = regulator_get_voltage(vsram_reg_id);

	if (info->dump_div > 0)
		dump_div = info->dump_div;

	info->vvpu = vvpu / dump_div;
	info->vcore = vcore / dump_div;
	info->vsram = vsram / dump_div;
	LOG_DBG("vvpu=%d, vcore=%d, vsram=%d\n",
		vvpu, vcore, vsram);
}
