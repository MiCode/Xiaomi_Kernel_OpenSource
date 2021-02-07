#ifndef _UFS_STATISTICS_H
#define _UFS_STATISTICS_H

#include "ufshcd.h"

#ifdef CONFIG_HWCONF_MANAGER
void ufs_add_statistics(struct ufs_hba *hba);
void ufs_remove_statistics(struct ufs_hba *hba);
#else
static inline void ufs_add_statistics(struct ufs_hba *hba)
{
};
static inline void ufs_remove_statistics(struct ufs_hba *hba)
{
};

#endif

#endif /* End of Header */
