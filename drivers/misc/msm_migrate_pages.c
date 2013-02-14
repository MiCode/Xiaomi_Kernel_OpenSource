/*
 * Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/memory.h>
#include <linux/memory_hotplug.h>
#include <linux/module.h>
#include <mach/msm_migrate_pages.h>

static unsigned long unstable_memory_state;

unsigned long get_msm_migrate_pages_status(void)
{
	return unstable_memory_state;
}
EXPORT_SYMBOL(get_msm_migrate_pages_status);

#ifdef CONFIG_MEMORY_HOTPLUG
static int migrate_pages_callback(struct notifier_block *self,
				unsigned long action, void *arg)
{
	int ret = 0;

	switch (action) {
	case MEM_ONLINE:
		unstable_memory_state = action;
		break;
	case MEM_OFFLINE:
		unstable_memory_state = action;
		break;
	case MEM_GOING_OFFLINE:
	case MEM_GOING_ONLINE:
	case MEM_CANCEL_ONLINE:
	case MEM_CANCEL_OFFLINE:
		break;
	}
	return ret;
}
#endif

static int __devinit msm_migrate_pages_probe(struct platform_device *pdev)
{
#ifdef CONFIG_MEMORY_HOTPLUG
	hotplug_memory_notifier(migrate_pages_callback, 0);
#endif
	unstable_memory_state = 0;
	return 0;
}

static struct platform_driver msm_migrate_pages_driver = {
	.probe = msm_migrate_pages_probe,
	.driver = {
		.name = "msm_migrate_pages",
	},
};

static int __init msm_migrate_pages_init(void)
{
	return platform_driver_register(&msm_migrate_pages_driver);
}

static void __exit msm_migrate_pages_exit(void)
{
	platform_driver_unregister(&msm_migrate_pages_driver);
}

module_init(msm_migrate_pages_init);
module_exit(msm_migrate_pages_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Get Status of Unstable Memory Region");
