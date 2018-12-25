/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Huiguo.Zhu <huiguo.zhu@mediatek.com>
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

#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/random.h>
#include <linux/wait.h>
#include <linux/delay.h>

#include "mtk_nr_reg.h"
#include "mtk_nr_ctrl.h"
#include "mtk_nr_def.h"

#define vNrWriteReg(dAddr, dVal)  (*((unsigned int *)(dAddr)) = (dVal))
#define vNrReadReg(dAddr)         (*(unsigned int *)(dAddr))
#define vNRWriteRegMsk(dAddr, dVal, dMsk) vNrWriteReg((dAddr), (vNrReadReg(dAddr) & (~(dMsk))) | ((dVal) & (dMsk)))

static const unsigned int _bNRQtyTbl[QUALITY_NR_MAX] = {
	0x05,			//QUALITY_SNR_MessSft_SM_Co1Mo
	0x05,			//QUALITY_SNR_MessThl_SM_Co1Mo
	0x05,			//QUALITY_SNR_MessSft_Mess_Co1Mo
	0x05,			//QUALITY_SNR_MessThl_Mess_Co1Mo
	0x05,			//QUALITY_SNR_MessSft_Edge_Co1Mo
	0x05,			//QUALITY_SNR_MessThl_Edge_Co1Mo
	0x05,			//QUALITY_SNR_MessSft_Mos_Co1Mo
	0x05,			//QUALITY_SNR_MessThl_Mos_Co1Mo
	0x03,			//QUALITY_SNR_MessSft_SM_Co1St
	0x03,			//QUALITY_SNR_MessThl_SM_Co1St
	0x03,			//QUALITY_SNR_MessSft_Mess_Co1St
	0x03,			//QUALITY_SNR_MessThl_Mess_Co1St
	0x03,			//QUALITY_SNR_MessSft_Edge_Co1St
	0x03,			//QUALITY_SNR_MessThl_Edge_Co1St
	0x03,			//QUALITY_SNR_MessSft_Mos_Co1St
	0x03,			//QUALITY_SNR_MessThl_Mos_Co1St
	0x06,			//QUALITY_SNR_BldLv_BK_Co1
	0x02,			//QUALITY_SNR_BldLv_SM_Co1
	0x01,			//QUALITY_SNR_BldLv_Mess_Co1
	0x01,			//QUALITY_SNR_BldLv_Edge_Co1
	0x06,			//QUALITY_SNR_BldLv_Mos_Co1
	0x04,			//QUALITY_SNR_MessSft_SM_Co2Mo
	0x04,			//QUALITY_SNR_MessThl_SM_Co2Mo
	0x04,			//QUALITY_SNR_MessSft_Mess_Co2Mo
	0x04,			//QUALITY_SNR_MessThl_Mess_Co2Mo
	0x04,			//QUALITY_SNR_MessSft_Edge_Co2Mo
	0x04,			//QUALITY_SNR_MessThl_Edge_Co2Mo
	0x04,			//QUALITY_SNR_MessSft_Mos_Co2Mo
	0x04,			//QUALITY_SNR_MessThl_Mos_Co2Mo
	0x04,			//QUALITY_SNR_MessSft_SM_Co2St
	0x04,			//QUALITY_SNR_MessThl_SM_Co2St
	0x04,			//QUALITY_SNR_MessSft_Mess_Co2St
	0x04,			//QUALITY_SNR_MessThl_Mess_Co2St
	0x04,			//QUALITY_SNR_MessSft_Edge_Co2St
	0x04,			//QUALITY_SNR_MessThl_Edge_Co2St
	0x04,			//QUALITY_SNR_MessSft_Mos_Co2St
	0x04,			//QUALITY_SNR_MessThl_Mos_Co2St
	0x06,			//QUALITY_SNR_BldLv_BK_Co2
	0x02,			//QUALITY_SNR_BldLv_SM_Co2
	0x01,			//QUALITY_SNR_BldLv_Mess_Co2
	0x01,			//QUALITY_SNR_BldLv_Edge_Co2
	0x06,			//QUALITY_SNR_BldLv_Mos_Co2
	0x05,			//QUALITY_SNR_MessSft_SM_Co3Mo
	0x05,			//QUALITY_SNR_MessThl_SM_Co3Mo
	0x05,			//QUALITY_SNR_MessSft_Mess_Co3Mo
	0x05,			//QUALITY_SNR_MessThl_Mess_Co3Mo
	0x05,			//QUALITY_SNR_MessSft_Edge_Co3Mo
	0x05,			//QUALITY_SNR_MessThl_Edge_Co3Mo
	0x05,			//QUALITY_SNR_MessSft_Mos_Co3Mo
	0x05,			//QUALITY_SNR_MessThl_Mos_Co3Mo
	0x05,			//QUALITY_SNR_MessSft_SM_Co3St
	0x05,			//QUALITY_SNR_MessThl_SM_Co3St
	0x05,			//QUALITY_SNR_MessSft_Mess_Co3St
	0x05,			//QUALITY_SNR_MessThl_Mess_Co3St
	0x05,			//QUALITY_SNR_MessSft_Edge_Co3St
	0x05,			//QUALITY_SNR_MessThl_Edge_Co3St
	0x05,			//QUALITY_SNR_MessSft_Mos_Co3St
	0x05,			//QUALITY_SNR_MessThl_Mos_Co3St
	0x06,			//QUALITY_SNR_BldLv_BK_Co3
	0x03,			//QUALITY_SNR_BldLv_SM_Co3
	0x01,			//QUALITY_SNR_BldLv_Mess_Co3
	0x01,			//QUALITY_SNR_BldLv_Edge_Co3
	0x06,			//QUALITY_SNR_BldLv_Mos_Co3
	0x05,			//QUALITY_SNR_MessSft_SM_FrSt
	0x05,			//QUALITY_SNR_MessThl_SM_FrSt
	0x05,			//QUALITY_SNR_MessSft_Mess_FrSt
	0x05,			//QUALITY_SNR_MessThl_Mess_FrSt
	0x05,			//QUALITY_SNR_MessSft_Edge_FrSt
	0x05,			//QUALITY_SNR_MessThl_Edge_FrSt
	0x05,			//QUALITY_SNR_MessSft_Mos_FrSt
	0x05,			//QUALITY_SNR_MessThl_Mos_FrSt
	0x06,			//QUALITY_SNR_BldLv_BK_FrSt
	0x01,			//QUALITY_SNR_BldLv_SM_FrSt
	0x01,			//QUALITY_SNR_BldLv_Mess_FrSt
	0x01,			//QUALITY_SNR_BldLv_Edge_FrSt
	0x06,			//QUALITY_SNR_BldLv_Mos_FrSt
	0x06,			//QUALITY_SNR_MessSft_SM_Mo
	0x06,			//QUALITY_SNR_MessThl_SM_Mo
	0x06,			//QUALITY_SNR_MessSft_Mess_Mo
	0x06,			//QUALITY_SNR_MessThl_Mess_Mo
	0x06,			//QUALITY_SNR_MessSft_Edge_Mo
	0x06,			//QUALITY_SNR_MessThl_Edge_Mo
	0x06,			//QUALITY_SNR_MessSft_Mos_Mo
	0x06,			//QUALITY_SNR_MessThl_Mos_Mo
	0x06,			//QUALITY_SNR_BldLv_BK_Mo
	0x03,			//QUALITY_SNR_BldLv_SM_Mo
	0x01,			//QUALITY_SNR_BldLv_Mess_Mo
	0x01,			//QUALITY_SNR_BldLv_Edge_Mo
	0x06,			//QUALITY_SNR_BldLv_Mos_Mo
	0x05,			//QUALITY_SNR_MessSft_SM_St
	0x05,			//QUALITY_SNR_MessThl_SM_St
	0x05,			//QUALITY_SNR_MessSft_Mess_St
	0x05,			//QUALITY_SNR_MessThl_Mess_St
	0x05,			//QUALITY_SNR_MessSft_Edge_St
	0x05,			//QUALITY_SNR_MessThl_Edge_St
	0x05,			//QUALITY_SNR_MessSft_Mos_St
	0x05,			//QUALITY_SNR_MessThl_Mos_St
	0x06,			//QUALITY_SNR_BldLv_BK_St
	0x03,			//QUALITY_SNR_BldLv_SM_St
	0x01,			//QUALITY_SNR_BldLv_Mess_St
	0x01,			//QUALITY_SNR_BldLv_Edge_St
	0x06,			//QUALITY_SNR_BldLv_Mos_St
	0x05,			//QUALITY_SNR_MessSft_SM_BK
	0x05,			//QUALITY_SNR_MessThl_SM_BK
	0x05,			//QUALITY_SNR_MessSft_Mess_BK
	0x05,			//QUALITY_SNR_MessThl_Mess_BK
	0x05,			//QUALITY_SNR_MessSft_Edge_BK
	0x05,			//QUALITY_SNR_MessThl_Edge_BK
	0x05,			//QUALITY_SNR_MessSft_Mos_BK
	0x05,			//QUALITY_SNR_MessThl_Mos_BK
	0x07,			//QUALITY_SNR_BldLv_BK_BK
	0x03,			//QUALITY_SNR_BldLv_SM_BK
	0x02,			//QUALITY_SNR_BldLv_Mess_BK
	0x02,			//QUALITY_SNR_BldLv_Edge_BK
	0x06,			//QUALITY_SNR_BldLv_Mos_BK
	0x05,			//QUALITY_SNR_MessSft_SM_Def
	0x05,			//QUALITY_SNR_MessThl_SM_Def
	0x05,			//QUALITY_SNR_MessSft_Mess_Def
	0x05,			//QUALITY_SNR_MessThl_Mess_Def
	0x05,			//QUALITY_SNR_MessSft_Edge_Def
	0x05,			//QUALITY_SNR_MessThl_Edge_Def
	0x05,			//QUALITY_SNR_MessSft_Mos_Def
	0x05,			//QUALITY_SNR_MessThl_Mos_Def
	0x02,			//QUALITY_SNR_BldLv_SM_Def
	0x01,			//QUALITY_SNR_BldLv_Mess_Def
	0x01,			//QUALITY_SNR_BldLv_Edge_Def
	0x06,			//QUALITY_SNR_BldLv_Mos_Def
	0x02,			//QUALITY_SNR_cur_sm_num
	0x0C,			//QUALITY_SNR_cur_sm_thr
	0x04,			//QUALITY_SNR_nearedge_selwidth
	0x19,			//QUALITY_SNR_nearedge_edge_thr
	0x0A,			//QUALITY_SNR_global_blend
	0x04,			//QUALITY_TNR_reg_tbthx1
	0x0C,			//QUALITY_TNR_reg_tbthx2
	0x14,			//QUALITY_TNR_reg_tbthx3
	0x1E,			//QUALITY_TNR_reg_tbthx4
	0x24,			//QUALITY_TNR_reg_tbthx5
	0x2A,			//QUALITY_TNR_reg_tbthx6
	0x30,			//QUALITY_TNR_reg_tbthx7
	0x38,			//QUALITY_TNR_reg_tbthx8
	0x04,			//QUALITY_TNR_Def_TBL0
	0x0D,			//QUALITY_TNR_Def_TBL1
	0x08,			//QUALITY_TNR_Def_TBL2
	0x06,			//QUALITY_TNR_Def_TBL3
	0x04,			//QUALITY_TNR_Def_TBL4
	0x03,			//QUALITY_TNR_Def_TBL5
	0x02,			//QUALITY_TNR_Def_TBL6
	0x01,			//QUALITY_TNR_Def_TBL7
	0x04,			//QUALITY_TNR_CIIR_TBL0
	0x0D,			//QUALITY_TNR_CIIR_TBL1
	0x08,			//QUALITY_TNR_CIIR_TBL2
	0x06,			//QUALITY_TNR_CIIR_TBL3
	0x04,			//QUALITY_TNR_CIIR_TBL4
	0x03,			//QUALITY_TNR_CIIR_TBL5
	0x02,			//QUALITY_TNR_CIIR_TBL6
	0x01,			//QUALITY_TNR_CIIR_TBL7
};

