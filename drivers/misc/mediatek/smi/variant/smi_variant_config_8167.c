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

#define SMI_LARB0_PORT_NUM	8
#define SMI_LARB1_PORT_NUM	13
#define SMI_LARB2_PORT_NUM	7

static void mt8167_rest_setting(void)
{
	int i;

	/* initialize OSTD to 1 */
	for (i = 0; i < SMI_LARB0_PORT_NUM; i++)
		M4U_WriteReg32(LARB0_BASE, 0x200 + i * 4, 0x1);

	for (i = 0; i < SMI_LARB1_PORT_NUM; i++)
		M4U_WriteReg32(LARB1_BASE, 0x200 + i * 4, 0x1);

	for (i = 0; i < SMI_LARB2_PORT_NUM; i++)
		M4U_WriteReg32(LARB2_BASE, 0x200 + i * 4, 0x1);
}

static void mt8167_init_setting(struct mtk_smi_data *smidev, bool *default_saved,
			u32 *default_smi_val, unsigned int larbid)
{
	/* save default larb regs */
	if (!(*default_saved)) {
		SMIMSG("Save default config:\n");
		default_smi_val[0] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0);
		default_smi_val[1] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1);
		default_smi_val[2] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2);

		SMIMSG("l1arb[0-2]= 0x%x,  0x%x, 0x%x\n", default_smi_val[0],
		       default_smi_val[1], default_smi_val[2]);
		SMIMSG("l1arb[3]= 0x%x\n", default_smi_val[3]);

		*default_saved = true;
	}
	/* Keep the HW's init setting in REG_SMI_L1ARB0 ~ REG_SMI_L1ARB4 */
	if (!default_smi_val[0] && !default_smi_val[1] && !default_smi_val[2]) {
		M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, 0x14cb);
		M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, 0x1001);
		/* rainier do not have this larb, just larb2 is for vdec, set it from 8163 */
		M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, 0x1001);

	} else {
		M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, default_smi_val[0]);
		M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, default_smi_val[1]);
		M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, default_smi_val[2]);
	}
	/* SMIL1LEN*/
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x100, 0x2);
	/* 0x220 is controlled by M4U */
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x220, 0x1);disp: emi0, other:emi1 */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE,
			0x234, (0x8 + (0x6 << 8)));
	/* Register offset 0x230 have changed too much and needed to be checked with DE */
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x230, (0x1f + (0x8 << 2) + (0x7 << 7))); */

	/* Set VC priority: MMSYS = ISP > VENC > VDEC = MJC */
	M4U_WriteReg32(LARB0_BASE, 0x20, 0x0);	/* MMSYS */
	M4U_WriteReg32(LARB1_BASE, 0x20, 0x0);	/* VDEC */
	M4U_WriteReg32(LARB2_BASE, 0x20, 0x2);	/* ISP */
	/* M4U_WriteReg32(LARB3_BASE, 0x20, 0x1); */	/* VENC */
	/* M4U_WriteReg32(LARB4_BASE, 0x20, 0x2); // MJC */

	/* turn off EMI empty double OSTD, mt8167 do not have the EMI related register */
	/*
	M4U_WriteReg32(LARB0_BASE, 0x2c, M4U_ReadReg32(LARB0_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB1_BASE, 0x2c, M4U_ReadReg32(LARB1_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB2_BASE, 0x2c, M4U_ReadReg32(LARB2_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB3_BASE, 0x2c, M4U_ReadReg32(LARB3_BASE, 0x2c) | (1 << 2));
	*/
	/* M4U_WriteReg32(LARB4_BASE, 0x2c, M4U_ReadReg32(LARB4_BASE, 0x2c) | (1 << 2)); */

	/* for ISP HRT */
	M4U_WriteReg32(LARB1_BASE, 0x24, (M4U_ReadReg32(LARB2_BASE, 0x24) & 0xf7ffffff));

	/* for UI */
	mt8167_rest_setting();

	/* SMI common BW limiter, do not set SMI common BW limiter here, each register control on master */
	/*
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x104, 0x1A5A);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x108, 0x1000);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x10C, 0x1000);
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x110, 0x1000);
	*/
}

static void mt8167_vp_setting(struct mtk_smi_data *smidev)
{
	/* all the setting needed to be checked with DE, we could not determin the setting for now */
	return;
	/* VP 4K */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x204, 0x13DB);	/* LARB0, DISP+MDP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x208, 0x117D);	/* LARB1, VDEC */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x20C, 0x1000);	/* LARB2, ISP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x210, 0x10AD);	/* LARB3, VENC+JPG */
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x214, 0x1000); //LARB4, MJC */

	mt8167_rest_setting();

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

static void mt8167_vpwfd_setting(struct mtk_smi_data *smidev)
{
	/* all the setting needed to be checked with DE, we could not determin the setting for now */
	return;
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x204, 0x13DB);	/* LARB0, DISP+MDP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x208, 0x117D);	/* LARB1, VDEC */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x20C, 0x1000);	/* LARB2, ISP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x210, 0x10AD);	/* LARB3, VENC+JPG */
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x214, 0x1000); //LARB4, MJC */

	mt8167_rest_setting();

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

static void mt8167_vr_setting(struct mtk_smi_data *smidev)
{
	/* all the setting needed to be checked with DE, we could not determin the setting for now */
	return;
	/* SMI BW limit */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB0, 0x129F);	/* LARB0, DISP+MDP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB1, 0x1000);	/* LARB1, VDEC */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB2, 0x1224);	/* LARB2, ISP */
	M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB3, 0x1112);	/* LARB3, VENC+JPG */
	/* M4U_WriteReg32(SMI_COMMON_EXT_BASE, REG_OFFSET_SMI_L1ARB4, 0x1000); //LARB4, MJC */

	/* SMI LARB config */

	mt8167_rest_setting();

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

static void mt8167_hdmi_setting(struct mtk_smi_data *smidev)
{
}

static void mt8167_hdmi4k_setting(struct mtk_smi_data *smidev)
{
}

const struct mtk_smi_priv smi_mt8167_priv = {
	.plat = MTK_PLAT_MT8167,
	.larb_port_num = {
		SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM,
		SMI_LARB2_PORT_NUM
	},
	.larb_vc_setting = {0, 2, 0},
	.init_setting = mt8167_init_setting,
	.vp_setting = mt8167_vp_setting,
	.vp_wfd_setting = mt8167_vpwfd_setting,
	.vr_setting = mt8167_vr_setting,
	.hdmi_setting = mt8167_hdmi_setting,
	.hdmi_4k_setting = mt8167_hdmi4k_setting,
};

