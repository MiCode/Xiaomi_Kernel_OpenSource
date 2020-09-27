/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_layering_rule_base.h"
#include "mtk_rect.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_pmqos.h"
#ifdef CONFIG_MTK_IOMMU_V2
#include <linux/iommu.h>
#include "mtk_iommu_ext.h"
#endif
#include "cmdq-sec.h"
#include "mtk_layer_layout_trace.h"
#include "mtk_drm_mmp.h"

#define REG_FLD(width, shift)                                                  \
	((unsigned int)((((width)&0xFF) << 16) | ((shift)&0xFF)))

#define REG_FLD_MSB_LSB(msb, lsb) REG_FLD((msb) - (lsb) + 1, (lsb))

#define REG_FLD_WIDTH(field) ((unsigned int)(((field) >> 16) & 0xFF))

#define REG_FLD_SHIFT(field) ((unsigned int)((field)&0xFF))

#define REG_FLD_MASK(field)                                                    \
	((unsigned int)((1ULL << REG_FLD_WIDTH(field)) - 1)                    \
	 << REG_FLD_SHIFT(field))

#define REG_FLD_VAL(field, val)                                                \
	(((val) << REG_FLD_SHIFT(field)) & REG_FLD_MASK(field))

#define DISP_REG_OVL_STA (0x000UL)
#define DISP_REG_OVL_INTEN 0x0004
#define INTEN_FLD_REG_CMT_INTEN REG_FLD_MSB_LSB(0, 0)
#define INTEN_FLD_FME_CPL_INTEN REG_FLD_MSB_LSB(1, 1)
#define INTEN_FLD_FME_UND_INTEN REG_FLD_MSB_LSB(2, 2)
#define INTEN_FLD_FME_SWRST_DONE_INTEN REG_FLD_MSB_LSB(3, 3)
#define INTEN_FLD_FME_HWRST_DONE_INTEN REG_FLD_MSB_LSB(4, 4)
#define INTEN_FLD_RDMA0_EOF_ABNORMAL_INTEN REG_FLD_MSB_LSB(5, 5)
#define INTEN_FLD_RDMA1_EOF_ABNORMAL_INTEN REG_FLD_MSB_LSB(6, 6)
#define INTEN_FLD_RDMA2_EOF_ABNORMAL_INTEN REG_FLD_MSB_LSB(7, 7)
#define INTEN_FLD_RDMA3_EOF_ABNORMAL_INTEN REG_FLD_MSB_LSB(8, 8)
#define INTEN_FLD_RDMA0_SMI_UNDERFLOW_INTEN REG_FLD_MSB_LSB(9, 9)
#define INTEN_FLD_RDMA1_SMI_UNDERFLOW_INTEN REG_FLD_MSB_LSB(10, 10)
#define INTEN_FLD_RDMA2_SMI_UNDERFLOW_INTEN REG_FLD_MSB_LSB(11, 11)
#define INTEN_FLD_RDMA3_SMI_UNDERFLOW_INTEN REG_FLD_MSB_LSB(12, 12)
#define INTEN_FLD_ABNORMAL_SOF REG_FLD_MSB_LSB(13, 13)
#define INTEN_FLD_START_INTEN REG_FLD_MSB_LSB(14, 14)

#define DISP_REG_OVL_INTSTA 0x0008
#define DISP_REG_OVL_EN (0x000CUL)
#define EN_FLD_BLOCK_EXT_ULTRA			REG_FLD_MSB_LSB(18, 18)
#define EN_FLD_BLOCK_EXT_PREULTRA		REG_FLD_MSB_LSB(19, 19)
#define DISP_OVL_READ_WRK_REG			BIT(20)
#define DISP_OVL_BYPASS_SHADOW			BIT(22)
#define DISP_REG_OVL_TRIG (0x010UL)

#define DISP_REG_OVL_RST 0x0014
#define DISP_REG_OVL_ROI_SIZE 0x0020
#define DISP_REG_OVL_DATAPATH_CON	(0x024UL)
#define DISP_OVL_BGCLR_IN_SEL BIT(2)
#define DISP_OVL_OUTPUT_CLAMP BIT(26)
#define DATAPATH_CON_FLD_LAYER_SMI_ID_EN	REG_FLD_MSB_LSB(0, 0)
#define DATAPATH_CON_FLD_GCLAST_EN		REG_FLD_MSB_LSB(24, 24)
#define DATAPATH_CON_FLD_HDR_GCLAST_EN		REG_FLD_MSB_LSB(25, 25)
#define DATAPATH_CON_FLD_OUTPUT_CLAMP		REG_FLD_MSB_LSB(26, 26)

#define DISP_REG_OVL_ROI_BGCLR 0x0028
#define DISP_REG_OVL_SRC_CON 0x002c
#define DISP_OVL_FORCE_RELAY_MODE BIT(8)

#define DISP_REG_OVL_CON(n) (0x0030 + 0x20 * (n))
#define L_CON_FLD_APHA REG_FLD_MSB_LSB(7, 0)
#define L_CON_FLD_AEN REG_FLD_MSB_LSB(8, 8)
#define L_CON_FLD_VIRTICAL_FLIP REG_FLD_MSB_LSB(9, 9)
#define L_CON_FLD_HORI_FLIP REG_FLD_MSB_LSB(10, 10)
#define L_CON_FLD_EXT_MTX_EN REG_FLD_MSB_LSB(11, 11)
#define L_CON_FLD_CFMT REG_FLD_MSB_LSB(15, 12)
#define L_CON_FLD_MTX REG_FLD_MSB_LSB(19, 16)
#define L_CON_FLD_EN_3D REG_FLD_MSB_LSB(20, 20)
#define L_CON_FLD_EN_LANDSCAPE REG_FLD_MSB_LSB(21, 21)
#define L_CON_FLD_EN_R_FIRST REG_FLD_MSB_LSB(22, 22)
#define L_CON_FLD_CLRFMT_MAN REG_FLD_MSB_LSB(23, 23)
#define L_CON_FLD_BTSW REG_FLD_MSB_LSB(24, 24)
#define L_CON_FLD_RGB_SWAP REG_FLD_MSB_LSB(25, 25)
#define L_CON_FLD_LSRC REG_FLD_MSB_LSB(29, 28)
#define L_CON_FLD_SKEN REG_FLD_MSB_LSB(30, 30)
#define L_CON_FLD_DKEN REG_FLD_MSB_LSB(31, 31)
#define CON_LSRC_RES BIT(28)
#define CON_VERTICAL_FLIP BIT(9)
#define CON_HORI_FLIP BIT(10)

#define DISP_REG_OVL_SRCKEY(n) (0x0034 + 0x20 * (n))
#define DISP_REG_OVL_SRC_SIZE(n) (0x0038 + 0x20 * (n))
#define DISP_REG_OVL_OFFSET(n) (0x003c + 0x20 * (n))
#define DISP_REG_OVL_PITCH_MSB(n) (0x0040 + 0x20 * (n))
#define L_PITCH_MSB_FLD_YUV_TRANS REG_FLD_MSB_LSB(20, 20)
#define L_PITCH_MSB_FLD_2ND_SUBBUF REG_FLD_MSB_LSB(16, 16)
#define L_PITCH_MSB_FLD_SRC_PITCH_MSB REG_FLD_MSB_LSB(3, 0)
#define DISP_REG_OVL_PITCH(n) (0x0044 + 0x20 * (n))
#define DISP_REG_OVL_CLIP(n) (0x04CUL + 0x20 * (n))
#define OVL_L_CLIP_FLD_LEFT REG_FLD_MSB_LSB(7, 0)
#define OVL_L_CLIP_FLD_RIGHT REG_FLD_MSB_LSB(15, 8)
#define OVL_L_CLIP_FLD_TOP REG_FLD_MSB_LSB(23, 16)
#define OVL_L_CLIP_FLD_BOTTOM REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_OVL_RDMA_CTRL(n) (0x00c0 + 0x20 * (n))
#define DISP_REG_OVL_RDMA_GMC(n) (0x00c8 + 0x20 * (n))
#define DISP_REG_OVL_RDMA_FIFO_CTRL(n) (0x00d0 + 0x20 * (n))
#define DISP_REG_OVL_RDMA0_MEM_GMC_S2 (0x1E0UL)
#define FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES	REG_FLD_MSB_LSB(11, 0)
#define FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES_URG	REG_FLD_MSB_LSB(27, 16)
#define FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_PREULTRA REG_FLD_MSB_LSB(28, 28)
#define FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_ULTRA	REG_FLD_MSB_LSB(29, 29)
#define FLD_OVL_RDMA_MEM_GMC2_FORCE_REQ_THRES	REG_FLD_MSB_LSB(30, 30)
#define DISP_REG_OVL_RDMA1_MEM_GMC_S2 (0x1E4UL)
#define DISP_REG_OVL_RDMA2_MEM_GMC_S2 (0x1E8UL)
#define DISP_REG_OVL_RDMA3_MEM_GMC_S2 (0x1ECUL)
#define DISP_REG_OVL_RDMA_BURST_CON1	(0x1F4UL)
#define FLD_RDMA_BURST_CON1_BURST16_EN		REG_FLD_MSB_LSB(28, 28)

#define DISP_REG_OVL_GDRDY_PRD (0x208UL)

#define DISP_REG_OVL_RDMA0_DBG (0x24CUL)
#define DISP_REG_OVL_RDMA1_DBG (0x250UL)
#define DISP_REG_OVL_RDMA2_DBG (0x254UL)
#define DISP_REG_OVL_RDMA3_DBG (0x258UL)
#define DISP_REG_OVL_L0_CLR(n) (0x25cUL + 0x4 * (n))

#define DISP_REG_OVL_LC_CON (0x280UL)
#define DISP_REG_OVL_LC_SRCKEY (0x284UL)
#define DISP_REG_OVL_LC_SRC_SIZE (0x288UL)
#define DISP_REG_OVL_LC_OFFSET (0x28cUL)
#define DISP_REG_OVL_LC_SRC_SEL (0x290UL)
#define DISP_REG_OVL_BANK_CON (0x29cUL)
#define DISP_REG_OVL_DEBUG_MON_SEL (0x1D4UL)
#define DISP_REG_OVL_RDMA_GREQ_NUM (0x1F8UL)
#define FLD_OVL_RDMA_GREQ_LAYER0_GREQ_NUM	REG_FLD_MSB_LSB(3, 0)
#define FLD_OVL_RDMA_GREQ_LAYER1_GREQ_NUM	REG_FLD_MSB_LSB(7, 4)
#define FLD_OVL_RDMA_GREQ_LAYER2_GREQ_NUM	REG_FLD_MSB_LSB(11, 8)
#define FLD_OVL_RDMA_GREQ_LAYER3_GREQ_NUM	REG_FLD_MSB_LSB(15, 12)
#define FLD_OVL_RDMA_GREQ_OSTD_GREQ_NUM		REG_FLD_MSB_LSB(23, 16)
#define FLD_OVL_RDMA_GREQ_GREQ_DIS_CNT		REG_FLD_MSB_LSB(26, 24)
#define FLD_OVL_RDMA_GREQ_STOP_EN		REG_FLD_MSB_LSB(27, 27)
#define FLD_OVL_RDMA_GREQ_GRP_END_STOP		REG_FLD_MSB_LSB(28, 28)
#define FLD_OVL_RDMA_GREQ_GRP_BRK_STOP		REG_FLD_MSB_LSB(29, 29)
#define FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_PREULTRA	REG_FLD_MSB_LSB(30, 30)
#define FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_ULTRA	REG_FLD_MSB_LSB(31, 31)
#define DISP_REG_OVL_RDMA_GREQ_URG_NUM (0x1FCUL)
#define FLD_OVL_RDMA_GREQ_LAYER0_GREQ_URG_NUM	REG_FLD_MSB_LSB(3, 0)
#define FLD_OVL_RDMA_GREQ_LAYER1_GREQ_URG_NUM	REG_FLD_MSB_LSB(7, 4)
#define FLD_OVL_RDMA_GREQ_LAYER2_GREQ_URG_NUM	REG_FLD_MSB_LSB(11, 8)
#define FLD_OVL_RDMA_GREQ_LAYER3_GREQ_URG_NUM	REG_FLD_MSB_LSB(15, 12)
#define FLD_OVL_RDMA_GREQ_ARG_GREQ_URG_TH	REG_FLD_MSB_LSB(25, 16)
#define FLD_OVL_RDMA_GREQ_ARG_URG_BIAS		REG_FLD_MSB_LSB(28, 28)
#define FLD_OVL_RDMA_GREQ_NUM_SHT_VAL		REG_FLD_MSB_LSB(29, 29)
#define DISP_REG_OVL_DUMMY_REG (0x200UL)
#define DISP_REG_OVL_RDMA_ULTRA_SRC (0x20CUL)
#define FLD_OVL_RDMA_PREULTRA_BUF_SRC		REG_FLD_MSB_LSB(1, 0)
#define FLD_OVL_RDMA_PREULTRA_SMI_SRC		REG_FLD_MSB_LSB(3, 2)
#define FLD_OVL_RDMA_PREULTRA_ROI_END_SRC	REG_FLD_MSB_LSB(5, 4)
#define FLD_OVL_RDMA_PREULTRA_RDMA_SRC		REG_FLD_MSB_LSB(7, 6)
#define FLD_OVL_RDMA_ULTRA_BUF_SRC		REG_FLD_MSB_LSB(9, 8)
#define FLD_OVL_RDMA_ULTRA_SMI_SRC		REG_FLD_MSB_LSB(11, 10)
#define FLD_OVL_RDMA_ULTRA_ROI_END_SRC		REG_FLD_MSB_LSB(13, 12)
#define FLD_OVL_RDMA_ULTRA_RDMA_SRC		REG_FLD_MSB_LSB(15, 14)
#define DISP_OVL_REG_GDRDY_PRD (0x208UL)
#define DISP_REG_OVL_RDMAn_BUF_LOW(layer) (0x210UL + ((layer) << 2))
#define FLD_OVL_RDMA_BUF_LOW_ULTRA_TH		REG_FLD_MSB_LSB(11, 0)
#define FLD_OVL_RDMA_BUF_LOW_PREULTRA_TH	REG_FLD_MSB_LSB(23, 12)
#define DISP_REG_OVL_RDMAn_BUF_HIGH(layer) (0x220UL + ((layer) << 2))
#define FLD_OVL_RDMA_BUF_HIGH_PREULTRA_TH REG_FLD_MSB_LSB(23, 12)
#define FLD_OVL_RDMA_BUF_HIGH_PREULTRA_DIS REG_FLD_MSB_LSB(31, 31)
#define DISP_REG_OVL_SMI_DBG (0x230UL)
#define DISP_REG_OVL_GREQ_LAYER_CNT (0x234UL)
#define DISP_REG_OVL_GDRDY_PRD_NUM (0x238UL)
#define DISP_REG_OVL_FLOW_CTRL_DBG (0x240UL)
#define DISP_REG_OVL_ADDCON_DBG (0x244UL)
#define DISP_REG_OVL_FUNC_DCM0 (0x2a0UL)
#define DISP_REG_OVL_FUNC_DCM1 (0x2a4UL)
#define DISP_REG_OVL_CLRFMT_EXT (0x2D0UL)
#define FLD_Ln_CLRFMT_NB(n) REG_FLD_MSB_LSB((n)*4 + 1, (n)*4)
#define FLD_ELn_CLRFMT_NB(n) REG_FLD_MSB_LSB((n)*4 + 17, (n)*4 + 16)
#define DISP_REG_OVL_WCG_CFG1 (0x2D8UL)
#define FLD_Ln_IGAMMA_EN(n) REG_FLD_MSB_LSB((n)*4, (n)*4)
#define FLD_Ln_CSC_EN(n) REG_FLD_MSB_LSB((n)*4 + 1, (n)*4 + 1)
#define FLD_Ln_GAMMA_EN(n) REG_FLD_MSB_LSB((n)*4 + 2, (n)*4 + 2)
#define FLD_ELn_IGAMMA_EN(n) REG_FLD_MSB_LSB((n)*4 + 16, (n)*4 + 16)
#define FLD_ELn_CSC_EN(n) REG_FLD_MSB_LSB((n)*4 + 17, (n)*4 + 17)
#define FLD_ELn_GAMMA_EN(n) REG_FLD_MSB_LSB((n)*4 + 18, (n)*4 + 18)
#define DISP_REG_OVL_WCG_CFG2 (0x2DCUL)
#define FLD_Ln_IGAMMA_SEL(n) REG_FLD_MSB_LSB((n)*4 + 1, (n)*4)
#define FLD_Ln_GAMMA_SEL(n) REG_FLD_MSB_LSB((n)*4 + 3, (n)*4 + 2)
#define FLD_ELn_IGAMMA_SEL(n) REG_FLD_MSB_LSB((n)*4 + 17, (n)*4 + 16)
#define FLD_ELn_GAMMA_SEL(n) REG_FLD_MSB_LSB((n)*4 + 19, (n)*4 + 18)
#define DISP_REG_OVL_DATAPATH_EXT_CON (0x324UL)
#define DISP_REG_OVL_EL_CON(n) (0x330UL + 0x20 * (n))
#define DISP_REG_OVL_EL_SRCKEY(n) (0x334UL + 0x20 * (n))
#define DISP_REG_OVL_EL_SRC_SIZE(n) (0x338UL + 0x20 * (n))
#define DISP_REG_OVL_EL_OFFSET(n) (0x33CUL + 0x20 * (n))
#define DISP_REG_OVL_EL_ADDR(n) (0xFB0UL + 0x04 * (n))
#define DISP_REG_OVL_EL_PITCH_MSB(n) (0x340U + 0x20 * (n))
#define DISP_REG_OVL_EL_PITCH(n) (0x344U + 0x20 * (n))
#define DISP_REG_OVL_EL_TILE(n) (0x348UL + 0x20 * (n))
#define DISP_REG_OVL_EL_CLIP(n) (0x34CUL + 0x20 * (n))
#define DISP_REG_OVL_EL0_CLR(n) (0x390UL + 0x4 * (n))
#define DISP_REG_OVL_ADDR_MT2701 0x0040
#define DISP_REG_OVL_ADDR_MT6779 0x0f40
#define DISP_REG_OVL_ADDR_MT6885 0x0f40
#define DISP_REG_OVL_ADDR_MT6873 0x0f40
#define DISP_REG_OVL_ADDR_MT6853 0x0f40
#define DISP_REG_OVL_ADDR_MT6833 0x0f40
#define DISP_REG_OVL_ADDR_MT8173 0x0f40
#define DISP_REG_OVL_ADDR(module, n) ((module)->data->addr + 0x20 * (n))

