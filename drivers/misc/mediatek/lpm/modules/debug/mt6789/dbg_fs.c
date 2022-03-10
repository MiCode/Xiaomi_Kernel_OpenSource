// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/fs.h>

#include <lpm_dbg_fs_common.h>

#include <lpm_spm_comm.h>
#include <gs/lpm_pwr_gs.h>
#include <gs/v1/lpm_power_gs.h>

void __exit dbg_fs_exit(void)
{
	lpm_cpuidle_fs_deinit();
	lpm_spm_fs_deinit();
	lpm_rc_fs_deinit();
	lpm_dbg_deinit();
}

int __init dbg_fs_init(void)
{
	lpm_dbg_init();
	lpm_rc_fs_init();
	lpm_spm_fs_init();
	lpm_cpuidle_fs_init();
	pr_info("%s %d: finish", __func__, __LINE__);
	return 0;
}
