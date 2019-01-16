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
    const char*             pszName,
	void*                   pvDir,
	struct seq_operations*  psReadOps,
    GED_ENTRY_WRITE_FUNC*   pfnWrite,
	void*                   pvData,
	struct dentry**         ppsEntry);

void ged_debugFS_remove_entry(
    struct dentry *psEntry);

GED_ERROR ged_debugFS_create_entry_dir(
    const char*     pszName,
    struct dentry*  psParentDir,
    struct dentry** ppsDir);

void ged_debugFS_remove_entry_dir(
    struct dentry *psDir);

GED_ERROR ged_debugFS_init(void);

void ged_debugFS_exit(void);

#endif