#define OVL_LAYER_OFFSET (0x20)
#define DISP_REG_OVL_L0_HDR_ADDR (0xF44UL)
#define DISP_REG_OVL_LX_HDR_ADDR(n) (0xF44UL + 0x20 * (n))
#define DISP_REG_OVL_L0_HDR_PITCH (0xF48UL)
#define DISP_REG_OVL_L1_HDR_ADDR (0xF64UL)
#define DISP_REG_OVL_LX_HDR_PITCH(n) (0xF48UL + 0x20 * (n))
#define DISP_REG_OVL_EL0_HDR_ADDR (0xFD0UL)
#define DISP_REG_OVL_EL1_HDR_ADDR (0xFD8UL)
#define DISP_REG_OVL_ELX_HDR_ADDR(n) (0xFD0UL + 0x8 * (n))
#define DISP_REG_OVL_ELX_HDR_PITCH(n) (0xFD4UL + 0x8 * (n))
#define DISP_REG_OVL_L0_OFFSET (0x03CUL)
#define DISP_REG_OVL_L0_SRC_SIZE (0x038UL)
#define DISP_REG_OVL_L0_PITCH (0x044UL)
#define L_PITCH_FLD_SRC_PITCH REG_FLD_MSB_LSB(15, 0)

#define ADDCON_DBG_FLD_ROI_X REG_FLD_MSB_LSB(12, 0)
#define ADDCON_DBG_FLD_L0_WIN_HIT REG_FLD_MSB_LSB(14, 14)
#define ADDCON_DBG_FLD_L1_WIN_HIT REG_FLD_MSB_LSB(15, 15)
#define ADDCON_DBG_FLD_ROI_Y REG_FLD_MSB_LSB(28, 16)
#define ADDCON_DBG_FLD_L2_WIN_HIT REG_FLD_MSB_LSB(30, 30)
#define ADDCON_DBG_FLD_L3_WIN_HIT REG_FLD_MSB_LSB(31, 31)
#define DATAPATH_CON_FLD_BGCLR_IN_SEL REG_FLD_MSB_LSB(2, 2)
#define DISP_REG_OVL_RDMA0_CTRL (0x0C0UL)
#define RDMA0_CTRL_FLD_RDMA_EN REG_FLD_MSB_LSB(0, 0)
#define RDMA0_CTRL_FLD_RDMA_INTERLACE REG_FLD_MSB_LSB(4, 4)
#define RDMA0_CTRL_FLD_RMDA_FIFO_USED_SZ REG_FLD_MSB_LSB(27, 16)

#define DISP_REG_OVL_RDMA0_MEM_GMC_SETTING (0x0C8UL)
#define FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD REG_FLD_MSB_LSB(9, 0)
#define FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD REG_FLD_MSB_LSB(25, 16)
#define FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD_HIGH_OFS REG_FLD_MSB_LSB(28, 28)
#define FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD_HIGH_OFS                      \
	REG_FLD_MSB_LSB(31, 31)

#define DISP_REG_OVL_RDMA0_MEM_SLOW_CON (0x0CCUL)
#define DISP_REG_OVL_RDMA0_FIFO_CTRL (0x0D0UL)
#define FLD_OVL_RDMA_FIFO_THRD REG_FLD_MSB_LSB(9, 0)
#define FLD_OVL_RDMA_FIFO_SIZE REG_FLD_MSB_LSB(27, 16)
#define FLD_OVL_RDMA_FIFO_UND_EN REG_FLD_MSB_LSB(31, 31)
#define DISP_REG_OVL_Ln_R2R_PARA(n) (0x500UL + 0x40 * (n))
#define DISP_REG_OVL_ELn_R2R_PARA(n) (0x600UL + 0x40 * (n))
#define DISP_REG_OVL_FBDC_CFG1 (0x804UL)
#define FLD_FBDC_8XE_MODE			REG_FLD_MSB_LSB(24, 24)
#define FLD_FBDC_FILTER_EN			REG_FLD_MSB_LSB(28, 28)
#define FBDC_8XE_MODE BIT(24)
#define FBDC_FILTER_EN BIT(28)

#define OVL_SECURE 0xfc0
#define EXT_SECURE_OFFSET 4

#define OVL_RDMA_DEBUG_OFFSET (0x4)

#define OVL_RDMA_MEM_GMC 0x40402020
#define OVL_ROI_BGCLR (0xFF000000)

#define OVL_CON_CLRFMT_MAN BIT(23)
#define OVL_CON_BYTE_SWAP BIT(24)
#define OVL_CON_RGB_SWAP BIT(25)
#define OVL_CON_MTX_JPEG_TO_RGB (4UL << 16)
#define OVL_CON_MTX_BT601_TO_RGB (6UL << 16)
#define OVL_CON_MTX_BT709_TO_RGB (7UL << 16)
#define OVL_CON_CLRFMT_RGB (1UL << 12)
#define OVL_CON_CLRFMT_RGBA8888 (2 << 12)
#define OVL_CON_CLRFMT_ARGB8888 (3 << 12)
#define OVL_CON_CLRFMT_DIM (1 << 28)
#define OVL_CON_CLRFMT_RGB565(module)                                          \
	(((module)->data->fmt_rgb565_is_0 == true) ? 0UL : OVL_CON_CLRFMT_RGB)
#define OVL_CON_CLRFMT_RGB888(module)                                          \
	(((module)->data->fmt_rgb565_is_0 == true) ? OVL_CON_CLRFMT_RGB : 0UL)
#define OVL_CON_CLRFMT_UYVY(module) ((module)->data->fmt_uyvy)
#define OVL_CON_CLRFMT_YUYV(module) ((module)->data->fmt_yuyv)
#define OVL_CON_AEN BIT(8)
#define OVL_CON_ALPHA 0xff

#define M4U_PORT_DISP_OVL0_HDR 1
#define M4U_PORT_DISP_OVL0 3
#define M4U_PORT_DISP_OVL0_2L_HDR ((1 << 5) + 0)
#define M4U_PORT_DISP_OVL0_2L ((1 << 5) + 2)

/* define for AFBC_V1_2 */
#define AFBC_V1_2_TILE_W (32)
#define AFBC_V1_2_TILE_H (8)
#define AFBC_V1_2_HEADER_ALIGN_BYTES (1024)
#define AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES (16)

enum GS_OVL_FLD {
	GS_OVL_RDMA_ULTRA_TH = 0,
	GS_OVL_RDMA_PRE_ULTRA_TH,
	GS_OVL_RDMA_FIFO_THRD,
	GS_OVL_RDMA_FIFO_SIZE,
	GS_OVL_RDMA_ISSUE_REQ_TH,
	GS_OVL_RDMA_ISSUE_REQ_TH_URG,
	GS_OVL_RDMA_REQ_TH_PRE_ULTRA,
	GS_OVL_RDMA_REQ_TH_ULTRA,
	GS_OVL_RDMA_FORCE_REQ_TH,
	GS_OVL_RDMA_GREQ_NUM,     /* whole reg */
	GS_OVL_RDMA_GREQ_URG_NUM, /* whole reg */
	GS_OVL_RDMA_ULTRA_SRC,    /* whole reg */
	GS_OVL_RDMA_ULTRA_LOW_TH,
	GS_OVL_RDMA_PRE_ULTRA_LOW_TH,
	GS_OVL_RDMA_PRE_ULTRA_HIGH_TH,
	GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS,
	GS_OVL_BLOCK_EXT_ULTRA,
	GS_OVL_BLOCK_EXT_PRE_ULTRA,
	GS_OVL_FLD_NUM,
};

#define CSC_COEF_NUM 9

static u32 sRGB_to_DCI_P3[CSC_COEF_NUM] = {215603, 46541, 0,     8702,  253442,
					   0,      4478,  18979, 238687};

static u32 DCI_P3_to_sRGB[CSC_COEF_NUM] = {
	321111, -58967, 0, -11025, 273169, 0, -5148, -20614, 287906};

#define DECLARE_MTK_OVL_COLORSPACE(EXPR)                                       \
	EXPR(OVL_SRGB)                                                         \
	EXPR(OVL_P3)                                                           \
	EXPR(OVL_CS_NUM)                                                       \
	EXPR(OVL_CS_UNKNOWN)

enum mtk_ovl_colorspace { DECLARE_MTK_OVL_COLORSPACE(DECLARE_NUM) };

static const char * const mtk_ovl_colorspace_str[] = {
	DECLARE_MTK_OVL_COLORSPACE(DECLARE_STR)};

#define DECLARE_MTK_OVL_TRANSFER(EXPR)                                         \
	EXPR(OVL_GAMMA2)                                                       \
	EXPR(OVL_GAMMA2_2)                                                     \
	EXPR(OVL_LINEAR)                                                       \
	EXPR(OVL_GAMMA_NUM)                                                    \
	EXPR(OVL_GAMMA_UNKNOWN)

enum mtk_ovl_transfer { DECLARE_MTK_OVL_TRANSFER(DECLARE_NUM) };

static const char * const mtk_ovl_transfer_str[] = {
	DECLARE_MTK_OVL_TRANSFER(DECLARE_STR)};

struct compress_info {
	/* naming rule: tech_version_MTK_sub-version,
	 * i.e.: PVRIC_V3_1_MTK_1
	 * sub-version is used when compression version is the same
	 * but mtk decoder is different among platforms.
	 */
	const char name[25];

	bool (*l_config)(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle);
};

struct mtk_disp_ovl_data {
	unsigned int addr;
	bool fmt_rgb565_is_0;
	unsigned int fmt_uyvy;
	unsigned int fmt_yuyv;
	const struct compress_info *compr_info;
	bool support_shadow;
};

#define MAX_LAYER_NUM 4
struct mtk_ovl_backup_info {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int con;
	unsigned long addr;
	unsigned int src_size;
	unsigned int src_pitch;
	unsigned int data_path_con;
};

/**
 * struct mtk_disp_ovl - DISP_OVL driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report vblank events to
 */
struct mtk_disp_ovl {
	struct mtk_ddp_comp ddp_comp;
	const struct mtk_disp_ovl_data *data;
	unsigned int underflow_cnt;
	int bg_w, bg_h;
	struct clk *fbdc_clk;
	struct mtk_ovl_backup_info backup_info[MAX_LAYER_NUM];
};

static inline struct mtk_disp_ovl *comp_to_ovl(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_ovl, ddp_comp);
}

int mtk_ovl_layer_num(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL1:
		return 4;
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL3_2L:
		return 2;
	default:
		DDPPR_ERR("invalid ovl module=%d\n", comp->id);
		return -1;
	}
	return 0;
}

static void dump_ovl_layer_trace(struct mtk_drm_crtc *mtk_crtc,
				 struct mtk_ddp_comp *ovl)
{
	struct cmdq_pkt_buffer *cmdq_buf = NULL;
	u32 offset = 0;
	u32 idx = 0;
	u32 gdrdy_num = 0, layer_en = 0, compress = 0;
	u32 ext_layer_en = 0, ext_layer_compress = 0;

	const int lnr = mtk_ovl_layer_num(ovl);
	int i = 0;
	u32 w = 0, h = 0, size = 0, con = 0, fmt = 0, src = 0;

	struct mtk_drm_private *priv = NULL;
	const int len = 1000;
	char msg[len];
	int n = 0;

	if (!mtk_crtc)
		return;
	priv = mtk_crtc->base.dev->dev_private;
	if (!(mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_LAYER_REC) &&
	      mtk_crtc->layer_rec_en))
		return;

	if (ovl->id == DDP_COMPONENT_OVL0_2L)
		offset = DISP_SLOT_LAYER_REC_OVL0_2L;
	else if (ovl->id == DDP_COMPONENT_OVL0)
		offset = DISP_SLOT_LAYER_REC_OVL0;
	else
		return;

	cmdq_buf = &mtk_crtc->gce_obj.buf;

	idx = *(u32 *)(cmdq_buf->va_base + DISP_SLOT_TRIG_CNT);

	gdrdy_num = *(u32 *)(cmdq_buf->va_base + offset);
	gdrdy_num <<= 4;
	n = snprintf(msg, len, "idx:%u,ovl%s:bw:%u", idx,
		     ovl->id == DDP_COMPONENT_OVL0 ? "0" : "0_2l", gdrdy_num);

	offset += 4;
	layer_en = *(u32 *)(cmdq_buf->va_base + offset);
	layer_en &= 0xf;

	offset += 4;
	compress = *(u32 *)(cmdq_buf->va_base + offset);
	compress = (compress >> 4) & 0xf;

	offset += 4;
	ext_layer_en = *(u32 *)(cmdq_buf->va_base + offset);
	ext_layer_compress = (ext_layer_en >> 4) & 0x7;
	ext_layer_en &= 0x7;

	for (i = 0; i < lnr + 3; i++) {
		if (i < lnr) {
			if (!(layer_en & 0x1)) {
				offset += (0x4 * 2);
				goto next;
			}
		} else {
			if (!(ext_layer_en & 0x1)) {
				offset += (0x4 * 2);
				goto next;
			}
		}

		offset += 0x4;
		con = *(u32 *)(cmdq_buf->va_base + offset);
		fmt = (con >> 12) & 0xf;
		src = (con >> 28) & 0x3;

		offset += 0x4;
		size = *(u32 *)(cmdq_buf->va_base + offset);
		w = size & 0x1fff;
		h = (size >> 16) & 0x1fff;

		if (i < lnr) {
			n += snprintf(msg + n, len - n,
				      "|L%d:%dx%d,f:0x%x,c:%d,src:%d",
				      i, w, h, fmt, compress & 0x1, src);
		} else {
			n += snprintf(msg + n, len - n,
				      "|L%d:%dx%d,f:0x%x,c:%d,src:%d",
				      i, w, h, fmt, ext_layer_compress & 0x1,
				      src);
		}

next:
		if (i < lnr) {
			layer_en >>= 1;
			compress >>= 1;
		} else {
			ext_layer_en >>= 1;
			ext_layer_compress >>= 1;
		}
	}

	n += snprintf(msg + n, len - n, "\n");
	trace_layer_bw(msg);
}

static irqreturn_t mtk_disp_ovl_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_ovl *priv = dev_id;
	struct mtk_ddp_comp *ovl = &priv->ddp_comp;
	struct mtk_drm_private *drv_priv = NULL;
	struct mtk_drm_crtc *mtk_crtc = ovl->mtk_crtc;
	unsigned int val = 0;
	unsigned int ret = 0;

	if (mtk_drm_top_clk_isr_get("ovl_irq") == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	val = readl(ovl->regs + DISP_REG_OVL_INTSTA);
	if (!val) {
		ret = IRQ_NONE;
		goto out;
	}

	DRM_MMP_MARK(IRQ, irq, val);

	if (ovl->id == DDP_COMPONENT_OVL0)
		DRM_MMP_MARK(ovl0, val, 0);
	else if (ovl->id == DDP_COMPONENT_OVL1)
		DRM_MMP_MARK(ovl1, val, 0);
	else if (ovl->id == DDP_COMPONENT_OVL0_2L)
		DRM_MMP_MARK(ovl0_2l, val, 0);
	else if (ovl->id == DDP_COMPONENT_OVL1_2L)
		DRM_MMP_MARK(ovl1_2l, val, 0);
	else if (ovl->id == DDP_COMPONENT_OVL2_2L)
		DRM_MMP_MARK(ovl2_2l, val, 0);
	else if (ovl->id == DDP_COMPONENT_OVL3_2L)
		DRM_MMP_MARK(ovl3_2l, val, 0);

	if (val & 0x1e0)
		DRM_MMP_MARK(abnormal_irq, val, ovl->id);

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(ovl), val);

	writel(~val, ovl->regs + DISP_REG_OVL_INTSTA);

	if (val & (1 << 0))
		DDPIRQ("[IRQ] %s: reg commit!\n", mtk_dump_comp_str(ovl));
	if (val & (1 << 1)) {
		DDPIRQ("[IRQ] %s: frame done!\n", mtk_dump_comp_str(ovl));
		dump_ovl_layer_trace(mtk_crtc, ovl);
	}
	if (val & (1 << 2)) {
		DDPPR_ERR("[IRQ] %s: frame underflow! cnt=%d\n",
			  mtk_dump_comp_str(ovl), priv->underflow_cnt);
		priv->underflow_cnt++;
		mtk_ovl_dump(ovl);
		mtk_ovl_analysis(ovl);
	}
	if (val & (1 << 3))
		DDPIRQ("[IRQ] %s: sw reset done!\n", mtk_dump_comp_str(ovl));
	if (val & (1 << 4))
		DDPPR_ERR("[IRQ] %s: hw reset done!\n", mtk_dump_comp_str(ovl));
	if (val & (1 << 5))
		DDPPR_ERR("[IRQ] %s: L0 not complete until EOF!\n",
			  mtk_dump_comp_str(ovl));
	if (val & (1 << 6))
		DDPPR_ERR("[IRQ] %s: L1 not complete until EOF!\n",
			  mtk_dump_comp_str(ovl));
	if (val & (1 << 7))
		DDPPR_ERR("[IRQ] %s: L2 not complete until EOF!\n",
			  mtk_dump_comp_str(ovl));
	if (val & (1 << 8))
		DDPPR_ERR("[IRQ] %s: L3 not complete until EOF!\n",
			  mtk_dump_comp_str(ovl));

	if (mtk_crtc) {
		drv_priv = mtk_crtc->base.dev->dev_private;
		if (!mtk_drm_helper_get_opt(
			    drv_priv->helper_opt,
			    MTK_DRM_OPT_COMMIT_NO_WAIT_VBLANK)) {
			mtk_crtc_vblank_irq(&mtk_crtc->base);
		}
	}

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put("ovl_irq");

	return ret;
}

