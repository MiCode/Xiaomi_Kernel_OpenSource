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

//Need to check isp3
/* ref: mtXXXX-larb-port.h */
struct m4u_port_larbX_cam {
	unsigned int l2_cam_imgo;
	unsigned int l2_cam_rrzo;
	unsigned int l2_cam_aao;
	unsigned int l2_cam_lcso;
	unsigned int l2_cam_esfko;
	unsigned int l2_cam_imgo_s;
	unsigned int l2_cam_imgo_s2;
	unsigned int l2_cam_lsci;
	unsigned int l2_cam_lsci_d;
	unsigned int l2_cam_afo;
	unsigned int l2_cam_afo_d;
	unsigned int l2_cam_bpci;
	unsigned int l2_cam_bpci_d;
	unsigned int l2_cam_ufdi;
	unsigned int l2_cam_imgi;
	unsigned int l2_cam_img2o;
	unsigned int l2_cam_img3o;
	unsigned int l2_cam_vipi;
	unsigned int l2_cam_vip2i;
	unsigned int l2_cam_vip3i;
	unsigned int l2_cam_icei;
};

extern int ISP3_SV_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_CAM_TYPE_ENUM module,
	unsigned int *pvalue);

extern int ISP3_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_CAM_TYPE_ENUM module,
	unsigned int *pvalue);

#endif
