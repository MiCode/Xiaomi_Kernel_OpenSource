/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef _CAM_QOS_H
#define _CAM_QOS_H
#include "inc/camera_isp.h"

enum E_QOS_OP {
	E_QOS_UNKNOWN,
	E_BW_ADD,
	E_BW_UPDATE,
	E_BW_CLR,
	E_BW_REMOVE,
	E_CLK_ADD,
	E_CLK_UPDATE,
	E_CLK_CLR,
	E_CLK_REMOVE,
	E_CLK_SUPPORTED,	/*supported clk*/
	E_CLK_CUR,		/*current clk*/
	E_MAX,
};

struct m4u_port_larbX_cam {
	unsigned int l13_cam_mrawi;
	unsigned int l13_cam_mrawo0;
	unsigned int l13_cam_mrawo1;
	unsigned int l13_cam_camsv1;
	unsigned int l13_cam_camsv2;
	unsigned int l13_cam_camsv3;
	unsigned int l13_cam_camsv4;
	unsigned int l13_cam_camsv5;
	unsigned int l13_cam_camsv6;

	unsigned int l14_cam_mrawi;
	unsigned int l14_cam_mrawo0;
	unsigned int l14_cam_mrawo1;
	unsigned int l14_cam_camsv0;

	unsigned int l16_cam_imgo_r1_a;
	unsigned int l16_cam_rrzo_r1_a;
	unsigned int l16_cam_cqi_r1_a;
	unsigned int l16_cam_bpci_r1_a;
	unsigned int l16_cam_yuvo_r1_a;
	unsigned int l16_cam_ufdi_r2_a;
	unsigned int l16_cam_rawi_r2_a;
	unsigned int l16_cam_rawi_r3_a;
	unsigned int l16_cam_aao_r1_a;
	unsigned int l16_cam_afo_r1_a;
	unsigned int l16_cam_flko_r1_a;
	unsigned int l16_cam_lceso_r1_a;
	unsigned int l16_cam_crzo_r1_a;
	unsigned int l16_cam_ltmso_r1_a;
	unsigned int l16_cam_rsso_r1_a;
	unsigned int l16_cam_aaho_r1_a;
	unsigned int l16_cam_lsci_r1_a;

	unsigned int l17_cam_imgo_r1_b;
	unsigned int l17_cam_rrzo_r1_b;
	unsigned int l17_cam_cqi_r1_b;
	unsigned int l17_cam_bpci_r1_b;
	unsigned int l17_cam_yuvo_r1_b;
	unsigned int l17_cam_ufdi_r2_b;
	unsigned int l17_cam_rawi_r2_b;
	unsigned int l17_cam_rawi_r3_b;
	unsigned int l17_cam_aao_r1_b;
	unsigned int l17_cam_afo_r1_b;
	unsigned int l17_cam_flko_r1_b;
	unsigned int l17_cam_lceso_r1_b;
	unsigned int l17_cam_crzo_r1_b;
	unsigned int l17_cam_ltmso_r1_b;
	unsigned int l17_cam_rsso_r1_b;
	unsigned int l17_cam_aaho_r1_b;
	unsigned int l17_cam_lsci_r1_b;

	unsigned int l18_cam_imgo_r1_c;
	unsigned int l18_cam_rrzo_r1_c;
	unsigned int l18_cam_cqi_r1_c;
	unsigned int l18_cam_bpci_r1_c;
	unsigned int l18_cam_yuvo_r1_c;
	unsigned int l18_cam_ufdi_r2_c;
	unsigned int l18_cam_rawi_r2_c;
	unsigned int l18_cam_rawi_r3_c;
	unsigned int l18_cam_aao_r1_c;
	unsigned int l18_cam_afo_r1_c;
	unsigned int l18_cam_flko_r1_c;
	unsigned int l18_cam_lceso_r1_c;
	unsigned int l18_cam_crzo_r1_c;
	unsigned int l18_cam_ltmso_r1_c;
	unsigned int l18_cam_rsso_r1_c;
	unsigned int l18_cam_aaho_r1_c;
	unsigned int l18_cam_lsci_r1_c;
};

extern int SV_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_IRQ_TYPE_ENUM module,
	unsigned int *pvalue);

extern int ISP_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_IRQ_TYPE_ENUM module,
	unsigned int *pvalue);

#endif