#if 0
static void mtk_ovl_enable_vblank(struct mtk_ddp_comp *comp,
				  struct drm_crtc *crtc,
				  struct cmdq_pkt *handle)
{
	unsigned int inten;

	writel(0x0, comp->regs + DISP_REG_OVL_INTSTA);
	inten = 0x1E0 | REG_FLD_VAL(INTEN_FLD_ABNORMAL_SOF, 1) |
		REG_FLD_VAL(INTEN_FLD_START_INTEN, 1);
	writel_relaxed(inten, comp->regs + DISP_REG_OVL_INTEN);
}

static void mtk_ovl_disable_vblank(struct mtk_ddp_comp *comp,
				   struct cmdq_pkt *handle)
{
	writel_relaxed(0x0, comp->regs + DISP_REG_OVL_INTEN);
}
#endif

static int mtk_ovl_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params);

static void mtk_ovl_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	int ret;
	unsigned int val;
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	const struct compress_info *compr_info = ovl->data->compr_info;
	unsigned int value = 0, mask = 0;

	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

	ret = pm_runtime_get_sync(comp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to enable power domain: %d\n", ret);

	mtk_ovl_io_cmd(comp, handle, IRQ_LEVEL_ALL, NULL);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_EN,
		       0x1, 0x1);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_INTEN,
		       0x61F2, ~0);

	/* In 6779 we need to set DISP_OVL_FORCE_RELAY_MODE */
	if (compr_info && strncmp(compr_info->name, "PVRIC_V3_1", 10) == 0) {
		val = FBDC_8XE_MODE | FBDC_FILTER_EN;
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_FBDC_CFG1, val, val);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_SRC_CON,
		       DISP_OVL_FORCE_RELAY_MODE, DISP_OVL_FORCE_RELAY_MODE);


	SET_VAL_MASK(value, mask, 1, FLD_RDMA_BURST_CON1_BURST16_EN);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_RDMA_BURST_CON1,
		       value, mask);

	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 1, DATAPATH_CON_FLD_LAYER_SMI_ID_EN);
	SET_VAL_MASK(value, mask, 1, DATAPATH_CON_FLD_GCLAST_EN);
	SET_VAL_MASK(value, mask, 1, DATAPATH_CON_FLD_OUTPUT_CLAMP);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
		       value, mask);

#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
	/* Enable feedback real BW consumed from OVL */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_OVL_GDRDY_PRD,
		0xFFFFFFFF, 0xFFFFFFFF);
#endif

	DDPDBG("%s-\n", __func__);
}

static void mtk_ovl_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	int ret;

	DDPDBG("%s+\n", __func__);

	ret = pm_runtime_put(comp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_INTEN, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_OVL_EN,
		       0x0, 0x1);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_RST, 1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_INTSTA, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_RST, 0, ~0);

	comp->qos_bw = 0;
	comp->fbdc_bw = 0;
	DDPDBG("%s-\n", __func__);
}

static void _store_bg_roi(struct mtk_ddp_comp *comp, int h, int w)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	ovl->bg_h = h;
	ovl->bg_w = w;
}

static void _get_bg_roi(struct mtk_ddp_comp *comp, int *h, int *w)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	*h = ovl->bg_h;
	*w = ovl->bg_w;
}

static int mtk_ovl_golden_setting(struct mtk_ddp_comp *comp,
				  struct mtk_ddp_config *cfg,
				  struct cmdq_pkt *handle);

static void mtk_ovl_config(struct mtk_ddp_comp *comp,
			   struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int width;

	if (comp->mtk_crtc->is_dual_pipe) {
		width = cfg->w / 2;
		DDPMSG("\n");
	} else
		width = cfg->w;

	if (cfg->w != 0 && cfg->h != 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ROI_SIZE,
			       cfg->h << 16 | width, ~0);
		_store_bg_roi(comp, cfg->h, width);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_ROI_BGCLR, OVL_ROI_BGCLR,
		       ~0);

	mtk_ovl_golden_setting(comp, cfg, handle);
}

static void mtk_ovl_layer_on(struct mtk_ddp_comp *comp, unsigned int idx,
			     unsigned int ext_idx, struct cmdq_pkt *handle)
{
	unsigned int con;

	if (ext_idx != LYE_NORMAL) {
		unsigned int con_mask;

		con_mask =
			BIT(ext_idx - 1) | (0xFFFF << ((ext_idx - 1) * 4 + 16));
		con = BIT(ext_idx - 1) | (idx << ((ext_idx - 1) * 4 + 16));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON,
			       con, con_mask);
		return;
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_RDMA_CTRL(idx), 0x1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_SRC_CON, BIT(idx),
		       BIT(idx));
}

static void mtk_ovl_layer_off(struct mtk_ddp_comp *comp, unsigned int idx,
			      unsigned int ext_idx, struct cmdq_pkt *handle)
{
	u32 wcg_mask = 0, wcg_value = 0, sel_value = 0, sel_mask = 0;

	if (ext_idx != LYE_NORMAL) {
		SET_VAL_MASK(wcg_value, wcg_mask, 0,
			     FLD_ELn_IGAMMA_EN(ext_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, 0,
			     FLD_ELn_GAMMA_EN(ext_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, 0,
			     FLD_ELn_CSC_EN(ext_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, 0,
			     FLD_ELn_IGAMMA_SEL(ext_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, 0,
			     FLD_ELn_GAMMA_SEL(ext_idx - 1));
	} else {
		SET_VAL_MASK(wcg_value, wcg_mask, 0, FLD_Ln_IGAMMA_EN(idx));
		SET_VAL_MASK(wcg_value, wcg_mask, 0, FLD_Ln_GAMMA_EN(idx));
		SET_VAL_MASK(wcg_value, wcg_mask, 0, FLD_Ln_CSC_EN(idx));
		SET_VAL_MASK(sel_value, sel_mask, 0, FLD_Ln_IGAMMA_SEL(idx));
		SET_VAL_MASK(sel_value, sel_mask, 0, FLD_Ln_GAMMA_SEL(idx));
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_WCG_CFG1, wcg_value,
		       wcg_mask);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_WCG_CFG2, sel_value,
		       sel_mask);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_WCG_CFG1, wcg_value,
		       wcg_mask);

	if (ext_idx != LYE_NORMAL)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON, 0,
			       BIT(ext_idx - 1) | BIT(ext_idx + 3));
	else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON, 0,
			       BIT(idx + 4));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SRC_CON, 0,
			       BIT(idx));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_RDMA_CTRL(idx), 0,
			       ~0);
	}
}

static unsigned int ovl_fmt_convert(struct mtk_disp_ovl *ovl, unsigned int fmt,
				    uint64_t modifier)
{
	switch (fmt) {
	default:
	case DRM_FORMAT_RGB565:
		return OVL_CON_CLRFMT_RGB565(ovl);
	case DRM_FORMAT_BGR565:
		return (unsigned int)OVL_CON_CLRFMT_RGB565(ovl) |
		       OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_RGB888:
		return OVL_CON_CLRFMT_RGB888(ovl);
	case DRM_FORMAT_BGR888:
		return (unsigned int)OVL_CON_CLRFMT_RGB888(ovl) |
		       OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		if (modifier & MTK_FMT_PREMULTIPLIER)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN;
		else
			return OVL_CON_CLRFMT_ARGB8888;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
		if (modifier & MTK_FMT_PREMULTIPLIER)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_BYTE_SWAP |
			       OVL_CON_CLRFMT_MAN;
		else
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		if (modifier & MTK_FMT_PREMULTIPLIER)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_BYTE_SWAP |
			       OVL_CON_CLRFMT_MAN | OVL_CON_RGB_SWAP;
		else
			return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		if (modifier & MTK_FMT_PREMULTIPLIER)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN |
			       OVL_CON_RGB_SWAP;
		else
			return OVL_CON_CLRFMT_RGBA8888 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_UYVY:
		return OVL_CON_CLRFMT_UYVY(ovl);
	case DRM_FORMAT_YUYV:
		return OVL_CON_CLRFMT_YUYV(ovl);
	case DRM_FORMAT_ABGR2101010:
		if (modifier & MTK_FMT_PREMULTIPLIER)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN |
			       OVL_CON_RGB_SWAP;
		return OVL_CON_CLRFMT_RGBA8888 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_ABGR16161616F:
		if (modifier & MTK_FMT_PREMULTIPLIER)
			return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_CLRFMT_MAN |
			       OVL_CON_RGB_SWAP;
		return OVL_CON_CLRFMT_RGBA8888 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_C8:
		return OVL_CON_CLRFMT_DIM | OVL_CON_CLRFMT_RGB888(ovl);
	}
}

static const char *mtk_ovl_get_transfer_str(enum mtk_ovl_transfer transfer)
{
	if (transfer < 0) {
		DDPPR_ERR("%s: Invalid ovl transfer:%d\n", __func__, transfer);
		transfer = 0;
	}

	return mtk_ovl_transfer_str[transfer];
}

static const char *
mtk_ovl_get_colorspace_str(enum mtk_ovl_colorspace colorspace)
{
	return mtk_ovl_colorspace_str[colorspace];
}

static enum mtk_ovl_colorspace mtk_ovl_map_cs(enum mtk_drm_dataspace ds)
{
	enum mtk_ovl_colorspace cs = OVL_SRGB;

	switch (ds & MTK_DRM_DATASPACE_STANDARD_MASK) {
	case MTK_DRM_DATASPACE_STANDARD_DCI_P3:
		cs = OVL_P3;
		break;
	case MTK_DRM_DATASPACE_STANDARD_ADOBE_RGB:
		DDPPR_ERR("%s: ovl get cs ADOBE_RGB\n", __func__);
	case MTK_DRM_DATASPACE_STANDARD_BT2020:
		DDPPR_ERR("%s: ovl does not support BT2020\n", __func__);
	default:
		cs = OVL_SRGB;
		break;
	}

	return cs;
}

static enum mtk_ovl_transfer mtk_ovl_map_transfer(enum mtk_drm_dataspace ds)
{
	enum mtk_ovl_transfer xfr = OVL_GAMMA_UNKNOWN;

	switch (ds & MTK_DRM_DATASPACE_TRANSFER_MASK) {
	case MTK_DRM_DATASPACE_TRANSFER_LINEAR:
		xfr = OVL_LINEAR;
		break;
	case MTK_DRM_DATASPACE_TRANSFER_GAMMA2_6:
	case MTK_DRM_DATASPACE_TRANSFER_GAMMA2_8:
		DDPPR_ERR("%s: ovl does not support gamma 2.6/2.8\n", __func__);
	case MTK_DRM_DATASPACE_TRANSFER_ST2084:
	case MTK_DRM_DATASPACE_TRANSFER_HLG:
		DDPPR_ERR("%s: HDR transfer\n", __func__);
	default:
		xfr = OVL_GAMMA2_2;
		break;
	}

	return xfr;
}

static int mtk_ovl_do_transfer(unsigned int idx,
			       enum mtk_drm_dataspace plane_ds,
			       enum mtk_drm_dataspace lcm_ds, bool *gamma_en,
			       bool *igamma_en, u32 *gamma_sel, u32 *igamma_sel)
{
	enum mtk_ovl_transfer xfr_in = OVL_GAMMA2_2, xfr_out = OVL_GAMMA2_2;
	enum mtk_ovl_colorspace cs_in = OVL_CS_UNKNOWN, cs_out = OVL_CS_UNKNOWN;
	bool en = false;

	xfr_in = mtk_ovl_map_transfer(plane_ds);
	xfr_out = mtk_ovl_map_transfer(lcm_ds);
	cs_in = mtk_ovl_map_cs(plane_ds);
	cs_out = mtk_ovl_map_cs(lcm_ds);

	DDPDBG("%s+ idx:%d transfer:%s->%s\n", __func__, idx,
	       mtk_ovl_get_transfer_str(xfr_in),
	       mtk_ovl_get_transfer_str(xfr_out));

	en = xfr_in != OVL_LINEAR && (xfr_in != xfr_out || cs_in != cs_out);

	if (en) {
		*igamma_en = true;
		*igamma_sel = xfr_in;
	} else
		*igamma_en = false;

	en = xfr_out != OVL_LINEAR && (xfr_in != xfr_out || cs_in != cs_out);

	if (en) {
		*gamma_en = true;
		*gamma_sel = xfr_out;
	} else
		*gamma_en = false;

	return 0;
}

static u32 *mtk_get_ovl_csc(enum mtk_ovl_colorspace in,
			    enum mtk_ovl_colorspace out)
{
	static u32 *ovl_csc[OVL_CS_NUM][OVL_CS_NUM];
	static bool inited;

	if (inited)
		goto done;

	if (in < 0) {
		DDPPR_ERR("%s: Invalid ovl colorspace in:%d\n", __func__, in);
		in = 0;
	}

	ovl_csc[OVL_SRGB][OVL_P3] = sRGB_to_DCI_P3;
	ovl_csc[OVL_P3][OVL_SRGB] = DCI_P3_to_sRGB;

	inited = true;

done:
	return ovl_csc[in][out];
}

static int mtk_ovl_do_csc(unsigned int idx, enum mtk_drm_dataspace plane_ds,
			  enum mtk_drm_dataspace lcm_ds, bool *csc_en,
			  u32 **csc)
{
	enum mtk_ovl_colorspace in = OVL_SRGB, out = OVL_SRGB;
	bool en = false;

	in = mtk_ovl_map_cs(plane_ds);
	out = mtk_ovl_map_cs(lcm_ds);

	DDPDBG("%s+ idx:%d csc:%s->%s\n", __func__, idx,
	       mtk_ovl_get_colorspace_str(in), mtk_ovl_get_colorspace_str(out));

	en = in != out;

	if (en)
		*csc_en = true;
	else
		*csc_en = false;

	if (!en)
		return 0;
	if (!csc) {
		DDPPR_ERR("%s+ invalid csc\n", __func__);
		return 0;
	}

	*csc = mtk_get_ovl_csc(in, out);
	if (!(*csc)) {
		DDPPR_ERR("%s+ idx:%d no ovl csc %s to %s, disable csc\n",
			  __func__, idx, mtk_ovl_get_colorspace_str(in),
			  mtk_ovl_get_colorspace_str(out));
		*csc_en = false;
	}

	return 0;
}

static enum mtk_drm_dataspace
mtk_ovl_map_lcm_color_mode(enum mtk_drm_color_mode cm)
{
	enum mtk_drm_dataspace ds = MTK_DRM_DATASPACE_SRGB;

	switch (cm) {
	case MTK_DRM_COLOR_MODE_DISPLAY_P3:
		ds = MTK_DRM_DATASPACE_DISPLAY_P3;
		break;
	default:
		ds = MTK_DRM_DATASPACE_SRGB;
		break;
	}

	return ds;
}

static int mtk_ovl_color_manage(struct mtk_ddp_comp *comp, unsigned int idx,
				struct mtk_plane_state *state,
				struct cmdq_pkt *handle)
{
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	struct mtk_plane_pending_state *pending = &state->pending;
	struct drm_crtc *crtc = state->crtc;
	struct mtk_drm_private *priv;
	bool gamma_en = false, igamma_en = false, csc_en = false;
	u32 gamma_sel = 0, igamma_sel = 0;
	u32 *csc = NULL;
	u32 wcg_mask = 0, wcg_value = 0, sel_mask = 0, sel_value = 0, reg = 0;
	enum mtk_drm_color_mode lcm_cm;
	enum mtk_drm_dataspace lcm_ds, plane_ds;
	struct mtk_panel_params *params;
	int i;

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	if (!crtc)
		goto done;

	priv = crtc->dev->dev_private;
	if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_WCG) ||
	    !pending->enable)
		goto done;

	params = mtk_drm_get_lcm_ext_params(crtc);
	if (params)
		lcm_cm = params->lcm_color_mode;
	else
		lcm_cm = MTK_DRM_COLOR_MODE_NATIVE;

	lcm_ds = mtk_ovl_map_lcm_color_mode(lcm_cm);
	plane_ds =
		(enum mtk_drm_dataspace)pending->prop_val[PLANE_PROP_DATASPACE];

	DDPDBG("%s+ idx:%d ds:0x%08x->0x%08x\n", __func__, idx, plane_ds,
	       lcm_ds);

	mtk_ovl_do_transfer(idx, plane_ds, lcm_ds, &gamma_en, &igamma_en,
			    &gamma_sel, &igamma_sel);
	mtk_ovl_do_csc(idx, plane_ds, lcm_ds, &csc_en, &csc);

