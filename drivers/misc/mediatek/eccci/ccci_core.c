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
#ifdef FEATURE_SCP_CCCI_SUPPORT
#include <scp.h>
#endif

static void *dev_class;
/*
 * for debug log:
 * 0 to disable; 1 for print to ram; 2 for print to uart
 * other value to desiable all log
 */
#ifndef CCCI_LOG_LEVEL /* for platform override */
#define CCCI_LOG_LEVEL CCCI_LOG_CRITICAL_UART
#endif
unsigned int ccci_debug_enable = CCCI_LOG_LEVEL;

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
EXPORT_SYMBOL(ccci_register_dev_node);

#ifdef FEATURE_SCP_CCCI_SUPPORT
static int apsync_event(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	switch (event) {
	case SCP_EVENT_READY:
		fsm_scp_init0();
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block apsync_notifier = {
	.notifier_call = apsync_event,
};
#endif
#ifndef CCCI_KMODULE_ENABLE
static int __init ccci_init(void)
{
	CCCI_INIT_LOG(-1, CORE, "ccci core init\n");
	dev_class = class_create(THIS_MODULE, "ccci_node");
	ccci_subsys_bm_init();
#ifdef FEATURE_SCP_CCCI_SUPPORT
	scp_A_register_notify(&apsync_notifier);
#endif
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
#ifdef FEATURE_SCP_CCCI_SUPPORT
	scp_A_register_notify(&apsync_notifier);
#endif
	return 0;
}
#endif
