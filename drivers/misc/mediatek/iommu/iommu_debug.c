// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt)    "[iommu_debug] " fmt

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <dt-bindings/memory/mtk-smi-larb-port.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif
#include "iommu_debug.h"

#define ERROR_LARB_PORT_ID		0xFFFF
#define F_MMU_INT_TF_MSK		GENMASK(11, 2)
#define F_APU_MMU_INT_TF_MSK(id)	FIELD_GET(GENMASK(10, 7), id)

enum mtk_iommu_type {
	MM_IOMMU,
	APU_IOMMU,
	TYPE_NUM
};

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define m4u_aee_print(string, args...) do {\
		char m4u_name[100];\
		snprintf(m4u_name, 100, "[M4U]"string, ##args); \
	aee_kernel_warning_api(__FILE__, __LINE__, \
		DB_OPT_MMPROFILE_BUFFER | DB_OPT_DUMP_DISPLAY, \
		m4u_name, "[M4U] error"string, ##args); \
	pr_err("[M4U] error:"string, ##args);  \
	} while (0)

#else
#define m4u_aee_print(string, args...) do {\
		char m4u_name[100];\
		snprintf(m4u_name, 100, "[M4U]"string, ##args); \
	pr_err("[M4U] error:"string, ##args);  \
	} while (0)
#endif

#define MM_IOMMU_PORT_INIT(name, m4u_id, larb_id, tf_larb, port_id)  {\
	name, m4u_id, larb_id, port_id, (((tf_larb)<<7)|((port_id)<<2)), 1\
}

#define APU_IOMMU_PORT_INIT(name, m4u_id, larb_id, port_id, tf_id) {\
	name, m4u_id, larb_id, port_id, tf_id\
}

#define mmu_translation_log_format \
	"CRDISPATCH_KEY:M4U_%s\ntranslation fault:port=%s,mva=0x%x,pa=0x%x\n"

struct mtk_iommu_port {
	char *name;
	unsigned m4u_id: 2;
	unsigned larb_id: 4;
	unsigned port_id: 8;
	unsigned tf_id: 12;     /* 12 bits */
	bool enable_tf;
};

struct mtk_iommu_cb {
	mtk_iommu_fault_callback_t fault_fn;
	void *fault_data;
};

struct mtk_m4u_data {
	struct device			*dev;
	struct dentry			*debug_root;
	struct mtk_iommu_cb		*m4u_cb;
	const struct mtk_m4u_plat_data	*plat_data;
};

struct iova_info {
	struct device *dev;
	dma_addr_t iova;
	size_t size;
	struct list_head list_node;
};

struct mtk_m4u_plat_data {
	const struct mtk_iommu_port	*port_list[TYPE_NUM];
	u32				port_nr[TYPE_NUM];
};

struct iova_buf_list {
	struct list_head head;
	struct mutex lock;
};

static const struct mtk_iommu_port iommu_port_mt6779[] = {
	/* larb0 -MMSYS-9 */
	MM_IOMMU_PORT_INIT("DISP_POSTMASK0", 0, 0, 0, 0),
	MM_IOMMU_PORT_INIT("DISP_OVL0_HDR", 0, 0, 0, 1),
	MM_IOMMU_PORT_INIT("DISP_OVL1_HDR", 0, 0, 0, 2),
	MM_IOMMU_PORT_INIT("DISP_OVL0", 0, 0, 0, 3),
	MM_IOMMU_PORT_INIT("DISP_OVL1", 0, 0, 0, 4),
	MM_IOMMU_PORT_INIT("DISP_PVRIC0", 0, 0, 0, 5),
	MM_IOMMU_PORT_INIT("DISP_RDMA0", 0, 0, 0, 6),
	MM_IOMMU_PORT_INIT("DISP_WDMA0", 0, 0, 0, 7),
	MM_IOMMU_PORT_INIT("DISP_FAKE0", 0, 0, 0, 8),
	/*larb1-MMSYS-14*/
	MM_IOMMU_PORT_INIT("DISP_OVL0_2L_HDR", 0, 1, 4, 0),
	MM_IOMMU_PORT_INIT("DISP_OVL1_2L_HDR", 0, 1, 4, 1),
	MM_IOMMU_PORT_INIT("DISP_OVL0_2L", 0, 1, 4, 2),
	MM_IOMMU_PORT_INIT("DISP_OVL1_2L", 0, 1, 4, 3),
	MM_IOMMU_PORT_INIT("DISP_RDMA1", 0, 1, 4, 4),
	MM_IOMMU_PORT_INIT("MDP_PVRIC0", 0, 1, 4, 5),
	MM_IOMMU_PORT_INIT("MDP_PVRIC1", 0, 1, 4, 6),
	MM_IOMMU_PORT_INIT("MDP_RDMA0", 0, 1, 4, 7),
	MM_IOMMU_PORT_INIT("MDP_RDMA1", 0, 1, 4, 8),
	MM_IOMMU_PORT_INIT("MDP_WROT0_R", 0, 1, 4, 9),
	MM_IOMMU_PORT_INIT("MDP_WROT0_W", 0, 1, 4, 10),
	MM_IOMMU_PORT_INIT("MDP_WROT1_R", 0, 1, 4, 11),
	MM_IOMMU_PORT_INIT("MDP_WROT1_W", 0, 1, 4, 12),
	MM_IOMMU_PORT_INIT("DISP_FAKE1", 0, 1, 4, 13),
	/*larb2-VDEC-12*/
	MM_IOMMU_PORT_INIT("VDEC_MC_EXT", 0, 2, 8, 0),
	MM_IOMMU_PORT_INIT("VDEC_UFO_EXT", 0, 2, 8, 1),
	MM_IOMMU_PORT_INIT("VDEC_PP_EXT", 0, 2, 8, 2),
	MM_IOMMU_PORT_INIT("VDEC_PRED_RD_EXT", 0, 2, 8, 3),
	MM_IOMMU_PORT_INIT("VDEC_PRED_WR_EXT", 0, 2, 8, 4),
	MM_IOMMU_PORT_INIT("VDEC_PPWRAP_EXT", 0, 2, 8, 5),
	MM_IOMMU_PORT_INIT("VDEC_TILE_EXT", 0, 2, 8, 6),
	MM_IOMMU_PORT_INIT("VDEC_VLD_EXT", 0, 2, 8, 7),
	MM_IOMMU_PORT_INIT("VDEC_VLD2_EXT", 0, 2, 8, 8),
	MM_IOMMU_PORT_INIT("VDEC_AVC_MV_EXT", 0, 2, 8, 9),
	MM_IOMMU_PORT_INIT("VDEC_UFO_ENC_EXT", 0, 2, 8, 10),
	MM_IOMMU_PORT_INIT("VDEC_RG_CTRL_DMA_EXT", 0, 2, 8, 11),
	/*larb3-VENC-19*/
	MM_IOMMU_PORT_INIT("VENC_RCPU", 0, 3, 12, 0),
	MM_IOMMU_PORT_INIT("VENC_REC", 0, 3, 12, 1),
	MM_IOMMU_PORT_INIT("VENC_BSDMA", 0, 3, 12, 2),
	MM_IOMMU_PORT_INIT("VENC_SV_COMV", 0, 3, 12, 3),
	MM_IOMMU_PORT_INIT("VENC_RD_COMV", 0, 3, 12, 4),
	MM_IOMMU_PORT_INIT("VENC_NBM_RDMA", 0, 3, 12, 5),
	MM_IOMMU_PORT_INIT("VENC_NBM_RDMA_LITE", 0, 3, 12, 6),
	MM_IOMMU_PORT_INIT("JPGENC_Y_RDMA", 0, 3, 12, 7),
	MM_IOMMU_PORT_INIT("JPGENC_C_RDMA", 0, 3, 12, 8),
	MM_IOMMU_PORT_INIT("JPGENC_Q_TABLE", 0, 3, 12, 9),
	MM_IOMMU_PORT_INIT("JPGENC_BSDMA", 0, 3, 12, 10),
	MM_IOMMU_PORT_INIT("JPGEDC_WDMA", 0, 3, 12, 11),
	MM_IOMMU_PORT_INIT("JPGEDC_BSDMA", 0, 3, 12, 12),
	MM_IOMMU_PORT_INIT("VENC_NBM_WDMA", 0, 3, 12, 13),
	MM_IOMMU_PORT_INIT("VENC_NBM_WDMA_LITE", 0, 3, 12, 14),
	MM_IOMMU_PORT_INIT("VENC_CUR_LUMA", 0, 3, 12, 15),
	MM_IOMMU_PORT_INIT("VENC_CUR_CHROMA", 0, 3, 12, 16),
	MM_IOMMU_PORT_INIT("VENC_REF_LUMA", 0, 3, 12, 17),
	MM_IOMMU_PORT_INIT("VENC_REF_CHROMA", 0, 3, 12, 18),
	/*larb4-dummy*/
	/*larb5-IMG-26*/
	MM_IOMMU_PORT_INIT("IMGI_D1", 0, 5, 16, 0),
	MM_IOMMU_PORT_INIT("IMGBI_D1", 0, 5, 16, 1),
	MM_IOMMU_PORT_INIT("DMGI_D1", 0, 5, 16, 2),
	MM_IOMMU_PORT_INIT("DEPI_D1", 0, 5, 16, 3),
	MM_IOMMU_PORT_INIT("LCEI_D1", 0, 5, 16, 4),
	MM_IOMMU_PORT_INIT("SMTI_D1", 0, 5, 16, 5),
	MM_IOMMU_PORT_INIT("SMTO_D2", 0, 5, 16, 6),
	MM_IOMMU_PORT_INIT("SMTO_D1", 0, 5, 16, 7),
	MM_IOMMU_PORT_INIT("CRZO_D1", 0, 5, 16, 8),
	MM_IOMMU_PORT_INIT("IMG3O_D1", 0, 5, 16, 9),
	MM_IOMMU_PORT_INIT("VIPI_D1", 0, 5, 16, 10),
	MM_IOMMU_PORT_INIT("WPE_A_RDMA1", 0, 5, 16, 11),
	MM_IOMMU_PORT_INIT("WPE_A_RDMA0", 0, 5, 16, 12),
	MM_IOMMU_PORT_INIT("WPE_A_WDMA", 0, 5, 16, 13),
	MM_IOMMU_PORT_INIT("TIMGO_D1", 0, 5, 16, 14),
	MM_IOMMU_PORT_INIT("MFB_RDMA0", 0, 5, 16, 15),
	MM_IOMMU_PORT_INIT("MFB_RDMA1", 0, 5, 16, 16),
	MM_IOMMU_PORT_INIT("MFB_RDMA2", 0, 5, 16, 17),
	MM_IOMMU_PORT_INIT("MFB_RDMA3", 0, 5, 16, 18),
	MM_IOMMU_PORT_INIT("MFB_WDMA", 0, 5, 16, 19),
	MM_IOMMU_PORT_INIT("RESERVED1", 0, 5, 16, 20),
	MM_IOMMU_PORT_INIT("RESERVED2", 0, 5, 16, 21),
	MM_IOMMU_PORT_INIT("RESERVED3", 0, 5, 16, 22),
	MM_IOMMU_PORT_INIT("RESERVED4", 0, 5, 16, 23),
	MM_IOMMU_PORT_INIT("RESERVED5", 0, 5, 16, 24),
	MM_IOMMU_PORT_INIT("RESERVED6", 0, 5, 16, 25),
	/*larb7-IPESYS-4*/
	MM_IOMMU_PORT_INIT("DVS_RDMA", 0, 7, 20, 0),
	MM_IOMMU_PORT_INIT("DVS_WDMA", 0, 7, 20, 1),
	MM_IOMMU_PORT_INIT("DVP_RDMA,", 0, 7, 20, 2),
	MM_IOMMU_PORT_INIT("DVP_WDMA,", 0, 7, 20, 3),
	/*larb8-IPESYS-10*/
	MM_IOMMU_PORT_INIT("FDVT_RDA", 0, 8, 21, 0),
	MM_IOMMU_PORT_INIT("FDVT_RDB", 0, 8, 21, 1),
	MM_IOMMU_PORT_INIT("FDVT_WRA", 0, 8, 21, 2),
	MM_IOMMU_PORT_INIT("FDVT_WRB", 0, 8, 21, 3),
	MM_IOMMU_PORT_INIT("FE_RD0", 0, 8, 21, 4),
	MM_IOMMU_PORT_INIT("FE_RD1", 0, 8, 21, 5),
	MM_IOMMU_PORT_INIT("FE_WR0", 0, 8, 21, 6),
	MM_IOMMU_PORT_INIT("FE_WR1", 0, 8, 21, 7),
	MM_IOMMU_PORT_INIT("RSC_RDMA0", 0, 8, 21, 8),
	MM_IOMMU_PORT_INIT("RSC_WDMA", 0, 8, 21, 9),
	/*larb9-CAM-24*/
	MM_IOMMU_PORT_INIT("CAM_IMGO_R1_C", 0, 9, 28, 0),
	MM_IOMMU_PORT_INIT("CAM_RRZO_R1_C", 0, 9, 28, 1),
	MM_IOMMU_PORT_INIT("CAM_LSCI_R1_C", 0, 9, 28, 2),
	MM_IOMMU_PORT_INIT("CAM_BPCI_R1_C", 0, 9, 28, 3),
	MM_IOMMU_PORT_INIT("CAM_YUVO_R1_C", 0, 9, 28, 4),
	MM_IOMMU_PORT_INIT("CAM_UFDI_R2_C", 0, 9, 28, 5),
	MM_IOMMU_PORT_INIT("CAM_RAWI_R2_C", 0, 9, 28, 6),
	MM_IOMMU_PORT_INIT("CAM_RAWI_R5_C", 0, 9, 28, 7),
	MM_IOMMU_PORT_INIT("CAM_CAMSV_1", 0, 9, 28, 8),
	MM_IOMMU_PORT_INIT("CAM_CAMSV_2", 0, 9, 28, 9),
	MM_IOMMU_PORT_INIT("CAM_CAMSV_3", 0, 9, 28, 10),
	MM_IOMMU_PORT_INIT("CAM_CAMSV_4", 0, 9, 28, 11),
	MM_IOMMU_PORT_INIT("CAM_CAMSV_5", 0, 9, 28, 12),
	MM_IOMMU_PORT_INIT("CAM_CAMSV_6", 0, 9, 28, 13),
	MM_IOMMU_PORT_INIT("CAM_AAO_R1_C", 0, 9, 28, 14),
	MM_IOMMU_PORT_INIT("CAM_AFO_R1_C", 0, 9, 28, 15),
	MM_IOMMU_PORT_INIT("CAM_FLKO_R1_C", 0, 9, 28, 16),
	MM_IOMMU_PORT_INIT("CAM_LCESO_R1_C", 0, 9, 28, 17),
	MM_IOMMU_PORT_INIT("CAM_CRZO_R1_C", 0, 9, 28, 18),
	MM_IOMMU_PORT_INIT("CAM_LTMSO_R1_C", 0, 9, 28, 19),
	MM_IOMMU_PORT_INIT("CAM_RSSO_R1_C", 0, 9, 28, 20),
	MM_IOMMU_PORT_INIT("CAM_CCUI", 0, 9, 28, 21),
	MM_IOMMU_PORT_INIT("CAM_CCUO", 0, 9, 28, 22),
	MM_IOMMU_PORT_INIT("CAM_FAKE", 0, 9, 28, 23),
	/*larb10-CAM-31*/
	MM_IOMMU_PORT_INIT("CAM_IMGO_R1_A", 0, 10, 25, 0),
	MM_IOMMU_PORT_INIT("CAM_RRZO_R1_A", 0, 10, 25, 1),
	MM_IOMMU_PORT_INIT("CAM_LSCI_R1_A", 0, 10, 25, 2),
	MM_IOMMU_PORT_INIT("CAM_BPCI_R1_A", 0, 10, 25, 3),
	MM_IOMMU_PORT_INIT("CAM_YUVO_R1_A", 0, 10, 25, 4),
	MM_IOMMU_PORT_INIT("CAM_UFDI_R2_A", 0, 10, 25, 5),
	MM_IOMMU_PORT_INIT("CAM_RAWI_R5_A", 0, 10, 25, 7),
	MM_IOMMU_PORT_INIT("CAM_IMGO_R1_B", 0, 10, 25, 8),
	MM_IOMMU_PORT_INIT("CAM_RRZO_R1_B", 0, 10, 25, 9),
	MM_IOMMU_PORT_INIT("CAM_LSCI_R1_B", 0, 10, 25, 10),
	MM_IOMMU_PORT_INIT("CAM_BPCI_R1_B", 0, 10, 25, 11),
	MM_IOMMU_PORT_INIT("CAM_YUVO_R1_B,", 0, 10, 25, 12),
	MM_IOMMU_PORT_INIT("CAM_UFDI_R2_B", 0, 10, 25, 13),
	MM_IOMMU_PORT_INIT("CAM_RAWI_R2_B", 0, 10, 25, 14),
	MM_IOMMU_PORT_INIT("CAM_RAWI_R5_B", 0, 10, 25, 15),
	MM_IOMMU_PORT_INIT("CAM_CAMSV_0", 0, 10, 25, 16),
	MM_IOMMU_PORT_INIT("CAM_AAO_R1_A", 0, 10, 25, 17),
	MM_IOMMU_PORT_INIT("CAM_AFO_R1_A", 0, 10, 25, 18),
	MM_IOMMU_PORT_INIT("CAM_FLKO_R1_A", 0, 10, 25, 19),
	MM_IOMMU_PORT_INIT("CAM_LCESO_R1_A", 0, 10, 25, 20),
	MM_IOMMU_PORT_INIT("CAM_CRZO_R1_A", 0, 10, 25, 21),
	MM_IOMMU_PORT_INIT("CAM_AAO_R1_B", 0, 10, 25, 22),
	MM_IOMMU_PORT_INIT("CAM_AFO_R1_B", 0, 10, 25, 23),
	MM_IOMMU_PORT_INIT("CAM_FLKO_R1_B", 0, 10, 25, 24),
	MM_IOMMU_PORT_INIT("CAM_LCESO_R1_B", 0, 10, 25, 25),
	MM_IOMMU_PORT_INIT("CAM_CRZO_R1_B", 0, 10, 25, 26),
	MM_IOMMU_PORT_INIT("CAM_LTMSO_R1_A", 0, 10, 25, 27),
	MM_IOMMU_PORT_INIT("CAM_RSSO_R1_A", 0, 10, 25, 28),
	MM_IOMMU_PORT_INIT("CAM_LTMSO_R1_B", 0, 10, 25, 29),
	MM_IOMMU_PORT_INIT("CAM_RSSO_R1_B", 0, 10, 25, 30),
	/* CCU */
	MM_IOMMU_PORT_INIT("CCU0", 0, 9, 24, 0),
	MM_IOMMU_PORT_INIT("CCU1", 0, 9, 24, 1),

	MM_IOMMU_PORT_INIT("UNKNOWN", 0, 0, 0, 0)
};

static const struct mtk_iommu_port mm_port_mt6873[] = {
	/* Larb0 -- 6 */
	MM_IOMMU_PORT_INIT("L0_DISP_POSTMASK0", 0, 0, 0x0, 0),
	MM_IOMMU_PORT_INIT("L0_OVL_RDMA0_HDR", 0, 0, 0x0, 1),
	MM_IOMMU_PORT_INIT("L0_OVL_RDMA0", 0, 0, 0x0, 2),
	MM_IOMMU_PORT_INIT("L0_DISP_RDMA0", 0, 0, 0x0, 3),
	MM_IOMMU_PORT_INIT("L0_DISP_WDMA0", 0, 0, 0x0, 4),
	MM_IOMMU_PORT_INIT("L0_DISP_FAKE0", 0, 0, 0x0, 5),
	/* Larb1 -- 8(14) */
	MM_IOMMU_PORT_INIT("L1_OVL_2L_RDMA0_HDR", 0, 1, 0x4, 0),
	MM_IOMMU_PORT_INIT("L1_OVL_2L_RDMA2_HDR", 0, 1, 0x4, 1),
	MM_IOMMU_PORT_INIT("L1_OVL_2L_RDMA0", 0, 1, 0x4, 2),
	MM_IOMMU_PORT_INIT("L1_OVL_2L_RDMA2", 0, 1, 0x4, 3),
	MM_IOMMU_PORT_INIT("L1_DISP_MDP_RDMA4", 0, 1, 0x4, 4),
	MM_IOMMU_PORT_INIT("L1_DISP_RDMA4", 0, 1, 0x4, 5),
	MM_IOMMU_PORT_INIT("L1_DISP_UFBC_WDMA0", 0, 1, 0x4, 6),
	MM_IOMMU_PORT_INIT("L1_DISP_FAKE1", 0, 1, 0x4, 7),
	/* Larb2 --5(19) */
	MM_IOMMU_PORT_INIT("L2_MDP_RDMA0", 0, 2, 0x10, 0),
	MM_IOMMU_PORT_INIT("L2_MDP_RDMA1", 0, 2, 0x10, 1),
	MM_IOMMU_PORT_INIT("L2_MDP_WROT0", 0, 2, 0x10, 2),
	MM_IOMMU_PORT_INIT("L2_MDP_WROT1", 0, 2, 0x10, 3),
	MM_IOMMU_PORT_INIT("L2_MDP_FAKE0", 0, 2, 0x10, 4),
	/* Larb4 -- 11(30) */
	MM_IOMMU_PORT_INIT("L4_VDEC_MC_EXT", 0, 4, 0x8, 0),
	MM_IOMMU_PORT_INIT("L4_VDEC_UFO_EXT", 0, 4, 0x8, 1),
	MM_IOMMU_PORT_INIT("L4_VDEC_PP_EXT", 0, 4, 0x8, 2),
	MM_IOMMU_PORT_INIT("L4_VDEC_PRED_RD_EXT", 0, 4, 0x8, 3),
	MM_IOMMU_PORT_INIT("L4_VDEC_PRED_WR_EXT", 0, 4, 0x8, 4),
	MM_IOMMU_PORT_INIT("L4_VDEC_PPWRAP_EXT", 0, 4, 0x8, 5),
	MM_IOMMU_PORT_INIT("L4_VDEC_TILE_EXT", 0, 4, 0x8, 6),
	MM_IOMMU_PORT_INIT("L4_VDEC_VLD_EXT", 0, 4, 0x8, 7),
	MM_IOMMU_PORT_INIT("L4_VDEC_VLD2_EXT", 0, 4, 0x8, 8),
	MM_IOMMU_PORT_INIT("L4_VDEC_AVC_MV_EXT", 0, 4, 0x8, 9),
	MM_IOMMU_PORT_INIT("L4_VDEC_RG_CTRL_DMA_EXT", 0, 4, 0x8, 10),
	/* Larb5 -- 8(38) */
	MM_IOMMU_PORT_INIT("L5_VDEC_LAT0_VLD_EXT", 0, 5, 0x9, 0),
	MM_IOMMU_PORT_INIT("L5_VDEC_LAT0_VLD2_EXT", 0, 5, 0x9, 1),
	MM_IOMMU_PORT_INIT("L5_VDEC_LAT0_AVC_MV_EXT", 0, 5, 0x9, 2),
	MM_IOMMU_PORT_INIT("L5_VDEC_LAT0_PRED_RD_EXT", 0, 5, 0x9, 3),
	MM_IOMMU_PORT_INIT("L5_VDEC_LAT0_TILE_EXT", 0, 5, 0x9, 4),
	MM_IOMMU_PORT_INIT("L5_VDEC_LAT0_WDMA_EXT", 0, 5, 0x9, 5),
	MM_IOMMU_PORT_INIT("L5_VDEC_LAT0_RG_CTRL_DMA_EXT", 0, 5, 0x9, 6),
	MM_IOMMU_PORT_INIT("L5_VDEC_UFO_ENC_EXT", 0, 5, 0x9, 7),
	/* Larb6 --dummy */
	/* Larb7 --15(53) */
	MM_IOMMU_PORT_INIT("L7_VENC_RCPU", 0, 7, 0xc, 0),
	MM_IOMMU_PORT_INIT("L7_VENC_REC", 0, 7, 0xc, 1),
	MM_IOMMU_PORT_INIT("L7_VENC_BSDMA", 0, 7, 0xc, 2),
	MM_IOMMU_PORT_INIT("L7_VENC_SV_COMV", 0, 7, 0xc, 3),
	MM_IOMMU_PORT_INIT("L7_VENC_RD_COMV", 0, 7, 0xc, 4),
	MM_IOMMU_PORT_INIT("L7_VENC_CUR_LUMA", 0, 7, 0xc, 5),
	MM_IOMMU_PORT_INIT("L7_VENC_CUR_CHROMA", 0, 7, 0xc, 6),
	MM_IOMMU_PORT_INIT("L7_VENC_REF_LUMA", 0, 7, 0xc, 7),
	MM_IOMMU_PORT_INIT("L7_VENC_REF_CHROMA", 0, 7, 0xc, 8),
	MM_IOMMU_PORT_INIT("L7_JPGENC_Y_RDMA", 0, 7, 0xc, 9),
	MM_IOMMU_PORT_INIT("L7_JPGENC_Q_RDMA", 0, 7, 0xc, 10),
	MM_IOMMU_PORT_INIT("L7_JPGENC_C_TABLE", 0, 7, 0xc, 11),
	MM_IOMMU_PORT_INIT("L7_JPGENC_BSDMA", 0, 7, 0xc, 12),
	MM_IOMMU_PORT_INIT("L7_VENC_SUB_R_LUMA", 0, 7, 0xc, 13),
	MM_IOMMU_PORT_INIT("L7_VENC_SUB_W_LUMA", 0, 7, 0xc, 14),
	/*Larb9 -- 29(82) */
	MM_IOMMU_PORT_INIT("L9_IMG_IMGI_D1", 0, 9, 0x14, 0),
	MM_IOMMU_PORT_INIT("L9_IMG_IMGBI_D1", 0, 9, 0x14, 1),
	MM_IOMMU_PORT_INIT("L9_IMG_DMGI_D1", 0, 9, 0x14, 2),
	MM_IOMMU_PORT_INIT("L9_IMG_DEPI_D1", 0, 9, 0x14, 3),
	MM_IOMMU_PORT_INIT("L9_IMG_ICE_D1", 0, 9, 0x14, 4),
	MM_IOMMU_PORT_INIT("L9_IMG_SMTI_D1", 0, 9, 0x14, 5),
	MM_IOMMU_PORT_INIT("L9_IMG_SMTO_D2", 0, 9, 0x14, 6),
	MM_IOMMU_PORT_INIT("L9_IMG_SMTO_D1", 0, 9, 0x14, 7),
	MM_IOMMU_PORT_INIT("L9_IMG_CRZO_D1", 0, 9, 0x14, 8),
	MM_IOMMU_PORT_INIT("L9_IMG_IMG3O_D1", 0, 9, 0x14, 9),
	MM_IOMMU_PORT_INIT("L9_IMG_VIPI_D1", 0, 9, 0x14, 10),
	MM_IOMMU_PORT_INIT("L9_IMG_SMTI_D5", 0, 9, 0x14, 11),
	MM_IOMMU_PORT_INIT("L9_IMG_TIMGO_D1", 0, 9, 0x14, 12),
	MM_IOMMU_PORT_INIT("L9_IMG_UFBC_W0", 0, 9, 0x14, 13),
	MM_IOMMU_PORT_INIT("L9_IMG_UFBC_R0", 0, 9, 0x14, 14),
	MM_IOMMU_PORT_INIT("L9_IMG_WPE_RDMA1", 0, 9, 0x14, 15),
	MM_IOMMU_PORT_INIT("L9_IMG_WPE_RDMA0", 0, 9, 0x14, 16),
	MM_IOMMU_PORT_INIT("L9_IMG_WPE_WDMA", 0, 9, 0x14, 17),
	MM_IOMMU_PORT_INIT("L9_IMG_MFB_RDMA0", 0, 9, 0x14, 18),
	MM_IOMMU_PORT_INIT("L9_IMG_MFB_RDMA1", 0, 9, 0x14, 19),
	MM_IOMMU_PORT_INIT("L9_IMG_MFB_RDMA2", 0, 9, 0x14, 20),
	MM_IOMMU_PORT_INIT("L9_IMG_MFB_RDMA3", 0, 9, 0x14, 21),
	MM_IOMMU_PORT_INIT("L9_IMG_MFB_RDMA4", 0, 9, 0x14, 22),
	MM_IOMMU_PORT_INIT("L9_IMG_MFB_RDMA5", 0, 9, 0x14, 23),
	MM_IOMMU_PORT_INIT("L9_IMG_MFB_WDMA0", 0, 9, 0x14, 24),
	MM_IOMMU_PORT_INIT("L9_IMG_MFB_WDMA1", 0, 9, 0x14, 25),
	MM_IOMMU_PORT_INIT("L9_IMG_RESERVE6", 0, 9, 0x14, 26),
	MM_IOMMU_PORT_INIT("L9_IMG_RESERVE7", 0, 9, 0x14, 27),
	MM_IOMMU_PORT_INIT("L9_IMG_RESERVE8", 0, 9, 0x14, 28),
	/*Larb11 -- 29(111) */
	MM_IOMMU_PORT_INIT("L11_IMG_IMGI_D1", 0, 11, 0x15, 0),
	MM_IOMMU_PORT_INIT("L11_IMG_IMGBI_D1", 0, 11, 0x15, 1),
	MM_IOMMU_PORT_INIT("L11_IMG_DMGI_D1", 0, 11, 0x15, 2),
	MM_IOMMU_PORT_INIT("L11_IMG_DEPI_D1", 0, 11, 0x15, 3),
	MM_IOMMU_PORT_INIT("L11_IMG_ICE_D1", 0, 11, 0x15, 4),
	MM_IOMMU_PORT_INIT("L11_IMG_SMTI_D1", 0, 11, 0x15, 5),
	MM_IOMMU_PORT_INIT("L11_IMG_SMTO_D2", 0, 11, 0x15, 6),
	MM_IOMMU_PORT_INIT("L11_IMG_SMTO_D1", 0, 11, 0x15, 7),
	MM_IOMMU_PORT_INIT("L11_IMG_CRZO_D1", 0, 11, 0x15, 8),
	MM_IOMMU_PORT_INIT("L11_IMG_IMG3O_D1", 0, 11, 0x15, 9),
	MM_IOMMU_PORT_INIT("L11_IMG_VIPI_D1", 0, 11, 0x15, 10),
	MM_IOMMU_PORT_INIT("L11_IMG_SMTI_D5", 0, 11, 0x15, 11),
	MM_IOMMU_PORT_INIT("L11_IMG_TIMGO_D1", 0, 11, 0x15, 12),
	MM_IOMMU_PORT_INIT("L11_IMG_UFBC_W0", 0, 11, 0x15, 13),
	MM_IOMMU_PORT_INIT("L11_IMG_UFBC_R0", 0, 11, 0x15, 14),
	MM_IOMMU_PORT_INIT("L11_IMG_WPE_RDMA1", 0, 11, 0x15, 15),
	MM_IOMMU_PORT_INIT("L11_IMG_WPE_RDMA0", 0, 11, 0x15, 16),
	MM_IOMMU_PORT_INIT("L11_IMG_WPE_WDMA", 0, 11, 0x15, 17),
	MM_IOMMU_PORT_INIT("L11_IMG_MFB_RDMA0", 0, 11, 0x15, 18),
	MM_IOMMU_PORT_INIT("L11_IMG_MFB_RDMA1", 0, 11, 0x15, 19),
	MM_IOMMU_PORT_INIT("L11_IMG_MFB_RDMA2", 0, 11, 0x15, 20),
	MM_IOMMU_PORT_INIT("L11_IMG_MFB_RDMA3", 0, 11, 0x15, 21),
	MM_IOMMU_PORT_INIT("L11_IMG_MFB_RDMA4", 0, 11, 0x15, 22),
	MM_IOMMU_PORT_INIT("L11_IMG_MFB_RDMA5", 0, 11, 0x15, 23),
	MM_IOMMU_PORT_INIT("L11_IMG_MFB_WDMA0", 0, 11, 0x15, 24),
	MM_IOMMU_PORT_INIT("L11_IMG_MFB_WDMA1", 0, 11, 0x15, 25),
	MM_IOMMU_PORT_INIT("L11_IMG_RESERVE6", 0, 11, 0x15, 26),
	MM_IOMMU_PORT_INIT("L11_IMG_RESERVE7", 0, 11, 0x15, 27),
	MM_IOMMU_PORT_INIT("L11_IMG_RESERVE8", 0, 11, 0x15, 28),
	/*Larb13 -- 12(123) */
	MM_IOMMU_PORT_INIT("L13_CAM_MRAWI", 0, 13, 0x1d, 0),
	MM_IOMMU_PORT_INIT("L13_CAM_MRAWO0", 0, 13, 0x1d, 1),
	MM_IOMMU_PORT_INIT("L13_CAM_MRAWO1", 0, 13, 0x1d, 2),
	MM_IOMMU_PORT_INIT("L13_CAM_CAMSV1", 0, 13, 0x1d, 3),
	MM_IOMMU_PORT_INIT("L13_CAM_CAMSV2", 0, 13, 0x1d, 4),
	MM_IOMMU_PORT_INIT("L13_CAM_CAMSV3", 0, 13, 0x1d, 5),
	MM_IOMMU_PORT_INIT("L13_CAM_CAMSV4", 0, 13, 0x1d, 6),
	MM_IOMMU_PORT_INIT("L13_CAM_CAMSV5", 0, 13, 0x1d, 7),
	MM_IOMMU_PORT_INIT("L13_CAM_CAMSV6", 0, 13, 0x1d, 8),
	MM_IOMMU_PORT_INIT("L13_CAM_CCUI", 0, 13, 0x1d, 9),
	MM_IOMMU_PORT_INIT("L13_CAM_CCUO", 0, 13, 0x1d, 10),
	MM_IOMMU_PORT_INIT("L13_CAM_FAKE", 0, 13, 0x1d, 11),
	/*Larb14 -- 6(129) */
	MM_IOMMU_PORT_INIT("L14_CAM_RESERVE1", 0, 14, 0x19, 0),
	MM_IOMMU_PORT_INIT("L14_CAM_RESERVE2", 0, 14, 0x19, 1),
	MM_IOMMU_PORT_INIT("L14_CAM_RESERVE3", 0, 14, 0x19, 2),
	MM_IOMMU_PORT_INIT("L14_CAM_CAMSV0", 0, 14, 0x19, 3),
	MM_IOMMU_PORT_INIT("L14_CAM_CCUI", 0, 14, 0x19, 4),
	MM_IOMMU_PORT_INIT("L14_CAM_CCUO", 0, 14, 0x19, 5),
	/*Larb16 -- 17(146) */
	MM_IOMMU_PORT_INIT("L16_CAM_IMGO_R1_A", 0, 16, 0x1a, 0),
	MM_IOMMU_PORT_INIT("L16_CAM_RRZO_R1_A", 0, 16, 0x1a, 1),
	MM_IOMMU_PORT_INIT("L16_CAM_CQI_R1_A", 0, 16, 0x1a, 2),
	MM_IOMMU_PORT_INIT("L16_CAM_BPCI_R1_A", 0, 16, 0x1a, 3),
	MM_IOMMU_PORT_INIT("L16_CAM_YUVO_R1_A", 0, 16, 0x1a, 4),
	MM_IOMMU_PORT_INIT("L16_CAM_UFDI_R2_A", 0, 16, 0x1a, 5),
	MM_IOMMU_PORT_INIT("L16_CAM_RAWI_R2_A", 0, 16, 0x1a, 6),
	MM_IOMMU_PORT_INIT("L16_CAM_RAWI_R3_A", 0, 16, 0x1a, 7),
	MM_IOMMU_PORT_INIT("L16_CAM_AAO_R1_A", 0, 16, 0x1a, 8),
	MM_IOMMU_PORT_INIT("L16_CAM_AFO_R1_A", 0, 16, 0x1a, 9),
	MM_IOMMU_PORT_INIT("L16_CAM_FLKO_R1_A", 0, 16, 0x1a, 10),
	MM_IOMMU_PORT_INIT("L16_CAM_LCESO_R1_A", 0, 16, 0x1a, 11),
	MM_IOMMU_PORT_INIT("L16_CAM_CRZO_R1_A", 0, 16, 0x1a, 12),
	MM_IOMMU_PORT_INIT("L16_CAM_LTMSO_R1_A", 0, 16, 0x1a, 13),
	MM_IOMMU_PORT_INIT("L16_CAM_RSSO_R1_A", 0, 16, 0x1a, 14),
	MM_IOMMU_PORT_INIT("L16_CAM_AAHO_R1_A_", 0, 16, 0x1a, 15),
	MM_IOMMU_PORT_INIT("L16_CAM_LSCI_R1_A", 0, 16, 0x1a, 16),
	/*Larb17 -- 17(163) */
	MM_IOMMU_PORT_INIT("L17_CAM_IMGO_R1_B", 0, 17, 0x1f, 0),
	MM_IOMMU_PORT_INIT("L17_CAM_RRZO_R1_B", 0, 17, 0x1f, 1),
	MM_IOMMU_PORT_INIT("L17_CAM_CQI_R1_B", 0, 17, 0x1f, 2),
	MM_IOMMU_PORT_INIT("L17_CAM_BPCI_R1_B", 0, 17, 0x1f, 3),
	MM_IOMMU_PORT_INIT("L17_CAM_YUVO_R1_B", 0, 17, 0x1f, 4),
	MM_IOMMU_PORT_INIT("L17_CAM_UFDI_R2_B", 0, 17, 0x1f, 5),
	MM_IOMMU_PORT_INIT("L17_CAM_RAWI_R2_B", 0, 17, 0x1f, 6),
	MM_IOMMU_PORT_INIT("L17_CAM_RAWI_R3_B", 0, 17, 0x1f, 7),
	MM_IOMMU_PORT_INIT("L17_CAM_AAO_R1_B", 0, 17, 0x1f, 8),
	MM_IOMMU_PORT_INIT("L17_CAM_AFO_R1_B", 0, 17, 0x1f, 9),
	MM_IOMMU_PORT_INIT("L17_CAM_FLKO_R1_B", 0, 17, 0x1f, 10),
	MM_IOMMU_PORT_INIT("L17_CAM_LCESO_R1_B", 0, 17, 0x1f, 11),
	MM_IOMMU_PORT_INIT("L17_CAM_CRZO_R1_B", 0, 17, 0x1f, 12),
	MM_IOMMU_PORT_INIT("L17_CAM_LTMSO_R1_B", 0, 17, 0x1f, 13),
	MM_IOMMU_PORT_INIT("L17_CAM_RSSO_R1_B", 0, 17, 0x1f, 14),
	MM_IOMMU_PORT_INIT("L17_CAM_AAHO_R1_B", 0, 17, 0x1f, 15),
	MM_IOMMU_PORT_INIT("L17_CAM_LSCI_R1_B", 0, 17, 0x1f, 16),
	/*Larb18 -- 17(180) */
	MM_IOMMU_PORT_INIT("L18_CAM_IMGO_R1_C", 0, 18, 0x1e, 0),
	MM_IOMMU_PORT_INIT("L18_CAM_RRZO_R1_C", 0, 18, 0x1e, 1),
	MM_IOMMU_PORT_INIT("L18_CAM_CQI_R1_C", 0, 18, 0x1e, 2),
	MM_IOMMU_PORT_INIT("L18_CAM_BPCI_R1_C", 0, 18, 0x1e, 3),
	MM_IOMMU_PORT_INIT("L18_CAM_YUVO_R1_C", 0, 18, 0x1e, 4),
	MM_IOMMU_PORT_INIT("L18_CAM_UFDI_R2_C", 0, 18, 0x1e, 5),
	MM_IOMMU_PORT_INIT("L18_CAM_RAWI_R2_C", 0, 18, 0x1e, 6),
	MM_IOMMU_PORT_INIT("L18_CAM_RAWI_R3_C", 0, 18, 0x1e, 7),
	MM_IOMMU_PORT_INIT("L18_CAM_AAO_R1_C", 0, 18, 0x1e, 8),
	MM_IOMMU_PORT_INIT("L18_CAM_AFO_R1_C", 0, 18, 0x1e, 9),
	MM_IOMMU_PORT_INIT("L18_CAM_FLKO_R1_C", 0, 18, 0x1e, 10),
	MM_IOMMU_PORT_INIT("L18_CAM_LCESO_R1_C", 0, 18, 0x1e, 11),
	MM_IOMMU_PORT_INIT("L18_CAM_CRZO_R1_C", 0, 18, 0x1e, 12),
	MM_IOMMU_PORT_INIT("L18_CAM_LTMSO_R1_C", 0, 18, 0x1e, 13),
	MM_IOMMU_PORT_INIT("L18_CAM_RSSO_R1_C", 0, 18, 0x1e, 14),
	MM_IOMMU_PORT_INIT("L18_CAM_AAHO_R1_C", 0, 18, 0x1e, 15),
	MM_IOMMU_PORT_INIT("L18_CAM_LSCI_R1_C", 0, 18, 0x1e, 16),
	/*Larb19 -- 4(184) */
	MM_IOMMU_PORT_INIT("L19_IPE_DVS_RDMA", 0, 19, 0x16, 0),
	MM_IOMMU_PORT_INIT("L19_IPE_DVS_WDMA", 0, 19, 0x16, 1),
	MM_IOMMU_PORT_INIT("L19_IPE_DVP_RDMA", 0, 19, 0x16, 2),
	MM_IOMMU_PORT_INIT("L19_IPE_DVP_WDMA", 0, 19, 0x16, 3),
	/*Larb20 -- 6(190) */
	MM_IOMMU_PORT_INIT("L20_IPE_FDVT_RDA", 0, 20, 0x17, 0),
	MM_IOMMU_PORT_INIT("L20_IPE_FDVT_RDB", 0, 20, 0x17, 1),
	MM_IOMMU_PORT_INIT("L20_IPE_FDVT_WRA", 0, 20, 0x17, 2),
	MM_IOMMU_PORT_INIT("L20_IPE_FDVT_WRB", 0, 20, 0x17, 3),
	MM_IOMMU_PORT_INIT("L20_IPE_RSC_RDMA0", 0, 20, 0x17, 4),
	MM_IOMMU_PORT_INIT("L20_IPE_RSC_WDMA", 0, 20, 0x17, 5),
	/*Larb22 -- 1(191) */
	MM_IOMMU_PORT_INIT("L22_CCU0", 0, 22, 0x1c, 0),
	/*Larb23  -- 1(192) */
	MM_IOMMU_PORT_INIT("L23_CCU1", 0, 23, 0x18, 0),

	MM_IOMMU_PORT_INIT("MM_UNKNOWN", 0, 0, 0, 0)
};

static const struct mtk_iommu_port apu_port_mt6873[] = {
	/* 8 */
	APU_IOMMU_PORT_INIT("APU_VP6_0", 0, 0, 0, 0x0),
	APU_IOMMU_PORT_INIT("APU_VP6_1", 0, 0, 0, 0x1),
	APU_IOMMU_PORT_INIT("APU_UP", 0, 0, 0, 0x2),
	APU_IOMMU_PORT_INIT("APU_RESERVED", 0, 0, 0, 0x3),
	APU_IOMMU_PORT_INIT("APU_XPU", 0, 0, 0, 0x4),
	APU_IOMMU_PORT_INIT("APU_EDMA", 0, 0, 0, 0x5),
	APU_IOMMU_PORT_INIT("APU_MDLA0", 0, 0, 0, 0x6),
	APU_IOMMU_PORT_INIT("APU_MDLA1", 0, 0, 0, 0x7),

	APU_IOMMU_PORT_INIT("APU_UNKNOWN", 0, 0, 0, 0xf)
};

static struct mtk_m4u_data *m4u_data;
static struct iova_buf_list iova_list;

void mtk_iova_dbg_alloc(struct device *dev, dma_addr_t iova, size_t size)
{
	struct iova_info *iova_buf;

	iova_buf = kzalloc(sizeof(*iova_buf), GFP_KERNEL);
	if (!iova_buf)
		return;

	iova_buf->dev = dev;
	iova_buf->iova = iova;
	iova_buf->size = size;
	mutex_lock(&iova_list.lock);
	list_add(&iova_buf->list_node, &iova_list.head);
	mutex_unlock(&iova_list.lock);
}
EXPORT_SYMBOL_GPL(mtk_iova_dbg_alloc);

void mtk_iova_dbg_free(dma_addr_t iova, size_t size)
{
	struct iova_info *plist;
	struct iova_info *tmp_plist;

	mutex_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, tmp_plist,
				 &iova_list.head, list_node) {
		if (plist->iova == iova && plist->size == size) {
			list_del(&plist->list_node);
			kfree(plist);
			break;
		}
	}
	mutex_unlock(&iova_list.lock);
}
EXPORT_SYMBOL_GPL(mtk_iova_dbg_free);

void mtk_iova_dbg_dump(struct seq_file *s)
{
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;

	mutex_lock(&iova_list.lock);

	if (s)
		seq_printf(s, "%18s %8s %18s\n", "iova", "size", "dev");
	else
		pr_info("%18s %8s %18s\n", "iova", "size", "dev");
	list_for_each_entry_safe(plist, n, &iova_list.head,
				 list_node) {
		if (s)
			seq_printf(s, "%pa %8zu %18s\n",
				   &plist->iova,
				   plist->size,
				   dev_name(plist->dev));
		else
			pr_info("%pa %8zu %18s\n",
				   &plist->iova,
				   plist->size,
				   dev_name(plist->dev));
	}
	mutex_unlock(&iova_list.lock);
}

static int mtk_iommu_get_tf_port_idx(int tf_id, bool is_vpu)
{
	int i;
	u32 vld_id, port_nr;
	const struct mtk_iommu_port *port_list;
	enum mtk_iommu_type type = is_vpu ? APU_IOMMU : MM_IOMMU;

	if (is_vpu)
		vld_id = F_APU_MMU_INT_TF_MSK(tf_id);
	else
		vld_id = tf_id & F_MMU_INT_TF_MSK;

	pr_info("get vld tf_id:0x%x\n", vld_id);
	port_nr =  m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];
	for (i = 0; i < port_nr; i++) {
		if (port_list[i].tf_id == vld_id)
			return i;
	}

	return port_nr;
}

