/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __CCCI_UTIL_SIB_H__
#define __CCCI_UTIL_SIB_H__

struct ccci_sib_region {
	phys_addr_t base_md_view_phy;
	phys_addr_t base_ap_view_phy;
	void __iomem *base_ap_view_vir;
	unsigned int size;
};

#endif

