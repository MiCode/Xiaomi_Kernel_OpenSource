/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __SMI_PMQOS_H__
#define __SMI_PMQOS_H__

#include <smi_port.h>

#define SMI_PMQOS_BWL_MAX	(0xfff)
#define SMI_PMQOS_BWL_MASK(b) \
	((b) > (SMI_PMQOS_BWL_MAX) ? (SMI_PMQOS_BWL_MAX) : (b))

#if IS_ENABLED(PMQOS_USE_IOMMU_PORT)
#define SMI_PMQOS_PORT_MASK(p)	MTK_IOMMU_TO_PORT(p)
#define SMI_PMQOS_LARB_DEC(l)	MTK_IOMMU_TO_LARB(l)
#define SMI_PMQOS_ENC(l, p)	MTK_M4U_ID(l, p)
#else
#define SMI_PMQOS_PORT_MASK(p)	((p) & 0xffff)
#define SMI_PMQOS_LARB_DEC(l)	((l) >> 16)
#define SMI_PMQOS_LARB_ENC(l)	((l) << 16)
#define SMI_PMQOS_ENC(l, p)	(SMI_PMQOS_LARB_ENC(l) | SMI_PMQOS_PORT_MASK(p))
#endif

#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
void
smi_bwl_update(const u32 id, const u32 bwl, const bool soft, const char *user);
void smi_ostd_update(const struct plist_head *head, const char *user);
#else
#define smi_bwl_update(id, bwl, soft, user) ((void)0)
#define smi_ostd_update(head, user) ((void)0)
#endif

#endif
