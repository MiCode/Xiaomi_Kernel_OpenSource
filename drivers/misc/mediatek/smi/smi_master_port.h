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

#ifndef __SMI_MASTER_PORT_H__
#define __SMI_MASTER_PORT_H__

#define SMI_MASTER_LARB_OFFSET	(1 << 4)
#define SMI_MASTER_PORT_MASK	(0xFFFF)

#define	SMI_MASTER_ID(larb, port) \
	(((larb) << SMI_MASTER_LARB_OFFSET) | ((port) & SMI_MASTER_PORT_MASK))
#define SMI_LARB_ID_GET(master)	((master) >> SMI_MASTER_LARB_OFFSET)
#define SMI_PORT_ID_GET(master)	((master) & SMI_MASTER_PORT_MASK)

#if IS_ENABLED(CONFIG_MACH_MT6779)
#include "smi_master_mt6779.h"

#define SMI_COMM_MASTER_NR	(1 << 3)
#define SMI_LARB_MMDVFS_NR		(12)

void smi_ostd_update(struct plist_head *head);
void smi_bwl_update(const u32 larb, const u32 bwl, const bool soft);
#else
#define SMI_COMM_MASTER_NR	(0)
#define SMI_LARB_MMDVFS_NR		(0)

#define smi_ostd_update(head) ((void)0)
#define smi_bwl_update(larb, bwl, soft) ((void)0)
#endif

extern const u32 SMI_L1ARB_LARB[SMI_LARB_MMDVFS_NR];
extern const bool SMI_BUS_SEL_MASTER[SMI_COMM_MASTER_NR];

#endif
