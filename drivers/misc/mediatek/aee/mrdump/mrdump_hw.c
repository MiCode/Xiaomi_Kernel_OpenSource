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

#if IS_ENABLED(CONFIG_MTK_WATCHDOG)
#include <mtk_wd_api.h>

static void mrdump_wd_dram_reserved_mode(bool drm_ready)
{
	int res;
	struct wd_api *wd_api;

	res = get_wd_api(&wd_api);
	if (res < 0) {
		pr_notice("%s: get wd api error (%d)\n", __func__, res);
	} else {
		if (drm_ready) {
			res = wd_api->wd_dram_reserved_mode(drm_enabled);
			pr_notice("%s: DDR reserved mode enabled\n",
				  __func__);
		} else {
			pr_notice("%s: DDR reserved mode not ready, disabled\n",
				  __func__);
		}
		pr_notice("%s: config is not enabled yet\n", __func__);
	}
}

#endif

int __init mrdump_hw_init(bool drm_ready)
{
#if IS_ENABLED(CONFIG_MTK_WATCHDOG)
	mrdump_wd_dram_reserved_mode(drm_enabled);
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
