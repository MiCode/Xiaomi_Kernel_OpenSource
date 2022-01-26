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

#include "dpi_dvt_test.h"

#if defined(RDMA_DPI_PATH_SUPPORT) || defined(DPI_DVT_TEST_SUPPORT)

#ifdef BUILD_UBOOT
#include <asm/arch/disp_drv_platform.h>
#else
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include "cmdq_record.h"
#include <disp_drv_log.h>
#endif
#include <debug.h>
#include <mt-plat/sync_write.h>
#include <linux/types.h>
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
/* #include <mach/irqs.h> */
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/wait.h>

#include "mtkfb.h"
#include "ddp_drv.h"
#include "ddp_hal.h"
#include "ddp_manager.h"
#include "ddp_dpi_reg.h"
#include "ddp_reg.h"
#include "ddp_log.h"

#include "dpi_dvt_test.h"
#include "ddp_dpi_ext.h"

#include <linux/of.h>
#include <linux/of_irq.h>
/*#include "mach/eint.h"*/

/* #ifdef DPI_EXT_INREG32 */
/* #undef DPI_EXT_INREG32 */
#define DPI_EXT_INREG32(x)          (__raw_readl((unsigned long *)(x)))
/* #endif */

#define DPI_EXT_OUTREG32(cmdq, addr, val) \
	{\
		mt_reg_sync_writel(val, addr); \
	}

#define DPI_EXT_LOG_PRINT(fmt, arg...)  \
	{\
		pr_debug(fmt, ##arg); \
	}

/***************************DPI DVT Case Start********************************/
int configInterlaceMode(unsigned int resolution)
{
	/*Enable Interlace mode */
	struct DPI_REG_CNTL ctr = DPI_REG->CNTL;
	/*Set LODD,LEVEN Vsize */
	struct DPI_REG_SIZE size = DPI_REG->SIZE;
	/*Set LODD,VFP/VPW/VBP */
	struct DPI_REG_TGEN_VWIDTH_LODD vwidth_lodd = DPI_REG->TGEN_VWIDTH_LODD;
	struct DPI_REG_TGEN_VPORCH_LODD vporch_lodd = DPI_REG->TGEN_VPORCH_LODD;
	/*Set LEVEN,VFP/VPW/VBP */
	struct DPI_REG_TGEN_VWIDTH_LEVEN vwidth_leven =
		DPI_REG->TGEN_VWIDTH_LEVEN;
	struct DPI_REG_TGEN_VPORCH_LEVEN vporch_leven =
		DPI_REG->TGEN_VPORCH_LEVEN;

	DPI_EXT_LOG_PRINT("%s, resolution: %d\n", __func__, resolution);

	if (resolution == 0x0D || resolution == 0x0E) {
	/*HDMI_VIDEO_720x480i_60Hz */

		ctr.INTL_EN = 1;
		ctr.VS_LODD_EN = 1;
		ctr.VS_LEVEN_EN = 1;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->CNTL, AS_UINT32(&ctr));

		size.HEIGHT = 240;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->SIZE, AS_UINT32(&size));

		vwidth_lodd.VPW_LODD = 3;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VWIDTH_LODD,
				AS_UINT32(&vwidth_lodd));
		DPI_EXT_LOG_PRINT("TGEN_VWIDTH_LODD: 0x%x\n",
				DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x28));

		vporch_lodd.VFP_LODD = 4;
		vporch_lodd.VBP_LODD = 15;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VPORCH_LODD,
				AS_UINT32(&vporch_lodd));
		DPI_EXT_LOG_PRINT("TGEN_VPORCH_LODD: 0x%x\n",
				DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x2C));

		vwidth_leven.VPW_LEVEN = 3;
		vwidth_leven.VPW_HALF_LEVEN = 1;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VWIDTH_LEVEN,
				AS_UINT32(&vwidth_leven));
		DPI_EXT_LOG_PRINT("TGEN_VWIDTH_LEVEN: 0x%x\n",
				DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x68));

		vporch_leven.VFP_LEVEN = 4;
		vporch_leven.VBP_LEVEN = 16;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VPORCH_LEVEN,
				AS_UINT32(&vporch_leven));
		DPI_EXT_LOG_PRINT("TGEN_VPORCH_LEVEN: 0x%x\n",
				DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x6C));
	} else if (resolution == 0x0C) {	/*HDMI_VIDEO_1920x1080i_60Hz */
		ctr.INTL_EN = 1;
		ctr.VS_LODD_EN = 1;
		ctr.VS_LEVEN_EN = 1;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->CNTL, AS_UINT32(&ctr));

		size.HEIGHT = 540;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->SIZE, AS_UINT32(&size));

		vwidth_lodd.VPW_LODD = 5;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VWIDTH_LODD,
				AS_UINT32(&vwidth_lodd));
		DPI_EXT_LOG_PRINT("TGEN_VWIDTH_LODD: 0x%x\n",
				DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x28));

		vporch_lodd.VFP_LODD = 2;
		vporch_lodd.VBP_LODD = 15;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VPORCH_LODD,
				AS_UINT32(&vporch_lodd));
		DPI_EXT_LOG_PRINT("TGEN_VPORCH_LODD: 0x%x\n",
				DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x2C));

		vwidth_leven.VPW_LEVEN = 5;
		vwidth_leven.VPW_HALF_LEVEN = 1;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VWIDTH_LEVEN,
				AS_UINT32(&vwidth_leven));
		DPI_EXT_LOG_PRINT("TGEN_VWIDTH_LEVEN: 0x%x\n",
				DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x68));

		vporch_leven.VFP_LEVEN = 2;
		vporch_leven.VBP_LEVEN = 16;
		DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VPORCH_LEVEN,
				AS_UINT32(&vporch_leven));
		DPI_EXT_LOG_PRINT("TGEN_VPORCH_LEVEN: 0x%x\n",
				DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x6C));
	}

	return 0;
}

