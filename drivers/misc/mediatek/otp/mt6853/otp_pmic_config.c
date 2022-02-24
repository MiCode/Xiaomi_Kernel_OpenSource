// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <mt-plat/mtk_otp.h>
#include <mt-plat/upmu_common.h>
#include <linux/regulator/consumer.h>
#include "otp_pmic_config.h"

#define kal_uint32 unsigned int
static struct pm_qos_request dvfsrc_vcore_opp_req;
static struct regulator *reg_vcore;
static unsigned int org_volt;

/**************************************************************************
 *EXTERN FUNCTION
 **************************************************************************/
u32 otp_pmic_fsource_set(void)
{
	u32 ret_val = 0;

	/* 1.8V */
	ret_val |= pmic_config_interface(
			(kal_uint32)(PMIC_RG_VEFUSE_VOSEL_ADDR),
			(kal_uint32)(0xC),
			(kal_uint32)(PMIC_RG_VEFUSE_VOSEL_MASK),
			(kal_uint32)(PMIC_RG_VEFUSE_VOSEL_SHIFT)
			);

	/* +0mV */
	ret_val |= pmic_config_interface(
			(kal_uint32)(PMIC_RG_VEFUSE_VOCAL_ADDR),
			(kal_uint32)(0x0),
			(kal_uint32)(PMIC_RG_VEFUSE_VOCAL_MASK),
			(kal_uint32)(PMIC_RG_VEFUSE_VOCAL_SHIFT)
			);

	/* Fsource(VEFUSE) enabled */
	ret_val |= pmic_config_interface(
			(kal_uint32)(PMIC_RG_LDO_VEFUSE_EN_ADDR),
			(kal_uint32)(1),
			(kal_uint32)(PMIC_RG_LDO_VEFUSE_EN_MASK),
			(kal_uint32)(PMIC_RG_LDO_VEFUSE_EN_SHIFT));

	mdelay(10);

	return ret_val;
}

u32 otp_pmic_fsource_release(void)
{
	u32 ret_val = 0;

	/* Fsource(VEFUSE) disabled */
	ret_val |= pmic_config_interface(
			(kal_uint32)(PMIC_RG_LDO_VEFUSE_EN_ADDR),
			(kal_uint32)(0),
			(kal_uint32)(PMIC_RG_LDO_VEFUSE_EN_MASK),
			(kal_uint32)(PMIC_RG_LDO_VEFUSE_EN_SHIFT));

	mdelay(10);

	return ret_val;
}

u32 otp_pmic_is_fsource_enabled(void)
{
	u32 regVal = 0;

	/*  Check Fsource(VEFUSE) Status */
	pmic_read_interface((kal_uint32)(PMIC_RG_LDO_VEFUSE_EN_ADDR),
			&regVal,
			(kal_uint32)(PMIC_RG_LDO_VEFUSE_EN_MASK),
			(kal_uint32)(PMIC_RG_LDO_VEFUSE_EN_SHIFT)
			);

	/* return 1 : fsource enabled
	 * return 0 : fsource disabled
	 */

	return regVal;
}

u32 otp_pmic_high_vcore_init(void)
{
	reg_vcore = regulator_get(NULL, "vcore");

	pm_qos_add_request(&dvfsrc_vcore_opp_req, PM_QOS_VCORE_OPP,
			PM_QOS_VCORE_OPP_DEFAULT_VALUE);

	return 0;
}

u32 otp_pmic_high_vcore_set(void)
{
	/* opp 0 -> 0.725V */
	pm_qos_update_request(&dvfsrc_vcore_opp_req, 0);

	/* to 0.75V and save vcore */
	if (reg_vcore) {
		org_volt = regulator_get_voltage(reg_vcore);
		regulator_set_voltage(reg_vcore, 750000,
				750000);
	}
	return STATUS_DONE;
}

u32 otp_pmic_high_vcore_release(void)
{
	/* restore vcore */
	if (reg_vcore) {
		regulator_set_voltage(reg_vcore, org_volt,
				org_volt);
	}
	pm_qos_update_request(&dvfsrc_vcore_opp_req,
		PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	return STATUS_DONE;
}
