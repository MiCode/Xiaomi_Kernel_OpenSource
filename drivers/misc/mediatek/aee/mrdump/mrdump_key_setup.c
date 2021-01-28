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

#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/kconfig.h>
#include <linux/module.h>
#include <linux/io.h>           /* ioremap() */
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#ifdef CONFIG_MTK_WATCHDOG_COMMON
#include <mt-plat/mtk_wd_api.h>
#ifdef CONFIG_MTK_PMIC_COMMON
#include <mt-plat/upmu_common.h>
#endif
#endif

enum MRDUMP_RST_SOURCE {
	MRDUMP_SYSRST,
	MRDUMP_EINT};

#ifdef CONFIG_MTK_PMIC_COMMON
enum MRDUMP_LONG_PRESS_MODE {
	LONG_PRESS_NONE,
	LONG_PRESS_SHUTDOWN};
#endif

static const struct of_device_id mrdump_key_of_ids[] = {
	{ .compatible = "mediatek, mrdump_ext_rst-eint", },
	{}
};

__weak void pmic_enable_smart_reset(unsigned char smart_en,
			       unsigned char smart_sdn_en)
{
		pr_info("weak func: %s", __func__);
}

static int __init mrdump_key_probe(struct platform_device *pdev)
{
#ifdef CONFIG_MTK_WATCHDOG_COMMON
	int res;
	struct wd_api *wd_api = NULL;
	const char *mode_str;
	enum wk_req_mode mode = WD_REQ_IRQ_MODE;
#ifdef CONFIG_MTK_PMIC_COMMON
	enum MRDUMP_LONG_PRESS_MODE long_press_mode
			= LONG_PRESS_NONE;
#endif
#endif

#ifdef CONFIG_MTK_ENG_BUILD
	enum MRDUMP_RST_SOURCE source = MRDUMP_SYSRST;
#else
	enum MRDUMP_RST_SOURCE source = MRDUMP_EINT;
#endif

	struct device_node *node;
	const char *source_str, *interrupts;
	char node_name[] = "mediatek, mrdump_ext_rst-eint";

	pr_notice("%s:%d\n", __func__, __LINE__);
	node = of_find_compatible_node(NULL, NULL, node_name);
	if (!node) {
		pr_notice("MRDUMP_KEY:node %s is not exist\n", node_name);
		goto out;
	}

	pr_notice("%s:default to %s\n", __func__,
		(source == MRDUMP_EINT) ? "EINT":"SYSRST");

	if (!of_property_read_string(node, "force_mode", &source_str)) {
		if (strcmp(source_str, "SYSRST") == 0) {
			source = MRDUMP_SYSRST;
			pr_notice("%s:force_mode=%s\n", __func__, "SYSRST");
		} else if (strcmp(source_str, "EINT") == 0) {
			source = MRDUMP_EINT;
			pr_notice("%s:force_mode=%s\n", __func__, "EINT");
			if (of_property_read_string(node,
				"interrupts", &interrupts)) {
				pr_notice("mrdump_key:no interrupts in dws config, exit\n");
				goto out;
			}
		} else
			pr_notice("%s:no valid force_mode\n", __func__);
	} else
		pr_notice("%s:no force_mode\n", __func__);



#ifdef CONFIG_MTK_WATCHDOG_COMMON
	if (!of_property_read_string(node, "mode", &mode_str)) {
		if (strcmp(mode_str, "RST") == 0)
			mode = WD_REQ_RST_MODE;
		pr_notice("%s:mode=%s\n", __func__,
			(strcmp(mode_str, "RST") == 0)?"RST":"IRQ");
	} else
		pr_notice("MRDUMP_KEY: no mode property,default IRQ");

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

void mrdump_key_shutdown(struct platform_device *pdev)
{

#ifdef CONFIG_MTK_WATCHDOG_COMMON
	int res;
	struct wd_api *wd_api = NULL;
#endif

#ifdef CONFIG_MTK_PMIC_COMMON
	pr_notice("restore pmic long_press_mode = SHUTDOWN\n");
	pmic_enable_smart_reset(0, 0);
#endif

#ifdef CONFIG_MTK_WATCHDOG_COMMON
	pr_notice("restore RGU to default value\n");
	res = get_wd_api(&wd_api);
	if (res < 0)
		pr_notice("%s: get_wd_api failed:%d\n", __func__, res);
	else {
		res = wd_api->wd_debug_key_eint_config(0, WD_REQ_RST_MODE);
		if (res == -1)
			pr_notice("%s: disable EINT failed\n", __func__);
		else
			pr_notice("%s:disable EINT mode\n", __func__);
		res = wd_api->wd_debug_key_sysrst_config(0, WD_REQ_RST_MODE);
		if (res == -1)
			pr_notice("%s: disable SYSRST failed\n", __func__);
		else
			pr_notice("%s:disable SYSRST OK\n", __func__);
	}
#endif
}

static void __exit mrdump_key_exit(void)
{
	mrdump_key_shutdown(NULL);
}
static int mrdump_key_remove(struct platform_device *dev)
{
	mrdump_key_shutdown(NULL);
	return 0;
}

/* variable with __init* or __refdata (see linux/init.h) or */
/* name the variable *_template, *_timer, *_sht, *_ops, *_probe, */
/* *_probe_one, *_console */
static struct platform_driver mrdump_key_driver_probe = {
	.probe = mrdump_key_probe,
	.shutdown = mrdump_key_shutdown,
	.remove = mrdump_key_remove,
	.driver = {
		.name = "mrdump_key",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mrdump_key_of_ids,
#endif
	},
};

static int __init mrdump_key_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mrdump_key_driver_probe);
	if (ret)
		pr_err("mrdump_key init FAIL, ret 0x%x!!!\n", ret);

	return ret;
}


module_init(mrdump_key_init);
module_exit(mrdump_key_exit);

