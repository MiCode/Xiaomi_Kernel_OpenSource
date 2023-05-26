/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __M4U_PORT_PRIV_H__
#define __M4U_PORT_PRIV_H__


#define M4U0_PORT_INIT(name, slave, larb_id, smi_select_larb_id, port)  {\
		name, 0, slave, larb_id, port, \
		(((smi_select_larb_id)<<7)|((port)<<2)), 1\
}

#define M4U1_PORT_INIT(name, slave, larb_id, smi_select_larb_id, port)  {\
		name, 1, slave, larb_id, port, \
		(((smi_select_larb_id)<<7)|((port)<<2)), 1\
}
/* HW code
 * Reference from "SMI Bus ID Map"
 * File Owner: Curtis Lee
 */
#define smi_select_larb0_id		(0<<2)
#define smi_select_larb1_id		(1<<2)
#define smi_select_larb2_id		(2<<2)
#define smi_select_larb3_id		(3<<2)
/* larb 4 : HW disconnected Jolin Chou */
/* #define smi_select_larb4_id	(0<<2) */
#define smi_select_larb5_id		(4<<2)
#define smi_select_larb6_id		(7<<2)
#define smi_select_larb_com_id	(6<<2)



struct m4u_port_t gM4uPort[] = {
	/* larb0 -MMSYS-9 */
	M4U0_PORT_INIT("DISP_POSTMASK0", 0, 0, smi_select_larb0_id, 0),
	M4U0_PORT_INIT("DISP_OVL0_HDR", 0, 0, smi_select_larb0_id, 1),
	M4U0_PORT_INIT("DISP_OVL1_HDR", 0, 0, smi_select_larb0_id, 2),
	M4U0_PORT_INIT("DISP_OVL0", 0, 0, smi_select_larb0_id, 3),
	M4U0_PORT_INIT("DISP_OVL1", 0, 0, smi_select_larb0_id, 4),
	M4U0_PORT_INIT("DISP_PVRIC0", 0, 0, smi_select_larb0_id, 5),
	M4U0_PORT_INIT("DISP_RDMA0", 0, 0, smi_select_larb0_id, 6),
	M4U0_PORT_INIT("DISP_WDMA0", 0, 0, smi_select_larb0_id, 7),
	M4U0_PORT_INIT("DISP_FAKE0", 0, 0, smi_select_larb0_id, 8),
	/*larb1-MMSYS-14*/
	M4U0_PORT_INIT("DISP_OVL0_2L_HDR", 0, 1, smi_select_larb1_id, 0),
	M4U0_PORT_INIT("DISP_OVL1_2L_HDR", 0, 1, smi_select_larb1_id, 1),
	M4U0_PORT_INIT("DISP_OVL0_2L", 0, 1, smi_select_larb1_id, 2),
	M4U0_PORT_INIT("DISP_OVL1_2L", 0, 1, smi_select_larb1_id, 3),
	M4U0_PORT_INIT("DISP_RDMA1", 0, 1, smi_select_larb1_id, 4),
	M4U0_PORT_INIT("MDP_PVRIC0", 0, 1, smi_select_larb1_id, 5),
	M4U0_PORT_INIT("MDP_PVRIC1", 0, 1, smi_select_larb1_id, 6),
	M4U0_PORT_INIT("MDP_RDMA0", 0, 1, smi_select_larb1_id, 7),
	M4U0_PORT_INIT("MDP_RDMA1", 0, 1, smi_select_larb1_id, 8),
	M4U0_PORT_INIT("MDP_WROT0_R", 0, 1, smi_select_larb1_id, 9),
	M4U0_PORT_INIT("MDP_WROT0_W", 0, 1, smi_select_larb1_id, 10),
	M4U0_PORT_INIT("MDP_WROT1_R", 0, 1, smi_select_larb1_id, 11),
	M4U0_PORT_INIT("MDP_WROT1_W", 0, 1, smi_select_larb1_id, 12),
	M4U0_PORT_INIT("DISP_FAKE1", 0, 1, smi_select_larb1_id, 13),
	/*larb2-VDEC-12*/
	M4U0_PORT_INIT("VDEC_MC_EXT", 0, 2, smi_select_larb2_id, 0),
	M4U0_PORT_INIT("VDEC_UFO_EXT", 0, 2, smi_select_larb2_id, 1),
	M4U0_PORT_INIT("VDEC_PP_EXT", 0, 2, smi_select_larb2_id, 2),
	M4U0_PORT_INIT("VDEC_PRED_RD_EXT", 0, 2, smi_select_larb2_id, 3),
	M4U0_PORT_INIT("VDEC_PRED_WR_EXT", 0, 2, smi_select_larb2_id, 4),
	M4U0_PORT_INIT("VDEC_PPWRAP_EXT", 0, 2, smi_select_larb2_id, 5),
	M4U0_PORT_INIT("VDEC_TILE_EXT", 0, 2, smi_select_larb2_id, 6),
	M4U0_PORT_INIT("VDEC_VLD_EXT", 0, 2, smi_select_larb2_id, 7),
	M4U0_PORT_INIT("VDEC_VLD2_EXT", 0, 2, smi_select_larb2_id, 8),
	M4U0_PORT_INIT("VDEC_AVC_MV_EXT", 0, 2, smi_select_larb2_id, 9),
	M4U0_PORT_INIT("VDEC_UFO_ENC_EXT", 0, 2, smi_select_larb2_id, 10),
	M4U0_PORT_INIT("VDEC_RG_CTRL_DMA_EXT", 0, 2, smi_select_larb2_id, 11),
	/*larb3-VENC-19*/
	M4U0_PORT_INIT("VENC_RCPU", 0, 3, smi_select_larb3_id, 0),
	M4U0_PORT_INIT("VENC_REC", 0, 3, smi_select_larb3_id, 1),
	M4U0_PORT_INIT("VENC_BSDMA", 0, 3, smi_select_larb3_id, 2),
	M4U0_PORT_INIT("VENC_SV_COMV", 0, 3, smi_select_larb3_id, 3),
	M4U0_PORT_INIT("VENC_RD_COMV", 0, 3, smi_select_larb3_id, 4),