#define wReadNRQualityTable(wAddr)	 _bNRQtyTbl[wAddr]

static void MTK_NR_Hw_Reset(unsigned long reg_base)
{
	vNrWriteReg(reg_base + RW_NR_MAIN_CTRL_01, 0xfe000000);

	vNrWriteReg(reg_base + RW_NR_MAIN_CTRL_01, 0x00000000);
}

static void _vNRSetNoiseMeter(unsigned short u2PicWidth, unsigned short u2PicHeight, unsigned long reg_base)
{
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_80, ((u2PicHeight - 0x8) << 16) | 0x00000008, 0x07FF07FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_82, ((u2PicWidth - 0x8) << 16) | 0x00000008, 0x0FFF0FFF);
}

static void _vNRSetSwapMode(unsigned char u1SwapMode, unsigned long reg_base)
{
	unsigned int u4Value;

	switch (u1SwapMode) {
	case MT8520_SWAP_MODE_0:
		u4Value = vNrReadReg(reg_base + RW_NR_HD_MODE_CTRL);
		u4Value = u4Value & 0xFC8FFFFF;
		u4Value = u4Value | 0x01400000;
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, u4Value);

		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_00);
		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000040;

		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_00, u4Value);
#ifndef ONLY_2D_NR_SUPPORT
		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_10);
		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000040;
		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_10, u4Value);
#endif
		break;
	case MT8520_SWAP_MODE_1:
		u4Value = vNrReadReg(reg_base + RW_NR_HD_MODE_CTRL);
		u4Value = u4Value & 0xFC8FFFFF;
		u4Value = u4Value | 0x01500000;
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, u4Value);

		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_00);

		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000050;

		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_00, u4Value);
#ifndef ONLY_2D_NR_SUPPORT
		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_10);
		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000050;
		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_10, u4Value);
#endif
		break;
	case MT8520_SWAP_MODE_2:
		u4Value = vNrReadReg(reg_base + RW_NR_HD_MODE_CTRL);
		u4Value = u4Value & 0xFC8FFFFF;
		u4Value = u4Value | 0x01600000;
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, u4Value);

		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_00);

		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000060;

		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_00, u4Value);
#ifndef ONLY_2D_NR_SUPPORT
		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_10);
		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000060;
		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_10, u4Value);
#endif
		break;
	case MT5351_SWAP_MODE_0:
		u4Value = vNrReadReg(reg_base + RW_NR_HD_MODE_CTRL);
		u4Value = u4Value & 0xFC8FFFFF;
		u4Value = u4Value | 0x00000000;
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, u4Value);

		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_00);

		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000000;

		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_00, u4Value);
#ifndef ONLY_2D_NR_SUPPORT
		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_10);
		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000000;
		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_10, u4Value);
#endif
		break;
	case MT5351_SWAP_MODE_1:
		u4Value = vNrReadReg(reg_base + RW_NR_HD_MODE_CTRL);
		u4Value = u4Value & 0xFC8FFFFF;
		u4Value = u4Value | 0x00100000;
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, u4Value);

		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_00);
		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000010;

		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_00, u4Value);
#ifndef ONLY_2D_NR_SUPPORT
		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_10);
		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000010;
		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_10, u4Value);
#endif
		break;
	case MT5351_SWAP_MODE_2:
		u4Value = vNrReadReg(reg_base + RW_NR_HD_MODE_CTRL);
		u4Value = u4Value & 0xFC8FFFFF;
		u4Value = u4Value | 0x00200000;
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, u4Value);

		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_00);

		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000020;

		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_00, u4Value);
#ifndef ONLY_2D_NR_SUPPORT
		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_10);
		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000020;
		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_10, u4Value);
#endif
		break;
	case MT5351_SWAP_MODE_3:
		u4Value = vNrReadReg(reg_base + RW_NR_HD_MODE_CTRL);
		u4Value = u4Value & 0xFC8FFFFF;
		u4Value = u4Value | 0x01300000;
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, u4Value);

		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_00);

		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000030;

		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_00, u4Value);
#ifndef ONLY_2D_NR_SUPPORT
		u4Value = vNrReadReg(reg_base + RW_NR_DRAM_CTRL_10);
		u4Value = u4Value & 0xFFFFFF8F;
		u4Value = u4Value | 0x00000030;
		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_10, u4Value);
#endif
		break;
	}
}

static void _vNRSetDramBurstRead(unsigned long reg_base, bool fgEnable)
{
	unsigned int u4Value;

	if ((vNrReadReg(reg_base + RW_NR_HD_LINE_OFST) & 0x3) != 0)
		fgEnable = false;

	if (fgEnable) {
		u4Value = (vNrReadReg(reg_base + RW_NR_HD_MODE_CTRL) | BURST_READ);
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, u4Value);
#ifndef ONLY_2D_NR_SUPPORT
		u4Value = (vNrReadReg(reg_base + RW_NR_DRAM_CTRL_10) | LAST_BURST_READ);
		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_10, u4Value);
#endif
	} else {
		u4Value = (vNrReadReg(reg_base + RW_NR_HD_MODE_CTRL) & (~BURST_READ));
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, u4Value);
#ifndef ONLY_2D_NR_SUPPORT
		u4Value = (vNrReadReg(reg_base + RW_NR_DRAM_CTRL_10) & (~LAST_BURST_READ));
		vNrWriteReg(reg_base + RW_NR_DRAM_CTRL_10, u4Value);
#endif
	}
}

static void _vNRAdaptiveBNRPara(struct NR_PRM_INFO_T *ptNrPrm, unsigned long reg_base)
{
	unsigned char u1HineCnt, u1VLineCnt, u1HBKLv, u1VBKLv;
	unsigned int u4RegValue;

	u4RegValue = vNrReadReg(reg_base + RW_NR_2DNR_CTRL_8F);

	u1HineCnt = ((u4RegValue >> 8) & 0x7FF);
	u1VLineCnt = ((u4RegValue >> 20) & 0x3FF);

	u1HBKLv = ((u4RegValue >> 0) & 0x7);
	u1VBKLv = ((u4RegValue >> 4) & 0x7);

	if (u1VLineCnt >= ptNrPrm->u2PicWidth / (16 * 2) ||
	    u1HineCnt >= ptNrPrm->u2PicHeight / (16 * 2)) {
		ptNrPrm->fgUseBlockMeter = false;
	} else {
		ptNrPrm->fgUseBlockMeter = true;
	}

	if (u1VBKLv == 7) {
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_08, 0x0005A040, 0x007FF7FF);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0C, 0x0006405A, 0x007FF7FF);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_10, 0x0005A040, 0x007FF7FF);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_14, 0x0005A040, 0x007FF7FF);
	} else {
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_08, 0x0002D020, 0x007FF7FF);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0C, 0x0003602D, 0x007FF7FF);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_10, 0x0002D820, 0x007FF7FF);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_14, 0x0002D020, 0x007FF7FF);
	}
	if (u1HBKLv == 7) {
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0A, 0x20180000, 0xFFFF0000);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0E, 0x20180000, 0xFFFF0000);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_12, 0x20180000, 0xFFFF0000);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_16, 0x20180000, 0xFFFF0000);
	} else {
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0A, 0x0E0A0000, 0xFFFF0000);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0E, 0x0E0A0000, 0xFFFF0000);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_12, 0x0E0A0000, 0xFFFF0000);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_16, 0x0E0A0000, 0xFFFF0000);
	}
}

