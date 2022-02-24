// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
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


int SV_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_IRQ_TYPE_ENUM module,
	unsigned int *pvalue);

int ISP_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_IRQ_TYPE_ENUM module,
	unsigned int *pvalue);

#endif
