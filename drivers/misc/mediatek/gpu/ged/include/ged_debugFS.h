/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __GED_DEBUG_FS_H__
#define __GED_DEBUG_FS_H__

#include <linux/seq_file.h>
#include "ged_type.h"

typedef ssize_t (GED_ENTRY_WRITE_FUNC)(
    const char __user *pszBuffer,
	size_t uiCount,
	loff_t uiPosition,
	void *pvData);

GED_ERROR ged_debugFS_create_entry(
    const char             *pszName,
	void                   *pvDir,
	const struct seq_operations *psReadOps,
    GED_ENTRY_WRITE_FUNC   *pfnWrite,
	void                   *pvData,
	struct dentry         **ppsEntry);

void ged_debugFS_remove_entry(
    struct dentry *psEntry);

GED_ERROR ged_debugFS_create_entry_dir(
    const char     *pszName,
    struct dentry  *psParentDir,
    struct dentry **ppsDir);

void ged_debugFS_remove_entry_dir(
    struct dentry *psDir);

GED_ERROR ged_debugFS_init(void);

void ged_debugFS_exit(void);

#endif
