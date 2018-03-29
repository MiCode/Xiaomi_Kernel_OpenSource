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

#include <linux/module.h>
#include <linux/slab.h>

#include <linux/debugfs.h>

#include "ged_base.h"
#include "ged_debugFS.h"

#define GED_DEBUGFS_DIR_NAME "ged"

static struct dentry *gpsDebugFSEntryDir = NULL;

typedef struct _GED_DEBUGFS_PRIV_DATA_
{
	const struct seq_operations *psReadOps;
	GED_ENTRY_WRITE_FUNC*   pfnWrite;
	void*                   pvData;
} GED_DEBUGFS_PRIV_DATA;
//-----------------------------------------------------------------------------
static int ged_debugFS_open(struct inode *psINode, struct file *psFile)
{
	GED_DEBUGFS_PRIV_DATA *psPrivData = (GED_DEBUGFS_PRIV_DATA *)psINode->i_private;
	int iResult;

	iResult = seq_open(psFile, psPrivData->psReadOps);
	if (iResult == 0)
	{
		struct seq_file *psSeqFile = psFile->private_data;

		psSeqFile->private = psPrivData->pvData;

		return GED_OK;
	}

	return GED_ERROR_FAIL;
}
//-----------------------------------------------------------------------------
static ssize_t ged_debugFS_write(
		struct file*        psFile,
		const char __user*  pszBuffer,
		size_t              uiCount,
		loff_t*             puiPosition)
{
	struct inode *psINode = psFile->f_path.dentry->d_inode;
	GED_DEBUGFS_PRIV_DATA *psPrivData = (GED_DEBUGFS_PRIV_DATA *)psINode->i_private;

	if (psPrivData->pfnWrite == NULL)
	{
		return -EIO;
	}

	return psPrivData->pfnWrite(pszBuffer, uiCount, *puiPosition, psPrivData->pvData);
}
//-----------------------------------------------------------------------------
static const struct file_operations gsGEDDebugFSFileOps =
{
	.owner = THIS_MODULE,
	.open = ged_debugFS_open,
	.read = seq_read,
	.write = ged_debugFS_write,
	.llseek = seq_lseek,
	.release = seq_release,
};
//-----------------------------------------------------------------------------
GED_ERROR ged_debugFS_create_entry(
		const char*             pszName,
		void*                   pvDir,
		const struct seq_operations *psReadOps,
		GED_ENTRY_WRITE_FUNC*   pfnWrite,
		void*                   pvData,
		struct dentry**         ppsEntry)
{
	GED_DEBUGFS_PRIV_DATA* psPrivData;
	struct dentry* psEntry;
	umode_t uiMode;

	//assert(gpkDebugFSEntryDir != NULL);

	psPrivData = ged_alloc(sizeof(GED_DEBUGFS_PRIV_DATA));
	if (psPrivData == NULL)
	{
		return GED_ERROR_OOM;
	}

	psPrivData->psReadOps = psReadOps;
	psPrivData->pfnWrite = pfnWrite;
	psPrivData->pvData = pvData;

	uiMode = S_IFREG;

	if (psReadOps != NULL)
	{
		uiMode |= S_IRUGO;
	}

	if (pfnWrite != NULL)
	{
		uiMode |= S_IWUSR | S_IWGRP;
	}

	psEntry = debugfs_create_file(pszName,
			uiMode,
			(pvDir != NULL) ? (struct dentry *)pvDir : gpsDebugFSEntryDir,
			psPrivData,
			&gsGEDDebugFSFileOps);
	if (IS_ERR(psEntry))
	{
		GED_LOGE("Failed to create '%s' debugfs entry\n", pszName);
		return GED_ERROR_FAIL;
	}

	*ppsEntry = psEntry;

	return GED_OK;
}
//-----------------------------------------------------------------------------
void ged_debugFS_remove_entry(struct dentry *psEntry)
{
	if (psEntry->d_inode->i_private != NULL)
	{
		ged_free(psEntry->d_inode->i_private, sizeof(GED_DEBUGFS_PRIV_DATA));
	}

	debugfs_remove(psEntry);
}
//-----------------------------------------------------------------------------
GED_ERROR ged_debugFS_create_entry_dir(
		const char*     pszName,
		struct dentry*  psParentDir,
		struct dentry** ppsDir)
{
	struct dentry *psDir;

	if (pszName == NULL || ppsDir == NULL)
	{
		return GED_ERROR_INVALID_PARAMS;
	}

	psDir = debugfs_create_dir(pszName, (psParentDir) ? psParentDir : gpsDebugFSEntryDir);
	if (psDir == NULL)
	{
		GED_LOGE("Failed to create '%s' debugfs directory\n", pszName);
		return GED_ERROR_OOM;
	}

	*ppsDir = psDir;

	return GED_OK;
}
//-----------------------------------------------------------------------------
void ged_debugFS_remove_entry_dir(struct dentry *psDir)
{
	debugfs_remove(psDir);
}
//-----------------------------------------------------------------------------
GED_ERROR ged_debugFS_init(void)
{
	//assert(gpkDebugFSEntryDir == NULL);

	gpsDebugFSEntryDir = debugfs_create_dir(GED_DEBUGFS_DIR_NAME, NULL);
	if (gpsDebugFSEntryDir == NULL)
	{
		GED_LOGE("Failed to create '%s' debugfs root directory\n", GED_DEBUGFS_DIR_NAME);
		return GED_ERROR_OOM;
	}

	return GED_OK;
}
//-----------------------------------------------------------------------------
void ged_debugFS_exit(void)
{
	//assert(gpkDebugFSEntryDir != NULL);

	debugfs_remove(gpsDebugFSEntryDir);
	gpsDebugFSEntryDir = NULL;
}
//-----------------------------------------------------------------------------

