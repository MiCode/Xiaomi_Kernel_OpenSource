/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __AP_MODEM_MEM_H__
#define __AP_MODEM_MEM_H__

#include "mt-plat/mtk_ccci_common.h"

enum {
	SMF_CLR_RESET = (1 << 0), /* clear when reset modem */
	SMF_NCLR_FIRST = (1 << 1), /* do not clear even in MD first boot up */
	SMF_MD3_RELATED = (1 << 2), /* MD3 related share memory */
	SMF_NO_REMAP = (1 << 3), /* no need mapping region */
};

struct ccci_mem_region {
	phys_addr_t base_md_view_phy;
	phys_addr_t base_ap_view_phy;
	void __iomem *base_ap_view_vir;
	unsigned int size;
};

struct ccci_smem_region {
	/* pre-defined */
	unsigned int id;
	unsigned int offset; /* in bank4 */
	unsigned int size;
	unsigned int flag;
	/* runtime calculated */
	phys_addr_t base_md_view_phy;
	phys_addr_t base_ap_view_phy;
	void __iomem *base_ap_view_vir;
};

struct ccci_mem_layout {
	/* MD RO and RW (bank0) */
	struct ccci_mem_region md_bank0;

	/* share memory (bank4) */
	struct ccci_mem_region md_bank4_noncacheable_total;
	struct ccci_mem_region md_bank4_cacheable_total;

	/* share memory detail */
	struct ccci_smem_region *md_bank4_noncacheable;
	struct ccci_smem_region *md_bank4_cacheable;
};

struct ccci_mem_layout *ccci_md_get_mem(void);
struct ccci_smem_region *ccci_md_get_smem_by_user_id(enum SMEM_USER_ID user_id);
//void ccci_md_clear_smem(int first_boot);
void ap_md_mem_init(struct ccci_mem_layout *mem_layout);
int smem_md_state_notification(unsigned char state);

unsigned int mtk_ccci_get_md_nc_smem_inf(void __iomem **o_ap_vir, phys_addr_t *o_ap_phy,
						u32 *o_md_phy);
unsigned int mtk_ccci_get_md_c_smem_inf(void __iomem **o_ap_vir, phys_addr_t *o_ap_phy,
						u32 *o_md_phy);
u32 mtk_ccci_get_smem_by_id(enum SMEM_USER_ID user_id,
				void __iomem **o_ap_vir, phys_addr_t *o_ap_phy, u32 *o_md_phy);
#endif
