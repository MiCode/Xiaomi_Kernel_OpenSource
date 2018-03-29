/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MMDVFS_MGR_H__
#define __MMDVFS_MGR_H__

#include "aee.h"
#include "mt_smi.h"

#define MMDVFS_LOG_TAG	"MMDVFS"

#define MMDVFSMSG(string, args...) \
	pr_err("[MMDVFS][pid=%d]"string, current->tgid, ##args)

#define MMDVFSERR(string, args...) do {\
	pr_err("[MMDVFS]error: "string, ##args); \
	aee_kernel_warning(MMDVFS_LOG_TAG, "error: "string, ##args);\
} while (0)


/* screen size */
extern unsigned int DISP_GetScreenWidth(void);
extern unsigned int DISP_GetScreenHeight(void);

extern void mmdvfs_init(MTK_SMI_BWC_MM_INFO *info);
extern void mmdvfs_notify_scenario_enter(MTK_SMI_BWC_SCEN scen);
extern void mmdvfs_notify_scenario_exit(MTK_SMI_BWC_SCEN scen);
extern void mmdvfs_notify_scenario_concurrency(unsigned int u4Concurrency);
extern void mmdvfs_handle_cmd(MTK_MMDVFS_CMD *cmd);

#endif
