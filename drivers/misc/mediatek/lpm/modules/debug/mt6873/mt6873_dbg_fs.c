// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/fs.h>

#include <mt6873_dbg_fs_common.h>

void __exit mt6873_dbg_fs_exit(void)
{
	mt6873_dbg_cpuidle_fs_deinit();
	mt6873_dbg_spm_fs_deinit();
	mt6873_dbg_lpm_fs_deinit();
	mt6873_dbg_lpm_deinit();
}

int __init mt6873_dbg_fs_init(void)
{
	mt6873_dbg_lpm_init();
	mt6873_dbg_lpm_fs_init();
	mt6873_dbg_spm_fs_init();
	mt6873_dbg_cpuidle_fs_init();
	pr_info("%s %d: finish", __func__, __LINE__);
	return 0;
}
