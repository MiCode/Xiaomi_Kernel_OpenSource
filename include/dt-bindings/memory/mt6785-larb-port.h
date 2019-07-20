/*
 * Copyright (c) 2019 MediaTek Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _DTS_IOMMU_PORT_H_
#define _DTS_IOMMU_PORT_H_

#define MTK_M4U_ID(larb, port)            (((larb) << 5) | port)

/* Local arbiter ID */
#define MTK_IOMMU_TO_LARB(id)	(((id) >> 5) & 0x1f)
/* PortID within the local arbiter */
#define MTK_IOMMU_TO_PORT(id)	((id) & 0x1f)

#define M4U_LARB_CCU                      (7) // 2 ports
#define M4U_LARB_APU                      (8) // 2 ports
#define MTK_IOMMU_LARB_NR	(9)

/*larb0 -MMSYS-9*/
#define M4U_PORT_DISP_POSTMASK0	            MTK_M4U_ID(0, 0)
#define M4U_PORT_DISP_OVL0_HDR	            MTK_M4U_ID(0, 1)
#define M4U_PORT_DISP_OVL1_HDR	            MTK_M4U_ID(0, 2)
#define M4U_PORT_DISP_OVL0	                MTK_M4U_ID(0, 3)
#define M4U_PORT_DISP_OVL1	                MTK_M4U_ID(0, 4)
#define M4U_PORT_DISP_PVRIC0	            MTK_M4U_ID(0, 5)
#define M4U_PORT_DISP_RDMA0	                MTK_M4U_ID(0, 6)
#define M4U_PORT_DISP_WDMA0	                MTK_M4U_ID(0, 7)
#define M4U_PORT_DISP_FAKE0	                MTK_M4U_ID(0, 8)
/*larb1-MMSYS-14*/
#define M4U_PORT_DISP_OVL0_2L_HDR	        MTK_M4U_ID(1, 0)
#define M4U_PORT_DISP_OVL1_2L_HDR	        MTK_M4U_ID(1, 1)
#define M4U_PORT_DISP_OVL0_2L	            MTK_M4U_ID(1, 2)
#define M4U_PORT_DISP_OVL1_2L	            MTK_M4U_ID(1, 3)
#define M4U_PORT_DISP_RDMA1	                MTK_M4U_ID(1, 4)
#define M4U_PORT_MDP_PVRIC0                 MTK_M4U_ID(1, 5)
#define M4U_PORT_MDP_PVRIC1	                MTK_M4U_ID(1, 6)
#define M4U_PORT_MDP_RDMA0                  MTK_M4U_ID(1, 7)
#define M4U_PORT_MDP_RDMA1                  MTK_M4U_ID(1, 8)
#define M4U_PORT_MDP_WROT0_R                MTK_M4U_ID(1, 9)
#define M4U_PORT_MDP_WROT0_W                MTK_M4U_ID(1, 10)
#define M4U_PORT_MDP_WROT1_R                MTK_M4U_ID(1, 11)
#define M4U_PORT_MDP_WROT1_W                MTK_M4U_ID(1, 12)
#define M4U_PORT_DISP_FAKE1                 MTK_M4U_ID(1, 13)
/*larb2-VDEC-12*/
#define M4U_PORT_HW_VDEC_MC_EXT             MTK_M4U_ID(2, 0)
#define M4U_PORT_HW_VDEC_UFO_EXT	        MTK_M4U_ID(2, 1)
#define M4U_PORT_HW_VDEC_PP_EXT             MTK_M4U_ID(2, 2)
#define M4U_PORT_HW_VDEC_PRED_RD_EXT	    MTK_M4U_ID(2, 3)
#define M4U_PORT_HW_VDEC_PRED_WR_EXT	    MTK_M4U_ID(2, 4)
#define M4U_PORT_HW_VDEC_PPWRAP_EXT         MTK_M4U_ID(2, 5)
#define M4U_PORT_HW_VDEC_TILE_EXT           MTK_M4U_ID(2, 6)
#define M4U_PORT_HW_VDEC_VLD_EXT	        MTK_M4U_ID(2, 7)
#define M4U_PORT_HW_VDEC_VLD2_EXT	        MTK_M4U_ID(2, 8)
#define M4U_PORT_HW_VDEC_AVC_MV_EXT	        MTK_M4U_ID(2, 9)
#define M4U_PORT_HW_VDEC_UFO_ENC_EXT	    MTK_M4U_ID(2, 10)
#define M4U_PORT_HW_VDEC_RG_CTRL_DMA_EXT	MTK_M4U_ID(2, 11)
/*larb3-VENC-19*/
#define M4U_PORT_VENC_RCPU                  MTK_M4U_ID(3, 0)
#define M4U_PORT_VENC_REC                   MTK_M4U_ID(3, 1)
#define M4U_PORT_VENC_BSDMA                 MTK_M4U_ID(3, 2)
#define M4U_PORT_VENC_SV_COMV               MTK_M4U_ID(3, 3)
#define M4U_PORT_VENC_RD_COMV               MTK_M4U_ID(3, 4)
#define M4U_PORT_VENC_NBM_RDMA              MTK_M4U_ID(3, 5)
#define M4U_PORT_VENC_NBM_RDMA_LITE         MTK_M4U_ID(3, 6)
#define M4U_PORT_JPGENC_Y_RDMA              MTK_M4U_ID(3, 7)
#define M4U_PORT_JPGENC_C_RDMA              MTK_M4U_ID(3, 8)
#define M4U_PORT_JPGENC_Q_TABLE             MTK_M4U_ID(3, 9)
#define M4U_PORT_JPGENC_BSDMA               MTK_M4U_ID(3, 10)
#define M4U_PORT_JPGDEC_WDMA                MTK_M4U_ID(3, 11)
#define M4U_PORT_JPGDEC_BSDMA               MTK_M4U_ID(3, 12)
#define M4U_PORT_VENC_NBM_WDMA              MTK_M4U_ID(3, 13)
#define M4U_PORT_VENC_NBM_WDMA_LITE         MTK_M4U_ID(3, 14)
#define M4U_PORT_VENC_CUR_LUMA              MTK_M4U_ID(3, 15)
#define M4U_PORT_VENC_CUR_CHROMA            MTK_M4U_ID(3, 16)
#define M4U_PORT_VENC_REF_LUMA              MTK_M4U_ID(3, 17)
#define M4U_PORT_VENC_REF_CHROMA            MTK_M4U_ID(3, 18)
/* larb4-IMG-3
 * HW disconnected
#define M4U_PORT_IPUO_LARB4		    MTK_M4U_ID(4, 0)
#define M4U_PORT_IPU3O_LARB4	            MTK_M4U_ID(4, 1)
#define M4U_PORT_IPUI_LARB4		    MTK_M4U_ID(4, 2)
 */
