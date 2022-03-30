// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/kobject.h>

#include "ccci_core.h"
#include "ccci_fsm.h"
#include "ccci_modem.h"
#include "ccci_port.h"
#include "ccci_hif.h"
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

struct ccci_tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

static void *dev_class;
/*
 * for debug log:
 * 0 to disable; 1 for print to ram; 2 for print to uart
 * other value to desiable all log
 */
#ifndef CCCI_LOG_LEVEL /* for platform override */
#define CCCI_LOG_LEVEL CCCI_LOG_CRITICAL_UART
#endif

//#define CCCI_LOG_LEVEL CCCI_LOG_ALL_UART

unsigned int ccci_debug_enable = CCCI_LOG_LEVEL;

unsigned int get_boot_mode_from_dts(void)
{
	struct device_node *np_chosen = NULL;
	struct ccci_tag_bootmode *tag = NULL;
	u32 bootmode = NORMAL_BOOT_ID;
	static int ap_boot_mode = -1;

	if (ap_boot_mode >= 0) {
		CCCI_NORMAL_LOG(-1, CORE,
			"[%s] bootmode: 0x%x\n", __func__, ap_boot_mode);
		return ap_boot_mode;
	}

	np_chosen = of_find_node_by_path("/chosen");
	if (!np_chosen) {
		CCCI_ERROR_LOG(-1, CORE, "warning: not find node: '/chosen'\n");

		np_chosen = of_find_node_by_path("/chosen@0");
		if (!np_chosen) {
			CCCI_ERROR_LOG(-1, CORE,
				"[%s] error: not find node: '/chosen@0'\n",
				__func__);
			return NORMAL_BOOT_ID;
		}
	}

	tag = (struct ccci_tag_bootmode *)
			of_get_property(np_chosen, "atag,boot", NULL);
	if (!tag) {
		CCCI_ERROR_LOG(-1, CORE,
			"[%s] error: not find tag: 'atag,boot';\n", __func__);
		return NORMAL_BOOT_ID;
	}

	if (tag->bootmode == META_BOOT || tag->bootmode == ADVMETA_BOOT)
		bootmode = META_BOOT_ID;

	else if (tag->bootmode == FACTORY_BOOT ||
			tag->bootmode == ATE_FACTORY_BOOT)
		bootmode = FACTORY_BOOT_ID;

	CCCI_NORMAL_LOG(-1, CORE,
		"[%s] bootmode: 0x%x boottype: 0x%x; return: 0x%x\n",
		__func__, tag->bootmode, tag->boottype, bootmode);
	ap_boot_mode = bootmode;

	return bootmode;
}

int ccci_register_dev_node(const char *name, int major_id, int minor)
{
	int ret = 0;
	dev_t dev_n;
	struct device *dev;

	dev_n = MKDEV(major_id, minor);
	dev = device_create(dev_class, NULL, dev_n, NULL, "%s", name);

	if (IS_ERR(dev))
		ret = PTR_ERR(dev);

	return ret;
}
//EXPORT_SYMBOL(ccci_register_dev_node);

#ifndef CCCI_KMODULE_ENABLE
static int __init ccci_init(void)
{
	CCCI_INIT_LOG(-1, CORE, "ccci core init\n");
	dev_class = class_create(THIS_MODULE, "ccci_node");
	ccci_subsys_bm_init();
	return 0;
}

subsys_initcall(ccci_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("Evolved CCCI driver");
MODULE_LICENSE("GPL");

#else
int ccci_init(void)
{
	CCCI_INIT_LOG(-1, CORE, "ccci core init\n");
	dev_class = class_create(THIS_MODULE, "ccci_node");
	ccci_subsys_bm_init();
	return 0;
}
#endif
