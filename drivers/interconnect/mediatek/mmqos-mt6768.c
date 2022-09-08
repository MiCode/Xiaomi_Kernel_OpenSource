// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ming-Fan Chen <ming-fan.chen@mediatek.com>
 */
#include <dt-bindings/interconnect/mtk,mmqos.h>
//#include <dt-bindings/interconnect/mtk,mt6768-emi.h>
#include <dt-bindings/memory/mt6768-larb-port.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include "mmqos-mtk.h"

static const struct mtk_node_desc node_descs_mt6768[] = {
	DEFINE_MNODE(common0,
		SLAVE_COMMON(0), 0, false, 0x0, MMQOS_NO_LINK),
	DEFINE_MNODE(common0_port0,
		MASTER_COMMON_PORT(0, 0), 0, false, 0x0, SLAVE_COMMON(0)),
	DEFINE_MNODE(common0_port1,
		MASTER_COMMON_PORT(0, 1), 0, false, 0x0, SLAVE_COMMON(0)),
	DEFINE_MNODE(common0_port2,
		MASTER_COMMON_PORT(0, 2), 0, false, 0x0, SLAVE_COMMON(0)),
	DEFINE_MNODE(common0_port3,
		MASTER_COMMON_PORT(0, 3), 0, false, 0x0, SLAVE_COMMON(0)),
	DEFINE_MNODE(common0_port4,
		MASTER_COMMON_PORT(0, 4), 0, false, 0x0, SLAVE_COMMON(0)),
	DEFINE_MNODE(common0_port4,
		MASTER_COMMON_PORT(0, 5), 0, false, 0x0, SLAVE_COMMON(0)),
    /*SMI Common*/
	DEFINE_MNODE(larb0, SLAVE_LARB(0), 0, false, 0x0, MASTER_COMMON_PORT(0, 0)),
	DEFINE_MNODE(larb1, SLAVE_LARB(1), 0, false, 0x0, MASTER_COMMON_PORT(0, 1)),
	DEFINE_MNODE(larb2, SLAVE_LARB(2), 0, false, 0x0, MASTER_COMMON_PORT(0, 2)),
	DEFINE_MNODE(larb3, SLAVE_LARB(3), 0, false, 0x0, MASTER_COMMON_PORT(0, 3)),
	DEFINE_MNODE(larb4, SLAVE_LARB(4), 0, false, 0x0, MASTER_COMMON_PORT(0, 4)),
	DEFINE_MNODE(larb21, SLAVE_LARB(21), 0, false, 0x0, MASTER_COMMON_PORT(0, 5)),
    /*Larb 0*/
	DEFINE_MNODE(l0_disp_ovl0,
		MASTER_LARB_PORT(M4U_PORT_DISP_OVL0), 8, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_disp_ovl0_2l,
		MASTER_LARB_PORT(M4U_PORT_DISP_2L_OVL0_LARB0), 8, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_disp_rdma0,
		MASTER_LARB_PORT(M4U_PORT_DISP_RDMA0), 7, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_w_disp_wdma0,
		MASTER_LARB_PORT(M4U_PORT_DISP_WDMA0), 9, true, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_mdp_rdma0,
		MASTER_LARB_PORT(M4U_PORT_MDP_RDMA0), 7, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_mdp_wdma0,
		MASTER_LARB_PORT(M4U_PORT_MDP_WDMA0), 7, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_mdp_wrot0,
		MASTER_LARB_PORT(M4U_PORT_MDP_WROT0), 7, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_disp_fake,
		MASTER_LARB_PORT(M4U_PORT_DISP_FAKE0), 7, false, 0x0, SLAVE_LARB(0)),
    /*Larb 1*/
	DEFINE_MNODE(l1_hw_vdec_mc_ext,
		MASTER_LARB_PORT(M4U_PORT_HW_VDEC_MC_EXT), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_hw_vdec_pp_ext,
		MASTER_LARB_PORT(M4U_PORT_HW_VDEC_PP_EXT), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_hw_vdec_vld_ext,
		MASTER_LARB_PORT(M4U_PORT_HW_VDEC_VLD_EXT), 7, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_w_hw_vdec_vld2_ext,
		MASTER_LARB_PORT(M4U_PORT_HW_VDEC_VLD2_EXT), 9, true, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_hw_vdec_avc_mv_ext,
		MASTER_LARB_PORT(M4U_PORT_HW_VDEC_AVC_MV_EXT), 7, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_hw_vdec_pred_rd_ext,
		MASTER_LARB_PORT(M4U_PORT_HW_VDEC_PRED_RD_EXT), 7, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_hw_vdec_pred_wr_ext,
		MASTER_LARB_PORT(M4U_PORT_HW_VDEC_PRED_WR_EXT), 7, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_hw_vdec_ppwrap_ext,
		MASTER_LARB_PORT(M4U_PORT_HW_VDEC_PPWRAP_EXT), 7, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_hw_vdec_tile_ext,
		MASTER_LARB_PORT(M4U_PORT_HW_VDEC_TILE_EXT), 7, false, 0x0, SLAVE_LARB(1)),
    /*Larb 2*/
	DEFINE_MNODE(l2_imgi,
		MASTER_LARB_PORT(M4U_PORT_CAM_IMGI), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_img2o,
		MASTER_LARB_PORT(M4U_PORT_CAM_IMG2O), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_img3o,
		MASTER_LARB_PORT(M4U_PORT_CAM_IMG3O), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_vipi,
		MASTER_LARB_PORT(M4U_PORT_CAM_VIPI), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_lcei,
		MASTER_LARB_PORT(M4U_PORT_CAM_LCEI), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_rp,
		MASTER_LARB_PORT(M4U_PORT_CAM_FD_RP), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_wr,
		MASTER_LARB_PORT(M4U_PORT_CAM_FD_WR), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_rb,
		MASTER_LARB_PORT(M4U_PORT_CAM_FD_RB), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_dpe_rdma,
		MASTER_LARB_PORT(M4U_PORT_CAM_DPE_RDMA), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_dpe_wdma,
		MASTER_LARB_PORT(M4U_PORT_CAM_DPE_WDMA), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_rsc_rdma0,
		MASTER_LARB_PORT(M4U_PORT_CAM_RSC_RDMA), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_rsc_wdma,
		MASTER_LARB_PORT(M4U_PORT_CAM_RSC_WDMA), 8, false, 0x0, SLAVE_LARB(2)),
    /*Larb 3*/
	DEFINE_MNODE(l3_imgo,
		MASTER_LARB_PORT(M4U_PORT_CAM_IMGO), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_rrzo,
		MASTER_LARB_PORT(M4U_PORT_CAM_RRZO), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_aao,
		MASTER_LARB_PORT(M4U_PORT_CAM_AAO), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_afo,
		MASTER_LARB_PORT(M4U_PORT_CAM_AFO), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_lsci_0,
		MASTER_LARB_PORT(M4U_PORT_CAM_LSCI0), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_lsci_1,
		MASTER_LARB_PORT(M4U_PORT_CAM_LSCI1), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_pdo,
		MASTER_LARB_PORT(M4U_PORT_CAM_PDO), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_bpci,
		MASTER_LARB_PORT(M4U_PORT_CAM_BPCI), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_lcso,
		MASTER_LARB_PORT(M4U_PORT_CAM_LCSO), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_rsso_a,
		MASTER_LARB_PORT(M4U_PORT_CAM_RSSO_A), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_rsso_b,
		MASTER_LARB_PORT(M4U_PORT_CAM_RSSO_B), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_ufeo,
		MASTER_LARB_PORT(M4U_PORT_CAM_UFEO), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_soco,
		MASTER_LARB_PORT(M4U_PORT_CAM_SOC0), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_soc1,
		MASTER_LARB_PORT(M4U_PORT_CAM_SOC1), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_soc2,
		MASTER_LARB_PORT(M4U_PORT_CAM_SOC2), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_ccui,
		MASTER_LARB_PORT(M4U_PORT_CAM_CCUI), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_ccuo,
		MASTER_LARB_PORT(M4U_PORT_CAM_CCUO), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_caci,
		MASTER_LARB_PORT(M4U_PORT_CAM_CACI), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_rawi_a,
		MASTER_LARB_PORT(M4U_PORT_CAM_RAWI_A), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_rawi_b,
		MASTER_LARB_PORT(M4U_PORT_CAM_RAWI_B), 8, false, 0x0, SLAVE_LARB(3)),
	DEFINE_MNODE(l3_ccug,
		MASTER_LARB_PORT(M4U_PORT_CAM_CCUG), 8, false, 0x0, SLAVE_LARB(3)),
    /*Larb 4*/
	DEFINE_MNODE(l4_venc_rcpu,
		MASTER_LARB_PORT(M4U_PORT_VENC_RCPU), 8, false, 0x0, SLAVE_LARB(4)),
	DEFINE_MNODE(l4_venc_rec,
		MASTER_LARB_PORT(M4U_PORT_VENC_REC), 8, false, 0x0, SLAVE_LARB(4)),
	DEFINE_MNODE(l4_venc_bsdma,
		MASTER_LARB_PORT(M4U_PORT_VENC_BSDMA), 8, false, 0x0, SLAVE_LARB(4)),
	DEFINE_MNODE(l4_venc_sv_comv,
		MASTER_LARB_PORT(M4U_PORT_VENC_SV_COMV), 8, false, 0x0, SLAVE_LARB(4)),
	DEFINE_MNODE(l4_venc_rd_comv,
		MASTER_LARB_PORT(M4U_PORT_VENC_RD_COMV), 8, false, 0x0, SLAVE_LARB(4)),
	DEFINE_MNODE(l4_jpgenc_rdma,
		MASTER_LARB_PORT(M4U_PORT_JPGENC_RDMA), 8, false, 0x0, SLAVE_LARB(4)),
	DEFINE_MNODE(l4_jpgenc_bsdma,
		MASTER_LARB_PORT(M4U_PORT_JPGENC_BSDMA), 8, false, 0x0, SLAVE_LARB(4)),
	DEFINE_MNODE(l4_venc_cur_luma,
		MASTER_LARB_PORT(M4U_PORT_VENC_CUR_LUMA), 8, false, 0x0, SLAVE_LARB(4)),
	DEFINE_MNODE(l4_venc_cur_chroma,
		MASTER_LARB_PORT(M4U_PORT_VENC_CUR_CHROMA), 8, false, 0x0, SLAVE_LARB(4)),
	DEFINE_MNODE(l4_venc_ref_luma,
		MASTER_LARB_PORT(M4U_PORT_VENC_REF_LUMA), 8, false, 0x0, SLAVE_LARB(4)),
	DEFINE_MNODE(l4_venc_ref_chroma,
		MASTER_LARB_PORT(M4U_PORT_VENC_REF_CHROMA), 8, false, 0x0, SLAVE_LARB(4)),
};
static const char * const comm_muxes_mt6768[] = { "mm" };
static const char * const comm_icc_path_names_mt6768[] = { "icc-bw" };
static const char * const comm_icc_hrt_path_names_mt6768[] = { "icc-hrt-bw" };
static const struct mtk_mmqos_desc mmqos_desc_mt6768 = {
	.nodes = node_descs_mt6768,
	.num_nodes = ARRAY_SIZE(node_descs_mt6768),
	.comm_muxes = comm_muxes_mt6768,
	.comm_icc_path_names = comm_icc_path_names_mt6768,
	.comm_icc_hrt_path_names = comm_icc_hrt_path_names_mt6768,
	.max_ratio = 40,
	.hrt = {
		.hrt_bw = {5332, 0, 0},
		.hrt_total_bw = 22000, /*Todo: Use DRAMC API 5500*2(channel)*2(io width)*/
		.md_speech_bw = { 5332, 5332},
		.hrt_ratio = {1000, 860, 880, 1000}, /* MD, CAM, DISP, MML */
		.blocking = true,
		.emi_ratio = 705,
	},
	.hrt_LPDDR4 = {
		.hrt_bw = {5141, 0, 0},
		.hrt_total_bw = 17064, /*Todo: Use DRAMC API 4266*2(channel)*2(io width)*/
		.md_speech_bw = { 5141, 5141},
		.hrt_ratio = {1000, 880, 900, 1000}, /* MD, CAM, DISP, MML */
		.blocking = true,
		.emi_ratio = 800,
	},
	.comm_port_channels = {
		{ 0x1, 0x2, 0x1, 0x2, 0x1, 0x3}
	},
	.comm_port_hrt_types = {
		{ HRT_MAX_BWL, HRT_NONE, HRT_NONE, HRT_NONE, HRT_NONE, HRT_DISP },
	},
};
static const struct of_device_id mtk_mmqos_mt6768_of_ids[] = {
	{
		.compatible = "mediatek,mt6768-mmqos",
		.data = &mmqos_desc_mt6768,
	},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_mmqos_mt6768_of_ids);
static struct platform_driver mtk_mmqos_mt6768_driver = {
	.probe = mtk_mmqos_probe,
	.remove = mtk_mmqos_remove,
	.driver = {
		.name = "mtk-mt6768-mmqos",
		.of_match_table = mtk_mmqos_mt6768_of_ids,
	},
};
module_platform_driver(mtk_mmqos_mt6768_driver);
MODULE_LICENSE("GPL v2");

