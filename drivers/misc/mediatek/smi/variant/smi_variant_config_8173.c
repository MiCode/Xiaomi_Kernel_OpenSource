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

static void mt8173_init_setting(struct mtk_smi_data *smidev, bool *default_saved,
			u32 *default_smi_val, unsigned int larbid)
{

	/* save default larb regs */
	if (!(*default_saved)) {
		SMIMSG("Save default config:\n");
		default_smi_val[0] = M4U_ReadReg32(SMI_COMMON_EXT_BASE,
							 REG_OFFSET_SMI_L1ARB0);
		default_smi_val[1] = M4U_ReadReg32(SMI_COMMON_EXT_BASE,
							 REG_OFFSET_SMI_L1ARB1);
		default_smi_val[2] = M4U_ReadReg32(SMI_COMMON_EXT_BASE,
							 REG_OFFSET_SMI_L1ARB2);
		default_smi_val[3] = M4U_ReadReg32(SMI_COMMON_EXT_BASE,
							 REG_OFFSET_SMI_L1ARB3);
		default_smi_val[4] = M4U_ReadReg32(SMI_COMMON_EXT_BASE,
							 REG_OFFSET_SMI_L1ARB4);
		default_smi_val[5] = M4U_ReadReg32(SMI_COMMON_EXT_BASE,
							 REG_OFFSET_SMI_L1ARB5);
		SMIMSG("l1arb[0-2]= 0x%x,  0x%x, 0x%x\n", default_smi_val[0],
		       default_smi_val[1], default_smi_val[2]);
		SMIMSG("l1arb[3-4]= 0x%x,  0x%x 0x%x\n", default_smi_val[3],
		       default_smi_val[4], default_smi_val[5]);

		*default_saved = true;
	}
	/* Keep the HW's init setting in REG_SMI_L1ARB0 ~ REG_SMI_L1ARB4 */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, default_smi_val[0]);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, default_smi_val[1]);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, default_smi_val[2]);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB3, default_smi_val[3]);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB4, default_smi_val[4]);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB5, default_smi_val[5]);

	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x200, 0x1b);
	/* disp(larb0+larb4): emi0, other:emi1 */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x220, (0x1<<0) | (0x1<<8));
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x234,
		       (0x1 << 31) + (0x13 << 26) + (0x14 << 21) + (0x0 << 20) + (0x2 << 15) +
		       (0x3 << 10) + (0x4 << 5) + 0x5);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x238,
		       (0x2 << 25) + (0x3 << 20) + (0x4 << 15) + (0x5 << 10) + (0x6 << 5) + 0x8);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x230, 0x1f + (0x8 << 5) + (0x6 << 10));

	/* Set VC priority: MMSYS = ISP > VENC > VDEC = MJC */
	M4U_WriteReg32(LARB0_BASE, 0x20, 0x0);	/* MMSYS */
	M4U_WriteReg32(LARB1_BASE, 0x20, 0x2);	/* VDEC */
	M4U_WriteReg32(LARB2_BASE, 0x20, 0x0);	/* ISP */
	M4U_WriteReg32(LARB3_BASE, 0x20, 0x1);	/* VENC */
	M4U_WriteReg32(LARB4_BASE, 0x20, 0x0);	/* DISP1 */
	M4U_WriteReg32(LARB5_BASE, 0x20, 0x1);	/* VENC2 */

	/* turn off EMI empty double OSTD */
	M4U_WriteReg32(LARB0_BASE, 0x2c, M4U_ReadReg32(LARB0_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB1_BASE, 0x2c, M4U_ReadReg32(LARB1_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB2_BASE, 0x2c, M4U_ReadReg32(LARB2_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB3_BASE, 0x2c, M4U_ReadReg32(LARB3_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB4_BASE, 0x2c, M4U_ReadReg32(LARB4_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB5_BASE, 0x2c, M4U_ReadReg32(LARB5_BASE, 0x2c) | (1 << 2));

	/* confirm. sometimes the reg can not be wrote while its clock is disable */
	if ((M4U_ReadReg32(LARB1_BASE, 0x20) != 0x2) ||
	   (M4U_ReadReg32(LARB0_BASE, 0x20) != 0x0)) {
		SMIMSG("warning setting failed. please check clk. 0x%x-0x%x\n",
			M4U_ReadReg32(LARB1_BASE , 0x20),
			M4U_ReadReg32(LARB0_BASE , 0x20));
	}
}

static void mt8173_vp_setting(struct mtk_smi_data *smidev)
{
	/* VP 4K */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, 0x17C0);	/* LARB0, DISP+MDP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, 0x161B);	/* LARB1, VDEC */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, 0x1000);	/* LARB2, ISP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB3, 0x1000);	/* LARB3, VENC+JPG */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB4, 0x17C0);	/* LARB4, DISP2+MDP2 */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB5, 0x1000);	/* LARB5, VENC2 */

	M4U_WriteReg32(LARB0_BASE, 0x200, 0x18);	/* ovl_ch0_0/1 */
	M4U_WriteReg32(LARB0_BASE, 0x214, 0x4);	/* mdp_rdma0; min(4,5) */
	M4U_WriteReg32(LARB0_BASE, 0x21c, 0x5);	/* mdp_wrot0 */

	M4U_WriteReg32(LARB4_BASE, 0x200, 0x8);	/* ovl_ch1_0/1 */
	M4U_WriteReg32(LARB4_BASE, 0x210, 0x4);	/* mdp_rdma1; min(4,5) */
	M4U_WriteReg32(LARB4_BASE, 0x214, 0x3);	/* mdp_wrot1 */

	M4U_WriteReg32(LARB1_BASE, 0x200, 0x1f);	/* port#0, mc */
	M4U_WriteReg32(LARB1_BASE, 0x204, 0x06);	/* port#1, pp */
	M4U_WriteReg32(LARB1_BASE, 0x208, 0x1);	/* port#2, ufo */
	M4U_WriteReg32(LARB1_BASE, 0x20c, 0x1);	/* port#3, vld */
	M4U_WriteReg32(LARB1_BASE, 0x210, 0x1);	/* port#4, vld2 */
	M4U_WriteReg32(LARB1_BASE, 0x214, 0x2);	/* port#5, mv */
	M4U_WriteReg32(LARB1_BASE, 0x218, 0x1);	/* port#6, pred rd */
	M4U_WriteReg32(LARB1_BASE, 0x21c, 0x1);	/* port#7, pred wr */
	M4U_WriteReg32(LARB1_BASE, 0x220, 0x1);	/* port#8, ppwrap */

}

