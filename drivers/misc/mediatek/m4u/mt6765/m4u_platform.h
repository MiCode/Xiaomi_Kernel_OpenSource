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

#ifndef __M4U_PORT_PRIV_H__
#define __M4U_PORT_PRIV_H__

#ifdef CONFIG_MACH_MT6765
static const char *const gM4U_SMILARB[] = {
	"mediatek,smi_larb0", "mediatek,smi_larb1", "mediatek,smi_larb2", "mediatek,smi_larb3"
};

#define M4U0_PORT_INIT(name, slave, larb, port)  {\
		name, 0, slave, larb, port, (((larb)<<7)|((port)<<2)), 1\
}

struct m4u_port_t gM4uPort[] = {
	/*Larb0 */
	M4U0_PORT_INIT("DISP_OVL0", 0, 0, 0),
	M4U0_PORT_INIT("DISP_2L_OVL0_LARB0", 0, 0, 1),
	M4U0_PORT_INIT("DISP_RDMA0", 0, 0, 2),
	M4U0_PORT_INIT("DISP_WDMA0", 0, 0, 3),
	M4U0_PORT_INIT("MDP_RDMA0", 0, 0, 4),
	M4U0_PORT_INIT("MDP_WDMA0", 0, 0, 5),
	M4U0_PORT_INIT("MDP_WROT0", 0, 0, 6),
	M4U0_PORT_INIT("DISP_FAKE0", 0, 0, 7),
	/*Larb1 */
	M4U0_PORT_INIT("VDEC_MC", 0, 1, 0),
	M4U0_PORT_INIT("VENC_REC", 0, 1, 1),
	M4U0_PORT_INIT("VDEC_PP", 0, 1, 2),
	M4U0_PORT_INIT("VDEC_PRED_WR", 0, 1, 3),
	M4U0_PORT_INIT("VDEC_PRED_RD", 0, 1, 4),
	M4U0_PORT_INIT("JPGENC_RDMA", 0, 1, 5),
	M4U0_PORT_INIT("JPGENC_BSDMA", 0, 1, 6),
	M4U0_PORT_INIT("VDEC_VLD", 0, 1, 7),
	M4U0_PORT_INIT("VDEC_PPWRAP", 0, 1, 8),
	M4U0_PORT_INIT("VDEC_AVC_MV", 0, 1, 9),
	M4U0_PORT_INIT("VENC_REF_CHROMA", 0, 1, 10),
	/*Larb2 */
	M4U0_PORT_INIT("CAM_IMGI", 0, 2, 0),
	M4U0_PORT_INIT("CAM_IMG2O", 0, 2, 1),
	M4U0_PORT_INIT("CAM_IMG3O", 0, 2, 2),
	M4U0_PORT_INIT("CAM_VIPI", 0, 2, 3),
	M4U0_PORT_INIT("CAM_LCEI", 0, 2, 4),
	M4U0_PORT_INIT("CAM_FD_RP", 0, 2, 5),
	M4U0_PORT_INIT("CAM_FD_WR", 0, 2, 6),
	M4U0_PORT_INIT("CAM_FD_RB", 0, 2, 7),
	M4U0_PORT_INIT("CAM_DPE_RDMA", 0, 2, 8),
	M4U0_PORT_INIT("CAM_DPE_WDMA", 0, 2, 9),
	M4U0_PORT_INIT("CAM_RSC_RDMA0", 0, 2, 10),
	M4U0_PORT_INIT("CAM_RSC_WDMA", 0, 2, 11),
	/*Larb3 */
	M4U0_PORT_INIT("CAM_IMGO", 0, 3, 0),
	M4U0_PORT_INIT("CAM_RRZO", 0, 3, 1),
	M4U0_PORT_INIT("CAM_AAO", 0, 3, 2),
	M4U0_PORT_INIT("CAM_AFO", 0, 3, 3),
	M4U0_PORT_INIT("CAM_LSCI0", 0, 3, 4),
	M4U0_PORT_INIT("CAM_LSCI1", 0, 3, 5),
	M4U0_PORT_INIT("CAM_PDO", 0, 3, 6),
	M4U0_PORT_INIT("CAM_BPCI", 0, 3, 7),
	M4U0_PORT_INIT("CAM_LCSO", 0, 3, 8),
	M4U0_PORT_INIT("CAM_RSSO_A", 0, 3, 9),
	M4U0_PORT_INIT("CAM_RSSO_B", 0, 3, 10),
	M4U0_PORT_INIT("CAM_UFEO", 0, 3, 11),
	M4U0_PORT_INIT("CAM_SOCO", 0, 3, 12),
	M4U0_PORT_INIT("CAM_SOC1", 0, 3, 13),
	M4U0_PORT_INIT("CAM_SOC2", 0, 3, 14),
	M4U0_PORT_INIT("CAM_CCUI", 0, 3, 15),
	M4U0_PORT_INIT("CAM_CCUO", 0, 3, 16),
	M4U0_PORT_INIT("CAM_CACI", 0, 3, 17),
	M4U0_PORT_INIT("CAM_RAWI_A", 0, 3, 18),
	M4U0_PORT_INIT("CAM_RAWI_B", 0, 3, 19),
	M4U0_PORT_INIT("CAM_CCUG", 0, 3, 20),