static void MTK_NR_Level_Set(unsigned int u4Strength, unsigned int u4FNRStrength,
		      unsigned int u4MNRStrength, unsigned int u4BNRStrength, unsigned long reg_base)
{
	// 2D NR setting
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_CO1MO) << 24,
		       MESSSFT_SMOOTH_CO1MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_CO1MO) << 16,
		       MESSTHL_SMOOTH_CO1MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38,
		       wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_CO1MO) << 24,
		       MESSSFT_MESS_CO1MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38,
		       wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_CO1MO) << 16,
		       MESSTHL_MESS_CO1MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_CO1MO) << 8,
		       MESSSFT_EDGE_CO1MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_CO1MO) << 0,
		       MESSTHL_EDGE_CO1MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_CO1ST) << 24,
		       MESSSFT_SMOOTH_CO1ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_CO1ST) << 16,
		       MESSTHL_SMOOTH_CO1ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A,
		       wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_CO1ST) << 24,
		       MESSSFT_MESS_CO1ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A,
		       wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_CO1ST) << 16,
		       MESSTHL_MESS_CO1ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_CO1ST) << 8,
		       MESSSFT_EDGE_CO1ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_CO1ST) << 0,
		       MESSTHL_EDGE_CO1ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, wReadNRQualityTable(QUALITY_SNR_BLDLV_SM_CO1) << 8,
		       BLDLV_SM_CO1);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, wReadNRQualityTable(QUALITY_SNR_BLDLV_MESS_CO1) << 4,
		       BLDLV_MESS_CO1);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, wReadNRQualityTable(QUALITY_SNR_BLDLV_EDGE_CO1) << 0,
		       BLDLV_EDGE_CO1);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_CO2MO) << 24,
		       MESSSFT_SMOOTH_CO2MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_CO2MO) << 16,
		       MESSTHL_SMOOTH_CO2MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C,
		       wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_CO2MO) << 24,
		       MESSSFT_MESS_CO2MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C,
		       wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_CO2MO) << 16,
		       MESSTHL_MESS_CO2MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_CO2MO) << 8,
		       MESSSFT_EDGE_CO2MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_CO2MO) << 0,
		       MESSTHL_EDGE_CO2MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_CO2ST) << 24,
		       MESSSFT_SMOOTH_CO2ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_CO2ST) << 16,
		       MESSTHL_SMOOTH_CO2ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E,
		       wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_CO2ST) << 24,
		       MESSSFT_MESS_CO2ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E,
		       wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_CO2ST) << 16,
		       MESSTHL_MESS_CO2ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_CO2ST) << 8,
		       MESSSFT_EDGE_CO2ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_CO2ST) << 0,
		       MESSTHL_EDGE_CO2ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, wReadNRQualityTable(QUALITY_SNR_BLDLV_SM_CO2) << 24,
		       BLDLV_SM_CO2);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, wReadNRQualityTable(QUALITY_SNR_BLDLV_MESS_CO2) << 20,
		       BLDLV_MESS_CO2);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, wReadNRQualityTable(QUALITY_SNR_BLDLV_EDGE_CO2) << 16,
		       BLDLV_EDGE_CO2);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_CO3MO) << 24,
		       MESSSFT_SMOOTH_CO3MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_CO3MO) << 16,
		       MESSTHL_SMOOTH_CO3MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40,
		       wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_CO3MO) << 24,
		       MESSSFT_MESS_CO3MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40,
		       wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_CO3MO) << 16,
		       MESSTHL_MESS_CO3MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_CO3MO) << 8,
		       MESSSFT_EDGE_CO3MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_CO3MO) << 0,
		       MESSTHL_EDGE_CO3MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_CO3ST) << 24,
		       MESSSFT_SMOOTH_CO3ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_CO3ST) << 16,
		       MESSTHL_SMOOTH_CO3ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42,
		       wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_CO3ST) << 24,
		       MESSSFT_MESS_CO3ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42,
		       wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_CO3ST) << 16,
		       MESSTHL_MESS_CO3ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_CO3ST) << 8,
		       MESSSFT_EDGE_CO3ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_CO3ST) << 0,
		       MESSTHL_EDGE_CO3ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, wReadNRQualityTable(QUALITY_SNR_BLDLV_SM_CO3) << 8,
		       BLDLV_SM_CO3);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, wReadNRQualityTable(QUALITY_SNR_BLDLV_MESS_CO3) << 4,
		       BLDLV_MESS_CO3);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, wReadNRQualityTable(QUALITY_SNR_BLDLV_EDGE_CO3) << 0,
		       BLDLV_EDGE_CO3);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_FRST) << 24,
		       MESSSFT_SMOOTH_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_FRST) << 16,
		       MESSTHL_SMOOTH_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_FRST) << 24,
		       MESSSFT_MESS_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_FRST) << 16,
		       MESSTHL_MESS_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_FRST) << 8,
		       MESSSFT_EDGE_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_FRST) << 0,
		       MESSTHL_EDGE_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, wReadNRQualityTable(QUALITY_SNR_BLDLV_SM_FRST) << 24,
		       BLDLV_SM_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, wReadNRQualityTable(QUALITY_SNR_BLDLV_MESS_FRST) << 20,
		       BLDLV_MESS_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, wReadNRQualityTable(QUALITY_SNR_BLDLV_EDGE_FRST) << 16,
		       BLDLV_EDGE_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_MO) << 24,
		       MESSSFT_SMOOTH_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_MO) << 16,
		       MESSTHL_SMOOTH_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_MO) << 24,
		       MESSSFT_MESS_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_MO) << 16,
		       MESSTHL_MESS_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_MO) << 8,
		       MESSSFT_EDGE_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_MO) << 0,
		       MESSTHL_EDGE_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, wReadNRQualityTable(QUALITY_SNR_BLDLV_SM_MO) << 8,
		       BLDLV_SM_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, wReadNRQualityTable(QUALITY_SNR_BLDLV_MESS_MO) << 4,
		       BLDLV_MESS_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, wReadNRQualityTable(QUALITY_SNR_BLDLV_EDGE_MO) << 0,
		       BLDLV_EDGE_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_ST) << 24,
		       MESSSFT_SMOOTH_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_ST) << 16,
		       MESSTHL_SMOOTH_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_ST) << 24,
		       MESSSFT_MESS_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_ST) << 16,
		       MESSTHL_MESS_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_ST) << 8,
		       MESSSFT_EDGE_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_ST) << 0,
		       MESSTHL_EDGE_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, wReadNRQualityTable(QUALITY_SNR_BLDLV_SM_ST) << 24,
		       BLDLV_SM_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, wReadNRQualityTable(QUALITY_SNR_BLDLV_MESS_ST) << 20,
		       BLDLV_MESS_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, wReadNRQualityTable(QUALITY_SNR_BLDLV_EDGE_ST) << 16,
		       BLDLV_EDGE_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_BK) << 24,
		       MESSSFT_SMOOTH_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_BK) << 16,
		       MESSTHL_SMOOTH_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_BK) << 24,
		       MESSSFT_MESS_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_BK) << 16,
		       MESSTHL_MESS_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_BK) << 8,
		       MESSSFT_EDGE_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_BK) << 0,
		       MESSTHL_EDGE_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, wReadNRQualityTable(QUALITY_SNR_BLDLV_SM_BK) << 24,
		       BLDLV_SM_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, wReadNRQualityTable(QUALITY_SNR_BLDLV_MESS_BK) << 20,
		       BLDLV_MESS_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, wReadNRQualityTable(QUALITY_SNR_BLDLV_EDGE_BK) << 16,
		       BLDLV_EDGE_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, wReadNRQualityTable(QUALITY_SNR_MESSSFT_SM_DEF) << 24,
		       MESSSFT_SMOOTH_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, wReadNRQualityTable(QUALITY_SNR_MESSTHL_SM_DEF) << 16,
		       MESSTHL_SMOOTH_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MESS_DEF) << 24,
		       MESSSFT_MESS_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MESS_DEF) << 16,
		       MESSTHL_MESS_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, wReadNRQualityTable(QUALITY_SNR_MESSSFT_EDGE_DEF) << 8,
		       MESSSFT_EDGE_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, wReadNRQualityTable(QUALITY_SNR_MESSTHL_EDGE_DEF) << 0,
		       MESSTHL_EDGE_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, wReadNRQualityTable(QUALITY_SNR_BLDLV_SM_DEF) << 8,
		       BLDLV_SM_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, wReadNRQualityTable(QUALITY_SNR_BLDLV_MESS_DEF) << 4,
		       BLDLV_MESS_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, wReadNRQualityTable(QUALITY_SNR_BLDLV_EDGE_DEF) << 0,
		       BLDLV_EDGE_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_6E, wReadNRQualityTable(QUALITY_SNR_GLOBAL_BLEND) << 28,
		       Y_GLOBAL_BLEND);

	// BNR setting
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_CO1MO) << 8,
		       MESSSFT_MOS_CO1MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_CO1MO) << 0,
		       MESSTHL_MOS_CO1MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_CO1ST) << 8,
		       MESSSFT_MOS_CO1ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_CO1ST) << 0,
		       MESSTHL_MOS_CO1ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, wReadNRQualityTable(QUALITY_SNR_BLDLV_BK_CO1) << 12,
		       BLDLV_BK_CO1);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, wReadNRQualityTable(QUALITY_SNR_BLDLV_MOS_CO1) << 4,
		       BLDLV_MOS_CO1);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_CO2MO) << 8,
		       MESSSFT_MOS_CO2MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_CO2MO) << 0,
		       MESSTHL_MOS_CO2MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_CO2ST) << 8,
		       MESSSFT_MOS_CO2ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_CO2ST) << 0,
		       MESSTHL_MOS_CO2ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, wReadNRQualityTable(QUALITY_SNR_BLDLV_BK_CO2) << 28,
		       BLDLV_BK_CO2);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, wReadNRQualityTable(QUALITY_SNR_BLDLV_MOS_CO2) << 8,
		       BLDLV_MOS_CO2);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_CO3MO) << 8,
		       MESSSFT_MOS_CO3MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_CO3MO) << 0,
		       MESSTHL_MOS_CO3MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_CO3ST) << 8,
		       MESSSFT_MOS_CO3ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_CO3ST) << 0,
		       MESSTHL_MOS_CO3ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, wReadNRQualityTable(QUALITY_SNR_BLDLV_BK_CO3) << 12,
		       BLDLV_BK_CO3);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, wReadNRQualityTable(QUALITY_SNR_BLDLV_MOS_CO3) << 12,
		       BLDLV_MOS_CO3);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_FRST) << 8,
		       MESSSFT_MOS_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_FRST) << 0,
		       MESSTHL_MOS_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, wReadNRQualityTable(QUALITY_SNR_BLDLV_BK_FRST) << 28,
		       BLDLV_BK_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, wReadNRQualityTable(QUALITY_SNR_BLDLV_MOS_FRST) << 0,
		       BLDLV_NEAR_FRST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_MO) << 8,
		       MESSSFT_MOS_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_MO) << 0,
		       MESSTHL_MOS_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, wReadNRQualityTable(QUALITY_SNR_BLDLV_BK_MO) << 12,
		       BLDLV_BK_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, wReadNRQualityTable(QUALITY_SNR_BLDLV_MOS_MO) << 24,
		       BLDLV_MOS_MO);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_ST) << 8,
		       MESSSFT_MOS_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_ST) << 0,
		       MESSTHL_MOS_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, wReadNRQualityTable(QUALITY_SNR_BLDLV_BK_ST) << 28,
		       BLDLV_BK_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, wReadNRQualityTable(QUALITY_SNR_BLDLV_MOS_ST) << 20,
		       BLDLV_MOS_ST);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_BK) << 8,
		       MESSSFT_MOS_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_BK) << 0,
		       MESSTHL_MOS_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, wReadNRQualityTable(QUALITY_SNR_BLDLV_BK_BK) << 28,
		       BLDLV_BK_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, wReadNRQualityTable(QUALITY_SNR_BLDLV_MOS_BK) << 28,
		       BLDLV_MOS_BK);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, wReadNRQualityTable(QUALITY_SNR_MESSSFT_MOS_DEF) << 8,
		       MESSSFT_MOS_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, wReadNRQualityTable(QUALITY_SNR_MESSTHL_MOS_DEF) << 0,
		       MESSTHL_MOS_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, wReadNRQualityTable(QUALITY_SNR_BLDLV_MOS_DEF) << 16,
		       BLDLV_MOS_DEF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_65, wReadNRQualityTable(QUALITY_SNR_CUR_SM_NUM) << 8,
		       SM_NUM_THR);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_65, wReadNRQualityTable(QUALITY_SNR_CUR_SM_THR) << 24,
		       MNR_SM_THR);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_65, wReadNRQualityTable(QUALITY_SNR_NEAREDGE_SELWIDTH) << 0,
		       NEAREDGE_SEL_WIDTH);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_65, wReadNRQualityTable(QUALITY_SNR_NEAREDGE_EDGE_THR) << 16,
		       MNR_EDGE_THR);

	// 3D NR setting
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2C, wReadNRQualityTable(QUALITY_TNR_REG_TBTHX1) << 0,
		       REGTBTHX1);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2C, wReadNRQualityTable(QUALITY_TNR_REG_TBTHX2) << 8,
		       REGTBTHX2);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2C, wReadNRQualityTable(QUALITY_TNR_REG_TBTHX3) << 16,
		       REGTBTHX3);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2C, wReadNRQualityTable(QUALITY_TNR_REG_TBTHX4) << 24,
		       REGTBTHX4);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2D, wReadNRQualityTable(QUALITY_TNR_REG_TBTHX5) << 0,
		       REGTBTHX5);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2D, wReadNRQualityTable(QUALITY_TNR_REG_TBTHX6) << 8,
		       REGTBTHX6);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2D, wReadNRQualityTable(QUALITY_TNR_REG_TBTHX7) << 16,
		       REGTBTHX7);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2D, wReadNRQualityTable(QUALITY_TNR_REG_TBTHX8) << 24,
		       REGTBTHX8);

	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_1C, (wReadNRQualityTable(QUALITY_TNR_DEF_TBL7) << 28 |
					    wReadNRQualityTable(QUALITY_TNR_DEF_TBL6) << 24 |
					    wReadNRQualityTable(QUALITY_TNR_DEF_TBL5) << 20 |
					    wReadNRQualityTable(QUALITY_TNR_DEF_TBL4) << 16 |
					    wReadNRQualityTable(QUALITY_TNR_DEF_TBL3) << 12 |
					    wReadNRQualityTable(QUALITY_TNR_DEF_TBL2) << 8 |
					    wReadNRQualityTable(QUALITY_TNR_DEF_TBL1) << 4 |
					    wReadNRQualityTable(QUALITY_TNR_DEF_TBL0) << 0),
		       DEF_TBL);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_09,
		       (wReadNRQualityTable(QUALITY_TNR_CIIR_TBL7) << 28 |
			wReadNRQualityTable(QUALITY_TNR_CIIR_TBL6) << 24 |
			wReadNRQualityTable(QUALITY_TNR_CIIR_TBL5) << 20 |
			wReadNRQualityTable(QUALITY_TNR_CIIR_TBL4) << 16 |
			wReadNRQualityTable(QUALITY_TNR_CIIR_TBL3) << 12 |
			wReadNRQualityTable(QUALITY_TNR_CIIR_TBL2) << 8 |
			wReadNRQualityTable(QUALITY_TNR_CIIR_TBL1) << 4 |
			wReadNRQualityTable(QUALITY_TNR_CIIR_TBL0) << 0), CIIR_TBL);


	if (u4Strength != NR_STRENGTH_OFF) {
		switch (u4Strength) {
		case NR_STRENGTH_LOW:
			vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1D, 0x00112344);	// big motion table
			vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1B, 0x00124568);	// small motion table
			vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1C, 0x00112456);	// default table
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x4 << 12), BLDLV_BK_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x4 << 28), BLDLV_BK_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x4 << 12), BLDLV_BK_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x4 << 28), BLDLV_BK_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x4 << 12), BLDLV_BK_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x4 << 28), BLDLV_BK_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x4 << 12), BLDLV_BK_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x4 << 28), BLDLV_BK_BK);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 4), BLDLV_MOS_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 8), BLDLV_MOS_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 12), BLDLV_MOS_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 20), BLDLV_MOS_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 24), BLDLV_MOS_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 0), BLDLV_NEAR_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 16), BLDLV_MOS_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 28), BLDLV_MOS_BK);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 8), BLDLV_SM_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 4), BLDLV_MESS_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 0), BLDLV_EDGE_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 24), BLDLV_SM_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 20), BLDLV_MESS_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 16), BLDLV_EDGE_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 8), BLDLV_SM_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 4), BLDLV_MESS_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 0), BLDLV_EDGE_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 24), BLDLV_SM_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 20), BLDLV_MESS_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 16), BLDLV_EDGE_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x1 << 8), BLDLV_SM_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x1 << 4), BLDLV_MESS_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x1 << 0), BLDLV_EDGE_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 24), BLDLV_SM_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 20), BLDLV_MESS_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 16), BLDLV_EDGE_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 8), BLDLV_SM_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 4), BLDLV_MESS_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 0), BLDLV_EDGE_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, 0x48084808, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x20000000, 0x28000000);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8E, 0x00000000, 0x02000000);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x10000000, 0x14000000);
			break;
		case NR_STRENGTH_MED:
			vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1D, 0x01123445);	// big motion table
			vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1B, 0x01246789);	// small motion table
			vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1C, 0x01124678);	// default table
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x6 << 12), BLDLV_BK_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x6 << 28), BLDLV_BK_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x6 << 12), BLDLV_BK_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x6 << 28), BLDLV_BK_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x6 << 12), BLDLV_BK_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x6 << 28), BLDLV_BK_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x6 << 12), BLDLV_BK_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x6 << 28), BLDLV_BK_BK);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 4), BLDLV_MOS_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 8), BLDLV_MOS_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 12), BLDLV_MOS_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 20), BLDLV_MOS_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 24), BLDLV_MOS_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 0), BLDLV_NEAR_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 16), BLDLV_MOS_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 28), BLDLV_MOS_BK);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 8), BLDLV_SM_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 4), BLDLV_MESS_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 0), BLDLV_EDGE_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 24), BLDLV_SM_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 20), BLDLV_MESS_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 16), BLDLV_EDGE_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 8), BLDLV_SM_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 4), BLDLV_MESS_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 0), BLDLV_EDGE_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 24), BLDLV_SM_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 20), BLDLV_MESS_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 16), BLDLV_EDGE_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x2 << 8), BLDLV_SM_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x2 << 4), BLDLV_MESS_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x2 << 0), BLDLV_EDGE_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 24), BLDLV_SM_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 20), BLDLV_MESS_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 16), BLDLV_EDGE_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 8), BLDLV_SM_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 4), BLDLV_MESS_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 0), BLDLV_EDGE_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, 0x4B0B4B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x08000000, 0x28000000);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8E, 0x00000000, 0x02000000);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x10000000, 0x14000000);
			break;
		case NR_STRENGTH_HIGH:
			vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1D, 0x11234566);	// big motion table
			vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1B, 0x124689AA);	// small motion table
			vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1C, 0x11246899);	// default table
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x8 << 12), BLDLV_BK_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x8 << 28), BLDLV_BK_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x8 << 12), BLDLV_BK_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x8 << 28), BLDLV_BK_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x8 << 12), BLDLV_BK_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x8 << 28), BLDLV_BK_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x8 << 12), BLDLV_BK_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x8 << 28), BLDLV_BK_BK);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 4), BLDLV_MOS_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 8), BLDLV_MOS_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 12), BLDLV_MOS_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 20), BLDLV_MOS_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 24), BLDLV_MOS_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 0), BLDLV_NEAR_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 16), BLDLV_MOS_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 28), BLDLV_MOS_BK);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 8), BLDLV_SM_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 4), BLDLV_MESS_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 0), BLDLV_EDGE_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 24), BLDLV_SM_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 20), BLDLV_MESS_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 16), BLDLV_EDGE_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 8), BLDLV_SM_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 4), BLDLV_MESS_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 0), BLDLV_EDGE_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 24), BLDLV_SM_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 20), BLDLV_MESS_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 16), BLDLV_EDGE_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x3 << 8), BLDLV_SM_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x3 << 4), BLDLV_MESS_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x3 << 0), BLDLV_EDGE_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 24), BLDLV_SM_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 20), BLDLV_MESS_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 16), BLDLV_EDGE_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 8), BLDLV_SM_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 4), BLDLV_MESS_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 0), BLDLV_EDGE_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, 0x8F0F8B0B, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, 0x8B0B8F0F, 0xFF3FFF3F);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x08000000, 0x28000000);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8E, 0x00000000, 0x02000000);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x04000000, 0x14000000);
			break;
		default:
			break;
		}
	} else {
		if (u4FNRStrength == NR_STRENGTH_OFF) {
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_02, 0x00001000, 0x00001000);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3C, 0x00000000, 0x0000003F);
		} else {
			switch (u4FNRStrength) {
			case NR_STRENGTH_LOW:
				vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1D, 0x00112344);	// big motion table
				vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1B, 0x00124568);	// small motion table
				vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1C, 0x00112456);	// default table
				break;
			case NR_STRENGTH_MED:
				vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1D, 0x01123445);	// big motion table
				vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1B, 0x01246789);	// small motion table
				vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1C, 0x01124678);	// default table
				break;
			case NR_STRENGTH_HIGH:
				vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1D, 0x11234566);	// big motion table
				vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1B, 0x124689AA);	// small motion table
				vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1C, 0x11246899);	// default table
				break;
			default:
				break;
			}
		}

		if (u4MNRStrength == NR_STRENGTH_OFF) {
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x0 << 4), BLDLV_MOS_CO1);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x0 << 8), BLDLV_MOS_CO2);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x0 << 12), BLDLV_MOS_CO3);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x0 << 20), BLDLV_MOS_ST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x0 << 24), BLDLV_MOS_MO);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x0 << 0), BLDLV_NEAR_FRST);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x0 << 16), BLDLV_MOS_DEF);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x0 << 28), BLDLV_MOS_BK);
		} else {
			switch (u4MNRStrength) {
			case NR_STRENGTH_LOW:
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x4 << 12), BLDLV_BK_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x4 << 28), BLDLV_BK_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x4 << 12), BLDLV_BK_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x4 << 28), BLDLV_BK_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x4 << 12), BLDLV_BK_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x4 << 28), BLDLV_BK_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x4 << 12), BLDLV_BK_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x4 << 28), BLDLV_BK_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 4), BLDLV_MOS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 8), BLDLV_MOS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 12), BLDLV_MOS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 20), BLDLV_MOS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 24), BLDLV_MOS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 0), BLDLV_NEAR_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 16), BLDLV_MOS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 28), BLDLV_MOS_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 8), BLDLV_SM_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 4), BLDLV_MESS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 0), BLDLV_EDGE_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 24), BLDLV_SM_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 20), BLDLV_MESS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 16), BLDLV_EDGE_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 8), BLDLV_SM_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 4), BLDLV_MESS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 0), BLDLV_EDGE_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 24), BLDLV_SM_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 20), BLDLV_MESS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 16), BLDLV_EDGE_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x1 << 8), BLDLV_SM_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x1 << 4), BLDLV_MESS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x1 << 0), BLDLV_EDGE_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 24), BLDLV_SM_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 20), BLDLV_MESS_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 16), BLDLV_EDGE_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 8), BLDLV_SM_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 4), BLDLV_MESS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 0), BLDLV_EDGE_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x20000000, 0x28000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8E, 0x00000000, 0x02000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x10000000, 0x14000000);
				break;
			case NR_STRENGTH_MED:
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x6 << 12), BLDLV_BK_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x6 << 28), BLDLV_BK_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x6 << 12), BLDLV_BK_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x6 << 28), BLDLV_BK_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x6 << 12), BLDLV_BK_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x6 << 28), BLDLV_BK_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x6 << 12), BLDLV_BK_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x6 << 28), BLDLV_BK_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 4), BLDLV_MOS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 8), BLDLV_MOS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 12), BLDLV_MOS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 20), BLDLV_MOS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 24), BLDLV_MOS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 0), BLDLV_NEAR_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 16), BLDLV_MOS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 28), BLDLV_MOS_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 8), BLDLV_SM_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 4), BLDLV_MESS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 0), BLDLV_EDGE_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 24), BLDLV_SM_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 20), BLDLV_MESS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 16), BLDLV_EDGE_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 8), BLDLV_SM_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 4), BLDLV_MESS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 0), BLDLV_EDGE_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 24), BLDLV_SM_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 20), BLDLV_MESS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 16), BLDLV_EDGE_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x2 << 8), BLDLV_SM_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x2 << 4), BLDLV_MESS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x2 << 0), BLDLV_EDGE_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 24), BLDLV_SM_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 20), BLDLV_MESS_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 16), BLDLV_EDGE_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 8), BLDLV_SM_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 4), BLDLV_MESS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 0), BLDLV_EDGE_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x08000000, 0x28000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8E, 0x00000000, 0x02000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x10000000, 0x14000000);
				break;
			case NR_STRENGTH_HIGH:
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x8 << 12), BLDLV_BK_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x8 << 28), BLDLV_BK_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x8 << 12), BLDLV_BK_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x8 << 28), BLDLV_BK_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x8 << 12), BLDLV_BK_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x8 << 28), BLDLV_BK_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x8 << 12), BLDLV_BK_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x8 << 28), BLDLV_BK_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 4), BLDLV_MOS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 8), BLDLV_MOS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 12), BLDLV_MOS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 20), BLDLV_MOS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 24), BLDLV_MOS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 0), BLDLV_NEAR_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 16), BLDLV_MOS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 28), BLDLV_MOS_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 8), BLDLV_SM_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 4), BLDLV_MESS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 0), BLDLV_EDGE_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 24), BLDLV_SM_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 20), BLDLV_MESS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 16), BLDLV_EDGE_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 8), BLDLV_SM_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 4), BLDLV_MESS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 0), BLDLV_EDGE_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 24), BLDLV_SM_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 20), BLDLV_MESS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 16), BLDLV_EDGE_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x3 << 8), BLDLV_SM_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x3 << 4), BLDLV_MESS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x3 << 0), BLDLV_EDGE_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 24), BLDLV_SM_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 20), BLDLV_MESS_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 16), BLDLV_EDGE_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 8), BLDLV_SM_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 4), BLDLV_MESS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 0), BLDLV_EDGE_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x08000000, 0x28000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8E, 0x00000000, 0x02000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x04000000, 0x14000000);
				break;
			default:
				break;
			}
		}

		if (u4BNRStrength == NR_STRENGTH_OFF) {
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, 0x00000000, BLOCK_PROC_ENABLE);
		} else {
			switch (u4BNRStrength) {
			case NR_STRENGTH_LOW:
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x4 << 12), BLDLV_BK_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x4 << 28), BLDLV_BK_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x4 << 12), BLDLV_BK_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x4 << 28), BLDLV_BK_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x4 << 12), BLDLV_BK_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x4 << 28), BLDLV_BK_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x4 << 12), BLDLV_BK_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x4 << 28), BLDLV_BK_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 4), BLDLV_MOS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 8), BLDLV_MOS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 12), BLDLV_MOS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 20), BLDLV_MOS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 24), BLDLV_MOS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 0), BLDLV_NEAR_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 16), BLDLV_MOS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 28), BLDLV_MOS_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 8), BLDLV_SM_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 4), BLDLV_MESS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 0), BLDLV_EDGE_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 24), BLDLV_SM_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 20), BLDLV_MESS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 16), BLDLV_EDGE_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 8), BLDLV_SM_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 4), BLDLV_MESS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x1 << 0), BLDLV_EDGE_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 24), BLDLV_SM_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 20), BLDLV_MESS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 16), BLDLV_EDGE_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x1 << 8), BLDLV_SM_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x1 << 4), BLDLV_MESS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x1 << 0), BLDLV_EDGE_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 24), BLDLV_SM_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 20), BLDLV_MESS_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x1 << 16), BLDLV_EDGE_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 8), BLDLV_SM_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 4), BLDLV_MESS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x1 << 0), BLDLV_EDGE_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, 0x48084808, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x20000000, 0x28000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8E, 0x00000000, 0x02000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x10000000, 0x14000000);
				break;
			case NR_STRENGTH_MED:
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x6 << 12), BLDLV_BK_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x6 << 28), BLDLV_BK_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x6 << 12), BLDLV_BK_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x6 << 28), BLDLV_BK_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x6 << 12), BLDLV_BK_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x6 << 28), BLDLV_BK_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x6 << 12), BLDLV_BK_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x6 << 28), BLDLV_BK_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 4), BLDLV_MOS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 8), BLDLV_MOS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 12), BLDLV_MOS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 20), BLDLV_MOS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 24), BLDLV_MOS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 0), BLDLV_NEAR_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 16), BLDLV_MOS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 28), BLDLV_MOS_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 8), BLDLV_SM_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 4), BLDLV_MESS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 0), BLDLV_EDGE_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 24), BLDLV_SM_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 20), BLDLV_MESS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 16), BLDLV_EDGE_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 8), BLDLV_SM_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 4), BLDLV_MESS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x2 << 0), BLDLV_EDGE_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 24), BLDLV_SM_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 20), BLDLV_MESS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 16), BLDLV_EDGE_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x2 << 8), BLDLV_SM_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x2 << 4), BLDLV_MESS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x2 << 0), BLDLV_EDGE_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 24), BLDLV_SM_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 20), BLDLV_MESS_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x2 << 16), BLDLV_EDGE_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 8), BLDLV_SM_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 4), BLDLV_MESS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x2 << 0), BLDLV_EDGE_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, 0x4B0B4B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x08000000, 0x28000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8E, 0x00000000, 0x02000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x10000000, 0x14000000);
				break;
			case NR_STRENGTH_HIGH:
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x8 << 12), BLDLV_BK_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x8 << 28), BLDLV_BK_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x8 << 12), BLDLV_BK_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x8 << 28), BLDLV_BK_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x8 << 12), BLDLV_BK_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x8 << 28), BLDLV_BK_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x8 << 12), BLDLV_BK_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x8 << 28), BLDLV_BK_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 4), BLDLV_MOS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 8), BLDLV_MOS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 12), BLDLV_MOS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 20), BLDLV_MOS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 24), BLDLV_MOS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 0), BLDLV_NEAR_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 16), BLDLV_MOS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 28), BLDLV_MOS_BK);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 8), BLDLV_SM_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 4), BLDLV_MESS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 0), BLDLV_EDGE_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 24), BLDLV_SM_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 20), BLDLV_MESS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 16), BLDLV_EDGE_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 8), BLDLV_SM_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 4), BLDLV_MESS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x3 << 0), BLDLV_EDGE_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 24), BLDLV_SM_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 20), BLDLV_MESS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 16), BLDLV_EDGE_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x3 << 8), BLDLV_SM_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x3 << 4), BLDLV_MESS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x3 << 0), BLDLV_EDGE_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 24), BLDLV_SM_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 20), BLDLV_MESS_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x3 << 16), BLDLV_EDGE_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 8), BLDLV_SM_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 4), BLDLV_MESS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x3 << 0), BLDLV_EDGE_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, 0x8F0F8B0B, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, 0x8B0B8F0F, 0xFF3FFF3F);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x08000000, 0x28000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8E, 0x00000000, 0x02000000);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x04000000, 0x14000000);
				break;
			default:
				break;
			}
		}
	}
}