int config3DMode(unsigned int resolution)
{
	/*Enable Interlace mode */
	struct DPI_REG_CNTL ctr = DPI_REG->CNTL;
	/*Set LODD,LEVEN Vsize */
	struct DPI_REG_SIZE size = DPI_REG->SIZE;
	/*Set LODD,VFP/VPW/VBP */
	struct DPI_REG_TGEN_VWIDTH_LODD vwidth_lodd = DPI_REG->TGEN_VWIDTH_LODD;
	struct DPI_REG_TGEN_VPORCH_LODD vporch_lodd = DPI_REG->TGEN_VPORCH_LODD;
	/*Set LEVEN,VFP/VPW/VBP */
	struct DPI_REG_TGEN_VWIDTH_RODD vwidth_rodd = DPI_REG->TGEN_VWIDTH_RODD;
	struct DPI_REG_TGEN_VPORCH_RODD vporch_rodd = DPI_REG->TGEN_VPORCH_RODD;

	DPI_EXT_LOG_PRINT("%s\n", __func__);

	ctr.TDFP_EN = 1;
	ctr.VS_RODD_EN = 0;
	ctr.FAKE_DE_RODD = 1;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->CNTL, AS_UINT32(&ctr));

	size.WIDTH = 1280;
	size.HEIGHT = 720;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->SIZE, AS_UINT32(&size));

	vwidth_lodd.VPW_LODD = 5;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VWIDTH_LODD,
			AS_UINT32(&vwidth_lodd));
	DPI_EXT_LOG_PRINT("TGEN_VWIDTH_LODD: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x28));

	vporch_lodd.VFP_LODD = 5;
	vporch_lodd.VBP_LODD = 20;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VPORCH_LODD,
			AS_UINT32(&vporch_lodd));
	DPI_EXT_LOG_PRINT("TGEN_VPORCH_LODD: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x2C));

	vwidth_rodd.VPW_RODD = 5;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VWIDTH_RODD,
			AS_UINT32(&vwidth_rodd));
	DPI_EXT_LOG_PRINT("TGEN_VWIDTH_RODD: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x70));

	vporch_rodd.VFP_RODD = 5;
	vporch_rodd.VBP_RODD = 20;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VPORCH_RODD,
			AS_UINT32(&vporch_rodd));
	DPI_EXT_LOG_PRINT("TGEN_VPORCH_RODD: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x74));

	return 0;
}

