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

void mt6779_dbg_fs_exit(void)
{
	mt6779_dbg_idle_fs_deinit();
	mt6779_dbg_spm_fs_deinit();
}

int mt6779_dbg_fs_init(void)
{
	mt6779_logger_init();
	mt6779_dbg_idle_fs_init();
	mt6779_dbg_spm_fs_init();
	pr_info("%s %d: finish", __func__, __LINE__);
	return 0;
}

