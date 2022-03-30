/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HCP_ISP71_H
#define MTK_HCP_ISP71_H

#include "mtk-hcp.h"

enum isp71_rsv_mem_id_t {
	DIP_MEM_FOR_HW_ID,
	IMG_MEM_FOR_HW_ID = DIP_MEM_FOR_HW_ID, /*shared buffer for ipi_param*/
	/*need replace DIP_MEM_FOR_HW_ID & DIP_MEM_FOR_SW_ID*/
	WPE_MEM_C_ID,	/*module cq buffer*/
	WPE_MEM_T_ID,	/*module tdr buffer*/
	TRAW_MEM_C_ID,	/*module cq buffer*/
	TRAW_MEM_T_ID,	/*module tdr buffer*/
	DIP_MEM_C_ID,	/*module cq buffer*/
	DIP_MEM_T_ID,	/*module tdr buffer*/
	PQDIP_MEM_C_ID,	/*module cq buffer*/
	PQDIP_MEM_T_ID,	/*module tdr buffer*/
	ADL_MEM_C_ID,	/*module cq buffer*/
	ADL_MEM_T_ID,	/*module tdr buffer*/
	IMG_MEM_G_ID,	/*gce cmd buffer*/
	NUMS_MEM_ID,
};

int isp71_release_working_buffer(struct mtk_hcp *hcp_dev);
int isp71_allocate_working_buffer(struct mtk_hcp *hcp_dev, unsigned int mode);
int isp71_get_init_info(struct img_init_info *info);
void *isp71_get_gce_virt(void);
void *isp71_get_hwid_virt(void);

extern struct mtk_hcp_data isp71_hcp_data;

#endif /* _MTK_HCP_ISP71_H */