	M4U0_PORT_INIT("VENC_NBM_RDMA", 0, 3, smi_select_larb3_id, 5),
	M4U0_PORT_INIT("VENC_NBM_RDMA_LITE", 0, 3, smi_select_larb3_id, 6),
	M4U0_PORT_INIT("JPGENC_Y_RDMA", 0, 3, smi_select_larb3_id, 7),
	M4U0_PORT_INIT("JPGENC_C_RDMA", 0, 3, smi_select_larb3_id, 8),
	M4U0_PORT_INIT("JPGENC_Q_TABLE", 0, 3, smi_select_larb3_id, 9),

	M4U0_PORT_INIT("JPGENC_BSDMA", 0, 3, smi_select_larb3_id, 10),
	M4U0_PORT_INIT("JPGDEC_WDMA", 0, 3, smi_select_larb3_id, 11),
	M4U0_PORT_INIT("JPGDEC_BSDMA", 0, 3, smi_select_larb3_id, 12),
	M4U0_PORT_INIT("VENC_NBM_WDMA", 0, 3, smi_select_larb3_id, 13),
	M4U0_PORT_INIT("VENC_NBM_WDMA_LITE", 0, 3, smi_select_larb3_id, 14),

	M4U0_PORT_INIT("VENC_CUR_LUMA", 0, 3, smi_select_larb3_id, 15),
	M4U0_PORT_INIT("VENC_CUR_CHROMA", 0, 3, smi_select_larb3_id, 16),
	M4U0_PORT_INIT("VENC_REF_LUMA", 0, 3, smi_select_larb3_id, 17),
	M4U0_PORT_INIT("VENC_REF_CHROMA", 0, 3, smi_select_larb3_id, 18),
	/* larb4-IMG-3
	 * HW disconnected
	 * Jolin Chou
	 */

	/*larb5-IMG-26*/
	M4U0_PORT_INIT("IMGI", 0, 5, smi_select_larb5_id, 0),
	M4U0_PORT_INIT("IMG2O", 0, 5, smi_select_larb5_id, 1),
	M4U0_PORT_INIT("IMG3O", 0, 5, smi_select_larb5_id, 2),
	M4U0_PORT_INIT("VIPI", 0, 5, smi_select_larb5_id, 3),
	M4U0_PORT_INIT("LCEI", 0, 5, smi_select_larb5_id, 4),
	M4U0_PORT_INIT("SMXI", 0, 5, smi_select_larb5_id, 5),
	M4U0_PORT_INIT("SMXO", 0, 5, smi_select_larb5_id, 6),
	M4U0_PORT_INIT("WPE0_RDMA1", 0, 5, smi_select_larb5_id, 7),

	M4U0_PORT_INIT("WPE0_RDMA0", 0, 5, smi_select_larb5_id, 8),
	M4U0_PORT_INIT("WPE0_WDMA", 0, 5, smi_select_larb5_id, 9),
	M4U0_PORT_INIT("FDVT_RDB", 0, 5, smi_select_larb5_id, 10),
	M4U0_PORT_INIT("FDVT_WRA", 0, 5, smi_select_larb5_id, 11),
	M4U0_PORT_INIT("FDVT_RDA", 0, 5, smi_select_larb5_id, 12),
	M4U0_PORT_INIT("WPE1_RDMA0", 0, 5, smi_select_larb5_id, 13),
	M4U0_PORT_INIT("WPE1_RDMA1", 0, 5, smi_select_larb5_id, 14),
	M4U0_PORT_INIT("WPE1_WDMA", 0, 5, smi_select_larb5_id, 15),

