/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013-2016, 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * UFS debugfs - add debugfs interface to the ufshcd.
 * This is currently used for statistics collection and exporting from the
 * UFS driver.
 * This infrastructure can be used for debugging or direct tweaking
 * of the driver from userspace.
 *
 */

#ifndef _UFS_DEBUGFS_H
#define _UFS_DEBUGFS_H

#include <linux/debugfs.h>
#include "ufshcd.h"

enum ufsdbg_err_inject_scenario {
	ERR_INJECT_INTR,
	ERR_INJECT_PWR_CHANGE,
	ERR_INJECT_UIC,
	ERR_INJECT_DME_ATTR,
	ERR_INJECT_QUERY,
	ERR_INJECT_HIBERN8_ENTER,
	ERR_INJECT_HIBERN8_EXIT,
	ERR_INJECT_MAX_ERR_SCENARIOS,
};

#ifdef CONFIG_DEBUG_FS
void ufsdbg_add_debugfs(struct ufs_hba *hba);
void ufsdbg_remove_debugfs(struct ufs_hba *hba);
void ufsdbg_pr_buf_to_std(struct ufs_hba *hba, int offset, int num_regs,
				char *str, void *priv);
void ufsdbg_set_err_state(struct ufs_hba *hba);
void ufsdbg_clr_err_state(struct ufs_hba *hba);
#else
static inline void ufsdbg_add_debugfs(struct ufs_hba *hba)
{
}
static inline void ufsdbg_remove_debugfs(struct ufs_hba *hba)
{
}
static inline void ufsdbg_pr_buf_to_std(struct ufs_hba *hba, int offset,
	int num_regs, char *str, void *priv)
{
}
static inline void ufsdbg_set_err_state(struct ufs_hba *hba)
{
}
static inline void ufsdbg_clr_err_state(struct ufs_hba *hba)
{
}
#endif

#ifdef CONFIG_UFS_FAULT_INJECTION
void ufsdbg_error_inject_dispatcher(struct ufs_hba *hba,
			enum ufsdbg_err_inject_scenario err_scenario,
			int success_value, int *ret_value);
#else
static inline void ufsdbg_error_inject_dispatcher(struct ufs_hba *hba,
			enum ufsdbg_err_inject_scenario err_scenario,
			int success_value, int *ret_value)
{
}
#endif

#endif /* End of Header */
