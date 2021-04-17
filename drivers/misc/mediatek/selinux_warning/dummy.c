// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: kuanhsin Lee <kuan-hsin.lee@mediatek.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
static int __init selinux_init(void)
{
	pr_info("[SELinux] MTK SELinux init done\n");

	return 0;
}

static void __exit selinux_exit(void)
{
    pr_info("[SELinux] MTK SELinux func exit\n");
}

module_init(selinux_init);
module_exit(selinux_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SELINUX Driver");
MODULE_AUTHOR("Kuan-Hsin Lee <kuan-hsin.lee@mediatek.com>");
