/*
 * Copyright (C) 2016 MediaTek Inc.
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

#define SMI_LARB0_PORT_NUM	17
#define SMI_LARB1_PORT_NUM	7
#define SMI_LARB2_PORT_NUM	6
#define SMI_LARB3_PORT_NUM	13

static void mt8163_rest_setting(void)
{
	/* initialize OSTD to 1 */
	M4U_WriteReg32(LARB0_BASE, 0x200, 0x1);	/* disp_ovl0 */
	M4U_WriteReg32(LARB0_BASE, 0x204, 0x1);	/* disp_rdma0 */
	M4U_WriteReg32(LARB0_BASE, 0x208, 0x1);	/* disp_wdma0 */
	M4U_WriteReg32(LARB0_BASE, 0x20c, 0x1);	/* disp_ovl1 */
	M4U_WriteReg32(LARB0_BASE, 0x210, 0x1);	/* disp_rdma1 */
	M4U_WriteReg32(LARB0_BASE, 0x214, 0x1);	/* disp_wdma1 */

	/* To be chech with DE since the offset seems to be worng for MDP rdma, wdma mad rot */
	M4U_WriteReg32(LARB0_BASE, 0x238, 0x1);	/* mdp_rdma */
	M4U_WriteReg32(LARB0_BASE, 0x23c, 0x1);	/* mdp_wdma */
	M4U_WriteReg32(LARB0_BASE, 0x240, 0x1);	/* mdp_wrot */

	M4U_WriteReg32(LARB1_BASE, 0x200, 0x1);	/* hw_vdec_mc_ext */
	M4U_WriteReg32(LARB1_BASE, 0x204, 0x1);	/* hw_vdec_pp_ext */
	M4U_WriteReg32(LARB1_BASE, 0x208, 0x1);	/* hw_vdec_vld_ext */
	M4U_WriteReg32(LARB1_BASE, 0x20c, 0x1);	/* hw_vdec_avc_mv_ext */
	M4U_WriteReg32(LARB1_BASE, 0x210, 0x1);	/* hw_vdec_pred_rd_ext */
	M4U_WriteReg32(LARB1_BASE, 0x214, 0x1);	/* hw_vdec_pred_wr_ext */
	M4U_WriteReg32(LARB1_BASE, 0x218, 0x1);	/* hw_vdec_ppwrap_ext */

	M4U_WriteReg32(LARB2_BASE, 0x200, 0x1);	/* imgo */
	M4U_WriteReg32(LARB2_BASE, 0x204, 0x1);	/* img2o */
	M4U_WriteReg32(LARB2_BASE, 0x208, 0x1);	/* lsci */
	M4U_WriteReg32(LARB2_BASE, 0x20c, 0x1);	/* imgi */
	M4U_WriteReg32(LARB2_BASE, 0x210, 0x1);	/* esfko */
	M4U_WriteReg32(LARB2_BASE, 0x214, 0x1);	/* aao */

	M4U_WriteReg32(LARB3_BASE, 0x200, 0x1);	/* venc_rcpu */
	M4U_WriteReg32(LARB3_BASE, 0x204, 0x2);	/* venc_rec */
	M4U_WriteReg32(LARB3_BASE, 0x208, 0x1);	/* venc_bsdma */
	M4U_WriteReg32(LARB3_BASE, 0x20c, 0x1);	/* venc_sv_comv */
	M4U_WriteReg32(LARB3_BASE, 0x210, 0x1);	/* venc_rd_comv */
	M4U_WriteReg32(LARB3_BASE, 0x214, 0x1);	/* jpgenc_rdma */
	M4U_WriteReg32(LARB3_BASE, 0x218, 0x1);	/* jpgenc_bsdma */
	M4U_WriteReg32(LARB3_BASE, 0x21c, 0x1);	/* jpgdec_wdma */
	M4U_WriteReg32(LARB3_BASE, 0x220, 0x1);	/* jpgdec_bsdma */
	M4U_WriteReg32(LARB3_BASE, 0x224, 0x1);	/* venc_cur_luma */
	M4U_WriteReg32(LARB3_BASE, 0x228, 0x1);	/* venc_cur_chroma */
	M4U_WriteReg32(LARB3_BASE, 0x22c, 0x1);	/* venc_ref_luma */
	M4U_WriteReg32(LARB3_BASE, 0x230, 0x1);	/* venc_ref_chroma */
}

