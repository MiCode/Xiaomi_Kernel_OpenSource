/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SMI_MASTER_PORT_H__
#define __SMI_MASTER_PORT_H__

#define SMI_MASTER_LARB_OFFSET	(1 << 4)
#define SMI_MASTER_PORT_MASK	(0xFFFF)

#define	SMI_MASTER_ID(larb, port) \
	(((larb) << SMI_MASTER_LARB_OFFSET) | ((port) & SMI_MASTER_PORT_MASK))
#define SMI_LARB_ID_GET(master)	((master) >> SMI_MASTER_LARB_OFFSET)
#define SMI_PORT_ID_GET(master)	((master) & SMI_MASTER_PORT_MASK)

#if IS_ENABLED(CONFIG_INTERCONNECT_MTK_MMQOS_MT6779)
#include "smi_master_mt6779.h"

#define SMI_COMM_MASTER_NR	(1 << 3)
#define SMI_LARB_MMDVFS_NR	(12)

#define PORT_VIRTUAL_DISP		MASTER_COMMON_PORT(0, 8)
#define PORT_VIRTUAL_CCU_COMMON		SLAVE_LARB(12)

void smi_ostd_update(struct plist_head *head);
void smi_bwl_update(const u32 larb, const u32 bwl, const bool soft);
#else
#define SMI_COMM_MASTER_NR	(0)
#define SMI_LARB_MMDVFS_NR	(0)

#define PORT_VIRTUAL_DISP		(0)
#define PORT_VIRTUAL_CCU_COMMON		(0)

#define smi_ostd_update(head) ((void)0)
#define smi_bwl_update(larb, bwl, soft) ((void)0)
#endif

extern const u32 SMI_L1ARB_LARB[SMI_LARB_MMDVFS_NR];
extern const bool SMI_BUS_SEL_MASTER[SMI_COMM_MASTER_NR];

#endif
