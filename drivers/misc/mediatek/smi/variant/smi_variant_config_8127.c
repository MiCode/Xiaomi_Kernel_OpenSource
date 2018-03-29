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

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/kobject.h>

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include "smi_reg.h"
#include "smi_common.h"
#include "smi_priv.h"

#define SMI_LARB0_PORT_NUM	10
#define SMI_LARB1_PORT_NUM	7
#define SMI_LARB2_PORT_NUM	17

static void mt8127_init_setting(struct mtk_smi_data *smidev, bool *default_saved,
			u32 *default_smi_val, unsigned int larbid)
{

	if (!(*default_saved)) {
		SMIMSG("Save default config:\n");
		default_smi_val[0] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0);
		default_smi_val[1] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1);
		default_smi_val[2] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2);

		SMIMSG("l1arb[0-2]= 0x%x,  0x%x, 0x%x\n", default_smi_val[0],
		       default_smi_val[1], default_smi_val[2]);
		*default_saved = true;
	}

	/* Keep the HW's init setting in REG_SMI_L1ARB0 ~ REG_SMI_L1ARB4 */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, default_smi_val[0]);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, default_smi_val[1]);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, default_smi_val[2]);

	SMIMSG("Current Setting: GPU - new");
	if (!SMI_COMMON_EXT_BASE || !LARB0_BASE) {
		SMIMSG("smi and smi_larb should have been probe first\n");
		return;
	}
	/* 2 non-ultra write, 3 write command , 4 non-ultra read , 5 ultra read */
	M4U_WriteReg32(REG_SMI_M4U_TH, 0, ((0x3 << 15) + (0x4 << 10) + (0x4 << 5) + 0x5));
	/*
	 * Level 1 LARB, apply new outstanding control method, 1/4 bandwidth
	 * limiter overshoot control , enable warb channel
	 */
	M4U_WriteReg32(REG_SMI_L1LEN, 0, 0xB);
	/*
	 * total 8 commnads between smi common to M4U, 12 non ultra commands
	 * between smi common to M4U, 1 commnads can in write AXI slice for all LARBs
	 */
	M4U_WriteReg32(REG_SMI_READ_FIFO_TH, 0, ((0x7 << 11) + (0x8 << 6) + 0x3F));

	M4U_WriteReg32(LARB0_BASE, 0x200, 0xC);	/* DISP_OVL_0 */
	M4U_WriteReg32(LARB0_BASE, 0x204, 0x1);	/* DISP_RDMA_1 */
	M4U_WriteReg32(LARB0_BASE, 0x208, 0x1);	/* DISP_RDMA */
	M4U_WriteReg32(LARB0_BASE, 0x20C, 0x2);	/* DISP_WDMA */
	M4U_WriteReg32(LARB0_BASE, 0x210, 0x1);	/* MM_CMDQ */
	M4U_WriteReg32(LARB0_BASE, 0x214, 0x5);	/* MDP_RDMA */
	M4U_WriteReg32(LARB0_BASE, 0x218, 0x1);	/* MDP_WDMA */
	M4U_WriteReg32(LARB0_BASE, 0x21C, 0x3);	/* MDP_ROT */
	M4U_WriteReg32(LARB0_BASE, 0x220, 0x1);	/* MDP_ROTCO */
	M4U_WriteReg32(LARB0_BASE, 0x224, 0x1);	/* MDP ROTVO */

	M4U_WriteReg32(LARB1_BASE, 0x200, 0x1);	/* HW_VDEC_MC_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x204, 0x1);	/* HW_VDEC_PP_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x208, 0x1);	/* HW_VDEC_AVC_MV-EXT */
	M4U_WriteReg32(LARB1_BASE, 0x20C, 0x1);	/* HW_VDEC_PRED_RD_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x210, 0x1);	/* HW_VDEC_PRED_WR_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x214, 0x1);	/* HW_VDEC_VLD_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x218, 0x1);	/* HW_VDEC_PP_INT */

	M4U_WriteReg32(LARB2_BASE, 0x200, 0x1);	/* CAM_IMGO */
	M4U_WriteReg32(LARB2_BASE, 0x204, 0x1);	/* CAM_IMG2O */
	M4U_WriteReg32(LARB2_BASE, 0x208, 0x1);	/* CAM_LSCI */
	M4U_WriteReg32(LARB2_BASE, 0x20C, 0x1);	/* CAM_IMGI */
	M4U_WriteReg32(LARB2_BASE, 0x210, 0x1);	/* CAM_ESFKO */
	M4U_WriteReg32(LARB2_BASE, 0x214, 0x1);	/* CAM_AAO */
	M4U_WriteReg32(LARB2_BASE, 0x218, 0x1);	/* CAM_LCEI */
	M4U_WriteReg32(LARB2_BASE, 0x21C, 0x1);	/* CAM_LCSO */
	M4U_WriteReg32(LARB2_BASE, 0x220, 0x1);	/* JPGENC_RDMA */
	M4U_WriteReg32(LARB2_BASE, 0x224, 0x1);	/* JPGENC_BSDMA */
	M4U_WriteReg32(LARB2_BASE, 0x228, 0x1);	/* VENC_SV_COMV */
	M4U_WriteReg32(LARB2_BASE, 0x22C, 0x1);	/* VENC_RD_COMV */
	M4U_WriteReg32(LARB2_BASE, 0x230, 0x1);	/* VENC_RCPU */
	M4U_WriteReg32(LARB2_BASE, 0x234, 0x1);	/* VENC_REC_FRM */
	M4U_WriteReg32(LARB2_BASE, 0x238, 0x1);	/* VENC_REF_LUMA */
	M4U_WriteReg32(LARB2_BASE, 0x23C, 0x1);	/* VENC_REF_CHROMA */
	M4U_WriteReg32(LARB2_BASE, 0x244, 0x1);	/* VENC_BSDMA */
	M4U_WriteReg32(LARB2_BASE, 0x248, 0x1);	/* VENC_CUR_LUMA */
	M4U_WriteReg32(LARB2_BASE, 0x24C, 0x1);	/* VENC_CUR_CHROMA */
}