static void mt8163_init_setting(struct mtk_smi_data *smidev, bool *default_saved,
			u32 *default_smi_val, unsigned int larbid)
{
	/* save default larb regs */
	if (!(*default_saved)) {
		SMIMSG("Save default config:\n");
		default_smi_val[0] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0);
		default_smi_val[1] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1);
		default_smi_val[2] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2);
		default_smi_val[3] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB3);

		SMIMSG("l1arb[0-2]= 0x%x,  0x%x, 0x%x\n", default_smi_val[0],
		       default_smi_val[1], default_smi_val[2]);
		SMIMSG("l1arb[3]= 0x%x\n", default_smi_val[3]);

		*default_saved = true;
	}
	/* Keep the HW's init setting in REG_SMI_L1ARB0 ~ REG_SMI_L1ARB4 */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, default_smi_val[0]);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, default_smi_val[1]);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, default_smi_val[2]);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB3, default_smi_val[3]);
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB4, */
	/* default_val_smi_l1arb[4]); */

	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x200, 0x1b);
	/* 0x220 is controlled by M4U */
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x220, 0x1);disp: emi0, other:emi1 */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE,
		       0x234, (0x1 << 31) + (0x1d << 26) + (0x1f << 21) + (0x0 << 20) + (0x3 << 15)
		       + (0x4 << 10) + (0x4 << 5) + 0x5);
	/* To be checked with DE */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x230, 0x1f + (0x8 << 5) + (0x7 << 10));

	/* Set VC priority: MMSYS = ISP > VENC > VDEC = MJC */
	M4U_WriteReg32(LARB0_BASE, 0x20, 0x0);	/* MMSYS */
	M4U_WriteReg32(LARB1_BASE, 0x20, 0x2);	/* VDEC */
	M4U_WriteReg32(LARB2_BASE, 0x20, 0x0);	/* ISP */
	M4U_WriteReg32(LARB3_BASE, 0x20, 0x1);	/* VENC */
	/* M4U_WriteReg32(LARB4_BASE, 0x20, 0x2); // MJC */

	/* turn off EMI empty double OSTD */
	M4U_WriteReg32(LARB0_BASE, 0x2c, M4U_ReadReg32(LARB0_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB1_BASE, 0x2c, M4U_ReadReg32(LARB1_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB2_BASE, 0x2c, M4U_ReadReg32(LARB2_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB3_BASE, 0x2c, M4U_ReadReg32(LARB3_BASE, 0x2c) | (1 << 2));
	/* M4U_WriteReg32(LARB4_BASE, 0x2c, M4U_ReadReg32(LARB4_BASE, 0x2c) | (1 << 2)); */

	/* for ISP HRT */
	M4U_WriteReg32(LARB2_BASE, 0x24, (M4U_ReadReg32(LARB2_BASE, 0x24) & 0xf7ffffff));

	/* for UI */
	mt8163_rest_setting();

	/* SMI common BW limiter */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x204, 0x1A5A);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x208, 0x1000);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x20C, 0x1000);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x210, 0x1000);
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x214, 0x1000); */

	/* LARB 0 DISP+MDP */
	M4U_WriteReg32(LARB0_BASE, 0x200, 31);	/* disp_ovl0 */
	M4U_WriteReg32(LARB0_BASE, 0x204, 4);	/* disp_rdma0 */
	M4U_WriteReg32(LARB0_BASE, 0x208, 6);	/* disp_wdma0 */
	M4U_WriteReg32(LARB0_BASE, 0x20c, 31);	/* disp_ovl1 */
	M4U_WriteReg32(LARB0_BASE, 0x210, 4);	/* disp_rdma1 */
	M4U_WriteReg32(LARB0_BASE, 0x214, 0x1);	/* disp_wdma1 */

	/* To be chech with DE since the offset seems to be worng for MDP rdma, wdma mad rot */
	M4U_WriteReg32(LARB0_BASE, 0x238, 2);	/* mdp_rdma */
	M4U_WriteReg32(LARB0_BASE, 0x23c, 0x1);	/* mdp_wdma */
	M4U_WriteReg32(LARB0_BASE, 0x240, 3);	/* mdp_wrot */
}

static void mt8163_vp_setting(struct mtk_smi_data *smidev)
{
	/* VP 4K */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x204, 0x13DB);	/* LARB0, DISP+MDP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x208, 0x117D);	/* LARB1, VDEC */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x20C, 0x1000);	/* LARB2, ISP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x210, 0x10AD);	/* LARB3, VENC+JPG */
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x214, 0x1000); //LARB4, MJC */

	mt8163_rest_setting();

	M4U_WriteReg32(LARB0_BASE, 0x200, 0x12);	/* OVL_CH0_0+OVL_CH0_1 */
	M4U_WriteReg32(LARB0_BASE, 0x204, 4);	/* disp_rdma0 */
	M4U_WriteReg32(LARB0_BASE, 0x208, 6);	/* disp_wdma0 */
	M4U_WriteReg32(LARB0_BASE, 0x20C, 0x12);	/* port 3: disp ovl1 */
	M4U_WriteReg32(LARB0_BASE, 0x210, 4);	/* OVL_CH1_0+OVL_CH1_1 */
	M4U_WriteReg32(LARB0_BASE, 0x238, 2);	/* mdp_rdma */
	M4U_WriteReg32(LARB0_BASE, 0x23c, 0x2);	/* mdp_wdma */
	M4U_WriteReg32(LARB0_BASE, 0x240, 0x3);	/* mdp_wrot */

	M4U_WriteReg32(LARB1_BASE, 0x200, 0x8);	/* port#0, mc */
	M4U_WriteReg32(LARB1_BASE, 0x204, 0x2);	/* port#1, pp */
	M4U_WriteReg32(LARB1_BASE, 0x208, 0x1);	/* port#2, ufo */
	M4U_WriteReg32(LARB1_BASE, 0x20c, 0x1);	/* port#3, vld */
	M4U_WriteReg32(LARB1_BASE, 0x210, 0x1);	/* port#4, vld2 */
	M4U_WriteReg32(LARB1_BASE, 0x214, 0x1);	/* port#5, mv */
	M4U_WriteReg32(LARB1_BASE, 0x218, 0x1);	/* port#6, pred rd */
	M4U_WriteReg32(LARB1_BASE, 0x21c, 0x1);	/* port#7, pred wr */
	M4U_WriteReg32(LARB1_BASE, 0x220, 0x1);	/* port#8, ppwrap */
}

