/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HCP_ISP71_H
#define MTK_HCP_ISP71_H

#include "mtk-hcp.h"


int isp71_release_working_buffer(struct mtk_hcp *hcp_dev);
int isp71_allocate_working_buffer(struct mtk_hcp *hcp_dev, unsigned int mode);
int isp71_get_init_info(struct img_init_info *info);
void *isp71_get_gce_virt(void);
void *isp71_get_hwid_virt(void);

extern struct mtk_hcp_data isp71_hcp_data;

#endif /* _MTK_HCP_ISP71_H */