int config3DInterlaceMode(unsigned int resolution)
{
	/*Enable Interlace mode */
	struct DPI_REG_CNTL ctr = DPI_REG->CNTL;
	/*Set LODD,LEVEN Vsize */
	struct DPI_REG_SIZE size = DPI_REG->SIZE;
	/*Set LODD,VFP/VPW/VBP */
	struct DPI_REG_TGEN_VWIDTH_LODD vwidth_lodd = DPI_REG->TGEN_VWIDTH_LODD;
	struct DPI_REG_TGEN_VPORCH_LODD vporch_lodd = DPI_REG->TGEN_VPORCH_LODD;
	/*Set LEVEN,VFP/VPW/VBP */
	struct DPI_REG_TGEN_VWIDTH_RODD vwidth_rodd = DPI_REG->TGEN_VWIDTH_RODD;
	struct DPI_REG_TGEN_VPORCH_RODD vporch_rodd = DPI_REG->TGEN_VPORCH_RODD;
	/*Set LEVEN,VFP/VPW/VBP */
	struct DPI_REG_TGEN_VWIDTH_LEVEN vwidth_leven =
		DPI_REG->TGEN_VWIDTH_LEVEN;
	struct DPI_REG_TGEN_VPORCH_LEVEN vporch_leven =
		DPI_REG->TGEN_VPORCH_LEVEN;
	/*Set REVEN,VFP/VPW/VBP */
	struct DPI_REG_TGEN_VWIDTH_REVEN vwidth_reven =
		DPI_REG->TGEN_VWIDTH_REVEN;
	struct DPI_REG_TGEN_VPORCH_REVEN vporch_reven =
		DPI_REG->TGEN_VPORCH_REVEN;

	DPI_EXT_LOG_PRINT("%s\n", __func__);

	ctr.INTL_EN = 1;
	ctr.TDFP_EN = 1;

	ctr.VS_LODD_EN = 1;
	ctr.VS_LEVEN_EN = 0;
	ctr.VS_RODD_EN = 0;
	ctr.VS_REVEN_EN = 0;

	ctr.FAKE_DE_LODD = 0;
	ctr.FAKE_DE_RODD = 1;
	ctr.FAKE_DE_LEVEN = 1;
	ctr.FAKE_DE_REVEN = 1;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->CNTL, AS_UINT32(&ctr));

	size.HEIGHT = 240;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->SIZE, AS_UINT32(&size));

	vwidth_lodd.VPW_LODD = 3;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VWIDTH_LODD,
			AS_UINT32(&vwidth_lodd));
	DPI_EXT_LOG_PRINT("TGEN_VWIDTH_LODD: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x28));

	vporch_lodd.VFP_LODD = 4;
	vporch_lodd.VBP_LODD = 15;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VPORCH_LODD,
			AS_UINT32(&vporch_lodd));
	DPI_EXT_LOG_PRINT("TGEN_VPORCH_LODD: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x2C));

	vwidth_rodd.VPW_RODD = 3;
	vwidth_rodd.VPW_HALF_RODD = 1;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VWIDTH_RODD,
			AS_UINT32(&vwidth_rodd));
	DPI_EXT_LOG_PRINT("TGEN_VWIDTH_LEVEN: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x70));

	vporch_rodd.VFP_RODD = 4;
	vporch_rodd.VBP_RODD = 16;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VPORCH_RODD,
			AS_UINT32(&vporch_rodd));
	DPI_EXT_LOG_PRINT("TGEN_VPORCH_LEVEN: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x74));

	vwidth_leven.VPW_LEVEN = 3;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VWIDTH_LEVEN,
			AS_UINT32(&vwidth_leven));
	DPI_EXT_LOG_PRINT("TGEN_VWIDTH_LEVEN: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x68));

	vporch_leven.VFP_LEVEN = 4;
	vporch_leven.VBP_LEVEN = 15;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VPORCH_LEVEN,
			AS_UINT32(&vporch_leven));
	DPI_EXT_LOG_PRINT("TGEN_VPORCH_LEVEN: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x6C));

	vwidth_reven.VPW_REVEN = 3;
	vwidth_reven.VPW_HALF_REVEN = 1;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VWIDTH_REVEN,
			AS_UINT32(&vwidth_reven));
	DPI_EXT_LOG_PRINT("TGEN_VWIDTH_REVEN: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x78));

	vporch_reven.VFP_REVEN = 4;
	vporch_reven.VBP_REVEN = 16;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->TGEN_VPORCH_REVEN,
			AS_UINT32(&vporch_reven));
	DPI_EXT_LOG_PRINT("TGEN_VPORCH_REVEN: 0x%x\n",
			DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x7C));

	return 0;
}

unsigned int readDPIStatus(void)
{
	unsigned int status = DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x40);
	unsigned int field = (status >> 20) & 0x01;
	/* unsigned int line_num = status & 0x1FFF; */

	return field;
}