static void mt8173_vr_setting(struct mtk_smi_data *smidev)
{
	/* VR 4K */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, 0x1614);	/* LARB0, DISP+MDP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, 0x1000);	/* LARB1, VDEC */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, 0x11F7);	/* LARB2, ISP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB3, 0x1584);	/* LARB3, VENC+JPG */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB4, 0x1614);	/* LARB4, DISP2+MDP2 */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB5, 0x1584);	/* LARB5, VENC2 */

	M4U_WriteReg32(LARB0_BASE, 0x200, 0x1f);	/* ovl_ch0_0+ovl_ch0_1 */
	M4U_WriteReg32(LARB0_BASE, 0x218, 0x2);	/* mdp_wdma */
	M4U_WriteReg32(LARB0_BASE, 0x21C, 0x5);	/* mdp_wrot0; min(9,5) */

	M4U_WriteReg32(LARB2_BASE, 0x200, 0x8);	/* imgo */
	M4U_WriteReg32(LARB2_BASE, 0x208, 0x1);	/* aao */
	M4U_WriteReg32(LARB2_BASE, 0x20c, 0x1);	/* lsco */
	M4U_WriteReg32(LARB2_BASE, 0x210, 0x1);	/* esfko */
	M4U_WriteReg32(LARB2_BASE, 0x218, 0x1);	/* lsci */
	M4U_WriteReg32(LARB2_BASE, 0x220, 0x1);	/* bpci */
	M4U_WriteReg32(LARB2_BASE, 0x22c, 0x4);	/* imgi */
	M4U_WriteReg32(LARB2_BASE, 0x230, 0x1);	/* img2o */
	M4U_WriteReg32(LARB2_BASE, 0x244, 0x1);	/* lcei */

	M4U_WriteReg32(LARB3_BASE, 0x200, 0x1);	/* venc_rcpu */
	M4U_WriteReg32(LARB3_BASE, 0x204, 0x4);	/* venc_rec_frm */
	M4U_WriteReg32(LARB3_BASE, 0x208, 0x1);	/* venc_bsdma */
	M4U_WriteReg32(LARB3_BASE, 0x20c, 0x1);	/* venc_sv_comv */
	M4U_WriteReg32(LARB3_BASE, 0x210, 0x1);	/* venc_rd_comv */
	M4U_WriteReg32(LARB3_BASE, 0x224, 0x8);	/* venc_cur_luma */
	M4U_WriteReg32(LARB3_BASE, 0x228, 0x4);	/* venc_cur_chroma */
	M4U_WriteReg32(LARB3_BASE, 0x230, 0x10);	/* venc_ref_chroma */

	M4U_WriteReg32(LARB4_BASE, 0x200, 0x1f);	/* ovl_ch1_0+ovl_ch1_1 */
	M4U_WriteReg32(LARB4_BASE, 0x218, 0x2);	/* mdp_wdma */
	M4U_WriteReg32(LARB4_BASE, 0x21C, 0x5);	/* mdp_wrot0; min(9,5) */


	/* VP concurrent settings */
	/* LARB0 */
	/*M4U_WriteReg32(LARB0_BASE, 0x210, 0x8); *//* port 4:ovl_ch1_0/1 */
	/*M4U_WriteReg32(LARB0_BASE, 0x21C, 0x4); *//* port 7:mdp_rdma0; min(4,5) */
	/*M4U_WriteReg32(LARB0_BASE, 0x22C, 0x3); *//* port11:mdp_wrot1 */

	/* VDEC */
	M4U_WriteReg32(LARB1_BASE, 0x200, 0x1f);	/* port#0, mc */
	M4U_WriteReg32(LARB1_BASE, 0x204, 0x06);	/* port#1, pp */
	M4U_WriteReg32(LARB1_BASE, 0x208, 0x1);	/* port#2, ufo */
	M4U_WriteReg32(LARB1_BASE, 0x20c, 0x1);	/* port#3, vld */
	M4U_WriteReg32(LARB1_BASE, 0x210, 0x1);	/* port#4, vld2 */
	M4U_WriteReg32(LARB1_BASE, 0x214, 0x2);	/* port#5, avc mv */
	M4U_WriteReg32(LARB1_BASE, 0x218, 0x1);	/* port#6, pred rd */
	M4U_WriteReg32(LARB1_BASE, 0x21c, 0x1);	/* port#7, pred wr */
	M4U_WriteReg32(LARB1_BASE, 0x220, 0x1);	/* port#8, ppwrap */

	/*venc2 */
	M4U_WriteReg32(LARB5_BASE, 0x200, 0x1);	/* venc_rcpu2 */
	M4U_WriteReg32(LARB5_BASE, 0x204, 0x4);	/* venc_rec_frm2 */
	/* venc_ref_luma2 */
	M4U_WriteReg32(LARB5_BASE, 0x20c, 0x10);	/* venc_ref_chroma2 */
	M4U_WriteReg32(LARB5_BASE, 0x210, 0x1);	/* venc_bsdma2 */
	M4U_WriteReg32(LARB5_BASE, 0x214, 0x8);	/* venc_cur_luma2 */
	M4U_WriteReg32(LARB5_BASE, 0x218, 0x4);	/* venc_cur_chroma2 */
	M4U_WriteReg32(LARB5_BASE, 0x21c, 0x1);	/* venc_rd_comv2 */
	M4U_WriteReg32(LARB5_BASE, 0x220, 0x1);	/* venc_sv_comv2 */

}