done:

	if (ext_lye_idx != LYE_NORMAL) {
		SET_VAL_MASK(wcg_value, wcg_mask, igamma_en,
			     FLD_ELn_IGAMMA_EN(ext_lye_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, gamma_en,
			     FLD_ELn_GAMMA_EN(ext_lye_idx - 1));
		SET_VAL_MASK(wcg_value, wcg_mask, csc_en,
			     FLD_ELn_CSC_EN(ext_lye_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, igamma_sel,
			     FLD_ELn_IGAMMA_SEL(ext_lye_idx - 1));
		SET_VAL_MASK(sel_value, sel_mask, gamma_sel,
			     FLD_ELn_GAMMA_SEL(ext_lye_idx - 1));
	} else {
		SET_VAL_MASK(wcg_value, wcg_mask, igamma_en,
			     FLD_Ln_IGAMMA_EN(lye_idx));
		SET_VAL_MASK(wcg_value, wcg_mask, gamma_en,
			     FLD_Ln_GAMMA_EN(lye_idx));
		SET_VAL_MASK(wcg_value, wcg_mask, csc_en,
			     FLD_Ln_CSC_EN(lye_idx));
		SET_VAL_MASK(sel_value, sel_mask, igamma_sel,
			     FLD_Ln_IGAMMA_SEL(lye_idx));
		SET_VAL_MASK(sel_value, sel_mask, gamma_sel,
			     FLD_Ln_GAMMA_SEL(lye_idx));
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_WCG_CFG1, wcg_value,
		       wcg_mask);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_WCG_CFG2, sel_value,
		       sel_mask);

	if (csc_en) {
		if (ext_lye_idx != LYE_NORMAL)
			reg = DISP_REG_OVL_ELn_R2R_PARA(ext_lye_idx - 1);
		else
			reg = DISP_REG_OVL_Ln_R2R_PARA(lye_idx);

		for (i = 0; i < CSC_COEF_NUM; i++)
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + reg + 4 * i, csc[i], ~0);
	}

	return 0;
}

static int mtk_ovl_yuv_matrix_convert(enum mtk_drm_dataspace plane_ds)
{
	int ret = 0;

	switch (plane_ds & MTK_DRM_DATASPACE_STANDARD_MASK) {
	case MTK_DRM_DATASPACE_STANDARD_BT601_625:
	case MTK_DRM_DATASPACE_STANDARD_BT601_625_UNADJUSTED:
	case MTK_DRM_DATASPACE_STANDARD_BT601_525:
	case MTK_DRM_DATASPACE_STANDARD_BT601_525_UNADJUSTED:
		ret = ((plane_ds & MTK_DRM_DATASPACE_RANGE_MASK) ==
			MTK_DRM_DATASPACE_RANGE_FULL)
			       ? OVL_CON_MTX_JPEG_TO_RGB
			       : OVL_CON_MTX_BT601_TO_RGB;
		break;

	case MTK_DRM_DATASPACE_STANDARD_BT709:
	case MTK_DRM_DATASPACE_STANDARD_DCI_P3:
	case MTK_DRM_DATASPACE_STANDARD_BT2020:
		ret = OVL_CON_MTX_BT709_TO_RGB;
		break;

	case 0:
		switch (plane_ds & 0xffff) {
		case MTK_DRM_DATASPACE_JFIF:
		case MTK_DRM_DATASPACE_BT601_625:
		case MTK_DRM_DATASPACE_BT601_525:
			ret = OVL_CON_MTX_BT601_TO_RGB;
			break;

		case MTK_DRM_DATASPACE_SRGB_LINEAR:
		case MTK_DRM_DATASPACE_SRGB:
		case MTK_DRM_DATASPACE_BT709:
			ret = OVL_CON_MTX_BT709_TO_RGB;
			break;
		}
	}

	if (ret)
		return ret;

	return OVL_CON_MTX_BT601_TO_RGB;
}

/* config addr, pitch, src_size */
static void _ovl_common_config(struct mtk_ddp_comp *comp, unsigned int idx,
			       struct mtk_plane_state *state,
			       struct cmdq_pkt *handle)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	struct mtk_plane_pending_state *pending = &state->pending;
	unsigned int addr = pending->addr;
	unsigned int fmt = pending->format;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int pitch_msb = ((pending->pitch >> 16) & 0xf);
	unsigned int dst_h = pending->height;
	unsigned int dst_w = pending->width;
	unsigned int src_x = pending->src_x;
	unsigned int src_y = pending->src_y;
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int src_size = (dst_h << 16) | dst_w;
	unsigned int offset = 0;
	unsigned int clip = 0;
	unsigned int buf_size = 0;
	int rotate = 0;

	if (fmt == DRM_FORMAT_YUYV || fmt == DRM_FORMAT_YVYU ||
	    fmt == DRM_FORMAT_UYVY || fmt == DRM_FORMAT_VYUY) {
		if (src_x % 2) {
			src_x -= 1;
			dst_w += 1;
			clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT, 1);
		}
		if ((src_x + dst_w) % 2) {
			dst_w += 1;
			clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT, 1);
		}
	}

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif

	if (rotate)
		offset = (src_x + dst_w) * drm_format_plane_cpp(fmt, 0) +
			 (src_y + dst_h - 1) * pitch - 1;
	else
		offset = src_x * drm_format_plane_cpp(fmt, 0) + src_y * pitch;
	addr += offset;

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	src_size = (dst_h << 16) | dst_w;

	buf_size = (dst_h - 1) * pending->pitch +
		dst_w * drm_format_plane_cpp(fmt, 0);
	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_PITCH_MSB(id),
			pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_PITCH(id),
			pitch, ~0);
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		if (comp->mtk_crtc->sec_on) {
			u32 size, meta_type, regs_addr;

			size = buf_size;
			regs_addr = comp->regs_pa +
				DISP_REG_OVL_EL_ADDR(id);
			if (state->pending.is_sec) {
				meta_type = CMDQ_IWC_H_2_MVA;
				cmdq_sec_pkt_write_reg(handle, regs_addr,
					pending->addr, meta_type,
					offset, size, 0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					BIT(id + EXT_SECURE_OFFSET),
					BIT(id + EXT_SECURE_OFFSET));

				DDPDBG("%s:%d, addr:(%pad,0x%x), size:%d\n",
					__func__, __LINE__,
					&pending->addr,
					offset,
					size);
			} else {
				cmdq_pkt_write(handle, comp->cmdq_base,
					regs_addr, addr, ~0);
				DDPDBG("%s:%d, addr:0x%x, size:%d\n",
					__func__, __LINE__,
					addr,
					size);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					0, BIT(id + EXT_SECURE_OFFSET));
			}
		} else  {
#endif

			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_EL_ADDR(id),
				addr, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_SECURE,
				0, BIT(id + EXT_SECURE_OFFSET));
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		}
#endif
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_SRC_SIZE(id),
			src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_EL_CLIP(id), clip,
			~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_PITCH_MSB(lye_idx),
			pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_PITCH(lye_idx),
			pitch, ~0);
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		if (comp->mtk_crtc->sec_on) {
			u32 size, meta_type, regs_addr;

			size = buf_size;
			regs_addr = comp->regs_pa +
				DISP_REG_OVL_ADDR(ovl, lye_idx);
			if (state->pending.is_sec) {
				meta_type = CMDQ_IWC_H_2_MVA;
				cmdq_sec_pkt_write_reg(handle, regs_addr,
					pending->addr, meta_type,
					offset, size, 0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					BIT(lye_idx), BIT(lye_idx));
				DDPDBG("%s:%d, addr:(%pad,0x%x), size:%d\n",
					__func__, __LINE__,
					&pending->addr,
					offset,
					size);
			} else {
				cmdq_pkt_write(handle, comp->cmdq_base,
					regs_addr, addr, ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					0, BIT(lye_idx));
			}
		} else {
#endif
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_ADDR(ovl, lye_idx),
				addr, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_SECURE,
				0, BIT(lye_idx));
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		}
#endif
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_SRC_SIZE(lye_idx),
			src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_CLIP(lye_idx), clip,
			~0);
	}
}

static void mtk_ovl_layer_config(struct mtk_ddp_comp *comp, unsigned int idx,
				 struct mtk_plane_state *state,
				 struct cmdq_pkt *handle)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	struct mtk_plane_pending_state *pending = &state->pending;
	int rotate = 0;
	unsigned int fmt = pending->format;
	unsigned int offset;
	unsigned int con;
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int alpha;
	unsigned int alpha_con;
	unsigned int value = 0, mask = 0, fmt_ex = 0;
	unsigned long long temp_bw;
	unsigned int dim_color;

	/* handle dim layer for compression flag & color dim*/
	if (fmt == DRM_FORMAT_C8) {
		pending->prop_val[PLANE_PROP_COMPRESS] = 0;
		dim_color = pending->prop_val[PLANE_PROP_DIM_COLOR];
	} else {
		dim_color = 0xff000000;
	}

	/* handle buffer de-compression */
	if (ovl->data->compr_info && ovl->data->compr_info->l_config) {
		if (ovl->data->compr_info->l_config(comp,
			    idx, state, handle)) {
			DDPPR_ERR("wrong fbdc input config\n");
			return;
		}
	} else {
		/* Config common register which would be different according
		 * with
		 * this layer is compressed or not, i.e.: addr, pitch...
		 */
		_ovl_common_config(comp, idx, state, handle);
	}

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;
	DDPINFO("%s+ idx:%d, enable:%d, fmt:0x%x\n", __func__, idx,
		pending->enable, pending->format);
	if (!pending->enable)
		mtk_ovl_layer_off(comp, lye_idx, ext_lye_idx, handle);

	mtk_ovl_color_manage(comp, idx, state, handle);

	alpha_con = pending->prop_val[PLANE_PROP_ALPHA_CON];
	alpha = 0xFF & pending->prop_val[PLANE_PROP_PLANE_ALPHA];
	if (alpha == 0xFF &&
	    (fmt == DRM_FORMAT_RGBX8888 || fmt == DRM_FORMAT_BGRX8888 ||
	     fmt == DRM_FORMAT_XRGB8888 || fmt == DRM_FORMAT_XBGR8888))
		alpha_con = 0;

	con = ovl_fmt_convert(ovl, fmt, state->pending.modifier);
	con |= (alpha_con << 8) | alpha;

	if (fmt == DRM_FORMAT_UYVY || fmt == DRM_FORMAT_YUYV) {
		unsigned int prop = pending->prop_val[PLANE_PROP_DATASPACE];

		con |= mtk_ovl_yuv_matrix_convert((enum mtk_drm_dataspace)prop);
	}

	if (!pending->addr)
		con |= BIT(28);

	DDPINFO("%s+ id %d, idx:%d, enable:%d, fmt:0x%x, ",
		__func__, comp->id, idx, pending->enable, pending->format);
	DDPINFO("addr 0x%lx, compr %d, con 0x%x\n",
		pending->addr, pending->prop_val[PLANE_PROP_COMPRESS], con);

	if (rotate) {
		unsigned int bg_w = 0, bg_h = 0;

		_get_bg_roi(comp, &bg_h, &bg_w);
		offset = ((bg_h - pending->height - pending->dst_y) << 16) +
			 (bg_w - pending->width - pending->dst_x);
		DDPINFO("bg(%d,%d) (%d,%d,%dx%d)\n", bg_w, bg_h, pending->dst_x,
			pending->dst_y, pending->width, pending->height);
		con |= (CON_HORI_FLIP + CON_VERTICAL_FLIP);
	} else {
		offset = (pending->dst_y << 16) | pending->dst_x;
	}

	if (fmt == DRM_FORMAT_ABGR2101010)
		fmt_ex = 1;
	else if (fmt == DRM_FORMAT_ABGR16161616F)
		fmt_ex = 3;

	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

		SET_VAL_MASK(value, mask, fmt_ex, FLD_ELn_CLRFMT_NB(id));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_CLRFMT_EXT, value,
			       mask);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_EL_CON(id), con,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_EL_OFFSET(id),
			       offset, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_EL0_CLR(id),
			       dim_color, ~0);
	} else {
		SET_VAL_MASK(value, mask, fmt_ex, FLD_Ln_CLRFMT_NB(lye_idx));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_CLRFMT_EXT, value,
			       mask);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_CON(lye_idx), con,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_OFFSET(lye_idx),
			       offset, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_L0_CLR(lye_idx),
			       dim_color, ~0);
	}

	if (pending->enable) {
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;
		u32 vrefresh;
		u32 ratio_tmp = 0;
		unsigned int vact = 0;
		unsigned int vtotal = 0;
		struct mtk_ddp_comp *output_comp;
		struct drm_display_mode *mode = NULL;

		mtk_crtc = comp->mtk_crtc;
		crtc = &mtk_crtc->base;

		output_comp = mtk_ddp_comp_request_output(comp->mtk_crtc);

		vrefresh = crtc->state->adjusted_mode.vrefresh;

		if (output_comp && ((output_comp->id == DDP_COMPONENT_DSI0) ||
				(output_comp->id == DDP_COMPONENT_DSI1))
				&& !(mtk_dsi_is_cmd_mode(output_comp))) {
			mtk_ddp_comp_io_cmd(output_comp, NULL,
				DSI_GET_MODE_BY_MAX_VREFRESH, &mode);
			vtotal = mode->vtotal;
			vact = mode->vdisplay;
			ratio_tmp = vtotal * 100 / vact;
		} else
			ratio_tmp = 125;

		DDPDBG("%s, vrefresh=%d, ratio_tmp=%d\n",
			__func__, vrefresh, ratio_tmp);
		DDPDBG("%s, vtotal=%d, vact=%d\n",
			__func__, vtotal, vact);

		mtk_ovl_layer_on(comp, lye_idx, ext_lye_idx, handle);
		/*constant color :non RDMA source*/
		/* TODO: cause RPO abnormal */
//		if (!pending->addr)
//			cmdq_pkt_write(handle, comp->cmdq_base,
//		       comp->regs_pa + DISP_REG_OVL_RDMA_CTRL(idx), 0x0, ~0);
		/* TODO: consider FBDC */
		/* SRT BW (one layer) =
		 * layer_w * layer_h * bpp * vrefresh * max fps blanking_ratio
		 * Sum_SRT(all layer) *= 1.33
		 */
		temp_bw = (unsigned long long)pending->width * pending->height;
		temp_bw *= mtk_get_format_bpp(fmt);
		do_div(temp_bw, 1000);
		temp_bw *= ratio_tmp;
		do_div(temp_bw, 100);
		temp_bw = temp_bw * vrefresh;
		do_div(temp_bw, 1000);

		DDPDBG("comp %d bw %llu vtotal:%d vact:%d\n",
			comp->id, temp_bw, vtotal, vact);

		if (pending->prop_val[PLANE_PROP_COMPRESS])
			comp->fbdc_bw += temp_bw;
		else
			comp->qos_bw += temp_bw;

		mtk_dprec_mmp_dump_ovl_layer(state);

	}
}

