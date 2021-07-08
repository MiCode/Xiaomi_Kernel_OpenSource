// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/kconfig.h>
#include <linux/module.h>
#include <linux/string.h>
#include "mrdump_private.h"

#include <mrdump.h>
#if IS_ENABLED(CONFIG_MTK_DFD_INTERNAL_DUMP)
#include <mtk_platform_debug.h>
#endif

int __init mrdump_hw_init(bool drm_ready)
{
#if IS_ENABLED(CONFIG_MTK_DFD_INTERNAL_DUMP)
	if (dfd_setup(DFD_BASIC_DUMP) == -1)
		pr_notice("%s: DFD disabled\n", __func__);
	else
		pr_notice("%s: DFD enabled\n", __func__);
#endif

	return 0;
}


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek AEE module");
MODULE_AUTHOR("MediaTek Inc.");