void MTK_NR_Param_Init(unsigned long reg_base)
{
	//2d NR
	vNrWriteReg(reg_base + 0x42c, 0x11108080);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_01, 0x00000000, 0x80000000);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, 0x02C02CFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_20, 0x00046030, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_21, 0x3C230523, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_22, 0x01A22C96, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_23, 0x6C230523, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_24, 0x01A62C96, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_25, 0x4C140523, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_26, 0x01AC2C32, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_27, 0x4C230523, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_28, 0x01A42C96, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_29, 0x4C230523, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_2A, 0x01A42C96, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_2B, 0x6C050523, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_2C, 0x01A62C96, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_2D, 0x6C050523, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_2E, 0x01A62C96, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_2F, 0x6C080223, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_30, 0x01A62C96, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_31, 0x6C080223, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_32, 0x01662C96, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_33, 0x6C080223, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_34, 0x01662C96, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_35, 0x6C080223, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_36, 0x01662C96, 0x01FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_37, 0x01020102, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_38, 0x02030103, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_39, 0x01020102, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3A, 0x01020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3B, 0x01010101, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3C, 0x01010302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3D, 0x01010101, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3E, 0x01010302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_3F, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_40, 0x02030306, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_41, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_42, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_43, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_44, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_45, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_46, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_47, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_48, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_49, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4A, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4B, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4C, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4D, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4E, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_4F, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_50, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_51, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_52, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_53, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_54, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_55, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_56, 0x03020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_57, 0x01020302, 0xFF3FFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_58, 0x03040303, 0xFF3FFF3F);
	// FILTER GLOBAL CONTROL
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_64, 0x00000000, 0xCF3F3F7F);
	// MOSQUITO CONTROL
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_65, 0x0C120414, 0xFFFF0F7F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_66, 0x00100000, 0x00FFFFFF);
	// PRE_FILTER_CONTROL
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_67, 0x00000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_68, 0x00000000, 0x00FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_69, 0x00000000, 0xFFFFFFFF);
	// NEW_SMOOTH_DETECT_CONTROL
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_6A, 0x07F40202, 0x37FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_6B, 0x07F40202, 0x37FFFFFF);
	// COLOR_TONE_CONTROL
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_6C, 0x00555555, 0x3FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_71, 0x8264A783, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_72, 0x77408155, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_73, 0xAC7C7366, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_74, 0xFFFFFFFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_75, 0xFFFFFFFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_76, 0xFFFFFFFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_77, 0xE01AB027, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_78, 0x9033FFFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_79, 0xFFFFFFFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, 0x88888883, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, 0x88888888, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, 0x11118658, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, 0x88888888, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_82, 0x88888888, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, 0x88848888, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, 0x08888881, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_86, 0xFFFFFACF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_87, 0xFFFFFFFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_88, 0xFFFFFFFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_89, 0xFCCFFFFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8A, 0xFFFFFFFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8B, 0xFFFFFFFF, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8C, 0x0FFFFFFF, 0x0FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8D, 0x00000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_8E, 0x00000000, 0x03FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_9C, 0x000029FD, 0x001FFFFF);

	// Block NR setting
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_02, 0x07FF0808, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_03, 0x01807807, 0x07FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_04, 0x4092C864, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_05, 0x00990C04, 0x3FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_06, 0x32140602, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_07, 0x04040404, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_08, 0x4C900864, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_09, 0x03620101, 0x3FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0A, 0x32140101, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0B, 0x04040404, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0C, 0xCC900864, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0D, 0x03660101, 0x3FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0E, 0x32140101, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_0F, 0x04040404, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_10, 0xCC900864, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_11, 0x03960202, 0x3FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_12, 0x32140602, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_13, 0x04040404, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_14, 0x40100064, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_15, 0x10660101, 0x3FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_16, 0x32140101, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_17, 0x04040404, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_18, 0x140A140A, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_19, 0x140A140A, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1A, 0x00080000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1B, 0x3E3F7FEF, 0x3FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1C, 0x00D95FBF, 0x00FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1D, 0x00002452, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1E, 0x00000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_85, 0x00000000, 0x3FFFFFFF);

	// Block meter setting
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_92, 0xCC962A00, 0xFFFFFFC0);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_59, 0x10080048, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_5A, 0x04040404, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_5B, 0xD96748E7, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_5C, 0xE020E200, 0xFFFFFF0F);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_5D, 0x01000804, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_5E, 0x0FEDAEC0, 0x0FFFFFF0);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_5F, 0x0FED0EC0, 0x0FFF0FFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_60, 0xCC962A00, 0xFFF77FFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_61, 0xCC962A00, 0xFFF77FFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_62, 0xCC962A00, 0xFFF77FFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_63, 0x00000430, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_93, 0xD4024088, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_94, 0x00550260, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_95, 0x0000600E, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_96, 0x74C0D030, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_97, 0x0001A812, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_98, 0x05050505, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_99, 0x27272727, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_9A, 0x05050505, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_9B, 0x32323232, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_9D, 0x20000000, 0xFFFFFFFF);

	// 3D NR setting
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_02, 0x41C88AB0, 0xFFFFFEFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_03, 0x0C000011, 0xFFDE0FF7);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_04, 0x00000030, 0xF0F1FEFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_05, 0x00022002, 0x001EFFDF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_06, 0x00000000, 0x0000FFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_07, 0x0210300B, 0xFF33FF1F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_08, 0x00020000, 0x3FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_09, 0x12468ACE, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0C, 0x00100005, 0x007F007F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0D, 0x40000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0E, 0x20000000, 0x7FFF0000);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0F, 0x0C070240, 0xFF3FF3FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_10, 0x65432111, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_11, 0x65432111, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_12, 0x11224586, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_13, 0x65432111, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_14, 0x00030505, 0x007F7F7F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_15, 0x02005000, 0x0FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_16, 0x00002461, 0xE000FFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_17, 0x30000038, 0x3FFF03FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_18, 0x01900070, 0x7FF000FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_19, 0x00101000, 0x00FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_1A, 0x13579BCD, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_1B, 0x124689AB, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_1C, 0x11246899, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_1D, 0x11234567, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_1E, 0x04050508, 0x7F7F7F7F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_20, 0x00028000, 0xFF0FFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_21, 0x000600B0, 0x001FFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_22, 0x6220280F, 0x7BFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_23, 0x6030002F, 0xF0FFFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_24, 0x0C280470, 0x7DFFF4FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_25, 0x17E61711, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_26, 0x00051064, 0x3FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_27, 0x00000000, 0xBFFFFBFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_28, 0x00000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_29, 0x103C6031, 0xFFFFFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2A, 0x40000011, 0xC03FFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2B, 0x00000000, 0x003FF3FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2C, 0x20181008, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2D, 0x40383028, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2E, 0xD0000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_30, 0x00000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_31, 0x000FF000, 0x7F0FF3FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_32, 0x00FFF000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_33, 0x002D0000, 0x007FF7FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_34, 0x000E7001, 0x007FF7FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_35, 0x00000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_36, 0x40000000, 0xFF3FFF7F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_37, 0x00000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_38, 0x00000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_39, 0x00004040, 0xFF0FFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3A, 0x03050505, 0x7FFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3B, 0x00000303, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3C, 0x00000004, 0xFF3F3F3F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3D, 0x00009617, 0x0000FFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3E, 0x3F0007FF, 0x3F7FF7FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3F, 0x000017FF, 0x007FF7FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_40, 0x2368AACC, 0xFFFFFFFF);	//frame still still edge tbl
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_41, 0x11111111, 0xFFFFFFFF);	//frame still mtion edge tbl
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_42, 0x2368AACC, 0xFFFFFFFF);	//frame motion still edge tbl
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_43, 0x11111111, 0xFFFFFFFF);	//frame motion mtion edge tbl
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_44, 0x05051818, 0x7F7F7F7F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_45, 0x30301038, 0x73FF73FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_46, 0x10200017, 0x73FF03FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_47, 0x17F1805F, 0x37FFF7FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_48, 0x2CF00000, 0x7FF007FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_49, 0x0EF00001, 0x7FF007FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_4A, 0x10963838, 0x73FF7FFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_4C, 0x000017B4, 0x801FFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_4D, 0x2D004E5C, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_4E, 0x00003BD1, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_4F, 0xBB800000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_50, 0x00000000, 0x800000FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_60, 0x80000006, 0x8200FFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_70, 0x40050505, 0xFF7F7F7F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_71, 0x12345678, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_72, 0x124689A9, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_73, 0x124567AD, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_78, 0x00000000, 0xBFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_79, 0x00000000, 0xBFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_7A, 0x00000000, 0xBFFFFFFF);

	// Noise meter setting
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_80, 0x30000000, 0x70003000);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_81, 0x03201001, 0x07FFFF07);

	// CrossColorSuppression
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_21, 0x000600B0, 0x001FFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_22, 0x6220280F, 0x7BFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_23, 0x6030002F, 0xF0FFFF3F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_24, 0x0C280470, 0x7DFFF4FF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_25, 0x17E61711, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_2A, 0x40000011, 0xC03FFFFF);

	// Recursive filter setting
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_6E, 0xA0000008, 0xF003F73F);	//2D recursive filter
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_6F, 0x00200810, 0x00FFFFFF);	//2D recursive filter curve
	vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_70, 0xFF6C3C8C, 0xFFFFFFFF);	//2D recursive filter curve
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_07, 0x02103009, 0xFF33FF1F);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_19, 0x00101000, 0x00FFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_37, 0x00000000, 0xFFFFFFFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_38, 0x00000000, 0xFFFFFFFF);
}