static bool compr_l_config_PVRIC_V3_1(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle)
{
	/* input config */
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	struct mtk_plane_pending_state *pending = &state->pending;
	unsigned int addr = pending->addr;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int vpitch = pending->prop_val[PLANE_PROP_VPITCH];
	unsigned int dst_h = pending->height;
	unsigned int dst_w = pending->width;
	unsigned int src_x = pending->src_x, src_y = pending->src_y;
	unsigned int src_w = pending->width, src_h = pending->height;
	unsigned int fmt = pending->format;
	unsigned int Bpp = drm_format_plane_cpp(fmt, 0);
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int compress = pending->prop_val[PLANE_PROP_COMPRESS];
	int rotate = 0;

	/* variable to do calculation */
	unsigned int tile_w = 16, tile_h = 4;
	unsigned int tile_body_size = tile_w * tile_h * Bpp;
	unsigned int src_x_align, src_y_align;
	unsigned int src_w_align, src_h_align;
	unsigned int header_offset, tile_offset;
	unsigned int buf_addr;
	unsigned int src_buf_tile_num = 0;
	unsigned int buf_size = 0;
	unsigned int buf_total_size = 0;

	/* variable to config into register */
	unsigned int lx_fbdc_en;
	unsigned int lx_addr, lx_pitch;
	unsigned int lx_hdr_addr, lx_hdr_pitch;
	unsigned int lx_clip, lx_src_size;

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	/* 1. cal & set OVL_LX_FBDC_EN */
	lx_fbdc_en = (compress != 0);
	if (ext_lye_idx != LYE_NORMAL)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON,
			       lx_fbdc_en << (ext_lye_idx + 3),
			       BIT(ext_lye_idx + 3));
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
			       lx_fbdc_en << (lye_idx + 4), BIT(lye_idx + 4));

	/* if no compress, do common config and return */
	if (compress == 0) {
		_ovl_common_config(comp, idx, state, handle);
		return 0;
	}

	/* 2. pre-calculation */
	if (fmt == DRM_FORMAT_RGB888 || fmt == DRM_FORMAT_BGR888) {
		pitch = (4 * pitch / 3);
		Bpp = 4;
	}

	src_buf_tile_num = ALIGN_TO(pitch / 4, tile_w) *
			ALIGN_TO(vpitch, tile_h);
	src_buf_tile_num /= (tile_w * tile_h);
	header_offset = (src_buf_tile_num + 255) / 256 * 128;
	buf_addr = addr + header_offset;

	src_x_align = (src_x / tile_w) * tile_w;
	src_w_align = (1 + (src_x + src_w - 1) / tile_w) * tile_w - src_x_align;
	src_y_align = (src_y / tile_h) * tile_h;
	src_h_align = (1 + (src_y + src_h - 1) / tile_h) * tile_h - src_y_align;

	if (rotate)
		tile_offset = (src_x_align + src_w_align - tile_w) / tile_w +
			      (pitch / tile_w / 4) *
				      (src_y_align + src_h_align - tile_h) /
				      tile_h;
	else
		tile_offset = src_x_align / tile_w +
			      (pitch / tile_w / 4) * src_y_align / tile_h;

	/* 3. cal OVL_LX_ADDR * OVL_LX_PITCH */
	lx_addr = buf_addr + tile_offset * 256;
	lx_pitch = pitch * tile_h;

	/* 4. cal OVL_LX_HDR_ADDR, OVL_LX_HDR_PITCH */
	lx_hdr_addr = buf_addr - (tile_offset / 2) - 1;
	lx_hdr_pitch = (pitch / tile_w / 8) |
		       (((pitch / tile_w / 4) % 2) << 16) |
		       (((tile_offset + 1) % 2) << 20);

	/* 5. calculate OVL_LX_SRC_SIZE */
	lx_src_size = (src_h_align << 16) | src_w_align;

	/* 6. calculate OVL_LX_CLIP */
	lx_clip = 0;
	if (rotate) {
		if (src_x > src_x_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT,
					       src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT,
					       src_x_align + src_w_align -
						       src_x - src_w);
		if (src_y > src_y_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_BOTTOM,
					       src_y - src_y_align);
		if (src_y + src_h < src_y_align + src_h_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_TOP,
					       src_y_align + src_h_align -
						       src_y - src_h);
	} else {
		if (src_x > src_x_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT,
					       src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT,
					       src_x_align + src_w_align -
						       src_x - src_w);
		if (src_y > src_y_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_TOP,
					       src_y - src_y_align);
		if (src_y + src_h < src_y_align + src_h_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_BOTTOM,
					       src_y_align + src_h_align -
						       src_y - src_h);
	}

	/* 7. config register */
	buf_size = (dst_h - 1) * pending->pitch +
		dst_w * drm_format_plane_cpp(fmt, 0);
	buf_total_size = header_offset + src_buf_tile_num * tile_body_size;
	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		if (comp->mtk_crtc->sec_on) {
			u32 size, meta_type, regs_addr;

			regs_addr = comp->regs_pa +
				DISP_REG_OVL_EL_ADDR(id);
			if (state->pending.is_sec) {
				size = buf_size;
				meta_type = CMDQ_IWC_H_2_MVA;
				cmdq_sec_pkt_write_reg(handle, regs_addr,
					pending->addr, meta_type, 0, size, 0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					BIT(id + EXT_SECURE_OFFSET),
					BIT(id + EXT_SECURE_OFFSET));
				DDPDBG("%s:%d, addr:%pad, size:%d\n",
					__func__, __LINE__,
					&pending->addr,
					size);
			} else {
				cmdq_pkt_write(handle, comp->cmdq_base,
					regs_addr, lx_addr, ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					0, BIT(id + EXT_SECURE_OFFSET));
			}
		} else {
#endif
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_EL_ADDR(id),
				lx_addr, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_SECURE,
				0, BIT(id + EXT_SECURE_OFFSET));
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		}
#endif
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa +
				       DISP_REG_OVL_EL_PITCH(id),
			       lx_pitch, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_EL_SRC_SIZE(id),
			       lx_src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa +
				       DISP_REG_OVL_EL_CLIP(id),
			       lx_clip, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ELX_HDR_ADDR(id),
			       lx_hdr_addr, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ELX_HDR_PITCH(id),
			       lx_hdr_pitch, ~0);
	} else {
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		if (comp->mtk_crtc->sec_on) {
			u32 size, meta_type, regs_addr;

			regs_addr = comp->regs_pa +
				DISP_REG_OVL_ADDR(ovl, lye_idx);
			if (state->pending.is_sec) {
				size = buf_size;
				meta_type = CMDQ_IWC_H_2_MVA;
				cmdq_sec_pkt_write_reg(handle, regs_addr,
					pending->addr, meta_type, 0, size, 0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					BIT(lye_idx), BIT(lye_idx));
				DDPDBG("%s:%d, addr:%pad, size:%d\n",
					__func__, __LINE__,
					&pending->addr,
					size);
			} else {
				cmdq_pkt_write(handle, comp->cmdq_base,
					regs_addr, lx_addr, ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					0, BIT(lye_idx));
			}
		} else {
#endif
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_ADDR(ovl, lye_idx),
				lx_addr, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_SECURE,
				0, BIT(lye_idx));
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		}
#endif
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_PITCH(lye_idx),
			       lx_pitch, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SRC_SIZE(lye_idx),
			       lx_src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_CLIP(lye_idx),
			       lx_clip, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa +
				       DISP_REG_OVL_LX_HDR_ADDR(lye_idx),
			       lx_hdr_addr, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa +
				       DISP_REG_OVL_LX_HDR_PITCH(lye_idx),
			       lx_hdr_pitch, ~0);
	}

	return 0;
}

static bool compr_l_config_AFBC_V1_2(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle)
{
	/* input config */
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	struct mtk_plane_pending_state *pending = &state->pending;
	unsigned int addr = pending->addr;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int vpitch = pending->prop_val[PLANE_PROP_VPITCH];
	unsigned int src_x = pending->src_x, src_y = pending->src_y;
	unsigned int src_w = pending->width, src_h = pending->height;
	unsigned int fmt = pending->format;
	unsigned int Bpp = drm_format_plane_cpp(fmt, 0);
	unsigned int lye_idx = 0, ext_lye_idx = 0;
	unsigned int compress = pending->prop_val[PLANE_PROP_COMPRESS];
	int rotate = 0;

	/* variable to do calculation */
	unsigned int tile_w = AFBC_V1_2_TILE_W;
	unsigned int tile_h = AFBC_V1_2_TILE_H;
	unsigned int tile_body_size = tile_w * tile_h * Bpp;
	unsigned int dst_h = pending->height;
	unsigned int dst_w = pending->width;
	unsigned int src_x_align, src_w_align;
	unsigned int src_y_align, src_y_half_align;
	unsigned int src_y_end_align, src_y_end_half_align;
	unsigned int src_h_align, src_h_half_align = 0;
	unsigned int header_offset, tile_offset;
	unsigned int buf_addr;
	unsigned int src_buf_tile_num = 0;
	unsigned int buf_size = 0;
	unsigned int buf_total_size = 0;


	/* variable to config into register */
	unsigned int lx_fbdc_en;
	unsigned int lx_addr, lx_pitch;
	unsigned int lx_hdr_addr, lx_hdr_pitch;
	unsigned int lx_clip, lx_src_size;
	unsigned int lx_2nd_subbuf = 0;
	unsigned int lx_pitch_msb = 0;

	DDPDBG("%s:%d, addr:0x%x, pitch:%d, vpitch:%d\n",
		__func__, __LINE__, addr,
		pitch, vpitch);
	DDPDBG("src:(%d,%d,%d,%d), fmt:%d, Bpp:%d, compress:%d\n",
		src_x, src_y,
		src_w, src_h,
		fmt, Bpp,
		compress);

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
		rotate = 1;
#endif

	if (state->comp_state.comp_id) {
		lye_idx = state->comp_state.lye_id;
		ext_lye_idx = state->comp_state.ext_lye_id;
	} else
		lye_idx = idx;

	/* 1. cal & set OVL_LX_FBDC_EN */
	lx_fbdc_en = (compress != 0);
	if (ext_lye_idx != LYE_NORMAL)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON,
			lx_fbdc_en << (ext_lye_idx + 3),
			BIT(ext_lye_idx + 3));
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
			lx_fbdc_en << (lye_idx + 4), BIT(lye_idx + 4));

	/* if no compress, do common config and return */
	if (compress == 0) {
		_ovl_common_config(comp, idx, state, handle);
		return 0;
	}

	/* 2. pre-calculation */
	src_buf_tile_num = ALIGN_TO(pitch / Bpp, tile_w) *
	    ALIGN_TO(vpitch, tile_h);
	src_buf_tile_num /= (tile_w * tile_h);
	header_offset = ALIGN_TO(
		src_buf_tile_num * AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES,
		AFBC_V1_2_HEADER_ALIGN_BYTES);
	buf_addr = addr + header_offset;

	/* calculate for alignment */
	src_x_align = (src_x / tile_w) * tile_w;
	src_w_align = (1 + (src_x + src_w - 1) / tile_w) * tile_w - src_x_align;

	/* src_y_half_align, src_y_end_half_align,
	 * the start y offset and  stop y offset if half tile align
	 * such as 0 and 3, then the src_h_align is 4
	 */
	src_y_align = (src_y / tile_h) * tile_h;
	src_y_end_align = (1 + (src_y + src_h - 1) / tile_h) * tile_h - 1;
	src_h_align = src_y_end_align - src_y_align + 1;

	src_y_half_align = (src_y / (tile_h >> 1)) * (tile_h >> 1);
	src_y_end_half_align =
		(1 + (src_y + src_h - 1) / (tile_h >> 1)) * (tile_h >> 1) - 1;
	src_h_half_align = src_y_end_half_align - src_y_half_align + 1;

	if (rotate) {
		tile_offset = (src_x_align + src_w_align - tile_w) / tile_w +
			(pitch / tile_w / Bpp) *
			(src_y_align + src_h_align - tile_h) /
			tile_h;
		if (src_y_end_align == src_y_end_half_align)
			lx_2nd_subbuf = 1;
	} else {
		tile_offset = src_x_align / tile_w +
			(pitch / tile_w / Bpp) * src_y_align / tile_h;
		if (src_y_align != src_y_half_align)
			lx_2nd_subbuf = 1;
	}

	/* 3. cal OVL_LX_ADDR * OVL_LX_PITCH */
	lx_addr = buf_addr + tile_offset * tile_body_size;
	lx_pitch = ((pitch * tile_h) & 0xFFFF);
	lx_pitch_msb = (REG_FLD_VAL((L_PITCH_MSB_FLD_YUV_TRANS), (1)) |
		REG_FLD_VAL((L_PITCH_MSB_FLD_2ND_SUBBUF), (lx_2nd_subbuf)) |
		REG_FLD_VAL((L_PITCH_MSB_FLD_SRC_PITCH_MSB),
		((pitch * tile_h) >> 16) & 0xF));

	/* 4. cal OVL_LX_HDR_ADDR, OVL_LX_HDR_PITCH */
	lx_hdr_addr = addr + tile_offset *
	    AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES;
	lx_hdr_pitch = pitch / tile_w / Bpp *
	    AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES;

	/* 5. calculate OVL_LX_SRC_SIZE */
	lx_src_size = (src_h_half_align << 16) | src_w_align;

	/* 6. calculate OVL_LX_CLIP */
	lx_clip = 0;
	if (rotate) {
		if (src_x > src_x_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT,
				src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT,
				src_x_align + src_w_align - src_x - src_w);
		if (src_y > src_y_half_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_BOTTOM,
				src_y - src_y_half_align);
		if (src_y + src_h < src_y_half_align + src_h_half_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_TOP,
				src_y_half_align + src_h_half_align -
				src_y - src_h);
	} else {
		if (src_x > src_x_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_LEFT,
				src_x - src_x_align);
		if (src_x + src_w < src_x_align + src_w_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_RIGHT,
				src_x_align + src_w_align - src_x - src_w);
		if (src_y > src_y_half_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_TOP,
				src_y - src_y_half_align);
		if (src_y + src_h < src_y_half_align + src_h_half_align)
			lx_clip |= REG_FLD_VAL(OVL_L_CLIP_FLD_BOTTOM,
				src_y_half_align + src_h_half_align -
				src_y - src_h);
	}

	/* 7. config register */
	buf_size = (dst_h - 1) * pitch + dst_w * Bpp;
	buf_total_size = header_offset + src_buf_tile_num * tile_body_size;
	if (ext_lye_idx != LYE_NORMAL) {
		unsigned int id = ext_lye_idx - 1;

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		if (comp->mtk_crtc->sec_on) {
			u32 size, meta_type, regs_addr;

			regs_addr = comp->regs_pa +
				DISP_REG_OVL_EL_ADDR(id);
			if (state->pending.is_sec) {
				size = buf_size;
				meta_type = CMDQ_IWC_H_2_MVA;
				cmdq_sec_pkt_write_reg(handle, regs_addr,
					pending->addr, meta_type, 0, size, 0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					BIT(id + EXT_SECURE_OFFSET),
					BIT(id + EXT_SECURE_OFFSET));
				DDPDBG("%s:%d, addr:%pad, size:%d\n",
					__func__, __LINE__,
					&pending->addr,
					size);
			} else {
				cmdq_pkt_write(handle, comp->cmdq_base,
					regs_addr, lx_addr, ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					0, BIT(id + EXT_SECURE_OFFSET));
			}
		} else {
#endif
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_EL_ADDR(id),
				lx_addr, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_SECURE,
				0, BIT(id + EXT_SECURE_OFFSET));
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		}
#endif
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa +
			DISP_REG_OVL_EL_PITCH_MSB(id),
			lx_pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa +
			DISP_REG_OVL_EL_PITCH(id),
			lx_pitch, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa +
			DISP_REG_OVL_EL_SRC_SIZE(id),
			lx_src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa +
			DISP_REG_OVL_EL_CLIP(id),
			lx_clip, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa +
			DISP_REG_OVL_ELX_HDR_ADDR(id),
			lx_hdr_addr, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa +
			DISP_REG_OVL_ELX_HDR_PITCH(id),
			lx_hdr_pitch, ~0);
	} else {
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		if (comp->mtk_crtc->sec_on) {
			u32 size, meta_type, regs_addr;

			regs_addr = comp->regs_pa +
				DISP_REG_OVL_ADDR(ovl, lye_idx);
			if (state->pending.is_sec) {
				size = buf_size;
				meta_type = CMDQ_IWC_H_2_MVA;
				cmdq_sec_pkt_write_reg(handle, regs_addr,
					pending->addr, meta_type, 0, size, 0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					BIT(lye_idx), BIT(lye_idx));
				DDPDBG("%s:%d, addr:%pad, size:%d\n",
					__func__, __LINE__,
					&pending->addr,
					size);
			} else {
				cmdq_pkt_write(handle, comp->cmdq_base,
					regs_addr, lx_addr, ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + OVL_SECURE,
					0, BIT(lye_idx));
			}
		} else {
#endif
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_REG_OVL_ADDR(ovl, lye_idx),
				lx_addr, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + OVL_SECURE,
				0, BIT(lye_idx));
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		}
#endif
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_PITCH_MSB(lye_idx),
			lx_pitch_msb, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_PITCH(lye_idx),
			lx_pitch, 0xffff);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_SRC_SIZE(lye_idx),
			lx_src_size, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_CLIP(lye_idx),
			lx_clip, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_LX_HDR_ADDR(lye_idx),
			lx_hdr_addr, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_LX_HDR_PITCH(lye_idx),
			lx_hdr_pitch, ~0);
	}

	return 0;
}

static int _ovl_UFOd_in(struct mtk_ddp_comp *comp, int connect,
			struct cmdq_pkt *handle)
{
	unsigned int value = 0, mask = 0;

	if (!connect) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SRC_CON, 0, BIT(4));
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_LC_CON, 0, ~0);
		return 0;
	}

	SET_VAL_MASK(value, mask, 2, L_CON_FLD_LSRC);
	SET_VAL_MASK(value, mask, 0, L_CON_FLD_AEN);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_LC_CON, value, mask);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_LC_SRC_SEL, 0, 0x7);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_SRC_CON, 0x10, 0x10);

	return 0;
}

static void
mtk_ovl_addon_rsz_config(struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id prev,
			 enum mtk_ddp_comp_id next, struct mtk_rect rsz_src_roi,
			 struct mtk_rect rsz_dst_roi, struct cmdq_pkt *handle)
{
	if (prev == DDP_COMPONENT_RSZ0 ||
		prev == DDP_COMPONENT_RSZ1) {
		int lc_x = rsz_dst_roi.x, lc_y = rsz_dst_roi.y;
		int lc_w = rsz_dst_roi.width, lc_h = rsz_dst_roi.height;

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
		{
			int bg_w, bg_h;

			_get_bg_roi(comp, &bg_h, &bg_w);
			lc_y = bg_h - lc_h - lc_y;
			lc_x = bg_w - lc_w - lc_x;
		}
#endif
		_ovl_UFOd_in(comp, 1, handle);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_LC_OFFSET,
			       ((lc_y << 16) | lc_x), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_LC_SRC_SIZE,
			       ((lc_h << 16) | lc_w), ~0);
	} else
		_ovl_UFOd_in(comp, 0, handle);

	if (prev == DDP_COMPONENT_OVL0 || prev == DDP_COMPONENT_OVL0_2L ||
		prev == DDP_COMPONENT_OVL1 || prev == DDP_COMPONENT_OVL1_2L)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
			       DISP_OVL_BGCLR_IN_SEL, DISP_OVL_BGCLR_IN_SEL);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DATAPATH_CON, 0,
			       DISP_OVL_BGCLR_IN_SEL);

	if (prev == -1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ROI_SIZE,
			       rsz_src_roi.height << 16 | rsz_src_roi.width,
			       ~0);
		_store_bg_roi(comp, rsz_src_roi.height, rsz_src_roi.width);
	}
}

static void mtk_ovl_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	if ((addon_config->config_type.module == DISP_RSZ ||
		addon_config->config_type.module == DISP_RSZ_v2) &&
		addon_config->config_type.type == ADDON_BETWEEN) {
		struct mtk_addon_rsz_config *config =
			&addon_config->addon_rsz_config;

		mtk_ovl_addon_rsz_config(comp, prev, next, config->rsz_src_roi,
					 config->rsz_dst_roi, handle);
	}
}