static int mtk_iommu_port_idx(int id, enum mtk_iommu_type type)
{
	int i;
	u32 port_nr = m4u_data->plat_data->port_nr[type];
	const struct mtk_iommu_port *port_list;

	port_list = m4u_data->plat_data->port_list[type];
	for (i = 0; i < port_nr; i++) {
		if ((port_list[i].larb_id == MTK_M4U_TO_LARB(id)) &&
		     (port_list[i].port_id == MTK_M4U_TO_PORT(id)))
			return i;
	}
	return port_nr;
}

void report_custom_iommu_fault(
	u32 fault_iova,
	u32 fault_pa,
	u32 fault_id, bool is_vpu)
{
	int idx;
	int port;
	enum mtk_iommu_type type = is_vpu ? APU_IOMMU : MM_IOMMU;
	u32 port_nr = m4u_data->plat_data->port_nr[type];
	const struct mtk_iommu_port *port_list;

	pr_info("tf report start fault_id:0x%x\n", fault_id);
	port_list = m4u_data->plat_data->port_list[type];
	if (is_vpu) {
		idx = mtk_iommu_get_tf_port_idx(fault_id, true);
		m4u_aee_print(mmu_translation_log_format,
			      port_list[idx].name,
			      port_list[idx].name,
			      fault_iova, fault_pa);
		goto dump_pg;
	}

	idx = mtk_iommu_get_tf_port_idx(fault_id, false);
	if (idx >= port_nr) {
		pr_warn("fail,iova:0x%x, port:0x%x\n",
			fault_iova, fault_id);
		return;
	}
	port = MTK_M4U_ID(port_list[idx].larb_id,
			  port_list[idx].port_id);
	pr_info("tf report port:0x%x\n", port);
	if (port_list[idx].enable_tf &&
		m4u_data->m4u_cb[idx].fault_fn)
		m4u_data->m4u_cb[idx].fault_fn(port,
		fault_iova, m4u_data->m4u_cb[idx].fault_data);

	m4u_aee_print(mmu_translation_log_format,
		port_list[idx].name,
		port_list[idx].name, fault_iova,
		fault_pa);

dump_pg:
	mtk_iova_dbg_dump(NULL);
}
EXPORT_SYMBOL_GPL(report_custom_iommu_fault);