	M4U0_PORT_INIT("DPE_RDMA", 0, 5, smi_select_larb5_id, 16),
	M4U0_PORT_INIT("DPE_WDMA", 0, 5, smi_select_larb5_id, 17),
	M4U0_PORT_INIT("MFB_RDMA0", 0, 5, smi_select_larb5_id, 18),
	M4U0_PORT_INIT("MFB_RDMA1", 0, 5, smi_select_larb5_id, 19),
	M4U0_PORT_INIT("MFB_WDMA", 0, 5, smi_select_larb5_id, 20),
	M4U0_PORT_INIT("RSC_RDMA0", 0, 5, smi_select_larb5_id, 21),
	M4U0_PORT_INIT("RSC_WDMA", 0, 5, smi_select_larb5_id, 22),
	M4U0_PORT_INIT("OWE_RDMA", 0, 5, smi_select_larb5_id, 23),
	M4U0_PORT_INIT("OWE_WDMA", 0, 5, smi_select_larb5_id, 24),
	M4U0_PORT_INIT("FDVT_WRB", 0, 5, smi_select_larb5_id, 25),

	/*larb6-IMG-31 */
	M4U0_PORT_INIT("IMGO", 0, 6, smi_select_larb6_id, 0),
	M4U0_PORT_INIT("RRZO", 0, 6, smi_select_larb6_id, 1),
	M4U0_PORT_INIT("AAO,", 0, 6, smi_select_larb6_id, 2),
	M4U0_PORT_INIT("AFO,", 0, 6, smi_select_larb6_id, 3),
	M4U0_PORT_INIT("LSCI_0", 0, 6, smi_select_larb6_id, 4),
	M4U0_PORT_INIT("LSCI_1", 0, 6, smi_select_larb6_id, 5),
	M4U0_PORT_INIT("PDO", 0, 6, smi_select_larb6_id, 6),
	M4U0_PORT_INIT("BPCI", 0, 6, smi_select_larb6_id, 7),
	M4U0_PORT_INIT("LSCO", 0, 6, smi_select_larb6_id, 8),
	M4U0_PORT_INIT("RSSO_A", 0, 6, smi_select_larb6_id, 9),
	M4U0_PORT_INIT("UFEO", 0, 6, smi_select_larb6_id, 10),
	M4U0_PORT_INIT("SOCO", 0, 6, smi_select_larb6_id, 11),
	M4U0_PORT_INIT("SOC1", 0, 6, smi_select_larb6_id, 12),
	M4U0_PORT_INIT("SOC2", 0, 6, smi_select_larb6_id, 13),
	M4U0_PORT_INIT("CCUI", 0, 6, smi_select_larb6_id, 14),
	M4U0_PORT_INIT("CCUO", 0, 6, smi_select_larb6_id, 15),
	M4U0_PORT_INIT("RAWI_A", 0, 6, smi_select_larb6_id, 16),
	M4U0_PORT_INIT("CCUG", 0, 6, smi_select_larb6_id, 17),
	M4U0_PORT_INIT("PSO", 0, 6, smi_select_larb6_id, 18),
	M4U0_PORT_INIT("AFO_1", 0, 6, smi_select_larb6_id, 19),
	M4U0_PORT_INIT("LSCI_2", 0, 6, smi_select_larb6_id, 20),
	M4U0_PORT_INIT("PDI", 0, 6, smi_select_larb6_id, 21),
	M4U0_PORT_INIT("FLKO", 0, 6, smi_select_larb6_id, 22),
	M4U0_PORT_INIT("LMVO", 0, 6, smi_select_larb6_id, 23),
	M4U0_PORT_INIT("UFGO", 0, 6, smi_select_larb6_id, 24),
	M4U0_PORT_INIT("SPARE", 0, 6, smi_select_larb6_id, 25),
	M4U0_PORT_INIT("SPARE2", 0, 6, smi_select_larb6_id, 26),
	M4U0_PORT_INIT("SPARE3", 0, 6, smi_select_larb6_id, 27),
	M4U0_PORT_INIT("SPARE4", 0, 6, smi_select_larb6_id, 28),
	M4U0_PORT_INIT("SPARE5", 0, 6, smi_select_larb6_id, 29),
	M4U0_PORT_INIT("FAKE_ENGINE", 0, 6, smi_select_larb6_id, 30),

	/*SMI COMMON*/
	M4U0_PORT_INIT("CCU0", 0, 9, smi_select_larb_com_id, 0),
	M4U0_PORT_INIT("CCU1", 0, 9, smi_select_larb_com_id, 1),

	M4U1_PORT_INIT("VPU", 0, 0, 0, 0),

	M4U0_PORT_INIT("UNKNOWN", 0, 0, 0, 0)
};


#endif