static int MTK_NR_Config(struct NR_PRM_INFO_T *ptNrPrm, unsigned long reg_base)
{
	unsigned short u2HActive = 0;
	unsigned short u2VActive = 0;
	unsigned int u4MBCnt = 0;
	unsigned int u4Value = 0;

	u2HActive = ptNrPrm->u2FrameWidth;
	u2VActive = ptNrPrm->u2PicHeight;	//ptNrPrm->u2FrameHeight;
	u4MBCnt = ((ptNrPrm->u2FrameWidth + 15) >> 4);

#ifndef ONLY_2D_NR_SUPPORT
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_00, 0xB8000000, 0xB8000000);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_03, 0x00000000, BYPASS_2D_NR);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_01, 0x00000001, 0x00000FFF);
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_07, 0x00000009, 0x00000009);
#else
	vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_00, 0x38000000, 0x38000000);
#endif


	vNrWriteReg(reg_base + RW_NR_CURR_Y_RD_ADDR, ptNrPrm->u4CurrRdYAddr);
	vNrWriteReg(reg_base + RW_NR_CURR_C_RD_ADDR, ptNrPrm->u4CurrRdCAddr);

	vNrWriteReg(reg_base + RW_NR_Y_WR_SADDR, ptNrPrm->u4CurrWrYAddr);
	vNrWriteReg(reg_base + RW_NR_Y_WR_EADDR, 0xFFFFFFFF);
	vNrWriteReg(reg_base + RW_NR_C_WR_SADDR, ptNrPrm->u4CurrWrCAddr);
	vNrWriteReg(reg_base + RW_NR_C_WR_EADDR, 0xFFFFFFFF);