static void mtk_ovl_connect(struct mtk_ddp_comp *comp,
			    enum mtk_ddp_comp_id prev,
			    enum mtk_ddp_comp_id next)
{
	if (prev == DDP_COMPONENT_OVL0 || prev == DDP_COMPONENT_OVL0_2L ||
		prev == DDP_COMPONENT_OVL1 || prev == DDP_COMPONENT_OVL1_2L)
		mtk_ddp_cpu_mask_write(comp, DISP_REG_OVL_DATAPATH_CON,
				       DISP_OVL_BGCLR_IN_SEL,
				       DISP_OVL_BGCLR_IN_SEL);
	else
		mtk_ddp_cpu_mask_write(comp, DISP_REG_OVL_DATAPATH_CON,
				       DISP_OVL_BGCLR_IN_SEL, 0);
}

void mtk_ovl_cal_golden_setting(struct mtk_ddp_config *cfg, unsigned int *gs)
{
	bool is_dc = cfg->p_golden_setting_context->is_dc;

	DDPDBG("%s,is_dc:%d\n", __func__, is_dc);

#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
	/* OVL_RDMA_MEM_GMC_SETTING_1 */
	gs[GS_OVL_RDMA_ULTRA_TH] = 0x3ff;
	gs[GS_OVL_RDMA_PRE_ULTRA_TH] = (!is_dc) ? 0x3ff : 0x15e;

	/* OVL_RDMA_FIFO_CTRL */
	gs[GS_OVL_RDMA_FIFO_THRD] = 0;
	gs[GS_OVL_RDMA_FIFO_SIZE] = 384;

	/* OVL_RDMA_MEM_GMC_SETTING_2 */
	gs[GS_OVL_RDMA_ISSUE_REQ_TH] = (!is_dc) ? 255 : 15;
	gs[GS_OVL_RDMA_ISSUE_REQ_TH_URG] = (!is_dc) ? 127 : 15;
	gs[GS_OVL_RDMA_REQ_TH_PRE_ULTRA] = 0;
	gs[GS_OVL_RDMA_REQ_TH_ULTRA] = 1;
	gs[GS_OVL_RDMA_FORCE_REQ_TH] = 0;

	/* OVL_RDMA_GREQ_NUM */
	gs[GS_OVL_RDMA_GREQ_NUM] = (!is_dc) ? 0xF1FF7777 : 0xF1FF0000;

	/* OVL_RDMA_GREQURG_NUM */
	gs[GS_OVL_RDMA_GREQ_URG_NUM] = (!is_dc) ? 0x7777 : 0x0;

	/* OVL_RDMA_ULTRA_SRC */
	gs[GS_OVL_RDMA_ULTRA_SRC] = (!is_dc) ? 0x8040 : 0xA040;

	/* OVL_RDMA_BUF_LOW_TH */
	gs[GS_OVL_RDMA_ULTRA_LOW_TH] = 0;
	gs[GS_OVL_RDMA_PRE_ULTRA_LOW_TH] = (!is_dc) ?
				0 : (gs[GS_OVL_RDMA_FIFO_SIZE] / 8);

	/* OVL_RDMA_BUF_HIGH_TH */
	gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_TH] = (!is_dc) ?
				0 : (gs[GS_OVL_RDMA_FIFO_SIZE] * 6 / 8);
	gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS] = 1;

	/* OVL_EN */
	gs[GS_OVL_BLOCK_EXT_ULTRA] = (!is_dc) ? 0 : 1;
	gs[GS_OVL_BLOCK_EXT_PRE_ULTRA] = (!is_dc) ? 0 : 1;
#endif

#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
	/* OVL_RDMA_MEM_GMC_SETTING_1 */
	gs[GS_OVL_RDMA_ULTRA_TH] = 0x3ff;
	gs[GS_OVL_RDMA_PRE_ULTRA_TH] = (!is_dc) ? 0x3ff : 0xe0;

	/* OVL_RDMA_FIFO_CTRL */
	gs[GS_OVL_RDMA_FIFO_THRD] = 0;
	gs[GS_OVL_RDMA_FIFO_SIZE] = 288;

	/* OVL_RDMA_MEM_GMC_SETTING_2 */
	gs[GS_OVL_RDMA_ISSUE_REQ_TH] = (!is_dc) ? 191 : 15;
	gs[GS_OVL_RDMA_ISSUE_REQ_TH_URG] = (!is_dc) ? 95 : 15;
	gs[GS_OVL_RDMA_REQ_TH_PRE_ULTRA] = 0;
	gs[GS_OVL_RDMA_REQ_TH_ULTRA] = 1;
	gs[GS_OVL_RDMA_FORCE_REQ_TH] = 0;

	/* OVL_RDMA_GREQ_NUM */
	gs[GS_OVL_RDMA_GREQ_NUM] = (!is_dc) ? 0xF1FF5555 : 0xF1FF0000;

	/* OVL_RDMA_GREQURG_NUM */
	gs[GS_OVL_RDMA_GREQ_URG_NUM] = (!is_dc) ? 0x5555 : 0x0;

	/* OVL_RDMA_ULTRA_SRC */
	gs[GS_OVL_RDMA_ULTRA_SRC] = (!is_dc) ? 0x8040 : 0xA040;

	/* OVL_RDMA_BUF_LOW_TH */
	gs[GS_OVL_RDMA_ULTRA_LOW_TH] = 0;
	gs[GS_OVL_RDMA_PRE_ULTRA_LOW_TH] = (!is_dc) ? 0 : 24;

	/* OVL_RDMA_BUF_HIGH_TH */
	gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_TH] = (!is_dc) ?
				0 : (gs[GS_OVL_RDMA_FIFO_SIZE] * 6 / 8);
	gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS] = 1;

	/* OVL_EN */
	gs[GS_OVL_BLOCK_EXT_ULTRA] = (!is_dc) ? 0 : 1;
	gs[GS_OVL_BLOCK_EXT_PRE_ULTRA] = (!is_dc) ? 0 : 1;
#endif
}

static int mtk_ovl_golden_setting(struct mtk_ddp_comp *comp,
				  struct mtk_ddp_config *cfg,
				  struct cmdq_pkt *handle)
{
	unsigned long baddr = comp->regs_pa;
	unsigned int regval;
	unsigned int gs[GS_OVL_FLD_NUM];
	int i, layer_num;
	unsigned long Lx_base;

	layer_num = mtk_ovl_layer_num(comp);

	/* calculate ovl golden setting */
	mtk_ovl_cal_golden_setting(cfg, gs);

	/* OVL_RDMA_MEM_GMC_SETTING_1 */
	regval =
		gs[GS_OVL_RDMA_ULTRA_TH] + (gs[GS_OVL_RDMA_PRE_ULTRA_TH] << 16);
	for (i = 0; i < layer_num; i++) {
		Lx_base = i * OVL_LAYER_OFFSET + baddr;

		cmdq_pkt_write(handle, comp->cmdq_base,
			       Lx_base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING,
			       regval, ~0);
	}

	/* OVL_RDMA_FIFO_CTRL */
	regval = gs[GS_OVL_RDMA_FIFO_THRD] + (gs[GS_OVL_RDMA_FIFO_SIZE] << 16);
	for (i = 0; i < layer_num; i++) {
		Lx_base = i * OVL_LAYER_OFFSET + baddr;

		cmdq_pkt_write(handle, comp->cmdq_base,
			       Lx_base + DISP_REG_OVL_RDMA0_FIFO_CTRL, regval,
			       ~0);
	}

	/* OVL_RDMA_MEM_GMC_SETTING_2 */
	regval = gs[GS_OVL_RDMA_ISSUE_REQ_TH] +
		 (gs[GS_OVL_RDMA_ISSUE_REQ_TH_URG] << 16) +
		 (gs[GS_OVL_RDMA_REQ_TH_PRE_ULTRA] << 28) +
		 (gs[GS_OVL_RDMA_REQ_TH_ULTRA] << 29) +
		 (gs[GS_OVL_RDMA_FORCE_REQ_TH] << 30);
	for (i = 0; i < layer_num; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       baddr + DISP_REG_OVL_RDMA0_MEM_GMC_S2 + i * 4,
			       regval, ~0);

	/* DISP_REG_OVL_RDMA_GREQ_NUM */
	regval = gs[GS_OVL_RDMA_GREQ_NUM];
	cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + DISP_REG_OVL_RDMA_GREQ_NUM, regval, ~0);

	/* DISP_REG_OVL_RDMA_GREQ_URG_NUM */
	regval = gs[GS_OVL_RDMA_GREQ_URG_NUM];
	cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + DISP_REG_OVL_RDMA_GREQ_URG_NUM, regval, ~0);

	/* DISP_REG_OVL_RDMA_ULTRA_SRC */
	regval = gs[GS_OVL_RDMA_ULTRA_SRC];
	cmdq_pkt_write(handle, comp->cmdq_base,
		       baddr + DISP_REG_OVL_RDMA_ULTRA_SRC, regval, ~0);

	/* DISP_REG_OVL_RDMAn_BUF_LOW */
	regval = gs[GS_OVL_RDMA_ULTRA_LOW_TH] +
		 (gs[GS_OVL_RDMA_PRE_ULTRA_LOW_TH] << 12);

	for (i = 0; i < layer_num; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       baddr + DISP_REG_OVL_RDMAn_BUF_LOW(i), regval,
			       ~0);

	/* DISP_REG_OVL_RDMAn_BUF_HIGH */
	regval = (gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_TH] << 12) +
		 (gs[GS_OVL_RDMA_PRE_ULTRA_HIGH_DIS] << 31);

	for (i = 0; i < layer_num; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       baddr + DISP_REG_OVL_RDMAn_BUF_HIGH(i), regval,
			       ~0);

	/* OVL_EN */
	regval = (gs[GS_OVL_BLOCK_EXT_ULTRA] << 18) +
		 (gs[GS_OVL_BLOCK_EXT_PRE_ULTRA] << 19);
	cmdq_pkt_write(handle, comp->cmdq_base, baddr + DISP_REG_OVL_EN,
		       regval, 0x3 << 18);

	return 0;
}

static void mtk_ovl_all_layer_off(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int keep_first_layer)
{
	int i;
	unsigned int phy_layer0_on = 0;

	/* In 6779 we need to set DISP_OVL_FORCE_RELAY_MODE */

	/* To make sure the OVL_SRC_CON register keep the same value
	 * as readl while writing the new value in GCE. This function should
	 * only used in driver probe.
	 */
	if (keep_first_layer)
		phy_layer0_on = readl(comp->regs + DISP_REG_OVL_SRC_CON) & 0x1;
	if (phy_layer0_on)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SRC_CON, 0x1, ~0);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_SRC_CON,
			       DISP_OVL_FORCE_RELAY_MODE, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON, 0, ~0);

	for (i = phy_layer0_on ? 1 : 0; i < OVL_PHY_LAYER_NR; i++)
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_RDMA_CTRL(i), 0,
			       ~0);
}

static int mtk_ovl_replace_bootup_mva(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle, void *params,
				      struct mtk_ddp_fb_info *fb_info)
{
	unsigned int src_on = readl(comp->regs + DISP_REG_OVL_SRC_CON);
	unsigned int layer_addr, layer_mva;
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	if (src_on & 0x1) {
		layer_addr = readl(comp->regs + DISP_REG_OVL_ADDR(ovl, 0));
		layer_mva = layer_addr - fb_info->fb_pa + fb_info->fb_mva;
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ADDR(ovl, 0),
			       layer_mva, ~0);
	}

	if (src_on & 0x2) {
		layer_addr = readl(comp->regs + DISP_REG_OVL_ADDR(ovl, 1));
		layer_mva = layer_addr - fb_info->fb_pa + fb_info->fb_mva;
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ADDR(ovl, 1),
			       layer_mva, ~0);
	}

	if (src_on & 0x4) {
		layer_addr = readl(comp->regs + DISP_REG_OVL_ADDR(ovl, 2));
		layer_mva = layer_addr - fb_info->fb_pa + fb_info->fb_mva;
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ADDR(ovl, 2),
			       layer_mva, ~0);
	}

	if (src_on & 0x8) {
		layer_addr = readl(comp->regs + DISP_REG_OVL_ADDR(ovl, 3));
		layer_mva = layer_addr - fb_info->fb_pa + fb_info->fb_mva;
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_ADDR(ovl, 3),
			       layer_mva, ~0);
	}

	return 0;
}

static void mtk_ovl_backup_info_cmp(struct mtk_ddp_comp *comp, bool *compare)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
	void __iomem *baddr = comp->regs, *Lx_base = NULL;
	int i = 0;
	unsigned int src_on = readl(DISP_REG_OVL_SRC_CON + baddr);
	struct mtk_ovl_backup_info cur_info[MAX_LAYER_NUM];

	memset(cur_info, 0, sizeof(cur_info));
	for (i = 0; i < mtk_ovl_layer_num(comp); i++) {
		unsigned int val = 0;

		Lx_base = i * OVL_LAYER_OFFSET + baddr;
		cur_info[i].layer = i;
		cur_info[i].layer_en = src_on & (0x1 << i);
		if (!cur_info[i].layer_en) {
			DDPMSG("%s:layer%d,en %d,size 0x%x,addr %lu\n",
			       __func__, i, cur_info[i].layer_en,
			       cur_info[i].src_size, cur_info[i].addr);
			continue;
		}

		cur_info[i].con = readl(DISP_REG_OVL_CON(i) + baddr);
		cur_info[i].addr = readl(DISP_REG_OVL_ADDR(ovl, i) + baddr);
		cur_info[i].src_size =
			readl(DISP_REG_OVL_L0_SRC_SIZE + Lx_base);

		val = readl(DISP_REG_OVL_L0_PITCH + Lx_base);
		cur_info[i].src_pitch =
			REG_FLD_VAL_GET(L_PITCH_FLD_SRC_PITCH, val);

		val = readl(DISP_REG_OVL_DATAPATH_CON + Lx_base);
		cur_info[i].data_path_con =
			readl(DISP_REG_OVL_DATAPATH_CON + Lx_base);

		DDPMSG("%s:layer%d,en %d,size 0x%x, addr %lu\n", __func__, i,
		       cur_info[i].layer_en, cur_info[i].src_size,
		       cur_info[i].addr);
		if (memcmp(&cur_info[i], &ovl->backup_info[i],
			   sizeof(struct mtk_ovl_backup_info)) != 0)
			*compare = true;
	}
	memcpy(ovl->backup_info, cur_info,
	       sizeof(struct mtk_ovl_backup_info) * MAX_LAYER_NUM);
}

static int mtk_ovl_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = 0;

	switch (io_cmd) {
	case MTK_IO_CMD_OVL_GOLDEN_SETTING: {
		struct mtk_ddp_config *cfg;

		cfg = (struct mtk_ddp_config *)params;
		mtk_ovl_golden_setting(comp, cfg, handle);
		break;
	}
	case OVL_ALL_LAYER_OFF:
	{
		int *keep_first_layer = params;

		mtk_ovl_all_layer_off(comp, handle, *keep_first_layer);
		break;
	}
	case IRQ_LEVEL_ALL: {
		unsigned int inten;

		inten = REG_FLD_VAL(INTEN_FLD_RDMA0_EOF_ABNORMAL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_RDMA1_EOF_ABNORMAL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_RDMA2_EOF_ABNORMAL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_RDMA3_EOF_ABNORMAL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_ABNORMAL_SOF, 1) |
			REG_FLD_VAL(INTEN_FLD_START_INTEN, 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_INTEN, inten,
			       inten);
		break;
	}
	case IRQ_LEVEL_IDLE: {
		unsigned int inten;

		inten = REG_FLD_VAL(INTEN_FLD_REG_CMT_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_FME_CPL_INTEN, 1) |
			REG_FLD_VAL(INTEN_FLD_START_INTEN, 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_INTEN, 0, inten);
		break;
	}
#ifdef MTK_FB_MMDVFS_SUPPORT
	case PMQOS_SET_BW: {
		struct mtk_drm_crtc *mtk_crtc;
		struct cmdq_pkt_buffer *cmdq_buf;
		u32 ovl_bw, slot_num;

		mtk_crtc = comp->mtk_crtc;
		cmdq_buf = &(mtk_crtc->gce_obj.buf);

		/* process FBDC */
		slot_num = __mtk_disp_pmqos_slot_look_up(comp->id,
					    DISP_BW_FBDC_MODE);
		ovl_bw = *(unsigned int *)(cmdq_buf->va_base +
					    DISP_SLOT_PMQOS_BW(slot_num));

		__mtk_disp_set_module_bw(&comp->fbdc_qos_req, comp->id, ovl_bw,
					    DISP_BW_FBDC_MODE);

		/* process normal */
		slot_num = __mtk_disp_pmqos_slot_look_up(comp->id,
					    DISP_BW_NORMAL_MODE);
		ovl_bw = *(unsigned int *)(cmdq_buf->va_base +
					    DISP_SLOT_PMQOS_BW(slot_num));

		__mtk_disp_set_module_bw(&comp->qos_req, comp->id, ovl_bw,
					    DISP_BW_NORMAL_MODE);
		break;
	}
	case PMQOS_SET_HRT_BW: {
		u32 bw_val = *(unsigned int *)params;

		__mtk_disp_set_module_hrt(&comp->hrt_qos_req, bw_val);

		ret = OVL_REQ_HRT;
		break;
	}
	case PMQOS_UPDATE_BW: {
		struct drm_crtc *crtc;
		struct mtk_drm_crtc *mtk_crtc;
		struct cmdq_pkt_buffer *cmdq_buf;
		u32 slot_num;

		mtk_crtc = comp->mtk_crtc;
		crtc = &mtk_crtc->base;
		cmdq_buf = &(mtk_crtc->gce_obj.buf);

		/* process FBDC */
		slot_num = __mtk_disp_pmqos_slot_look_up(comp->id,
					DISP_BW_FBDC_MODE);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       cmdq_buf->pa_base + DISP_SLOT_PMQOS_BW(slot_num),
			       comp->fbdc_bw, ~0);

		/* process normal */
		slot_num = __mtk_disp_pmqos_slot_look_up(comp->id,
					DISP_BW_NORMAL_MODE);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       cmdq_buf->pa_base + DISP_SLOT_PMQOS_BW(slot_num),
			       comp->qos_bw, ~0);

		DDPDBG("update ovl fbdc_bw to %u, qos bw to %u\n",
			comp->fbdc_bw, comp->qos_bw);
		break;
	}