int mtk_iommu_register_fault_callback(int port,
			       mtk_iommu_fault_callback_t fn,
			       void *cb_data, bool is_vpu)
{
	enum mtk_iommu_type type = is_vpu ? APU_IOMMU : MM_IOMMU;
	int idx = mtk_iommu_port_idx(port, type);

	if (idx >= m4u_data->plat_data->port_nr[type]) {
		pr_info("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	if (is_vpu)
		idx += m4u_data->plat_data->port_nr[type];
	m4u_data->m4u_cb[idx].fault_fn = fn;
	m4u_data->m4u_cb[idx].fault_data = cb_data;
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_register_fault_callback);

int mtk_iommu_unregister_fault_callback(int port, bool is_vpu)
{
	enum mtk_iommu_type type = is_vpu ? APU_IOMMU : MM_IOMMU;
	int idx = mtk_iommu_port_idx(port, type);

	if (idx >= m4u_data->plat_data->port_nr[type]) {
		pr_info("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	if (is_vpu)
		idx += m4u_data->plat_data->port_nr[type];
	m4u_data->m4u_cb[idx].fault_fn = NULL;
	m4u_data->m4u_cb[idx].fault_data = NULL;
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_unregister_fault_callback);

static int m4u_debug_set(void *data, u64 val)
{
	pr_info("%s:val=%llu\n", __func__, val);

	switch (val) {
	case 1: /* dump iova info */
	{
		mtk_iova_dbg_dump(NULL);
		break;
	}
	case 2: /* mm translation fault test */
	{
		report_custom_iommu_fault(0, 0, 0x500000f, false);
		break;
	}
	case 3: /* mm translation fault test */
	{
		report_custom_iommu_fault(0, 0, 0x102, true);
		break;
	}
	default:
		pr_err("%s error,val=%llu\n", __func__, val);
	}

	return 0;
}

static int m4u_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_fops, m4u_debug_get, m4u_debug_set, "%llu\n");

static int m4u_debug_init(struct mtk_m4u_data *data)
{
	struct dentry *debug_file;

	data->debug_root = debugfs_create_dir("m4u", NULL);

	if (IS_ERR_OR_NULL(data->debug_root))
		pr_err("failed to create debug dir.\n");

	debug_file = debugfs_create_file("debug",
		0644, data->debug_root, NULL, &m4u_debug_fops);

	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to create debug files 2.\n");

	return 0;
}

static int mtk_m4u_dbg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	u32 total_port;

	pr_info("%s start\n", __func__);
	m4u_data = devm_kzalloc(dev, sizeof(struct mtk_m4u_data), GFP_KERNEL);
	if (!m4u_data)
		return -ENOMEM;

	m4u_data->dev = dev;
	m4u_data->plat_data = of_device_get_match_data(dev);
	total_port = m4u_data->plat_data->port_nr[MM_IOMMU] +
			m4u_data->plat_data->port_nr[APU_IOMMU];
	m4u_data->m4u_cb = devm_kzalloc(dev, total_port *
		sizeof(struct mtk_iommu_cb), GFP_KERNEL);
	if (!m4u_data->m4u_cb)
		return -ENOMEM;

	m4u_debug_init(m4u_data);
	mutex_init(&iova_list.lock);
	INIT_LIST_HEAD(&iova_list.head);
	pr_info("%s done: total:%u, apu:%u -- %s, mm:%u -- %s, 0x%x\n", __func__,
		total_port,
		m4u_data->plat_data->port_nr[APU_IOMMU],
		m4u_data->plat_data->port_list[APU_IOMMU][3].name,
		m4u_data->plat_data->port_nr[MM_IOMMU],
		m4u_data->plat_data->port_list[MM_IOMMU][10].name,
		F_APU_MMU_INT_TF_MSK(0x111));
	return 0;
}

static const struct mtk_m4u_plat_data mt6779_data = {
	.port_list[MM_IOMMU] = iommu_port_mt6779,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(iommu_port_mt6779),
};

static const struct mtk_m4u_plat_data mt6873_data = {
	.port_list[MM_IOMMU] = mm_port_mt6873,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6873),
	.port_list[APU_IOMMU] = apu_port_mt6873,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6873),
};

static const struct of_device_id mtk_m4u_dbg_of_ids[] = {
	{ .compatible = "mediatek,mt6779-iommu-debug", .data = &mt6779_data},
	{ .compatible = "mediatek,mt6873-iommu-debug", .data = &mt6873_data},
};

static struct platform_driver mtk_m4u_dbg_drv = {
	.probe	= mtk_m4u_dbg_probe,
	.driver	= {
		.name = "mtk-m4u-debug",
		.of_match_table = of_match_ptr(mtk_m4u_dbg_of_ids),
	}
};

module_platform_driver(mtk_m4u_dbg_drv);
MODULE_LICENSE("GPL v2");
