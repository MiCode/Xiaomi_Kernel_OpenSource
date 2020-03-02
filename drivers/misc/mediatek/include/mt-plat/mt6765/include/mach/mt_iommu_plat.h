/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MT_IOMMU_PLAT_H__
#define __MT_IOMMU_PLAT_H__
/*
 * this header file is only used for mtk_iomm module
 * the definition of header file is platform dependency.
 */

struct mtk_iommu_port {
	char *name;
	unsigned m4u_id: 2;
	unsigned m4u_slave: 2;
	unsigned larb_id: 4;
	unsigned larb_port: 8;
	unsigned tf_id: 12;     /* 12 bits */
	bool enable_tf;
	mtk_iommu_fault_callback_t fault_fn;
	void *fault_data;
};

#define MTK_IOMMU_PORT_INIT(name, slave, larb_inx, tf_larb_id, port)  {\
		name, 0, slave, larb_inx, port,\
		(((tf_larb_id)<<7)|((port)<<2)), 1\
}

struct mtk_iommu_port iommu_port[] = {
	/*Larb0 */
	MTK_IOMMU_PORT_INIT("DISP_OVL0", 0, 0, 0, 0),
	MTK_IOMMU_PORT_INIT("DISP_2L_OVL0_LARB0", 0, 0, 0, 1),
	MTK_IOMMU_PORT_INIT("DISP_RDMA0", 0, 0, 0, 2),
	MTK_IOMMU_PORT_INIT("DISP_WDMA0", 0, 0, 0, 3),
	MTK_IOMMU_PORT_INIT("MDP_RDMA0", 0, 0, 0, 4),
	MTK_IOMMU_PORT_INIT("MDP_WDMA0", 0, 0, 0, 5),
	MTK_IOMMU_PORT_INIT("MDP_WROT0", 0, 0, 0, 6),
	MTK_IOMMU_PORT_INIT("DISP_FAKE0", 0, 0, 0, 7),
	/*Larb1 */
	MTK_IOMMU_PORT_INIT("VDEC_MC", 0, 1, 1, 0),
	MTK_IOMMU_PORT_INIT("VENC_REC", 0, 1, 1, 1),
	MTK_IOMMU_PORT_INIT("VDEC_PP", 0, 1, 1, 2),
	MTK_IOMMU_PORT_INIT("VDEC_PRED_WR", 0, 1, 1, 3),
	MTK_IOMMU_PORT_INIT("VDEC_PRED_RD", 0, 1, 1, 4),
	MTK_IOMMU_PORT_INIT("JPGENC_RDMA", 0, 1, 1, 5),
	MTK_IOMMU_PORT_INIT("JPGENC_BSDMA", 0, 1, 1, 6),
	MTK_IOMMU_PORT_INIT("VDEC_VLD", 0, 1, 1, 7),
	MTK_IOMMU_PORT_INIT("VDEC_PPWRAP", 0, 1, 1, 8),
	MTK_IOMMU_PORT_INIT("VDEC_AVC_MV", 0, 1, 1, 9),
	MTK_IOMMU_PORT_INIT("VENC_REF_CHROMA", 0, 1, 1, 10),
	/*Larb2 */
	MTK_IOMMU_PORT_INIT("CAM_IMGI", 0, 2, 2, 0),
	MTK_IOMMU_PORT_INIT("CAM_IMG2O", 0, 2, 2, 1),
	MTK_IOMMU_PORT_INIT("CAM_IMG3O", 0, 2, 2, 2),
	MTK_IOMMU_PORT_INIT("CAM_VIPI", 0, 2, 2, 3),
	MTK_IOMMU_PORT_INIT("CAM_LCEI", 0, 2, 2, 4),
	MTK_IOMMU_PORT_INIT("CAM_FD_RP", 0, 2, 2, 5),
	MTK_IOMMU_PORT_INIT("CAM_FD_WR", 0, 2, 2, 6),
	MTK_IOMMU_PORT_INIT("CAM_FD_RB", 0, 2, 2, 7),
	MTK_IOMMU_PORT_INIT("CAM_DPE_RDMA", 0, 2, 2, 8),
	MTK_IOMMU_PORT_INIT("CAM_DPE_WDMA", 0, 2, 2, 9),
	MTK_IOMMU_PORT_INIT("CAM_RSC_RDMA0", 0, 2, 2, 10),
	MTK_IOMMU_PORT_INIT("CAM_RSC_WDMA", 0, 2, 2, 11),
	/*Larb3 */
	MTK_IOMMU_PORT_INIT("CAM_IMGO", 0, 3, 3, 0),
	MTK_IOMMU_PORT_INIT("CAM_RRZO", 0, 3, 3, 1),
	MTK_IOMMU_PORT_INIT("CAM_AAO", 0, 3, 3, 2),
	MTK_IOMMU_PORT_INIT("CAM_AFO", 0, 3, 3, 3),
	MTK_IOMMU_PORT_INIT("CAM_LSCI0", 0, 3, 3, 4),
	MTK_IOMMU_PORT_INIT("CAM_LSCI1", 0, 3, 3, 5),
	MTK_IOMMU_PORT_INIT("CAM_PDO", 0, 3, 3, 6),
	MTK_IOMMU_PORT_INIT("CAM_BPCI", 0, 3, 3, 7),
	MTK_IOMMU_PORT_INIT("CAM_LCSO", 0, 3, 3, 8),
	MTK_IOMMU_PORT_INIT("CAM_RSSO_A", 0, 3, 3, 9),
	MTK_IOMMU_PORT_INIT("CAM_RSSO_B", 0, 3, 3, 10),
	MTK_IOMMU_PORT_INIT("CAM_UFEO", 0, 3, 3, 11),
	MTK_IOMMU_PORT_INIT("CAM_SOCO", 0, 3, 3, 12),
	MTK_IOMMU_PORT_INIT("CAM_SOC1", 0, 3, 3, 13),
	MTK_IOMMU_PORT_INIT("CAM_SOC2", 0, 3, 3, 14),
	MTK_IOMMU_PORT_INIT("CAM_CCUI", 0, 3, 3, 15),
	MTK_IOMMU_PORT_INIT("CAM_CCUO", 0, 3, 3, 16),
	MTK_IOMMU_PORT_INIT("CAM_CACI", 0, 3, 3, 17),
	MTK_IOMMU_PORT_INIT("CAM_RAWI_A", 0, 3, 3, 18),
	MTK_IOMMU_PORT_INIT("CAM_RAWI_B", 0, 3, 3, 19),
	MTK_IOMMU_PORT_INIT("CAM_CCUG", 0, 3, 3, 20),

	MTK_IOMMU_PORT_INIT("UNKNOWN", 0, 0, 0, 0)
};

#endif