#ifndef ONLY_2D_NR_SUPPORT
	vNrWriteReg(reg_base + RW_NR_LAST_Y_RD_ADDR, ptNrPrm->u4LastRdYAddr);
	vNrWriteReg(reg_base + RW_NR_LAST_C_RD_ADDR, ptNrPrm->u4LastRdCAddr);
#endif
	vNrWriteReg(reg_base + RW_NR_HD_HDE_RATIO, 0x00000000);

	u2HActive = u2HActive / 2;
	if (ptNrPrm->u1FrameMode == MODE_FRAME) {
		u4Value = u2VActive;
		u4Value = u4Value << 20;
		u4Value = u4Value | u2HActive;
		vNrWriteReg(reg_base + RW_NR_HD_ACTIVE, u4Value);
	} else {
		u4Value = u2VActive / 2;
		u4Value = u4Value << 20;
		u4Value = u4Value | u2HActive;
		vNrWriteReg(reg_base + RW_NR_HD_ACTIVE, u4Value);
	}
	vNRWriteRegMsk(reg_base + RW_NR_HD_LINE_OFST, u4MBCnt, 0x0000007F);

	if (ptNrPrm->u1FrameMode == MODE_FRAME) {
		vNRWriteRegMsk(reg_base + RW_NR_DRAM_CTRL_00, 0x00001000, 0x0000FF01);
#ifndef ONLY_2D_NR_SUPPORT
		vNRWriteRegMsk(reg_base + RW_NR_DRAM_CTRL_10, 0x00001000, 0x0000FF01);
#endif
	} else {
		vNRWriteRegMsk(reg_base + RW_NR_DRAM_CTRL_00, 0x00001001, 0x0000FF01);
#ifndef ONLY_2D_NR_SUPPORT
		vNRWriteRegMsk(reg_base + RW_NR_DRAM_CTRL_10, 0x00001001, 0x0000FF01);
#endif
	}

	// reset NR module
	vNrWriteReg(reg_base + RW_NR_MISC_CTRL, 0x000000ff);

	// select CRC source
	vNrWriteReg(reg_base + RW_NR_MAIN_CTRL_00, 0x6010007c);	// current output as CRC source

	vNrWriteReg(reg_base + RW_NR_HD_PATH_ENABLE, 0x00000000);

	vNrWriteReg(reg_base + RW_NR_MISC_CTRL, 0x00000000);

	// Fram / Field / Top Field / Bot Field
	switch (ptNrPrm->u1FrameMode) {
	case MODE_FRAME:
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, 0x01400103);	// note :bit[14] bit[12] should be the same.???
		vNrWriteReg(reg_base + RW_NR_HD_SYNC_TRIGGER, 0x00000004);
		vNrWriteReg(reg_base + RW_NR_HD_SYNC_TRIGGER, 0x00000000);
		// enable CRC
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000100);
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000101);
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000100);
		break;
	case MODE_FIELD:
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, 0x01405143);
		vNrWriteReg(reg_base + RW_NR_HD_SYNC_TRIGGER, 0x00000004);
		vNrWriteReg(reg_base + RW_NR_HD_SYNC_TRIGGER, 0x00000000);
		// enable CRC
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000100);
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000109);
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000108);
		break;
	case MODE_TOP_FIELD:
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, 0x01405103);
		vNrWriteReg(reg_base + RW_NR_HD_SYNC_TRIGGER, 0x00000005);
		vNrWriteReg(reg_base + RW_NR_HD_SYNC_TRIGGER, 0x00000000);
		// enable CRC
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000100);
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000101);
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000100);
		break;
	case MODE_BOT_FIELD:
		vNrWriteReg(reg_base + RW_NR_HD_MODE_CTRL, 0x01405183);
		vNrWriteReg(reg_base + RW_NR_HD_SYNC_TRIGGER, 0x00000005);
		vNrWriteReg(reg_base + RW_NR_HD_SYNC_TRIGGER, 0x00000000);
		// enable CRC
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000100);
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000101);
		vNrWriteReg(reg_base + RW_NR_CRC_SETTING, 0x00000100);
		break;
	default:
		break;
	}

	_vNRSetSwapMode(ptNrPrm->u1AddrSwapMode, reg_base);

	_vNRSetDramBurstRead(reg_base, ptNrPrm->fgBurstRdEn);

	if (ptNrPrm->fgNoiseMeterEn) {
		_vNRSetNoiseMeter(ptNrPrm->u2PicWidth,
				  (ptNrPrm->u1FrameMode ==
				   MODE_FRAME) ? ptNrPrm->u2PicHeight : (ptNrPrm->u2PicHeight / 2), reg_base);
	}
	// Range Remap Setting
	{
		u4Value = (vNrReadReg(reg_base + RW_NR_HD_RANGE_MAP) & 0xFFFFE0E0);
		if (ptNrPrm->fgRangeRemapYEn)
			u4Value |= ((ptNrPrm->u4RangeMapY + 9) & 0x1F);
		else
			u4Value |= 0x8;

		if (ptNrPrm->fgRangeRemapUVEn)
			u4Value |= (((ptNrPrm->u4RangeMapUV + 9) & 0x1F) << 8);
		else
			u4Value |= 0x800;

		vNrWriteReg(reg_base + RW_NR_HD_RANGE_MAP, u4Value);
	}

	u4MBCnt = ((ptNrPrm->u2FrameWidth + 15) >> 4);
	u2HActive = ptNrPrm->u2FrameWidth;	//ptNrPrm->u2PicWidth;
	u2VActive = ptNrPrm->u2PicHeight;	//ptNrPrm->u2FrameHeight;

	// output buffer width
	vNRWriteRegMsk(reg_base + RW_NR_DRAM_CTRL_00, (u4MBCnt << 24), 0xFF000000);
