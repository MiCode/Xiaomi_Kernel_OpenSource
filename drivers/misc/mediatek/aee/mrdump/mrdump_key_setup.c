/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kconfig.h>
#include <linux/module.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#ifdef CONFIG_MTK_WATCHDOG_COMMON
#include <mt-plat/mtk_wd_api.h>
#ifdef CONFIG_MTK_PMIC_COMMON
#include <mt-plat/upmu_common.h>
#endif
#else
#include <mach/wd_api.h>
#endif

enum MRDUMP_RST_SOURCE {
	MRDUMP_SYSRST,
	MRDUMP_EINT};
enum MRDUMP_NOTIFY_MODE {
	MRDUMP_IRQ,
	MRDUMP_RST};

#ifdef CONFIG_MTK_PMIC_COMMON
enum MRDUMP_LONG_PRESS_MODE {
	LONG_PRESS_NONE,
	LONG_PRESS_SHUTDOWN};
#endif

static int __init mrdump_key_init(void)
{
#ifdef CONFIG_MTK_WATCHDOG_COMMON
	int res;
	struct wd_api *wd_api = NULL;
#endif
	enum MRDUMP_RST_SOURCE source = MRDUMP_EINT;
	enum MRDUMP_NOTIFY_MODE mode = MRDUMP_IRQ;
#ifdef CONFIG_MTK_PMIC_COMMON
	enum MRDUMP_LONG_PRESS_MODE long_press_mode
			= LONG_PRESS_NONE;
	const char *long_press;
#endif
	struct device_node *node;
	const char *source_str, *mode_str, *interrupts;
	char node_name[] = "mediatek, mrdump_ext_rst-eint";

	node = of_find_compatible_node(NULL, NULL, node_name);
	if (!node) {
		pr_notice("MRDUMP_KEY:node %s is not exist\n", node_name);
		goto out;
	}

	if (of_property_read_string(node, "interrupts", &interrupts)) {
		pr_notice("mrdump_key:no interrupts attribute from dws config\n");
		goto out;
	}

	if (!of_property_read_string(node, "source", &source_str)) {
		if (strcmp(source_str, "SYSRST") == 0) {
			source = MRDUMP_SYSRST;
#ifdef CONFIG_MTK_PMIC_COMMON
			if (!of_property_read_string(node, "long_press",
				&long_press)) {
				if (strcmp(long_press, "SHUTDOWN") == 0)
					long_press_mode = LONG_PRESS_SHUTDOWN;
				else
					long_press_mode = LONG_PRESS_NONE;
			}
#endif
		}
	} else
		pr_notice("MRDUMP_KEY:No attribute \"source\",  default to EINT\n");


	if (!of_property_read_string(node, "mode", &mode_str)) {
		if (strcmp(mode_str, "RST") == 0)
			mode = MRDUMP_RST;
	} else
		pr_notice("MRDUMP_KEY: no mode property,default IRQ");


#ifdef CONFIG_MTK_WATCHDOG_COMMON
	res = get_wd_api(&wd_api);
	if (res < 0) {
		pr_notice("MRDUMP_KEY: get_wd_api failed:%d\n", res);
		goto out;
	}

	if (source == MRDUMP_SYSRST) {
		res = wd_api->wd_debug_key_sysrst_config(1, mode);
		if (res == -1)
			pr_notice("%s: sysrst failed\n", __func__);
		else
			pr_notice("%s: enable MRDUMP_KEY SYSRST mode\n"
					, __func__);
#ifdef CONFIG_MTK_PMIC_COMMON
		pr_notice("%s: configure PMIC for smart reset\n"
				, __func__);
		if (long_press_mode == LONG_PRESS_SHUTDOWN) {
			pr_notice("long_press_mode = SHUTDOWN\n");
			pmic_enable_smart_reset(1, 1);
		} else {
			pr_notice("long_press_mode = NONE\n");
			pmic_enable_smart_reset(1, 0);
		}
#endif

	} else if (source == MRDUMP_EINT) {
		res = wd_api->wd_debug_key_eint_config(1, mode);
		if (res == -1)
			pr_notice("%s: eint failed\n", __func__);
		else
			pr_notice("%s: enabled MRDUMP_KEY EINT mode\n"
					, __func__);
	} else {
		pr_notice("%s:source %d is not match\n"
			"disable MRDUMP_KEY\n", __func__, source);
	}
#endif
out:
	of_node_put(node);
	return 0;
}

module_init(mrdump_key_init);

