// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

#include "tchbst.h"
#include "io_ctrl.h"
#include "boost_ctrl.h"
#include "mtk_perfmgr_internal.h"
#include "topo_ctrl.h"
#include "uload_ind.h"
#include "syslimiter.h"

#define API_READY 0

static int perfmgr_probe(struct platform_device *dev)
{
	return 0;
}

struct platform_device perfmgr_device = {
	.name   = "perfmgr",
	.id        = -1,
};

static int perfmgr_suspend(struct device *dev)
{
#ifdef CONFIG_MTK_PERFMGR_TOUCH_BOOST
	ktch_suspend();
#endif
	return 0;
}

static int perfmgr_resume(struct device *dev)
{
	return 0;
}
static int perfmgr_remove(struct platform_device *dev)
{
		/*TODO: workaround for k414
		 * topo_ctrl_exit();
		 */
#if API_READY
		cpu_ctrl_exit();
#endif
	return 0;
}
static struct platform_driver perfmgr_driver = {
	.probe      = perfmgr_probe,
	.remove     = perfmgr_remove,
	.driver     = {
		.name = "perfmgr",
		.pm = &(const struct dev_pm_ops){
			.suspend = perfmgr_suspend,
			.resume = perfmgr_resume,
		},
	},
};

/*--------------------INIT------------------------*/

static int __init init_perfmgr(void)
{
	struct proc_dir_entry *perfmgr_root = NULL;
	int ret = 0;

	ret = platform_device_register(&perfmgr_device);
	if (ret)
		return ret;
	ret = platform_driver_register(&perfmgr_driver);
	if (ret)
		return ret;

	perfmgr_root = proc_mkdir("perfmgr", NULL);
	pr_debug("MTK_TOUCH_BOOST function init_perfmgr_touch\n");

	init_boostctrl(perfmgr_root);
	init_tchbst(perfmgr_root);
	init_perfctl(perfmgr_root);
	syslimiter_init(perfmgr_root);

#ifdef CONFIG_MTK_LOAD_TRACKER
	init_uload_ind(NULL);
#endif
	return 0;
}
device_initcall(init_perfmgr);