#ifndef ONLY_2D_NR_SUPPORT
	vNRWriteRegMsk(reg_base + RW_NR_DRAM_CTRL_10, (u4MBCnt << 24), 0xFF000000);
#endif

	// NR target frame width and height
	u2HActive = (u2HActive / 2) << 1;
	if (ptNrPrm->u1FrameMode == MODE_FRAME) {
		u4Value = u2VActive;
		u4Value = u4Value << 16;
		u4Value = u4Value | u2HActive;
		vNRWriteRegMsk(reg_base + RW_NR_DRAM_CTRL_01, u4Value, 0x07FF0FFF);
#ifndef ONLY_2D_NR_SUPPORT
		vNRWriteRegMsk(reg_base + RW_NR_TARGET_SIZE_CTRL, u4Value, 0x07FF0FFF);
#endif
	} else {
		u4Value = (u2VActive / 2);
		u4Value = u4Value << 16;
		u4Value = u4Value | u2HActive;
		vNRWriteRegMsk(reg_base + RW_NR_DRAM_CTRL_01, u4Value, 0x07FF0FFF);
		vNRWriteRegMsk(reg_base + RW_NR_TARGET_SIZE_CTRL, u4Value, 0x07FF0FFF);
	}

	if (ptNrPrm->u1DemoMode == NR_DEMO_0) {
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0D, (((ptNrPrm->u2PicWidth / 2) - 1) << 20),
			       FRM_WD_WIDTH);
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_00, (0x1 << 12) | ((ptNrPrm->u2PicWidth / 4) - 1),
			       FRM_DW_WIDTH);
#ifndef ONLY_2D_NR_SUPPORT
		// Set 3D NR Metric/Active/New Still Window
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3E, (ptNrPrm->u2PicWidth / 2), 0x007FF7FF);
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_33, (ptNrPrm->u2PicWidth << 12), 0x007FF7FF);
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_48, (ptNrPrm->u2PicWidth << 20), 0x7FF007FF);
		if (ptNrPrm->u1FrameMode == MODE_FRAME) {
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3F, ptNrPrm->u2PicHeight, 0x007FF7FF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_34, (ptNrPrm->u2PicHeight << 12),
				       0x007FF7FF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_49, (ptNrPrm->u2PicHeight << 20),
				       0x7FF007FF);
			// set 2D frame still threshold

			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_20,
				       ((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight /
					 (64 * 32)) & 0x7FF), 0x000007FF);

			// set 3D bigmo/smlmo/still threshold
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_08,
				       (ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 8),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_15,
				       (ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 32),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0D,
				       (ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 16),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_47,
				       ((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight /
					 (64 * 32)) & 0x7FF), 0x000007FF);
		} else {
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3F, (ptNrPrm->u2PicHeight / 2), 0x007FF7FF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_34, ((ptNrPrm->u2PicHeight / 2) << 12),
				       0x007FF7FF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_49, ((ptNrPrm->u2PicHeight / 2) << 20),
				       0x7FF007FF);
			// set 2D frame still threshold

			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_20,
				       (((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 2) /
					 (64 * 32)) & 0x7FF), 0x000007FF);
			// set 3D bigmo/smlmo/still threshold
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_08,
				       ((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 8) / 2),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_15,
				       ((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 32) / 2),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0D,
				       ((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 16) / 2),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_47,
				       (((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 2) /
					 (64 * 32)) & 0x7FF), 0x000007FF);

		}
#endif
	} else {
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0D, (0x3FF << 20), FRM_WD_WIDTH);	//for 4096 width
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_00, 0xEF, FRM_DW_WIDTH);
#ifndef ONLY_2D_NR_SUPPORT
		// Set 3D NR Metric/Active/New Still Window
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3E, ptNrPrm->u2PicWidth, 0x007FF7FF);
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_33, (ptNrPrm->u2PicWidth << 12), 0x007FF7FF);
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_48, (ptNrPrm->u2PicWidth << 20), 0x7FF007FF);
		if (ptNrPrm->u1FrameMode == MODE_FRAME) {
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3F, ptNrPrm->u2PicHeight, 0x007FF7FF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_34, (ptNrPrm->u2PicHeight << 12),
				       0x007FF7FF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_49, (ptNrPrm->u2PicHeight << 20),
				       0x7FF007FF);
			// set 2D frame still threshold
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_20,
				       ((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight /
					 (64 * 32)) & 0x7FF), 0x000007FF);
			// set 3D bigmo/smlmo/still threshold
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_08,
				       (ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 8),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_15,
				       (ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 32),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0D,
				       (ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 16),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_47,
				       ((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight /
					 (64 * 32)) & 0x7FF), 0x000007FF);
		} else {
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3F, (ptNrPrm->u2PicHeight / 2), 0x007FF7FF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_34, ((ptNrPrm->u2PicHeight / 2) << 12),
				       0x007FF7FF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_49, ((ptNrPrm->u2PicHeight / 2) << 20),
				       0x7FF007FF);
			// set 2D frame still threshold
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_20,
				       (((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 2) /
					 (64 * 32)) & 0x7FF), 0x000007FF);

			// set 3D bigmo/smlmo/still threshold
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_08,
				       ((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 8) / 2),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_15,
				       ((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 32) / 2),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0D,
				       ((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 16) / 2),
				       0x000FFFFF);
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_47,
				       (((ptNrPrm->u2PicWidth * ptNrPrm->u2PicHeight / 2) /
					 (64 * 32)) & 0x7FF), 0x000007FF);
		}