static void mt8173_hdmi_setting(struct mtk_smi_data *smidev)
{
	/* VP 4K */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, 0x1117);	/* LARB0, DISP+MDP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, 0x1659);	/* LARB1, VDEC */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, 0x1000);	/* LARB2, ISP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB3, 0x1000);	/* LARB3, VENC+JPG */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB4, 0x1750);	/* LARB4, DISP2+MDP2 */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB5, 0x1000);	/* LARB5, VENC2 */

	M4U_WriteReg32(LARB0_BASE, 0x200, 0x18);	/* ovl_ch0_0/1 */
	M4U_WriteReg32(LARB0_BASE, 0x214, 0x4);	/* mdp_rdma0; min(4,5) */
	M4U_WriteReg32(LARB0_BASE, 0x21c, 0x5);	/* mdp_wrot0 */

	M4U_WriteReg32(LARB4_BASE, 0x200, 0x8);	/* ovl_ch1_0/1 */
	M4U_WriteReg32(LARB4_BASE, 0x210, 0x4);	/* mdp_rdma1; min(4,5) */
	M4U_WriteReg32(LARB4_BASE, 0x214, 0x3);	/* mdp_wrot1 */

	M4U_WriteReg32(LARB1_BASE, 0x200, 0x1f);	/* port#0, mc */
	M4U_WriteReg32(LARB1_BASE, 0x204, 0x06);	/* port#1, pp */
	M4U_WriteReg32(LARB1_BASE, 0x208, 0x1);	/* port#2, ufo */
	M4U_WriteReg32(LARB1_BASE, 0x20c, 0x1);	/* port#3, vld */
	M4U_WriteReg32(LARB1_BASE, 0x210, 0x1);	/* port#4, vld2 */
	M4U_WriteReg32(LARB1_BASE, 0x214, 0x2);	/* port#5, mv */
	M4U_WriteReg32(LARB1_BASE, 0x218, 0x1);	/* port#6, pred rd */
	M4U_WriteReg32(LARB1_BASE, 0x21c, 0x1);	/* port#7, pred wr */
	M4U_WriteReg32(LARB1_BASE, 0x220, 0x1);	/* port#8, ppwrap */

}

