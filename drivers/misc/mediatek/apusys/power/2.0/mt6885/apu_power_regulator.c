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

#include "apu_power_api.h"
#include "../apu_log.h"

/* regulator id */
static struct regulator *vvpu_reg_id;
static struct regulator *vmdla_reg_id;
static struct regulator *vcore_reg_id;
static struct regulator *vsram_reg_id;

/* pm qos client */
static struct pm_qos_request pm_qos_vcore_request[APUSYS_DVFS_USER_NUM];


/***************************************************
 * The following functions are mdla/vpu common usage
 ****************************************************/

void pm_qos_register(void)
{
	int i;

	for (i = 0 ; i < APUSYS_DVFS_USER_NUM ; i++) {
		pm_qos_add_request(&pm_qos_vcore_request[i],
						PM_QOS_VCORE_OPP,
						PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	}
}

void pm_qos_unregister(void)
{
	int i;

	for (i = 0 ; i < APUSYS_DVFS_USER_NUM ; i++)
		pm_qos_remove_request(&pm_qos_vcore_request[i]);
}

/*
 * regulator_get: vvpu, vmdla, vcore, vsram
 * regulator_enable: vvpu, vmdla
 */
int prepare_regulator(enum DVFS_BUCK buck, struct device *dev)
{
	int ret = 0;

	if (buck == VPU_BUCK) {
		vvpu_reg_id = regulator_get(dev, "vpu");
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

		udelay(200); // slew rate:rising10mV/us

	} else if (buck == MDLA_BUCK) {
		vmdla_reg_id = regulator_get(dev, "VMDLA");
		if (!vmdla_reg_id) {
			ret = -ENOENT;
			LOG_ERR("regulator_get vmdla_reg_id failed\n");
			return ret;
		}

		ret = regulator_enable(vmdla_reg_id);
		if (ret) {
			LOG_ERR("regulator_enable vmdla_reg_id failed\n");
			return ret;
		}

		udelay(200); // slew rate:rising10mV/us

	} else if (buck == VCORE_BUCK) {
		vcore_reg_id = regulator_get(dev, "vcore");
		if (!vcore_reg_id) {
			ret = -ENOENT;
			LOG_ERR("regulator_get vcore_reg_id failed\n");
			return ret;
		}

		vsram_reg_id = regulator_get(dev, "vsram_others");
		if (!vsram_reg_id) {
			ret = -ENOENT;
			LOG_ERR("regulator_get vsram_reg_id failed\n");
			return ret;
		}

	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
		return -1;
	}

	return ret;
}

/*
 * regulator_disable: vvpu, vmdla
 * regulator_put: vvpu, vmdla, vcore, vsram
 */
int unprepare_regulator(enum DVFS_BUCK buck)
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

		regulator_put(vvpu_reg_id);
		vvpu_reg_id = NULL;
		LOG_DBG("release vvpu_reg_id success\n");
		udelay(200); // slew rate:rising10mV/us

	} else if (buck == MDLA_BUCK) {
		if (!vmdla_reg_id) {
			ret = -ENOENT;
			LOG_ERR("vmdla_reg_id is invalid\n");
			return ret;
		}

		ret = regulator_disable(vmdla_reg_id);
		if (ret) {
			LOG_ERR("regulator_disable vmdla_reg_id failed\n");
			return ret;
		}

		regulator_put(vmdla_reg_id);
		vmdla_reg_id = NULL;
		LOG_DBG("release vmdla_reg_id success\n");
		udelay(200); // slew rate:rising10mV/us

	} else if (buck == VCORE_BUCK) {
		if (!vcore_reg_id) {
			ret = -ENOENT;
			LOG_ERR("vcore_reg_id is invalid\n");
			return ret;
		}

		regulator_put(vcore_reg_id);
		vcore_reg_id = NULL;
		LOG_DBG("release vcore_reg_id success\n");

		if (!vsram_reg_id) {
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

// call regulat_set_voltage directly to config buck vvpu and vmdla.
int config_normal_regulator(enum DVFS_BUCK buck, enum DVFS_VOLTAGE voltage_mV)
{
	int ret = 0;
	int voltage_MAX = voltage_mV + 50000;

	LOG_DBG("%s try to config buck : %d to %d(max:%d)\n", __func__,
						buck, voltage_mV, voltage_MAX);

	if (voltage_mV <= DVFS_VOLT_NOT_SUPPORT
		|| voltage_mV >= DVFS_VOLT_MAX) {
		LOG_ERR("%s voltage_mV over range : %d\n",
					__func__, voltage_mV);
		return -1;
	}

	if (buck == VPU_BUCK) {
		ret = regulator_set_voltage(
			vvpu_reg_id, voltage_mV, voltage_MAX);
		udelay(100); // FIXME: settle time

	} else if (buck == MDLA_BUCK) {
		ret = regulator_set_voltage(
			vmdla_reg_id, voltage_mV, voltage_MAX);
		udelay(100); // FIXME: settle time

	} else {
		LOG_ERR("%s not support buck : %d\n", __func__, buck);
		return -1;
	}

	return ret;
}

// buck vcore is shared resource so we need to notify pm_qos for voting.
int config_vcore(enum DVFS_USER user, int vcore_opp)
{
	int ret = 0;

	pm_qos_update_request(&pm_qos_vcore_request[user], vcore_opp);

	return ret;
}

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
				REGULATOR_MODE_NORMAL : REGULATOR_MODE_IDLE);
		udelay(100); // slew rate:rising10mV/us

	} else if (buck == MDLA_BUCK) {
		ret = regulator_set_mode(vmdla_reg_id, is_normal ?
				REGULATOR_MODE_NORMAL : REGULATOR_MODE_IDLE);
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
	unsigned int vmdla = 0;
	unsigned int vcore = 0;
	unsigned int vsram = 0;
	unsigned int dump_div = 1;

	if (vmdla_reg_id)
		vmdla = regulator_get_voltage(vmdla_reg_id);

	if (vvpu_reg_id)
		vvpu = regulator_get_voltage(vvpu_reg_id);

	if (vcore_reg_id)
		vcore = regulator_get_voltage(vcore_reg_id);

	if (vsram_reg_id)
		vsram = regulator_get_voltage(vsram_reg_id);

	if (info->dump_div > 0)
		dump_div = info->dump_div;

	info->vvpu = vvpu / dump_div;
	info->vmdla = vmdla / dump_div;
	info->vcore = vcore / dump_div;
	info->vsram = vsram / dump_div;

	LOG_DBG("vvpu=%d, vmdla=%d, vcore=%d, vsram=%d\n",
						vvpu, vmdla, vcore, vsram);
}
