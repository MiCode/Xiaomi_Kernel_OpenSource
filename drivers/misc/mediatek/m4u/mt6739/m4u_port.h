/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __M4U_PORT_H__
#define __M4U_PORT_H__

/* ==================================== */
/* about portid */
/* ==================================== */

enum {
	M4U_PORT_DISP_OVL0,
	M4U_PORT_DISP_RDMA0,
	M4U_PORT_DISP_WDMA0,
	M4U_PORT_MDP_RDMA0,
	M4U_PORT_MDP_WDMA0,
	M4U_PORT_MDP_WROT0,
	M4U_PORT_DISP_FAKE_LARB0,

	M4U_PORT_VENC_RCPU,
	M4U_PORT_VENC_REC,
	M4U_PORT_VENC_BSDMA,
	M4U_PORT_VENC_SV_COMV,
	M4U_PORT_VENC_RD_COMV,
	M4U_PORT_JPGENC_RDMA,
	M4U_PORT_JPGENC_BSDMA,
	M4U_PORT_VENC_CUR_LUMA,
	M4U_PORT_VENC_CUR_CHROMA,
	M4U_PORT_VENC_REF_LUMA,
	M4U_PORT_VENC_REF_CHROMA,

	M4U_PORT_CAM_IMGO,
	M4U_PORT_CAM_RRZO,
	M4U_PORT_CAM_LSCI_0,
	M4U_PORT_CAM_LSCI_1,
	M4U_PORT_CAM_BPCI_0,
	M4U_PORT_CAM_BPCI_1,
	M4U_PORT_CAM_ESFKO,
	M4U_PORT_CAM_AAO,
	M4U_PORT_CAM_SV0,
	M4U_PORT_CAM_IMGI,
	M4U_PORT_CAM_IMG2O,

	M4U_PORT_UNKNOWN
};
#define M4U_PORT_NR M4U_PORT_UNKNOWN

#endif
