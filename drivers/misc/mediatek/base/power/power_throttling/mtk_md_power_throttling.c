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

#include <linux/module.h>
#include <mtk_ccci_common.h>
#include <mach/upmu_sw.h>
#include <mtk_md_power_throttling.h>
#include <mach/mtk_pmic.h>

#ifndef DISABLE_LOW_BATTERY_PROTECT
static void md_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level)
{
	unsigned int md_throttle_cmd = 0;
	int ret = 0, intensity = 0;

	if (level <= LOW_BATTERY_LEVEL_2 &&
		level >= LOW_BATTERY_LEVEL_0) {

		if (level != LOW_BATTERY_LEVEL_0)
			intensity = LBAT_REDUCE_TX_POWER;

		md_throttle_cmd = TMC_CTRL_CMD_TX_POWER |
		level << 8 | PT_LOW_BATTERY_VOLTAGE << 16 | intensity << 24;
#if defined(CONFIG_MTK_ECCCI_DRIVER)
		ret = exec_ccci_kern_func_by_md_id(MD_SYS1,
			ID_THROTTLING_CFG,
			(char *) &md_throttle_cmd, 4);
#endif
	}

	if (ret || !md_throttle_cmd)
		pr_notice("%s: error, ret=%d, cmd=0x%x l=%d\n", __func__, ret,
			md_throttle_cmd, level);
}
#endif

#ifndef DISABLE_BATTERY_OC_PROTECT
static void md_pt_over_current_cb(enum BATTERY_OC_LEVEL_TAG level)
{
	unsigned int md_throttle_cmd = 0;
	int ret = 0, intensity = 0;

	if (level <= BATTERY_OC_LEVEL_1 &&
		level >= BATTERY_OC_LEVEL_0) {

		if (level != BATTERY_OC_LEVEL_0)
			intensity = OC_REDUCE_TX_POWER;

		md_throttle_cmd = TMC_CTRL_CMD_TX_POWER |
		level << 8 | PT_OVER_CURRENT << 16 | intensity << 24;
#if defined(CONFIG_MTK_ECCCI_DRIVER)
		ret = exec_ccci_kern_func_by_md_id(MD_SYS1,
			ID_THROTTLING_CFG,
			(char *) &md_throttle_cmd, 4);
#endif
	}

	if (ret || !md_throttle_cmd)
		pr_notice("%s: error, ret=%d, cmd=0x%x l=%d\n", __func__, ret,
			md_throttle_cmd, level);
}
#endif

#ifndef DISABLE_BATTERY_PERCENT_PROTECT
static void md_pt_battery_percent_cb(enum BATTERY_PERCENT_LEVEL_TAG level)
{
	unsigned int md_throttle_cmd = 0;
	int ret = 0;
	enum tmc_ctrl_low_pwr_enum val = 0;

	if (level <= BATTERY_PERCENT_LEVEL_1 &&
		level >= BATTERY_PERCENT_LEVEL_0) {

		if (level == BATTERY_PERCENT_LEVEL_0)
			val = TMC_CTRL_LOW_POWER_RECHARGE_BATTERY_EVENT;
		else if (level == BATTERY_PERCENT_LEVEL_1)
			val = TMC_CTRL_LOW_POWER_LOW_BATTERY_EVENT;

		md_throttle_cmd = TMC_CTRL_CMD_LOW_POWER_IND | val << 8;
#if defined(CONFIG_MTK_ECCCI_DRIVER)
		ret = exec_ccci_kern_func_by_md_id(MD_SYS1,
			ID_THROTTLING_CFG,
			(char *) &md_throttle_cmd, 4);
#endif
	}

	if (ret || !md_throttle_cmd)
		pr_notice("%s: error, ret=%d, cmd=0x%x l=%d\n", __func__, ret,
			md_throttle_cmd, level);
}
#endif

static int __init mtk_md_power_throttling_module_init(void)
{
#ifndef DISABLE_LOW_BATTERY_PROTECT
	register_low_battery_notify(
		&md_pt_low_battery_cb, LOW_BATTERY_PRIO_MD);
#endif

#ifndef DISABLE_BATTERY_OC_PROTECT
	register_battery_oc_notify(
		&md_pt_over_current_cb, BATTERY_OC_PRIO_MD);
#endif

#ifndef DISABLE_BATTERY_PERCENT_PROTECT
	register_battery_percent_notify_ext(
		&md_pt_battery_percent_cb, BATTERY_PERCENT_PRIO_MD);
#endif

	return 0;
}

static void __exit mtk_md_power_throttling_module_exit(void)
{

}

module_init(mtk_md_power_throttling_module_init);
module_exit(mtk_md_power_throttling_module_exit);

MODULE_DESCRIPTION("MD power throttle interface");