static void mt8127_vp_setting(struct mtk_smi_data *smidev)
{
	/* 2 non-ultra write, 3 write command , 4 non-ultra read , 5 ultra read */
	M4U_WriteReg32(REG_SMI_M4U_TH, 0, ((0x2 << 15) + (0x3 << 10) + (0x4 << 5) + 0x5));
	/*
	 * Level 1 LARB, apply new outstanding control method, 1/4 bandwidth limiter
	 * overshoot control , enable warb channel
	 */
	M4U_WriteReg32(REG_SMI_L1LEN, 0, 0x1B);
	/*
	 * total 8 commnads between smi common to M4U, 12 non ultra commands
	 * between smi common to M4U, 1 commnads can in write AXI slice for all LARBs
	 */
	M4U_WriteReg32(REG_SMI_READ_FIFO_TH, 0, 0x323F);

	M4U_WriteReg32(REG_SMI_L1ARB0, 0, 0xC3A);	/* 1111/4096 maximum grant counts, soft limiter */
	M4U_WriteReg32(REG_SMI_L1ARB1, 0, 0x9E8);	/* 503/4096 maximum grant counts, soft limiter */
	M4U_WriteReg32(REG_SMI_L1ARB2, 0, 0x943);	/* 353/4096 maximum grant counts, soft limiter */

	M4U_WriteReg32(LARB0_BASE, 0x200, 0xC);	/* DISP_OVL_0 */
	M4U_WriteReg32(LARB0_BASE, 0x204, 0x1);	/* DISP_RDMA_1 */
	M4U_WriteReg32(LARB0_BASE, 0x208, 0x1);	/* DISP_RDMA */
	M4U_WriteReg32(LARB0_BASE, 0x20C, 0x2);	/* DISP_WDMA */
	M4U_WriteReg32(LARB0_BASE, 0x210, 0x1);	/* MM_CMDQ */
	M4U_WriteReg32(LARB0_BASE, 0x214, 0x5);	/* MDP_RDMA */
	M4U_WriteReg32(LARB0_BASE, 0x218, 0x1);	/* MDP_WDMA */
	M4U_WriteReg32(LARB0_BASE, 0x21C, 0x3);	/* MDP_ROT */
	M4U_WriteReg32(LARB0_BASE, 0x220, 0x1);	/* MDP_ROTCO */
	M4U_WriteReg32(LARB0_BASE, 0x224, 0x1);	/* MDP ROTVO */

	M4U_WriteReg32(LARB1_BASE, 0x200, 0x6);	/* HW_VDEC_MC_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x204, 0x2);	/* HW_VDEC_PP_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x208, 0x1);	/* HW_VDEC_AVC_MV-EXT */
	M4U_WriteReg32(LARB1_BASE, 0x20C, 0x3);	/* HW_VDEC_PRED_RD_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x210, 0x3);	/* HW_VDEC_PRED_WR_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x214, 0x1);	/* HW_VDEC_VLD_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x218, 0x1);	/* HW_VDEC_PP_INT */

	M4U_WriteReg32(LARB2_BASE, 0x200, 0x1);	/* CAM_IMGO */
	M4U_WriteReg32(LARB2_BASE, 0x204, 0x1);	/* CAM_IMG2O */
	M4U_WriteReg32(LARB2_BASE, 0x208, 0x1);	/* CAM_LSCI */
	M4U_WriteReg32(LARB2_BASE, 0x20C, 0x1);	/* CAM_IMGI */
	M4U_WriteReg32(LARB2_BASE, 0x210, 0x1);	/* CAM_ESFKO */
	M4U_WriteReg32(LARB2_BASE, 0x214, 0x1);	/* CAM_AAO */
	M4U_WriteReg32(LARB2_BASE, 0x218, 0x1);	/* CAM_LCEI */
	M4U_WriteReg32(LARB2_BASE, 0x21C, 0x1);	/* CAM_LCSO */
	M4U_WriteReg32(LARB2_BASE, 0x220, 0x1);	/* JPGENC_RDMA */
	M4U_WriteReg32(LARB2_BASE, 0x224, 0x1);	/* JPGENC_BSDMA */
	M4U_WriteReg32(LARB2_BASE, 0x228, 0x1);	/* VENC_SV_COMV */
	M4U_WriteReg32(LARB2_BASE, 0x22C, 0x1);	/* VENC_RD_COMV */
	M4U_WriteReg32(LARB2_BASE, 0x230, 0x1);	/* VENC_RCPU */
	M4U_WriteReg32(LARB2_BASE, 0x234, 0x1);	/* VENC_REC_FRM */
	M4U_WriteReg32(LARB2_BASE, 0x238, 0x1);	/* VENC_REF_LUMA */
	M4U_WriteReg32(LARB2_BASE, 0x23C, 0x1);	/* VENC_REF_CHROMA */
	M4U_WriteReg32(LARB2_BASE, 0x244, 0x1);	/* VENC_BSDMA */
	M4U_WriteReg32(LARB2_BASE, 0x248, 0x1);	/* VENC_CUR_LUMA */
	M4U_WriteReg32(LARB2_BASE, 0x24C, 0x1);	/* VENC_CUR_CHROMA */

}

