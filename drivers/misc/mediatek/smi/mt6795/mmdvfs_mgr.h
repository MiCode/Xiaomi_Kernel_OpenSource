#ifndef __MMDVFS_MGR_H__
#define __MMDVFS_MGR_H__

#include <linux/aee.h>

#define MMDVFS_LOG_TAG	"MMDVFS"

#define MMDVFSMSG(string, args...) if(1){\
 pr_warn("[pid=%d]"string, current->tgid, ##args); \
 }
#define MMDVFSMSG2(string, args...) pr_warn(string, ##args)
#define MMDVFSTMP(string, args...) pr_warn("[pid=%d]"string, current->tgid, ##args)
#define MMDVFSERR(string, args...) do{\
	pr_err("error: "string, ##args); \
	aee_kernel_warning(MMDVFS_LOG_TAG, "error: "string, ##args);  \
} while(0)


/* MMDVFS extern APIs */
extern void mmdvfs_init(MTK_SMI_BWC_MM_INFO *info);
extern void mmdvfs_handle_cmd(MTK_MMDVFS_CMD *cmd);
extern void mmdvfs_notify_scenario_enter(MTK_SMI_BWC_SCEN scen);
extern void mmdvfs_notify_scenario_exit(MTK_SMI_BWC_SCEN scen);
extern void mmdvfs_notify_scenario_concurrency(unsigned int u4Concurrency);

#endif /* __MMDVFS_MGR_H__ */