	M4U0_PORT_INIT("UNKNOWN", 0, 0, 0)
};
#elif defined(CONFIG_MACH_MT6761)
static const char *const gM4U_SMILARB[] = {
	"mediatek,smi_larb0", "mediatek,smi_larb1", "mediatek,smi_larb2"
};

#define M4U0_PORT_INIT(name, slave, larb, port)  {\
		name, 0, slave, larb, port, (((larb)<<7)|((port)<<2)), 1\
}

m4u_port_t gM4uPort[] = {
	/*Larb0 */
	M4U0_PORT_INIT("DISP_OVL0", 0, 0, 0),
	M4U0_PORT_INIT("DISP_2L_OVL0_LARB0", 0, 0, 1),
	M4U0_PORT_INIT("DISP_RDMA0", 0, 0, 2),
	M4U0_PORT_INIT("DISP_WDMA0", 0, 0, 3),
	M4U0_PORT_INIT("MDP_RDMA0", 0, 0, 4),
	M4U0_PORT_INIT("MDP_WDMA0", 0, 0, 5),
	M4U0_PORT_INIT("MDP_WROT0", 0, 0, 6),
	M4U0_PORT_INIT("DISP_FAKE0", 0, 0, 7),
	/*Larb1 */
	M4U0_PORT_INIT("VDEC_MC", 0, 1, 0),
	M4U0_PORT_INIT("VENC_REC", 0, 1, 1),
	M4U0_PORT_INIT("VDEC_PP", 0, 1, 2),
	M4U0_PORT_INIT("VDEC_PRED_WR", 0, 1, 3),
	M4U0_PORT_INIT("VDEC_PRED_RD", 0, 1, 4),
	M4U0_PORT_INIT("JPGENC_RDMA", 0, 1, 5),
	M4U0_PORT_INIT("JPGENC_BSDMA", 0, 1, 6),
	M4U0_PORT_INIT("VDEC_VLD", 0, 1, 7),
	M4U0_PORT_INIT("VDEC_PPWRAP", 0, 1, 8),
	M4U0_PORT_INIT("VDEC_AVC_MV", 0, 1, 9),
	M4U0_PORT_INIT("VENC_REF_CHROMA", 0, 1, 10),
	/*Larb2 */
	M4U0_PORT_INIT("CAM_IMGO", 0, 2, 0),
	M4U0_PORT_INIT("CAM_RRZO", 0, 2, 1),
	M4U0_PORT_INIT("CAM_AAO", 0, 2, 2),
	M4U0_PORT_INIT("CAM_LCSO", 0, 2, 3),
	M4U0_PORT_INIT("CAM_ESFKO", 0, 2, 4),
	M4U0_PORT_INIT("CAM_IMGO_S", 0, 2, 5),
	M4U0_PORT_INIT("CAM_IMGO_S2", 0, 2, 6),
	M4U0_PORT_INIT("CAM_LSCI_0", 0, 2, 7),
	M4U0_PORT_INIT("CAM_LSCI_D", 0, 2, 8),
	M4U0_PORT_INIT("CAM_AFO", 0, 2, 9),
	M4U0_PORT_INIT("CAM_AFO_D", 0, 2, 10),
	M4U0_PORT_INIT("CAM_BPCI", 0, 2, 11),
	M4U0_PORT_INIT("CAM_BPCI_D", 0, 2, 12),
	M4U0_PORT_INIT("CAM_UFDI", 0, 2, 13),
	M4U0_PORT_INIT("CAM_IMGI", 0, 2, 14),
	M4U0_PORT_INIT("CAM_IMG2O", 0, 2, 15),
	M4U0_PORT_INIT("CAM_IMG3O", 0, 2, 16),
	M4U0_PORT_INIT("CAM_VIPI", 0, 2, 17),
	M4U0_PORT_INIT("CAM_VIP2I", 0, 2, 18),
	M4U0_PORT_INIT("CAM_VIP3I", 0, 2, 19),
	M4U0_PORT_INIT("CAM_ICEI", 0, 2, 20),
	M4U0_PORT_INIT("CAM_FD_RB", 0, 2, 21),
	M4U0_PORT_INIT("CAM_FD_RP", 0, 2, 22),
	M4U0_PORT_INIT("CAM_FD_WR", 0, 2, 23),

	M4U0_PORT_INIT("UNKNOWN", 0, 0, 0)
};
#endif
#endif