#endif
	case OVL_REPLACE_BOOTUP_MVA: {
		struct mtk_ddp_fb_info *fb_info =
			(struct mtk_ddp_fb_info *)params;

		mtk_ovl_replace_bootup_mva(comp, handle, params, fb_info);
		break;
	}
	case BACKUP_INFO_CMP: {
		mtk_ovl_backup_info_cmp(comp, params);
		break;
	}
	case BACKUP_OVL_STATUS: {
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct cmdq_pkt_buffer *cmdq_buf = &(mtk_crtc->gce_obj.buf);
		dma_addr_t slot = cmdq_buf->pa_base + DISP_SLOT_OVL_STATUS;

		cmdq_pkt_mem_move(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_OVL_STA,
			slot, CMDQ_THR_SPR_IDX3);
		break;
	}
	default:
		break;
	}

	return ret;
}

void mtk_ovl_dump_golden_setting(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned long rg0 = 0, rg1 = 0, rg2 = 0, rg3 = 0, rg4 = 0;
	int i = 0;
	unsigned int value;

	DDPDUMP("-- %s Golden Setting --\n", mtk_dump_comp_str(comp));
	for (i = 0; i < mtk_ovl_layer_num(comp); i++) {
		rg0 = DISP_REG_OVL_RDMA0_MEM_GMC_SETTING
			+ i * OVL_LAYER_OFFSET;
		rg1 = DISP_REG_OVL_RDMA0_FIFO_CTRL + i * OVL_LAYER_OFFSET;
		rg2 = DISP_REG_OVL_RDMA0_MEM_GMC_S2 + i * 0x4;
		rg3 = DISP_REG_OVL_RDMAn_BUF_LOW(i);
		rg4 = DISP_REG_OVL_RDMAn_BUF_HIGH(i);
		DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
			rg0, readl(rg0 + baddr), rg1, readl(rg1 + baddr),
			rg2, readl(rg2 + baddr));
		DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x\n",
			rg3, readl(rg3 + baddr),
			rg4, readl(rg4 + baddr));
	}

	rg0 = DISP_REG_OVL_RDMA_BURST_CON1;
	DDPDUMP("0x%03lx:0x%08x\n", rg0, readl(rg0 + baddr));

	rg0 = DISP_REG_OVL_RDMA_GREQ_NUM;
	rg1 = DISP_REG_OVL_RDMA_GREQ_URG_NUM;
	rg2 = DISP_REG_OVL_RDMA_ULTRA_SRC;
	DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
		rg0, readl(rg0 + baddr),
		rg1, readl(rg1 + baddr),
		rg2, readl(rg2 + baddr));

	rg0 = DISP_REG_OVL_EN;
	rg1 = DISP_REG_OVL_DATAPATH_CON;
	rg2 = DISP_REG_OVL_FBDC_CFG1;
	DDPDUMP("0x%03lx:0x%08x 0x%03lx:0x%08x 0x%03lx:0x%08x\n",
		rg0, readl(rg0 + baddr),
		rg1, readl(rg1 + baddr),
		rg2, readl(rg2 + baddr));

	value = readl(DISP_REG_OVL_RDMA0_MEM_GMC_SETTING + baddr);
	DDPDUMP("RDMA0_MEM_GMC_SETTING1\n");
	DDPDUMP("[9:0]:%x [25:16]:%x [28]:%x [31]:%x\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD, value),
		REG_FLD_VAL_GET(
			FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD, value),
		REG_FLD_VAL_GET(
			FLD_OVL_RDMA_MEM_GMC_ULTRA_THRESHOLD_HIGH_OFS, value),
		REG_FLD_VAL_GET(
			FLD_OVL_RDMA_MEM_GMC_PRE_ULTRA_THRESHOLD_HIGH_OFS,
			value));

	value = readl(DISP_REG_OVL_RDMA0_FIFO_CTRL + baddr);
	DDPDUMP("RDMA0_FIFO_CTRL\n");
	DDPDUMP("[9:0]:%u [25:16]:%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_FIFO_THRD, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_FIFO_SIZE, value));

	value = readl(DISP_REG_OVL_RDMA0_MEM_GMC_S2 + baddr);
	DDPDUMP("RDMA0_MEM_GMC_SETTING2\n");
	DDPDUMP("[11:0]:%u [27:16]:%u [28]:%u [29]:%u [30]:%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES, value),
		REG_FLD_VAL_GET(
			FLD_OVL_RDMA_MEM_GMC2_ISSUE_REQ_THRES_URG, value),
		REG_FLD_VAL_GET(
			FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_PREULTRA, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_MEM_GMC2_REQ_THRES_ULTRA, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_MEM_GMC2_FORCE_REQ_THRES, value));

	value = readl(DISP_REG_OVL_RDMA_BURST_CON1 + baddr);
	DDPDUMP("OVL_RDMA_BURST_CON1\n");
	DDPDUMP("[28]:%u\n",
		REG_FLD_VAL_GET(FLD_RDMA_BURST_CON1_BURST16_EN, value));

	value = readl(DISP_REG_OVL_RDMA_GREQ_NUM + baddr);
	DDPDUMP("RDMA_GREQ_NUM\n");
	DDPDUMP("[3:0]%u [7:4]%u [11:8]%u [15:12]%u [23:16]%x [26:24]%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER0_GREQ_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER1_GREQ_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER2_GREQ_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER3_GREQ_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_OSTD_GREQ_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_GREQ_DIS_CNT, value));
	DDPDUMP("[27]%u [28]%u [29]%u [30]%u [31]%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_STOP_EN, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_GRP_END_STOP, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_GRP_BRK_STOP, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_PREULTRA, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_IOBUF_FLUSH_ULTRA, value));

	value = readl(DISP_REG_OVL_RDMA_GREQ_URG_NUM + baddr);
	DDPDUMP("RDMA_GREQ_URG_NUM\n");
	DDPDUMP("[3:0]:%u [7:4]:%u [11:8]:%u [15:12]:%u [25:16]:%u [28]:%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER0_GREQ_URG_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER1_GREQ_URG_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER2_GREQ_URG_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_LAYER3_GREQ_URG_NUM, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_ARG_GREQ_URG_TH, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_GREQ_ARG_URG_BIAS, value));

	value = readl(DISP_REG_OVL_RDMA_ULTRA_SRC + baddr);
	DDPDUMP("RDMA_ULTRA_SRC\n");
	DDPDUMP("[1:0]%u [3:2]%u [5:4]%u [7:6]%u [9:8]%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_PREULTRA_BUF_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_PREULTRA_SMI_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_PREULTRA_ROI_END_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_PREULTRA_RDMA_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_ULTRA_BUF_SRC, value));
	DDPDUMP("[11:10]%u [13:12]%u [15:14]%u\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_ULTRA_SMI_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_ULTRA_ROI_END_SRC, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_ULTRA_RDMA_SRC, value));

	value = readl(DISP_REG_OVL_RDMAn_BUF_LOW(0) + baddr);
	DDPDUMP("RDMA0_BUF_LOW\n");
	DDPDUMP("[11:0]:%x [23:12]:%x\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_BUF_LOW_ULTRA_TH, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_BUF_LOW_PREULTRA_TH, value));

	value = readl(DISP_REG_OVL_RDMAn_BUF_HIGH(0) + baddr);
	DDPDUMP("RDMA0_BUF_HIGH\n");
	DDPDUMP("[23:12]:%x [31]:%x\n",
		REG_FLD_VAL_GET(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_TH, value),
		REG_FLD_VAL_GET(FLD_OVL_RDMA_BUF_HIGH_PREULTRA_DIS, value));

	value = readl(DISP_REG_OVL_EN + baddr);
	DDPDUMP("OVL_EN\n");
	DDPDUMP("[18]:%x [19]:%x\n",
		REG_FLD_VAL_GET(EN_FLD_BLOCK_EXT_ULTRA, value),
		REG_FLD_VAL_GET(EN_FLD_BLOCK_EXT_PREULTRA, value));

	value = readl(DISP_REG_OVL_DATAPATH_CON + baddr);
	DDPDUMP("DATAPATH_CON\n");
	DDPDUMP("[0]:%u [24]:%u [25]:%u [26]:%u\n",
		REG_FLD_VAL_GET(DATAPATH_CON_FLD_LAYER_SMI_ID_EN, value),
		REG_FLD_VAL_GET(DATAPATH_CON_FLD_GCLAST_EN, value),
		REG_FLD_VAL_GET(DATAPATH_CON_FLD_HDR_GCLAST_EN, value),
		REG_FLD_VAL_GET(DATAPATH_CON_FLD_OUTPUT_CLAMP, value));

	value = readl(DISP_REG_OVL_FBDC_CFG1 + baddr);
	DDPDUMP("OVL_FBDC_CFG1\n");
	DDPDUMP("[24]:%u [28]:%u\n",
		REG_FLD_VAL_GET(FLD_FBDC_8XE_MODE, value),
		REG_FLD_VAL_GET(FLD_FBDC_FILTER_EN, value));
}

int mtk_ovl_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i;

	if (comp->blank_mode)
		return 0;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	if (mtk_ddp_comp_helper_get_opt(comp,
					MTK_DRM_OPT_REG_PARSER_RAW_DUMP)) {
		unsigned int i = 0;

		for (i = 0; i < 0x8e0; i += 0x10)
			mtk_serial_dump_reg(baddr, i, 4);

		/* ADDR */
		mtk_cust_dump_reg(baddr, 0xF40, 0xF60, 0xF80, 0xFA0);
		mtk_cust_dump_reg(baddr, 0xFB0, 0xFB4, 0xFB8, -1);

		/* HDR_ADDR, HDR_PITCH */
		mtk_cust_dump_reg(baddr, 0xF44, 0xF48, 0xF64, 0xF68);
		mtk_cust_dump_reg(baddr, 0xF84, 0xF88, 0xFA4, 0xFA8);
		mtk_cust_dump_reg(baddr, 0xFD0, 0xFD4, 0xFD8, 0xFDC);
		mtk_cust_dump_reg(baddr, 0xFE0, 0xFE4, -1, -1);
	} else {
		/* STA, INTEN, INTSTA, EN*/
		mtk_serial_dump_reg(baddr, 0x0, 4);

		/* TRIG, RST */
		mtk_serial_dump_reg(baddr, 0x10, 2);

		/* ROI_SIZE, DATAPATH_CON, OVL_ROI_BGCLR, OVL_SRC_CON */
		mtk_serial_dump_reg(baddr, 0x20, 4);

		/* LX_CON... */
		for (i = 0; i < 4; i++) {
			mtk_serial_dump_reg(baddr, 0x30 + i * 0x20, 4);
			mtk_serial_dump_reg(baddr, 0x40 + i * 0x20, 4);
		}
		mtk_cust_dump_reg(baddr, 0xF40, 0xF60, 0xF80, 0xFA0);

		/* RDMAX_CON... */
		for (i = 0; i < 4; i++) {
			unsigned int off = 0xC0 + 0x20 * i;

			mtk_cust_dump_reg(baddr, off, off + 0x8, off + 0xC,
					  off + 0x10);
		}

		mtk_serial_dump_reg(baddr, 0x200, 4);
		mtk_serial_dump_reg(baddr, 0x230, 4);
		/* LC_CON */
		mtk_serial_dump_reg(baddr, 0x280, 4);

		mtk_serial_dump_reg(baddr, 0x2a0, 2);

		/* WCG */
		mtk_serial_dump_reg(baddr, 0x2D8, 2);

		/* DATAPATH_EXT_CON */
		mtk_serial_dump_reg(baddr, 0x324, 1);

		/* OVL_ELX_CON */
		for (i = 0; i < 3; i++) {
			mtk_serial_dump_reg(baddr, 0x330 + i * 0x20, 4);
			mtk_serial_dump_reg(baddr, 0x340 + i * 0x20, 4);
		}
		mtk_cust_dump_reg(baddr, 0xFB0, 0xFB4, 0xFB8, -1);

		/* SBCH */
		mtk_serial_dump_reg(baddr, 0x3A0, 3);

		/* WCG */
		for (i = 0 ; i < 4 ; i++) {
			mtk_serial_dump_reg(baddr, 0x500 + i * 40, 4);
			mtk_serial_dump_reg(baddr, 0x510 + i * 40, 4);
			mtk_serial_dump_reg(baddr, 0x520 + i * 40, 1);
		}

		for (i = 0 ; i < 3 ; i++) {
			mtk_serial_dump_reg(baddr, 0x600 + i * 40, 4);
			mtk_serial_dump_reg(baddr, 0x610 + i * 40, 4);
			mtk_serial_dump_reg(baddr, 0x620 + i * 40, 1);
		}

		/* FBDC */
		mtk_serial_dump_reg(baddr, 0x800, 3);
		for (i = 0; i < 4; i++)
			mtk_serial_dump_reg(baddr, 0xF44 + i * 0x20, 2);
		for (i = 0; i < 3; i++)
			mtk_serial_dump_reg(baddr, 0xFD0 + i * 0x8, 2);
	}
	/* For debug MPU violation issue */
	mtk_cust_dump_reg(baddr, 0xFC0, 0xFC4, 0xFC8, -1);

	mtk_ovl_dump_golden_setting(comp);

	return 0;
}

static void ovl_printf_status(unsigned int status)
{
	DDPDUMP("- OVL_FLOW_CONTROL_DEBUG -\n");
	DDPDUMP("addcon_idle:%d,blend_idle:%d\n",
		(status >> 10) & (0x1), (status >> 11) & (0x1));
	DDPDUMP("out_valid:%d,out_ready:%d,out_idle:%d\n",
		(status >> 12) & (0x1), (status >> 13) & (0x1),
		(status >> 15) & (0x1));
	DDPDUMP("rdma_idle3-0:(%d,%d,%d,%d),rst:%d\n", (status >> 16) & (0x1),
		(status >> 17) & (0x1), (status >> 18) & (0x1),
		(status >> 19) & (0x1), (status >> 20) & (0x1));
	DDPDUMP("trig:%d,frame_hwrst_done:%d\n",
		(status >> 21) & (0x1), (status >> 23) & (0x1));
	DDPDUMP("frame_swrst_done:%d,frame_underrun:%d,frame_done:%d\n",
		(status >> 24) & (0x1), (status >> 25) & (0x1),
		(status >> 26) & (0x1));
	DDPDUMP("ovl_running:%d,ovl_start:%d,ovl_clr:%d\n",
		(status >> 27) & (0x1), (status >> 28) & (0x1),
		(status >> 29) & (0x1));
	DDPDUMP("reg_update:%d,ovl_upd_reg:%d\n",
		(status >> 30) & (0x1),
		(status >> 31) & (0x1));

	DDPDUMP("ovl_fms_state:\n");
	switch (status & 0x3ff) {
	case 0x1:
		DDPDUMP("idle\n");
		break;
	case 0x2:
		DDPDUMP("wait_SOF\n");
		break;
	case 0x4:
		DDPDUMP("prepare\n");
		break;
	case 0x8:
		DDPDUMP("reg_update\n");
		break;
	case 0x10:
		DDPDUMP("eng_clr(internal reset)\n");
		break;
	case 0x20:
		DDPDUMP("eng_act(processing)\n");
		break;
	case 0x40:
		DDPDUMP("h_wait_w_rst\n");
		break;
	case 0x80:
		DDPDUMP("s_wait_w_rst\n");
		break;
	case 0x100:
		DDPDUMP("h_w_rst\n");
		break;
	case 0x200:
		DDPDUMP("s_w_rst\n");
		break;
	default:
		DDPDUMP("ovl_fsm_unknown\n");
		break;
	}
}

static void ovl_print_ovl_rdma_status(unsigned int status)
{
	DDPDUMP("warm_rst_cs:%d,layer_greq:%d,out_data:0x%x\n", status & 0x7,
		(status >> 3) & 0x1, (status >> 4) & 0xffffff);
	DDPDUMP("out_ready:%d,out_valid:%d,smi_busy:%d,smi_greq:%d\n",
		(status >> 28) & 0x1, (status >> 29) & 0x1,
		(status >> 30) & 0x1, (status >> 31) & 0x1);
}

static void ovl_dump_layer_info_compress(struct mtk_ddp_comp *comp, int layer,
					 bool is_ext_layer)
{
	unsigned int compr_en = 0, pitch_msb;
	void __iomem *baddr = comp->regs;
	void __iomem *Lx_PVRIC_hdr_base;

	if (is_ext_layer) {
		compr_en = DISP_REG_GET_FIELD(
			REG_FLD(1, layer + 4),
			baddr + DISP_REG_OVL_DATAPATH_EXT_CON);
		Lx_PVRIC_hdr_base = baddr +
				    layer * (DISP_REG_OVL_EL1_HDR_ADDR -
					     DISP_REG_OVL_EL0_HDR_ADDR);
		Lx_PVRIC_hdr_base +=
			(DISP_REG_OVL_EL0_HDR_ADDR - DISP_REG_OVL_L0_HDR_ADDR);
		pitch_msb = readl(baddr + DISP_REG_OVL_EL_PITCH_MSB(layer));
	} else {
		compr_en =
			DISP_REG_GET_FIELD(REG_FLD(1, layer + 4),
					   baddr + DISP_REG_OVL_DATAPATH_CON);
		Lx_PVRIC_hdr_base = baddr +
				    layer * (DISP_REG_OVL_L1_HDR_ADDR -
					     DISP_REG_OVL_L0_HDR_ADDR);
		pitch_msb = readl(baddr + DISP_REG_OVL_PITCH_MSB(layer));
	}

	if (compr_en == 0) {
		DDPDUMP("compr_en:%u\n", compr_en);
		return;
	}

	DDPDUMP("compr_en:%u, pitch_msb:0x%x, hdr_addr:0x%x, hdr_pitch:0x%x\n",
		compr_en, pitch_msb,
		readl(DISP_REG_OVL_L0_HDR_ADDR + Lx_PVRIC_hdr_base),
		readl(DISP_REG_OVL_L0_HDR_PITCH + Lx_PVRIC_hdr_base));
}

static void ovl_dump_layer_info(struct mtk_ddp_comp *comp, int layer,
				bool is_ext_layer)
{
	unsigned int con, src_size, offset, pitch, addr, clip;
	/*  enum UNIFIED_COLOR_FMT fmt; */
	void __iomem *baddr = comp->regs;
	void __iomem *Lx_base;
	void __iomem *Lx_addr_base;

	if (is_ext_layer) {
		Lx_base = baddr + layer * OVL_LAYER_OFFSET;
		Lx_base += (DISP_REG_OVL_EL_CON(0) - DISP_REG_OVL_CON(0));

		Lx_addr_base = baddr + layer * 0x4;
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
		Lx_addr_base +=
			(DISP_REG_OVL_EL_ADDR(0) - DISP_REG_OVL_ADDR_MT6885);
#endif
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
		Lx_addr_base +=
			(DISP_REG_OVL_EL_ADDR(0) - DISP_REG_OVL_ADDR_MT6873);
#endif
	} else {
		Lx_base = baddr + layer * OVL_LAYER_OFFSET;
		Lx_addr_base = baddr + layer * OVL_LAYER_OFFSET;
	}

	con = readl(DISP_REG_OVL_CON(0) + Lx_base);
	offset = readl(DISP_REG_OVL_L0_OFFSET + Lx_base);
	src_size = readl(DISP_REG_OVL_L0_SRC_SIZE + Lx_base);
	pitch = readl(DISP_REG_OVL_L0_PITCH + Lx_base);
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
	addr = readl(DISP_REG_OVL_ADDR_MT6885 + Lx_addr_base);
#endif
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
	addr = readl(DISP_REG_OVL_ADDR_MT6873 + Lx_addr_base);
#endif
	clip = readl(DISP_REG_OVL_CLIP(0) + Lx_base);

	/* TODO
	 * fmt = display_fmt_reg_to_unified_fmt(
	 * REG_FLD_VAL_GET(L_CON_FLD_CFMT, con),
	 * REG_FLD_VAL_GET(L_CON_FLD_BTSW, con),
	 * REG_FLD_VAL_GET(L_CON_FLD_RGB_SWAP, con));
	 */
	DDPDUMP("%s_L%d:(%u,%u,%ux%u)\n",
		is_ext_layer ? "ext" : "phy", layer, offset & 0xfff,
		(offset >> 16) & 0xfff, src_size & 0xfff,
		(src_size >> 16) & 0xfff);
	DDPDUMP("pitch=%u,addr=0x%08x,source=%s,aen=%u,alpha=%u,cl=0x%x\n",
		pitch & 0xffff,
		addr, /* unified_color_fmt_name(fmt),*/
		(REG_FLD_VAL_GET(L_CON_FLD_LSRC, con) == 0) ? "mem"
							    : "constant_color",
		REG_FLD_VAL_GET(L_CON_FLD_AEN, con),
		REG_FLD_VAL_GET(L_CON_FLD_APHA, con),
		clip);

	ovl_dump_layer_info_compress(comp, layer, is_ext_layer);
}

int mtk_ovl_analysis(struct mtk_ddp_comp *comp)
{
	int i = 0;
	void __iomem *Lx_base;
	void __iomem *rdma_offset;
	void __iomem *baddr = comp->regs;
	unsigned int src_con;
	unsigned int ext_con;
	unsigned int addcon;

	if (comp->blank_mode)
		return 0;

	src_con = readl(DISP_REG_OVL_SRC_CON + baddr);
	ext_con = readl(DISP_REG_OVL_DATAPATH_EXT_CON + baddr);
	addcon = readl(DISP_REG_OVL_ADDCON_DBG + baddr);

	DDPDUMP("== %s ANALYSIS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("ovl_en=%d,layer_en(%d,%d,%d,%d),bg(%dx%d)\n",
		readl(DISP_REG_OVL_EN + baddr) & 0x1, src_con & 0x1,
		(src_con >> 1) & 0x1, (src_con >> 2) & 0x1,
		(src_con >> 3) & 0x1,
		readl(DISP_REG_OVL_ROI_SIZE + baddr) & 0xfff,
		(readl(DISP_REG_OVL_ROI_SIZE + baddr) >> 16) & 0xfff);
	DDPDUMP("ext_layer:layer_en(%d,%d,%d),attach_layer(%d,%d,%d)\n",
		ext_con & 0x1, (ext_con >> 1) & 0x1, (ext_con >> 2) & 0x1,
		(ext_con >> 16) & 0xf, (ext_con >> 20) & 0xf,
		(ext_con >> 24) & 0xf);
	DDPDUMP("cur_pos(%u,%u),layer_hit(%u,%u,%u,%u),bg_mode=%s,sta=0x%x\n",
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_ROI_X, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_ROI_Y, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L0_WIN_HIT, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L1_WIN_HIT, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L2_WIN_HIT, addcon),
		REG_FLD_VAL_GET(ADDCON_DBG_FLD_L3_WIN_HIT, addcon),
		DISP_REG_GET_FIELD(DATAPATH_CON_FLD_BGCLR_IN_SEL,
				   DISP_REG_OVL_DATAPATH_CON + baddr)
			? "DL"
			: "const",
		readl(DISP_REG_OVL_STA + baddr));

	/* phy layer */
	for (i = 0; i < mtk_ovl_layer_num(comp); i++) {
		unsigned int rdma_ctrl;

		if (src_con & (0x1 << i))
			ovl_dump_layer_info(comp, i, false);
		else
			DDPDUMP("phy_L%d:disabled\n", i);

		Lx_base = i * OVL_LAYER_OFFSET + baddr;
		rdma_ctrl = readl(Lx_base + DISP_REG_OVL_RDMA0_CTRL);
		DDPDUMP("ovl rdma%d status:(en=%d,fifo_used:%d,GMC=0x%x)\n", i,
			REG_FLD_VAL_GET(RDMA0_CTRL_FLD_RDMA_EN, rdma_ctrl),
			REG_FLD_VAL_GET(RDMA0_CTRL_FLD_RMDA_FIFO_USED_SZ,
					rdma_ctrl),
			readl(Lx_base + DISP_REG_OVL_RDMA0_MEM_GMC_SETTING));

		rdma_offset = i * OVL_RDMA_DEBUG_OFFSET + baddr;
		ovl_print_ovl_rdma_status(
			readl(DISP_REG_OVL_RDMA0_DBG + rdma_offset));
	}

	/* ext layer */
	for (i = 0; i < 3; i++) {
		if (ext_con & (0x1 << i))
			ovl_dump_layer_info(comp, i, true);
		else
			DDPDUMP("ext_L%d:disabled\n", i);
	}
	ovl_printf_status(readl(DISP_REG_OVL_FLOW_CTRL_DBG + baddr));

	return 0;
}

static void mtk_ovl_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(comp->dev);
	int ret;
#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);
#endif
	struct mtk_drm_private *dev_priv = NULL;

	mtk_ddp_comp_clk_prepare(comp);

	if (priv->fbdc_clk != NULL) {
		ret = clk_prepare_enable(priv->fbdc_clk);
		if (ret)
			DDPPR_ERR("clk prepare enable failed:%s\n",
				mtk_dump_comp_str(comp));
	}

#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	if (ovl->data->support_shadow) {
		/* Enable shadow register and read shadow register */
		mtk_ddp_write_mask_cpu(comp, 0x0, DISP_REG_OVL_EN,
			DISP_OVL_BYPASS_SHADOW);
	} else {
		/* Bypass shadow register and read shadow register */
		mtk_ddp_write_mask_cpu(comp, DISP_OVL_BYPASS_SHADOW,
			DISP_REG_OVL_EN, DISP_OVL_BYPASS_SHADOW);
	}
#else
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853)
	/* Bypass shadow register and read shadow register */
	mtk_ddp_write_mask_cpu(comp, DISP_OVL_BYPASS_SHADOW,
		DISP_REG_OVL_EN, DISP_OVL_BYPASS_SHADOW);
#endif
#endif

	dev_priv = comp->mtk_crtc->base.dev->dev_private;
	if (mtk_drm_helper_get_opt(dev_priv->helper_opt, MTK_DRM_OPT_LAYER_REC))
		writel(0xffffffff, comp->regs + DISP_OVL_REG_GDRDY_PRD);
}

static void mtk_ovl_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(comp->dev);

	if (priv->fbdc_clk != NULL)
		clk_disable_unprepare(priv->fbdc_clk);

	mtk_ddp_comp_clk_unprepare(comp);
}