unsigned int readDPITDLRStatus(void)
{
	unsigned int status = DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x40);
	unsigned int tdlr = (status >> 21) & 0x01;

	return tdlr;
}

unsigned int clearDPIStatus(void)
{
	OUTREG32(&DPI_REG->STATUS, 0);

	return 0;
}

unsigned int clearDPIIntrStatus(void)
{
	OUTREG32(&DPI_REG->INT_STATUS, 0);

	return 0;
}


unsigned int readDPIIntrStatus(void)
{
	unsigned int status = DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x0C);
	return status;
}

unsigned int ClearDPIIntrStatus(void)
{
	OUTREG32(&DPI_REG->INT_STATUS, 0);

	return 0;
}

unsigned int enableRGB2YUV(enum AviColorSpace_e format)
{
	struct DPI_REG_CNTL ctr = DPI_REG->CNTL;
	struct DPI_REG_OUTPUT_SETTING output_setting = DPI_REG->OUTPUT_SETTING;

	DPI_EXT_LOG_PRINT("%s\n", __func__);

	if (format == acsYCbCr444) {
		ctr.RGB2YUV_EN = 1;

		DISP_REG_SET_FIELD(NULL, REG_FLD(1, 6),
				DISPSYS_DPI_BASE + 0x010, 1);
		DISP_REG_SET_FIELD(NULL, REG_FLD(3, 20),
				DISPSYS_DPI_BASE + 0x014, 3);
	} else if (format == acsYCbCr422) {
		ctr.RGB2YUV_EN = 1;
		ctr.YUV422_EN = 1;
		ctr.CLPF_EN = 1;
		output_setting.YC_MAP = 7;
	}

	return 0;
}

unsigned int enableSingleEdge(void)
{
	/*
	 *  Pixel clock = 2 * dpi clock
	 *  Disable dual edge
	 */
	struct DPI_REG_DDR_SETTING ddr_setting = DPI_REG->DDR_SETTING;

	DPI_EXT_LOG_PRINT("%s\n", __func__);

	ddr_setting.DDR_EN = 0;
	ddr_setting.DDR_4PHASE = 0;

	DPI_EXT_OUTREG32(NULL, &DPI_REG->OUTPUT_SETTING,
			AS_UINT32(&ddr_setting));
	return 0;
}

int enableAndGetChecksum(void)
{
	int checkSumNum = -1;
	struct DPI_REG_CHKSUM checkSum = DPI_REG->CHKSUM;

	DPI_EXT_LOG_PRINT("%s\n", __func__);

	checkSum.CHKSUM_EN = 1;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->CHKSUM, AS_UINT32(&checkSum));

	while (((DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x48) >> 30) & 0x1) == 0) {
		checkSumNum = (DPI_EXT_INREG32(DISPSYS_DPI_BASE + 0x48) &
				0x00FFFFFF);
		break;
	}

	return checkSumNum;
}

unsigned int configDpiRepetition(void)
{
	struct DPI_REG_CNTL ctrl = DPI_REG->CNTL;

	DPI_EXT_LOG_PRINT("%s\n", __func__);

	ctrl.PIXREP = 1;

	DPI_EXT_OUTREG32(NULL, &DPI_REG->CNTL, AS_UINT32(&ctrl));

	return 0;
}

unsigned int configDpiEmbsync(void)
{
	struct DPI_REG_CNTL ctrl = DPI_REG->CNTL;
	struct DPI_REG_DDR_SETTING ddr_setting = DPI_REG->DDR_SETTING;
	struct DPI_REG_EMBSYNC_SETTING emb_setting = DPI_REG->EMBSYNC_SETTING;

	DPI_EXT_LOG_PRINT("%s\n", __func__);

	ctrl.EMBSYNC_EN = 1;
	DPI_EXT_OUTREG32(NULL, &DPI_REG->CNTL, AS_UINT32(&ctrl));

	ddr_setting.DATA_THROT = 0;	/*should sii8348 support */
	DPI_EXT_OUTREG32(NULL, &DPI_REG->DDR_SETTING, AS_UINT32(&ddr_setting));

	emb_setting.EMBSYNC_OPT = 0;	/*should sii8348 support */
	DPI_EXT_OUTREG32(NULL, &DPI_REG->EMBSYNC_SETTING,
			AS_UINT32(&emb_setting));

	return 0;
}


/*****************************DPI DVT Case End*********************************/
#endif	/*#if defined(RDMA_DPI_PATH_SUPPORT) || defined(DPI_DVT_TEST_SUPPORT) */
