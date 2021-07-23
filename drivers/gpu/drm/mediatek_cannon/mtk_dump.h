/*
 * Copyright (C) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef __MTKFB_DUMP_H
#define __MTKFB_DUMP_H

#include "mtk_drm_ddp_comp.h"

int mtk_ovl_dump(struct mtk_ddp_comp *comp);
int mtk_rdma_dump(struct mtk_ddp_comp *comp);
int mtk_wdma_dump(struct mtk_ddp_comp *comp);
int mtk_rsz_dump(struct mtk_ddp_comp *comp);
int mtk_dsi_dump(struct mtk_ddp_comp *comp);
int mtk_dp_intf_dump(struct mtk_ddp_comp *comp);
int mtk_postmask_dump(struct mtk_ddp_comp *comp);
void mtk_color_dump(struct mtk_ddp_comp *comp);
void mtk_ccorr_dump(struct mtk_ddp_comp *comp);
void mtk_dither_dump(struct mtk_ddp_comp *comp);
void mtk_aal_dump(struct mtk_ddp_comp *comp);
void mtk_dmdp_aal_dump(struct mtk_ddp_comp *comp);
void mtk_gamma_dump(struct mtk_ddp_comp *comp);
void mtk_dsc_dump(struct mtk_ddp_comp *comp);
void mtk_merge_dump(struct mtk_ddp_comp *comp);

int mtk_ovl_analysis(struct mtk_ddp_comp *comp);
int mtk_rdma_analysis(struct mtk_ddp_comp *comp);
int mtk_wdma_analysis(struct mtk_ddp_comp *comp);
int mtk_rsz_analysis(struct mtk_ddp_comp *comp);
int mtk_dsi_analysis(struct mtk_ddp_comp *comp);
int mtk_dp_intf_analysis(struct mtk_ddp_comp *comp);
int mtk_postmask_analysis(struct mtk_ddp_comp *comp);
int mtk_dsc_analysis(struct mtk_ddp_comp *comp);
int mtk_merge_analysis(struct mtk_ddp_comp *comp);


int mtk_dump_reg(struct mtk_ddp_comp *comp);
int mtk_dump_analysis(struct mtk_ddp_comp *comp);

const char *mtk_dump_comp_str(struct mtk_ddp_comp *comp);
const char *mtk_dump_comp_str_id(unsigned int id);

void mtk_serial_dump_reg(void __iomem *base, unsigned int offset,
			 unsigned int num);
/* stop dump if offx is negative */
void mtk_cust_dump_reg(void __iomem *base, int off1, int off2, int o3, int o4);

#endif
