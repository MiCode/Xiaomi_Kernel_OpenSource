/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
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
void mtk_disp_tdshp_dump(struct mtk_ddp_comp *comp);
void mtk_color_dump(struct mtk_ddp_comp *comp);
void mtk_ccorr_dump(struct mtk_ddp_comp *comp);
void mtk_c3d_dump(struct mtk_ddp_comp *comp);
void mtk_dither_dump(struct mtk_ddp_comp *comp);
void mtk_aal_dump(struct mtk_ddp_comp *comp);
void mtk_dmdp_aal_dump(struct mtk_ddp_comp *comp);
void mtk_gamma_dump(struct mtk_ddp_comp *comp);
void mtk_dsc_dump(struct mtk_ddp_comp *comp);
void mtk_merge_dump(struct mtk_ddp_comp *comp);
void mtk_cm_dump(struct mtk_ddp_comp *comp);
void mtk_spr_dump(struct mtk_ddp_comp *comp);
void mtk_chist_dump(struct mtk_ddp_comp *comp);
void mtk_inlinerotate_dump(struct mtk_ddp_comp *comp);
void mtk_dli_async_dump(struct mtk_ddp_comp *comp);
void mtk_dlo_async_dump(struct mtk_ddp_comp *comp);
void mtk_y2r_dump(struct mtk_ddp_comp *comp);
void mtk_mmlsys_bypass_dump(struct mtk_ddp_comp *comp);

int mtk_ovl_analysis(struct mtk_ddp_comp *comp);
int mtk_rdma_analysis(struct mtk_ddp_comp *comp);
int mtk_wdma_analysis(struct mtk_ddp_comp *comp);
int mtk_rsz_analysis(struct mtk_ddp_comp *comp);
int mtk_dsi_analysis(struct mtk_ddp_comp *comp);
int mtk_dp_intf_analysis(struct mtk_ddp_comp *comp);
int mtk_postmask_analysis(struct mtk_ddp_comp *comp);
int mtk_dsc_analysis(struct mtk_ddp_comp *comp);
int mtk_merge_analysis(struct mtk_ddp_comp *comp);
int mtk_cm_analysis(struct mtk_ddp_comp *comp);
int mtk_spr_analysis(struct mtk_ddp_comp *comp);
int mtk_chist_analysis(struct mtk_ddp_comp *comp);
int mtk_inlinerotate_analysis(struct mtk_ddp_comp *comp);
int mtk_dli_async_analysis(struct mtk_ddp_comp *comp);
int mtk_dlo_async_analysis(struct mtk_ddp_comp *comp);
int mtk_y2r_analysis(struct mtk_ddp_comp *comp);
int mtk_mmlsys_bypass_analysis(struct mtk_ddp_comp *comp);

int mtk_dump_reg(struct mtk_ddp_comp *comp);
int mtk_dump_analysis(struct mtk_ddp_comp *comp);

const char *mtk_dump_comp_str(struct mtk_ddp_comp *comp);
const char *mtk_dump_comp_str_id(unsigned int id);

void mtk_serial_dump_reg(void __iomem *base, unsigned int offset,
			 unsigned int num);
/* stop dump if offx is negative */
void mtk_cust_dump_reg(void __iomem *base, int off1, int off2, int o3, int o4);

#endif
