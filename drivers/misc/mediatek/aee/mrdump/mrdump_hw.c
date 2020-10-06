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

#if IS_ENABLED(CONFIG_MTK_DBGTOP)
#include <dbgtop.h>
#endif

int __init mrdump_hw_init(bool drm_ready)
{
#if IS_ENABLED(CONFIG_MTK_DBGTOP)
	if (drm_ready) {
		if (!mtk_dbgtop_dram_reserved(true))
			pr_info("%s: DDR reserved mode enabled ok\n",
				__func__);
		else
			pr_info("%s: mtk_dbgtop_dram_reserved failed\n",
				__func__);
	}
#endif

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