#endif
	}

	if (ptNrPrm->u1DemoMode == NR_DEMO_0) {
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0D, (0x2 << 30), NR_DEMO_MODE);
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_00, (0x1 << 12), DEMO_SIDE);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, (0x3 << 12),
			       SLICE_DEMO_CTRL | SLICE_DEMO_ENABLE);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_02, ((ptNrPrm->u2PicWidth / 2) << 16),
			       SLICE_X_POSITION);
	} else if (ptNrPrm->u1DemoMode == NR_DEMO_1) {
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0D, (0x3 << 30), NR_DEMO_MODE);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, (0x1 << 12),
			       SLICE_DEMO_CTRL | SLICE_DEMO_ENABLE);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_02, ((ptNrPrm->u2PicWidth / 2) << 16),
			       SLICE_X_POSITION);
	} else {
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_0D, (0x3 << 30), NR_DEMO_MODE);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, (0x0 << 12),
			       SLICE_DEMO_CTRL | SLICE_DEMO_ENABLE);
	}


	// Block meter setting
	if (ptNrPrm->u1FrameMode == MODE_FRAME) {
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_01, ptNrPrm->u2PicHeight, 0x7FF);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_01, (ptNrPrm->u2PicWidth << 16), 0x7FF0000);
	} else {
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_01, (ptNrPrm->u2PicHeight / 2), 0x7FF);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_01, (ptNrPrm->u2PicWidth << 16), 0x7FF0000);
	}

	if (ptNrPrm->u1FrameMode == MODE_FRAME) {
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_5B, ptNrPrm->u2PicHeight, BK_METER_HEIGHT);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_5B, ((ptNrPrm->u2PicWidth / 2) << 16),
			       BK_METER_WIDTH);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_95,
			       ((((ptNrPrm->u2PicHeight / 8) / 2) / 6) * 2) << 10, 0x000FFC00);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_97,
			       (((((ptNrPrm->u2PicWidth / 8) / 2) / 6) * 2) << 11), 0x003FF800);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_96,
			       (((ptNrPrm->u2PicHeight + 31) / (8 * 8)) * 2) << 11, 0x0003FF800);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_94, ((ptNrPrm->u2PicWidth / (64 * 6)) * 3) << 15,
			       0x3FFF8000);
	} else {
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_5B, ptNrPrm->u2PicHeight / 2, BK_METER_HEIGHT);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_5B, ((ptNrPrm->u2PicWidth / 2) << 16),
			       BK_METER_WIDTH);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_95,
			       ((((ptNrPrm->u2PicHeight / 8) / 4) / 6) * 2) << 10, 0x000FFC00);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_97,
			       (((((ptNrPrm->u2PicWidth / 8) / 2) / 6) * 2) << 11), 0x003FF800);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_96,
			       (((ptNrPrm->u2PicHeight + 63) / (8 * 8 * 2)) * 2) << 11,
			       0x0003FF800);
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_94, ((ptNrPrm->u2PicWidth / (64 * 6)) * 3) << 15,
			       0x3FFF8000);
	}

	if (ptNrPrm->fgBypassEn) {
#ifndef ONLY_2D_NR_SUPPORT
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_02, 0x00001000, 0x00001000);
		vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3C, 0x00000000, 0x0000003F);
#endif
		vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, 0x00000000, 0x00000001);	// turn off NR kernel process
	} else {
#if 1
		_vNRAdaptiveBNRPara(ptNrPrm, reg_base);
		MTK_NR_Level_Set(ptNrPrm->u4Strength, ptNrPrm->u4FNRStrength,
				 ptNrPrm->u4MNRStrength, ptNrPrm->u4BNRStrength, reg_base);

#else
		if (ptNrPrm->u4Strength == NR_STRENGTH_OFF) {
#ifndef ONLY_2D_NR_SUPPORT
			if (ptNrPrm->u4FNRStrength == NR_STRENGTH_OFF) {
				vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_02, 0x00001000, 0x00001000);
				vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_3C, 0x00000000, 0x0000003F);
			} else {
				vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_02, 0x00000000, 0x00001000);
				switch (ptNrPrm->u4FNRStrength) {
				case NR_STRENGTH_LOW:
					vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1D, 0x00112344);	// big motion table
					vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1B, 0x00124568);	// small motion table
					vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1C, 0x00112456);	// default table
					break;
				case NR_STRENGTH_MED:
					vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1D, 0x01123445);	// big motion table
					vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1B, 0x01246789);	// small motion table
					vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1C, 0x01124678);	// default table
					break;
				case NR_STRENGTH_HIGH:
					vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1D, 0x11234566);	// big motion table
					vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1B, 0x124689AA);	// small motion table
					vNrWriteReg(reg_base + RW_NR_3DNR_CTRL_1C, 0x11246899);	// default table
					break;
				default:
					break;
				}
			}
#endif
			if (ptNrPrm->u4MNRStrength == NR_STRENGTH_OFF) {
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x0 << 4), BLDLV_MOS_CO1);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x0 << 8), BLDLV_MOS_CO2);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x0 << 12), BLDLV_MOS_CO3);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x0 << 20), BLDLV_MOS_ST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x0 << 24), BLDLV_MOS_MO);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x0 << 0), BLDLV_NEAR_FRST);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x0 << 16), BLDLV_MOS_DEF);
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x0 << 28), BLDLV_MOS_BK);
			} else {
				switch (ptNrPrm->u4MNRStrength) {
				case NR_STRENGTH_LOW:
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 4),
						       BLDLV_MOS_CO1);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 8),
						       BLDLV_MOS_CO2);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 12),
						       BLDLV_MOS_CO3);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 20),
						       BLDLV_MOS_ST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 24),
						       BLDLV_MOS_MO);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x4 << 0),
						       BLDLV_NEAR_FRST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 16),
						       BLDLV_MOS_DEF);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x4 << 28),
						       BLDLV_MOS_BK);
					break;
				case NR_STRENGTH_MED:
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 4),
						       BLDLV_MOS_CO1);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 8),
						       BLDLV_MOS_CO2);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 12),
						       BLDLV_MOS_CO3);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 20),
						       BLDLV_MOS_ST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 24),
						       BLDLV_MOS_MO);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x6 << 0),
						       BLDLV_NEAR_FRST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 16),
						       BLDLV_MOS_DEF);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x6 << 28),
						       BLDLV_MOS_BK);
					break;
				case NR_STRENGTH_HIGH:
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 4),
						       BLDLV_MOS_CO1);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 8),
						       BLDLV_MOS_CO2);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 12),
						       BLDLV_MOS_CO3);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 20),
						       BLDLV_MOS_ST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 24),
						       BLDLV_MOS_MO);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_84, (0x8 << 0),
						       BLDLV_NEAR_FRST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 16),
						       BLDLV_MOS_DEF);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_83, (0x8 << 28),
						       BLDLV_MOS_BK);
					break;
				default:
					break;
				}
			}
			if (ptNrPrm->u4BNRStrength == NR_STRENGTH_OFF) {
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, 0x00000000, BLOCK_PROC_ENABLE);
			} else {
				vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, 0x00000080, BLOCK_PROC_ENABLE);
				switch (ptNrPrm->u4BNRStrength) {
				case NR_STRENGTH_LOW:
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x4 << 12),
						       BLDLV_BK_CO1);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x4 << 28),
						       BLDLV_BK_CO2);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x4 << 12),
						       BLDLV_BK_CO3);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x4 << 28),
						       BLDLV_BK_ST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x4 << 12),
						       BLDLV_BK_MO);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x4 << 28),
						       BLDLV_BK_FRST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x4 << 12),
						       BLDLV_BK_DEF);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x4 << 28),
						       BLDLV_BK_BK);
					break;
				case NR_STRENGTH_MED:
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x6 << 12),
						       BLDLV_BK_CO1);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x6 << 28),
						       BLDLV_BK_CO2);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x6 << 12),
						       BLDLV_BK_CO3);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x6 << 28),
						       BLDLV_BK_ST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x6 << 12),
						       BLDLV_BK_MO);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x6 << 28),
						       BLDLV_BK_FRST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x6 << 12),
						       BLDLV_BK_DEF);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x6 << 28),
						       BLDLV_BK_BK);
					break;
				case NR_STRENGTH_HIGH:
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x8 << 12),
						       BLDLV_BK_CO1);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x8 << 28),
						       BLDLV_BK_CO2);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_81, (0x8 << 12),
						       BLDLV_BK_CO3);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x8 << 28),
						       BLDLV_BK_ST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x8 << 12),
						       BLDLV_BK_MO);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_80, (0x8 << 28),
						       BLDLV_BK_FRST);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7E, (0x8 << 12),
						       BLDLV_BK_DEF);
					vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_7F, (0x8 << 28),
						       BLDLV_BK_BK);
					break;
				default:
					break;
				}
			}
		} else {
#ifndef ONLY_2D_NR_SUPPORT
			vNRWriteRegMsk(reg_base + RW_NR_3DNR_CTRL_02, 0x00000000, 0x00001000);
#endif
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, 0x00000080, BLOCK_PROC_ENABLE);
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, 0x00000001, 0x00000001);
		}
#endif
	}

	// enable NR
	if (ptNrPrm->u4NRMode == NR_STD_MODE) {
		if (ptNrPrm->fgUseBlockMeter)
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, 0x100, BLOCK_METER_ENABLE);
		else
			vNRWriteRegMsk(reg_base + RW_NR_2DNR_CTRL_1F, 0x000, BLOCK_METER_ENABLE);

		vNrWriteReg(reg_base + RW_NR_HD_PATH_ENABLE, 0x00000001);
	}

	return 0;
}

int MTK_NR_Config_DispSysCfg(struct regmap *mmsys_regmap)
{
	unsigned int val;

	regmap_read(mmsys_regmap, BDP_DISPSYS_DISP_CLK_CONFIG1, &val);
	val &= 0xFFFFFFFE;
	regmap_write(mmsys_regmap, BDP_DISPSYS_DISP_CLK_CONFIG1, val);

	return 0;
}

int MTK_NR_Process(struct NR_PROCESS_PARAM_T *prNrParam, unsigned long reg_base)
{
	struct NR_PRM_INFO_T nr_param;

	memset(&nr_param, 0, sizeof(nr_param));

	nr_param.u4NRMode = NR_STD_MODE;

	nr_param.u2PicWidth = prNrParam->u2SrcPicWidth;
	nr_param.u2PicHeight = prNrParam->u2SrcPicHeight;
	nr_param.u2FrameWidth = prNrParam->u2SrcFrmWidth;
	nr_param.u2FrameHeight = prNrParam->u2SrcFrmHeight;


	nr_param.u4Strength = prNrParam->u4TotalLevel;
	nr_param.u4MNRStrength = prNrParam->u4MnrLevel;
	nr_param.u4BNRStrength = prNrParam->u4BnrLevel;
	nr_param.u4FNRStrength = prNrParam->u4FnrLevel;

	nr_param.u4CurrRdYAddr = prNrParam->u4InputAddrMvaYCurr;
	nr_param.u4CurrRdCAddr = prNrParam->u4InputAddrMvaCbcrCurr;
	nr_param.u4LastRdYAddr = prNrParam->u4InputAddrMvaYLast;
	nr_param.u4LastRdCAddr = prNrParam->u4InputAddrMvaCbcrLast;
	nr_param.u4CurrWrYAddr = prNrParam->u4OutputAddrMvaY;
	nr_param.u4CurrWrCAddr = prNrParam->u4OutputAddrMvaCbcr;

	MTK_NR_Hw_Reset(reg_base);
	MTK_NR_Config(&nr_param, reg_base);

	return 0;
}

void MTK_NR_Clear_Irq(unsigned long reg_base)
{
	unsigned int u4IrqClr;

	u4IrqClr = vNrReadReg(reg_base + RW_NR_INT_CLR);
	u4IrqClr |= 1 << 4;
	vNrWriteReg(reg_base + RW_NR_INT_CLR, u4IrqClr);
}

