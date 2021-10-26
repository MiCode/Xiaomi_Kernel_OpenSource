// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include <common/mdla_driver.h>
#include <common/mdla_device.h>

#include <utilities/mdla_debug.h>


struct apusys_core_info;

static struct mdla_dev *mdla_devices;
static u32 mdla_device_num;

struct mdla_dev *mdla_get_device(int id)
{
	if (unlikely(id >= mdla_device_num || !mdla_devices))
		return NULL;

	return &mdla_devices[id];
}

void mdla_set_device(struct mdla_dev *dev, u32 num)
{
	mdla_devices = dev;
	mdla_device_num = num;
}

int mdla_init(struct apusys_core_info *info)
{
	mdla_dbg_fs_init(NULL);

	/* Register platform driver after debugfs initialization */
	if (mdla_drv_init()) {
		mdla_dbg_fs_exit();
		return -1;
	}

	pr_info("%s() done\n", __func__);

	return 0;
}

void mdla_exit(void)
{
	mdla_dbg_fs_exit();
	mdla_drv_exit();

	pr_info("%s()!!\n", __func__);
}

#ifdef RV_COMP
int mdla_post_init(void)
{
	return mdla_init(NULL);
}

void mdla_post_exit(void)
{
	mdla_exit();
}
#endif

#ifdef INIT_MDLA_BY_SELF
#ifdef BUILD_MODULE
static int mdla_mod_init(void)
{
	return mdla_init(NULL);
}

static void mdla_mod_exit(void)
{
	mdla_exit();
}

module_init(mdla_mod_init);
module_exit(mdla_mod_exit);
MODULE_DESCRIPTION("MDLA Driver");
MODULE_AUTHOR("SPT1/SS5");
MODULE_LICENSE("GPL");
#else
static int mdla_mod_init(void)
{
	return mdla_init(NULL);
}
late_initcall(mdla_mod_init);
#endif
#endif