static void mt8163_vpwfd_setting(struct mtk_smi_data *smidev)
{
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x204, 0x13DB);	/* LARB0, DISP+MDP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x208, 0x117D);	/* LARB1, VDEC */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x20C, 0x1000);	/* LARB2, ISP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x210, 0x10AD);	/* LARB3, VENC+JPG */
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x214, 0x1000); //LARB4, MJC */

	mt8163_rest_setting();

	M4U_WriteReg32(LARB0_BASE, 0x200, 0x12);	/* OVL_CH0_0+OVL_CH0_1 */
	M4U_WriteReg32(LARB0_BASE, 0x204, 4);	/* disp_rdma0 */
	M4U_WriteReg32(LARB0_BASE, 0x208, 6);	/* disp_wdma0 */
	M4U_WriteReg32(LARB0_BASE, 0x20C, 0x12);	/* port 3: disp ovl1 */
	M4U_WriteReg32(LARB0_BASE, 0x210, 0x4);	/* port 3: disp ovl1 */
	M4U_WriteReg32(LARB0_BASE, 0x238, 2);	/* mdp_rdma */
	M4U_WriteReg32(LARB0_BASE, 0x23c, 0x2);	/* OVL_CH1_0+OVL_CH1_1 */
	M4U_WriteReg32(LARB0_BASE, 0x240, 0x3);	/* mdp_wrot */

	M4U_WriteReg32(LARB1_BASE, 0x200, 0x8);	/* port#0, mc */
	M4U_WriteReg32(LARB1_BASE, 0x204, 0x2);	/* port#1, pp */
}

static void mt8163_vr_setting(struct mtk_smi_data *smidev)
{
	/* SMI BW limit */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, 0x129F);	/* LARB0, DISP+MDP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, 0x1000);	/* LARB1, VDEC */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, 0x1224);	/* LARB2, ISP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB3, 0x1112);	/* LARB3, VENC+JPG */
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB4, 0x1000); //LARB4, MJC */

	/* SMI LARB config */

	mt8163_rest_setting();

	/* LARB 0 DISP+MDP */
	M4U_WriteReg32(LARB0_BASE, 0x200, 0x1F);	/* port 0: disp ovl0 */
	M4U_WriteReg32(LARB0_BASE, 0x204, 4);	/* disp_rdma0 */
	M4U_WriteReg32(LARB0_BASE, 0x208, 6);	/* disp_wdma0 */
	M4U_WriteReg32(LARB0_BASE, 0x20C, 0x1F);	/* port 3: disp ovl1 */
	M4U_WriteReg32(LARB0_BASE, 0x210, 4);	/* disp_rdma1 */
	M4U_WriteReg32(LARB0_BASE, 0x214, 0x1);	/* disp_wdma1 */

	M4U_WriteReg32(LARB0_BASE, 0x238, 2);	/* mdp_rdma */
	M4U_WriteReg32(LARB0_BASE, 0x23c, 0x1);	/* mdp_wdma */
	M4U_WriteReg32(LARB0_BASE, 0x240, 3);	/* mdp_wrot */

	M4U_WriteReg32(LARB2_BASE, 0x200, 0xA);	/* port#0, imgo */
	M4U_WriteReg32(LARB2_BASE, 0x204, 0x4);	/* port#1, rrzo */
	M4U_WriteReg32(LARB2_BASE, 0x228, 0x2);	/* port#10, */
}

static void mt8163_hdmi_setting(struct mtk_smi_data *smidev)
{
}

static void mt8163_hdmi4k_setting(struct mtk_smi_data *smidev)
{
}

const struct mtk_smi_priv smi_mt8163_priv = {
	.plat = MTK_PLAT_MT8163,
	.larb_port_num = {
		SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM,
		SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM
	},
	.larb_vc_setting = {0, 2, 0, 1},
	.init_setting = mt8163_init_setting,
	.vp_setting = mt8163_vp_setting,
	.vp_wfd_setting = mt8163_vpwfd_setting,
	.vr_setting = mt8163_vr_setting,
	.hdmi_setting = mt8163_hdmi_setting,
	.hdmi_4k_setting = mt8163_hdmi4k_setting,
};