static void mt8173_hdmi4k_setting(struct mtk_smi_data *smidev)
{

	/* VP 4K */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, 0x12A6);	/* LARB0, DISP+MDP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, 0x158B);	/* LARB1, VDEC */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, 0x1000);	/* LARB2, ISP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB3, 0x1000);	/* LARB3, VENC+JPG */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB4, 0x1A6D);	/* LARB4, DISP2+MDP2 */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB5, 0x1000);	/* LARB5, VENC2 */

	M4U_WriteReg32(LARB0_BASE, 0x200, 0x18);	/* ovl_ch0_0/1 */
	M4U_WriteReg32(LARB0_BASE, 0x214, 0x4);	/* mdp_rdma0; min(4,5) */
	M4U_WriteReg32(LARB0_BASE, 0x21c, 0x5);	/* mdp_wrot0 */

	M4U_WriteReg32(LARB4_BASE, 0x200, 0x8);	/* ovl_ch1_0/1 */
	M4U_WriteReg32(LARB4_BASE, 0x210, 0x4);	/* mdp_rdma1; min(4,5) */
	M4U_WriteReg32(LARB4_BASE, 0x214, 0x3);	/* mdp_wrot1 */

	M4U_WriteReg32(LARB1_BASE, 0x200, 0x1f);	/* port#0, mc */
	M4U_WriteReg32(LARB1_BASE, 0x204, 0x06);	/* port#1, pp */
	M4U_WriteReg32(LARB1_BASE, 0x208, 0x1);	/* port#2, ufo */
	M4U_WriteReg32(LARB1_BASE, 0x20c, 0x1);	/* port#3, vld */
	M4U_WriteReg32(LARB1_BASE, 0x210, 0x1);	/* port#4, vld2 */
	M4U_WriteReg32(LARB1_BASE, 0x214, 0x2);	/* port#5, mv */
	M4U_WriteReg32(LARB1_BASE, 0x218, 0x1);	/* port#6, pred rd */
	M4U_WriteReg32(LARB1_BASE, 0x21c, 0x1);	/* port#7, pred wr */
	M4U_WriteReg32(LARB1_BASE, 0x220, 0x1);	/* port#8, ppwrap */

}

/* Make sure all the clock is enabled */
const struct mtk_smi_priv smi_mt8173_priv = {
	.plat = MTK_PLAT_MT8173,
	.larb_port_num = {8, 9, 21, 15, 6, 9},
	.larb_vc_setting = { 0, 2, 0, 1, 0, 1 },
	.init_setting = mt8173_init_setting,
	.vp_setting = mt8173_vp_setting,
	.vr_setting = mt8173_vr_setting,
	.hdmi_setting = mt8173_hdmi_setting,
	.hdmi_4k_setting = mt8173_hdmi4k_setting,
};