/*larb5-IMG-26*/
#define M4U_PORT_IMGI                   MTK_M4U_ID(5, 0)
#define M4U_PORT_IMG2O                  MTK_M4U_ID(5, 1)
#define M4U_PORT_IMG3O                  MTK_M4U_ID(5, 2)
#define M4U_PORT_VIPI                   MTK_M4U_ID(5, 3)
#define M4U_PORT_LCEI                   MTK_M4U_ID(5, 4)
#define M4U_PORT_SMXI                   MTK_M4U_ID(5, 5)
#define M4U_PORT_SMXO                   MTK_M4U_ID(5, 6)
#define M4U_PORT_WPE0_RDMA1             MTK_M4U_ID(5, 7)
#define M4U_PORT_WPE0_RDMA0             MTK_M4U_ID(5, 8)
#define M4U_PORT_WPE0_WDMA              MTK_M4U_ID(5, 9)
#define M4U_PORT_FDVT_RDB                   MTK_M4U_ID(5, 10)
#define M4U_PORT_FDVT_WRA                   MTK_M4U_ID(5, 11)
#define M4U_PORT_FDVT_RDA                   MTK_M4U_ID(5, 12)
#define M4U_PORT_WPE1_RDMA0             MTK_M4U_ID(5, 13)
#define M4U_PORT_WPE1_RDMA1             MTK_M4U_ID(5, 14)
#define M4U_PORT_WPE1_WDMA              MTK_M4U_ID(5, 15)
#define M4U_PORT_DPE_RDMA               MTK_M4U_ID(5, 16)
#define M4U_PORT_DPE_WDMA               MTK_M4U_ID(5, 17)
#define M4U_PORT_MFB_RDMA0              MTK_M4U_ID(5, 18)
#define M4U_PORT_MFB_RDMA1              MTK_M4U_ID(5, 19)
#define M4U_PORT_MFB_WDMA               MTK_M4U_ID(5, 20)
#define M4U_PORT_RSC_RDMA0              MTK_M4U_ID(5, 21)
#define M4U_PORT_RSC_WDMA               MTK_M4U_ID(5, 22)
#define M4U_PORT_OWE_RDMA               MTK_M4U_ID(5, 23)
#define M4U_PORT_OWE_WDMA               MTK_M4U_ID(5, 24)
#define M4U_PORT_FDVT_WRB               MTK_M4U_ID(5, 25)
/*larb6-IMG-31*/
#define M4U_PORT_IMGO                   MTK_M4U_ID(6, 0)
#define M4U_PORT_RRZO                   MTK_M4U_ID(6, 1)
#define M4U_PORT_AAO                        MTK_M4U_ID(6, 2)
#define M4U_PORT_AFO                    MTK_M4U_ID(6, 3)
#define M4U_PORT_LSCI_0                     MTK_M4U_ID(6, 4)
#define M4U_PORT_LSCI_1                     MTK_M4U_ID(6, 5)
#define M4U_PORT_PDO                    MTK_M4U_ID(6, 6)
#define M4U_PORT_BPCI                   MTK_M4U_ID(6, 7)
#define M4U_PORT_LSCO                       MTK_M4U_ID(6, 8)
#define M4U_PORT_RSSO_A                      MTK_M4U_ID(6, 9)
#define M4U_PORT_UFEO                        MTK_M4U_ID(6, 10)
#define M4U_PORT_SOCO                    MTK_M4U_ID(6, 11)
#define M4U_PORT_SOC1                   MTK_M4U_ID(6, 12)
#define M4U_PORT_SOC2                   MTK_M4U_ID(6, 13)
#define M4U_PORT_CCUI                   MTK_M4U_ID(6, 14)
#define M4U_PORT_CCUO                   MTK_M4U_ID(6, 15)
#define M4U_PORT_RAWI_A                   MTK_M4U_ID(6, 16)
#define M4U_PORT_CCUG                       MTK_M4U_ID(6, 17)
#define M4U_PORT_PSO                     MTK_M4U_ID(6, 18)
#define M4U_PORT_AFO_1                    MTK_M4U_ID(6, 19)
#define M4U_PORT_LSCI_2                   MTK_M4U_ID(6, 20)
#define M4U_PORT_PDI                        MTK_M4U_ID(6, 21)
#define M4U_PORT_FLKO                       MTK_M4U_ID(6, 22)
#define M4U_PORT_LMVO                       MTK_M4U_ID(6, 23)
#define M4U_PORT_UFGO                       MTK_M4U_ID(6, 24)
#define M4U_PORT_SPARE                      MTK_M4U_ID(6, 25)
#define M4U_PORT_SPARE_2                    MTK_M4U_ID(6, 26)
#define M4U_PORT_SPARE_3                    MTK_M4U_ID(6, 27)
#define M4U_PORT_SPARE_4                    MTK_M4U_ID(6, 28)
#define M4U_PORT_SPARE_5                    MTK_M4U_ID(6, 29)
#define FAKE_ENGINE                         MTK_M4U_ID(6, 30)

