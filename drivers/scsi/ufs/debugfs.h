/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
#include <linux/scsi/ufs/ufshcd.h>

#ifdef CONFIG_DEBUG_FS
void ufsdbg_add_debugfs(struct ufs_hba *hba);

void ufsdbg_remove_debugfs(struct ufs_hba *hba);
void ufsdbg_fail_request(struct ufs_hba *hba, u32 *intr_status);
void ufsdbg_pr_buf_to_std(struct ufs_hba *hba, int offset, int num_regs,
				char *str, void *priv);
#else
void ufsdbg_add_debugfs(struct ufs_hba *hba)
{
}
void ufsdbg_remove_debugfs(struct ufs_hba *hba)
{
}
void ufsdbg_fail_request(struct ufs_hba *hba, u32 *intr_status)
{
}
void ufsdbg_pr_buf_to_std(struct ufs_hba *hba, int offset, int num_regs,
				char *str, void *priv)
{
}
#endif
#endif /* End of Header */
