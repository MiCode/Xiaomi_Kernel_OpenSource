// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/module.h>
#include <linux/of.h>
#include "mtk_ccci_common.h"
#include "mtk_md_power_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_battery_oc_throttling.h"

struct cpu_md_priv {
	u32 lbat_md_reduce_tx;
	u32 oc_md_reduce_tx;
};

static struct cpu_md_priv md_priv;

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void md_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level)
{
	unsigned int md_throttle_cmd;
	int ret, intensity;

	if (level <= LOW_BATTERY_LEVEL_2 && level >= LOW_BATTERY_LEVEL_0) {
		if (level != LOW_BATTERY_LEVEL_0)
			intensity = md_priv.lbat_md_reduce_tx;
		else
			intensity = 0;

		md_throttle_cmd = TMC_CTRL_CMD_TX_POWER | level << 8 |
			PT_LOW_BATTERY_VOLTAGE << 16 | intensity << 24;
		ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG,
			(char *)&md_throttle_cmd, 4);
		if (ret)
			pr_notice("%s: error, ret=%d, cmd=0x%x l=%d\n", __func__, ret,
				md_throttle_cmd, level);
	}
}
#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
static void md_pt_over_current_cb(enum BATTERY_OC_LEVEL_TAG level)
{
	unsigned int md_throttle_cmd;
	int ret, intensity;

	if (level <= BATTERY_OC_LEVEL_1 && level >= BATTERY_OC_LEVEL_0) {
		if (level != BATTERY_OC_LEVEL_0)
			intensity = md_priv.oc_md_reduce_tx;
		else
			intensity = 0;

		md_throttle_cmd = TMC_CTRL_CMD_TX_POWER | level << 8 | PT_OVER_CURRENT << 16 |
			intensity << 24;
		ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG,
			(char *)&md_throttle_cmd, 4);
		if (ret)
			pr_notice("%s: error, ret=%d, cmd=0x%x l=%d\n", __func__, ret,
				md_throttle_cmd, level);
	}
}
#endif

static int __init mtk_md_power_throttling_module_init(void)
{
	struct device_node *np;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "mediatek,power_throttling");

	if (!np) {
		pr_notice("get power_throttling node fail\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "lbat_md_reduce_tx", &md_priv.lbat_md_reduce_tx);
	if (ret) {
		pr_notice("get lbat_md_reduce_tx fail ret=%d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "oc_md_reduce_tx", &md_priv.oc_md_reduce_tx);
	if (ret) {
		pr_notice("get oc_md_reduce_tx fail  ret=%d\n", ret);
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
		register_low_battery_notify(&md_pt_low_battery_cb, LOW_BATTERY_PRIO_MD);
#endif
	
#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
		register_battery_oc_notify(&md_pt_over_current_cb, BATTERY_OC_PRIO_MD);
#endif

	return 0;
}

static void __exit mtk_md_power_throttling_module_exit(void)
{

}

module_init(mtk_md_power_throttling_module_init);
module_exit(mtk_md_power_throttling_module_exit);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK modem power throttling driver");
MODULE_LICENSE("GPL");
