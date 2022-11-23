/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef _CAM_QOS_H
#define _CAM_QOS_H
#include "inc/camera_isp.h"

#define CAM_QOS_COMP_NAME "mediatek,cam_qos_legacy"

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

/* ref: mtXXXX-larb-port.h */
struct m4u_port_larbX_cam {
	unsigned int l3_cam_imgo;
	unsigned int l3_cam_rrzo;
	unsigned int l3_cam_aao;
	unsigned int l3_cam_afo;
	unsigned int l3_cam_lsci0;
	unsigned int l3_cam_lsci1;
	unsigned int l3_cam_pdo;
	unsigned int l3_cam_bpci;
	unsigned int l3_cam_lcso;
	unsigned int l3_cam_rsso_a;
	unsigned int l3_cam_rsso_b;
	unsigned int l3_cam_ufeo;
	unsigned int l3_cam_soc0;
	unsigned int l3_cam_soc1;
	unsigned int l3_cam_soc2;
	unsigned int l3_cam_rawi_a;
	unsigned int l3_cam_rawi_b;
};

extern int ISP4_SV_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_IRQ_TYPE_ENUM module,
	unsigned int *pvalue);

extern int ISP4_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_IRQ_TYPE_ENUM module,
	unsigned int *pvalue);

#endif