static void mt8127_vr_setting(struct mtk_smi_data *smidev)
{
	/* 2 non-ultra write, 3 write command , 4 non-ultra read , 5 ultra read */
	M4U_WriteReg32(REG_SMI_M4U_TH, 0, ((0x2 << 15) + (0x3 << 10) + (0x4 << 5) + 0x5));
	/*
	 * Level 1 LARB, apply new outstanding control method, 1/4 bandwidth limiter
	 * overshoot control , enable warb channel
	 */
	M4U_WriteReg32(REG_SMI_L1LEN, 0, 0xB);
	/*
	 * total 8 commnads between smi common to M4U, 12 non ultra commands between smi common
	 * to M4U, 1 commnads can in write AXI slice for all LARBs
	 */
	M4U_WriteReg32(REG_SMI_READ_FIFO_TH, 0, ((0x6 << 11) + (0x8 << 6) + 0x3F));

	M4U_WriteReg32(REG_SMI_L1ARB0, 0, 0xC26);	/* 1111/4096 maximum grant counts, soft limiter */
	M4U_WriteReg32(REG_SMI_L1ARB1, 0, 0x943);	/* 503/4096 maximum grant counts, soft limiter */
	M4U_WriteReg32(REG_SMI_L1ARB2, 0, 0xD4F);	/* 1359/4096 maximum grant counts, soft limiter */

	M4U_WriteReg32(LARB0_BASE, 0x200, 0xC);	/* DISP_OVL_0 */
	M4U_WriteReg32(LARB0_BASE, 0x204, 0x1);	/* DISP_RDMA_1 */
	M4U_WriteReg32(LARB0_BASE, 0x208, 0x1);	/* DISP_RDMA */
	M4U_WriteReg32(LARB0_BASE, 0x20C, 0x1);	/* DISP_WDMA */
	M4U_WriteReg32(LARB0_BASE, 0x210, 0x1);	/* MM_CMDQ */
	M4U_WriteReg32(LARB0_BASE, 0x214, 0x2);	/* MDP_RDMA */
	M4U_WriteReg32(LARB0_BASE, 0x218, 0x2);	/* MDP_WDMA */
	M4U_WriteReg32(LARB0_BASE, 0x21C, 0x4);	/* MDP_ROT */
	M4U_WriteReg32(LARB0_BASE, 0x220, 0x2);	/* MDP_ROTCO */
	M4U_WriteReg32(LARB0_BASE, 0x224, 0x2);	/* MDP ROTVO */

	M4U_WriteReg32(LARB1_BASE, 0x200, 0x1);	/* HW_VDEC_MC_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x204, 0x1);	/* HW_VDEC_PP_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x208, 0x1);	/* HW_VDEC_AVC_MV-EXT */
	M4U_WriteReg32(LARB1_BASE, 0x20C, 0x1);	/* HW_VDEC_PRED_RD_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x210, 0x1);	/* HW_VDEC_PRED_WR_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x214, 0x1);	/* HW_VDEC_VLD_EXT */
	M4U_WriteReg32(LARB1_BASE, 0x218, 0x1);	/* HW_VDEC_PP_INT */

	M4U_WriteReg32(LARB2_BASE, 0x200, 0x6);	/* CAM_IMGO */
	M4U_WriteReg32(LARB2_BASE, 0x204, 0x1);	/* CAM_IMG2O */
	M4U_WriteReg32(LARB2_BASE, 0x208, 0x1);	/* CAM_LSCI */
	M4U_WriteReg32(LARB2_BASE, 0x20C, 0x4);	/* CAM_IMGI */
	M4U_WriteReg32(LARB2_BASE, 0x210, 0x1);	/* CAM_ESFKO */
	M4U_WriteReg32(LARB2_BASE, 0x214, 0x1);	/* CAM_AAO */
	M4U_WriteReg32(LARB2_BASE, 0x218, 0x1);	/* CAM_LCEI */
	M4U_WriteReg32(LARB2_BASE, 0x21C, 0x1);	/* CAM_LCSO */
	M4U_WriteReg32(LARB2_BASE, 0x220, 0x1);	/* JPGENC_RDMA */
	M4U_WriteReg32(LARB2_BASE, 0x224, 0x1);	/* JPGENC_BSDMA */
	M4U_WriteReg32(LARB2_BASE, 0x228, 0x1);	/* VENC_SV_COMV */
	M4U_WriteReg32(LARB2_BASE, 0x22C, 0x1);	/* VENC_RD_COMV */
	M4U_WriteReg32(LARB2_BASE, 0x230, 0x1);	/* VENC_RCPU */
	M4U_WriteReg32(LARB2_BASE, 0x234, 0x2);	/* VENC_REC_FRM */
	M4U_WriteReg32(LARB2_BASE, 0x238, 0x4);	/* VENC_REF_LUMA */
	M4U_WriteReg32(LARB2_BASE, 0x23C, 0x2);	/* VENC_REF_CHROMA */
	M4U_WriteReg32(LARB2_BASE, 0x244, 0x1);	/* VENC_BSDMA */
	M4U_WriteReg32(LARB2_BASE, 0x248, 0x2);	/* VENC_CUR_LUMA */
	M4U_WriteReg32(LARB2_BASE, 0x24C, 0x1);	/* VENC_CUR_CHROMA */
}

static void mt8127_hdmi_setting(struct mtk_smi_data *smidev)
{
}

static void mt8127_hdmi4k_setting(struct mtk_smi_data *smidev)
{
}

const struct mtk_smi_priv smi_mt8127_priv = {
	.plat = MTK_PLAT_MT8127,
	.larb_port_num = { SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM },
	.init_setting = mt8127_init_setting,
	.vp_setting = mt8127_vp_setting,
	.vr_setting = mt8127_vr_setting,
	.hdmi_setting = mt8127_hdmi_setting,
	.hdmi_4k_setting = mt8127_hdmi4k_setting,
};
