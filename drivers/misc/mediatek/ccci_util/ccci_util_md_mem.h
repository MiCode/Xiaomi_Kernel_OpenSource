/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_UTIL_MODEM_MEM_H__
#define __CCCI_UTIL_MODEM_MEM_H__

#include "mt-plat/mtk_ccci_common.h"

u32 mtk_ccci_get_smem_by_id(enum SMEM_USER_ID user_id,
				void __iomem **o_ap_vir, phys_addr_t *o_ap_phy, u32 *o_md_phy);
unsigned int mtk_ccci_get_sib_inf(unsigned long long *base);
void __iomem *mtk_ccci_get_smem_start_addr(enum SMEM_USER_ID user_id, int *size_o);
phys_addr_t mtk_ccci_get_smem_phy_start_addr(enum SMEM_USER_ID user_id, int *size_o);
int mtk_ccci_md_smem_layout_init(void);
void mtk_ccci_dump_md_mem_layout(void);

unsigned int mtk_ccci_get_md_nc_smem_inf(void __iomem **o_ap_vir,
					phys_addr_t *o_ap_phy, u32 *o_md_phy);
unsigned int mtk_ccci_get_md_nc_smem_mpu_size(void);

unsigned int mtk_ccci_get_md_c_smem_inf(void __iomem **o_ap_vir,
					phys_addr_t *o_ap_phy, u32 *o_md_phy);
unsigned int mtk_ccci_get_md_c_smem_mpu_size(void);
int mtk_ccci_find_tag_inf(char *name, char *buf, unsigned int size);


#endif
