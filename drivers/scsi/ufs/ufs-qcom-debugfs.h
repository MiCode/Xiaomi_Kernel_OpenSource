/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015, Linux Foundation. All rights reserved.
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
 */

#ifndef QCOM_DEBUGFS_H_
#define QCOM_DEBUGFS_H_

#include "ufshcd.h"

#ifdef CONFIG_DEBUG_FS
void ufs_qcom_dbg_add_debugfs(struct ufs_hba *hba, struct dentry *root);
#endif

#endif /* End of Header */
