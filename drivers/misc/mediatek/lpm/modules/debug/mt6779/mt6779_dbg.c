// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <mtk_dbg_common_v1.h>
#include <mtk_lpm_module.h>
#include <mt6779_dbg_fs_common.h>

static int __init mt6779_dbg_init(void)
{
#ifdef MTK_LPM_DBG_COMMON
	mtk_dbg_common_fs_init();
#endif
	mtk_cpupm_dbg_init();
	mt6779_dbg_fs_init();
	return 0;
}
static void __exit mt6779_dbg_exit(void)
{
#ifdef MTK_LPM_DBG_COMMON
	mtk_dbg_common_fs_exit();
#endif
	mt6779_dbg_fs_exit();
	mtk_cpupm_dbg_exit();
}


module_init(mt6779_dbg_init);
module_exit(mt6779_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT6779 Low Power debug KO");
MODULE_AUTHOR("MediaTek Inc.");