static void
mtk_ovl_config_trigger(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkt,
		       enum mtk_ddp_comp_trigger_flag flag)
{
	switch (flag) {
	case MTK_TRIG_FLAG_LAYER_REC:
	{
		u32 offset = 0;
		struct cmdq_pkt_buffer *qbuf;

		int i = 0;
		const int lnr = mtk_ovl_layer_num(comp);
		u32 ln_con = 0, ln_size = 0;

		struct mtk_drm_private *priv = NULL;

		if (!comp->mtk_crtc)
			return;

		priv = comp->mtk_crtc->base.dev->dev_private;
		if (!mtk_drm_helper_get_opt(priv->helper_opt,
					   MTK_DRM_OPT_LAYER_REC))
			return;

		if (comp->id == DDP_COMPONENT_OVL0_2L)
			offset = DISP_SLOT_LAYER_REC_OVL0_2L;
		else if (comp->id == DDP_COMPONENT_OVL0)
			offset = DISP_SLOT_LAYER_REC_OVL0;
		else
			return;

		qbuf = &comp->mtk_crtc->gce_obj.buf;

		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + DISP_REG_OVL_GDRDY_PRD_NUM,
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);

		offset += 4;
		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + DISP_REG_OVL_SRC_CON,
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);
		offset += 4;
		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + DISP_REG_OVL_DATAPATH_CON,
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);
		offset += 4;
		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + DISP_REG_OVL_DATAPATH_EXT_CON,
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);

		for (i = 0; i < lnr + 3; i++) {
			if (i < lnr) {
				ln_con = DISP_REG_OVL_CON(i);
				ln_size = DISP_REG_OVL_SRC_SIZE(i);
			} else {
				ln_con = DISP_REG_OVL_EL_CON(i - lnr);
				ln_size = DISP_REG_OVL_EL_SRC_SIZE(i - lnr);
			}

			offset += 0x4;
			cmdq_pkt_mem_move(pkt, comp->cmdq_base,
					  comp->regs_pa + ln_con,
					  qbuf->pa_base + offset,
					  CMDQ_THR_SPR_IDX3);
			offset += 0x4;
			cmdq_pkt_mem_move(pkt, comp->cmdq_base,
					  comp->regs_pa + ln_size,
					  qbuf->pa_base + offset,
					  CMDQ_THR_SPR_IDX3);
		}

		if (comp->id == DDP_COMPONENT_OVL0_2L) {
			if (offset >= DISP_SLOT_LAYER_REC_OVL0)
				DDPMSG("%s:error:ovl0_2l:offset overflow:%u\n",
				       __func__, offset);
		} else if (comp->id == DDP_COMPONENT_OVL0) {
			if (offset >= DISP_SLOT_LAYER_REC_END)
				DDPMSG("%s:error:ovl0:offset overflow:%u\n",
				       __func__, offset);
		}

		break;
	}
	default:
		break;
	}
}

static const struct mtk_ddp_comp_funcs mtk_disp_ovl_funcs = {
	.config = mtk_ovl_config,
	.start = mtk_ovl_start,
	.stop = mtk_ovl_stop,
#if 0
	.enable_vblank = mtk_ovl_enable_vblank,
	.disable_vblank = mtk_ovl_disable_vblank,
#endif
	.layer_on = mtk_ovl_layer_on,
	.layer_off = mtk_ovl_layer_off,
	.layer_config = mtk_ovl_layer_config,
	.addon_config = mtk_ovl_addon_config,
	.io_cmd = mtk_ovl_io_cmd,
	.prepare = mtk_ovl_prepare,
	.unprepare = mtk_ovl_unprepare,
	.connect = mtk_ovl_connect,
	.config_trigger = mtk_ovl_config_trigger,
};

/* TODO: to be refactored */
int drm_ovl_tf_cb(int port, unsigned long mva, void *data)
{
	struct mtk_disp_ovl *ovl = (struct mtk_disp_ovl *)data;

	DDPPR_ERR("%s tf mva: 0x%lx\n", mtk_dump_comp_str(&ovl->ddp_comp), mva);

	mtk_ovl_analysis(&ovl->ddp_comp);
	mtk_ovl_dump(&ovl->ddp_comp);

	return 0;
}

static int mtk_disp_ovl_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
#ifdef MTK_FB_MMDVFS_SUPPORT
	struct mtk_drm_private *drm_priv = drm_dev->dev_private;
	int qos_req_port;
#endif
	int ret;
	unsigned int bg_h, bg_w;
	void __iomem *baddr;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

#ifdef MTK_FB_MMDVFS_SUPPORT
	qos_req_port = __mtk_disp_pmqos_port_look_up(priv->ddp_comp.id);
	if (qos_req_port < 0) {
		DDPPR_ERR("Failed to request QOS port\n");
	} else {
		mm_qos_add_request(&drm_priv->bw_request_list,
				   &priv->ddp_comp.qos_req, qos_req_port);
		mm_qos_add_request(&drm_priv->bw_request_list,
				   &priv->ddp_comp.fbdc_qos_req, qos_req_port);
		mm_qos_add_request(&drm_priv->hrt_request_list,
				   &priv->ddp_comp.hrt_qos_req, qos_req_port);
	}
#endif

	baddr = priv->ddp_comp.regs;
	bg_w = readl(DISP_REG_OVL_ROI_SIZE + baddr) & 0xfff,
	bg_h = (readl(DISP_REG_OVL_ROI_SIZE + baddr) >> 16) & 0xfff,
	_store_bg_roi(&priv->ddp_comp, bg_h, bg_w);

	return 0;
}

static void mtk_disp_ovl_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_ovl_component_ops = {
	.bind = mtk_disp_ovl_bind, .unbind = mtk_disp_ovl_unbind,
};

static int mtk_disp_ovl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_ovl *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_OVL);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	DDPINFO("%s comp_id:%d\n", __func__, comp_id);

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_ovl_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}
	priv->fbdc_clk = of_clk_get(dev->of_node, 1);
	if (IS_ERR(priv->fbdc_clk))
		priv->fbdc_clk = NULL;

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ret = devm_request_irq(dev, irq, mtk_disp_ovl_irq_handler,
			       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
			       priv);
	if (ret < 0) {
		DDPAEE("%s:%d, failed to request irq:%d ret:%d comp_id:%d\n",
				__func__, __LINE__,
				irq, ret, comp_id);
		return ret;
	}

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_disp_ovl_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	DDPINFO("%s-\n", __func__);
	return ret;
}

static int mtk_disp_ovl_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_ovl_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct mtk_disp_ovl_data mt2701_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT2701,
	.fmt_rgb565_is_0 = false,
	.fmt_uyvy = 9U << 12,
	.fmt_yuyv = 8U << 12,
	.support_shadow = false,
};

static const struct compress_info compr_info_mt6779  = {
	.name = "PVRIC_V3_1_MTK_1",
	.l_config = &compr_l_config_PVRIC_V3_1,
};

static const struct mtk_disp_ovl_data mt6779_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT6779,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6779,
	.support_shadow = false,
};

static const struct compress_info compr_info_mt6885  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6885_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT6885,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6885,
	.support_shadow = false,
};

static const struct compress_info compr_info_mt6873  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6873_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT6873,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6873,
	.support_shadow = false,
};

static const struct compress_info compr_info_mt6853  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6853_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT6853,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6853,
	.support_shadow = false,
};

static const struct compress_info compr_info_mt6833  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_ovl_data mt6833_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT6833,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6833,
	.support_shadow = false,
};

static const struct mtk_disp_ovl_data mt8173_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT8173,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.support_shadow = false,
};

static const struct of_device_id mtk_disp_ovl_driver_dt_match[] = {
	{.compatible = "mediatek,mt2701-disp-ovl",
	 .data = &mt2701_ovl_driver_data},
	{.compatible = "mediatek,mt6779-disp-ovl",
	 .data = &mt6779_ovl_driver_data},
	{.compatible = "mediatek,mt8173-disp-ovl",
	 .data = &mt8173_ovl_driver_data},
	{.compatible = "mediatek,mt6885-disp-ovl",
	 .data = &mt6885_ovl_driver_data},
	{.compatible = "mediatek,mt6873-disp-ovl",
	 .data = &mt6873_ovl_driver_data},
	{.compatible = "mediatek,mt6853-disp-ovl",
	 .data = &mt6853_ovl_driver_data},
	{.compatible = "mediatek,mt6833-disp-ovl",
	 .data = &mt6833_ovl_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_ovl_driver_dt_match);

struct platform_driver mtk_disp_ovl_driver = {
	.probe = mtk_disp_ovl_probe,
	.remove = mtk_disp_ovl_remove,
	.driver = {

			.name = "mediatek-disp-ovl",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_ovl_driver_dt_match,
		},
};