/*larb7-CAM-5/
 * HW disconnected
#define M4U_PORT_IPUO                       MTK_M4U_ID(7, 0)
#define M4U_PORT_IPU2O                      MTK_M4U_ID(7, 1)
#define M4U_PORT_IPU3O                      MTK_M4U_ID(7, 2)
#define M4U_PORT_IPUI                       MTK_M4U_ID(7, 3)
#define M4U_PORT_IPU2I                      MTK_M4U_ID(7, 4)
*/

/* smi common - 2 */
#define M4U_PORT_CCU0                       MTK_M4U_ID(M4U_LARB_CCU, 0)
#define M4U_PORT_CCU1                       MTK_M4U_ID(M4U_LARB_CCU, 1)

#define M4U_PORT_VPU                        MTK_M4U_ID(M4U_LARB_APU, 0)
#define M4U_PORT_VPU_DATA                   MTK_M4U_ID(M4U_LARB_APU, 1)

#define M4U_PORT_UNKNOWN	                (M4U_PORT_VPU_DATA + 1)

#define M4U_PORT_APU          M4U_PORT_VPU_DATA
#define M4U_PORT_CCU          M4U_PORT_CCU0
#define M4U_PORT_OVL_DEBUG    M4U_PORT_DISP_FAKE0
#define M4U_PORT_MDP_DEBUG    M4U_PORT_DISP_FAKE1

#ifndef M4U_PORT_NR
#define M4U_PORT_NR (115)   // 9 + 14 + 12 +19 + 26 + 31 + 4
#endif

//sw design
#define MISC_PSEUDO_LARBID_DISP                  (MTK_IOMMU_LARB_NR)
#define APU_PSEUDO_LARBID_DATA                  (MTK_IOMMU_LARB_NR + 1)

#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT == 34)
#define MTK_IOVA_ADDR_BITS 34
#define MTK_PHYS_ADDR_BITS 35
#else
#define MTK_IOVA_ADDR_BITS 32
#define MTK_PHYS_ADDR_BITS 34
#endif

#ifdef CONFIG_FPGA_EARLY_PORTING
#define MTK_IOMMU_M4U_COUNT (1)
#else
#define MTK_IOMMU_M4U_COUNT (2)
#endif
#define MTK_IOMMU_DEBUG_REG_NR   (6)
#define MTK_IOMMU_WAY_NR   (4)
#define MTK_IOMMU_RS_COUNT	(16)
#define MTK_IOMMU_MMU_COUNT	(2)
#define MTK_IOMMU_TAG_COUNT	(64)
#define MTK_IOMMU_BANK_COUNT	(5)
#define MTK_IOMMU_MAU_COUNT	(1)
#define MTK_MMU_NUM_OF_IOMMU(m4u_id)	MTK_IOMMU_MMU_COUNT
#define MTK_MAU_NUM_OF_MMU(mmu_id)	MTK_IOMMU_MAU_COUNT
#define MMU_PAGE_PER_LINE     (8)
#define MTK_IOMMU_LARB_CODEC_MIN (4)
#define MTK_IOMMU_LARB_CODEC_MAX (11)
#define MTK_IOMMU_IOVA_BOUNDARY_COUNT \
(1 << (CONFIG_MTK_IOMMU_PGTABLE_EXT - 32))

#define APU_IOMMU_INDEX (1)

#endif
