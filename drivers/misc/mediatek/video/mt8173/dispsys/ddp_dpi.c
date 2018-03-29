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

#ifdef BUILD_UBOOT
#define ENABLE_DSI_INTERRUPT 0

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

#include <linux/types.h>
#include <mt-plat/sync_write.h>
#include <linux/clk.h>
/*#include <mach/irqs.h>*/

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/wait.h>

#include "mtkfb.h"
#include "ddp_drv.h"
#include "ddp_hal.h"
#include "ddp_manager.h"
#include "ddp_dpi_reg.h"
#include "ddp_dpi.h"
#include "ddp_reg.h"
#include "ddp_log.h"
#include "ddp_irq.h"
#include "disp_drv_platform.h"

#undef  LOG_TAG
#define LOG_TAG "DPI"
#define ENABLE_DPI_INTERRUPT        1
#define DPI_INTERFACE_NUM           2
#define DPI_IDX(module)             ((module == DISP_MODULE_DPI0)?(0):(1))
/* #define DPI_IDX(module)             0 */
#define DPI_REG_OFFSET(r)       offsetof(DPI_REGS, r)
#define REG_ADDR(base, offset)  (((uint8_t *)(base)) + (offset))
#define DPI_OUTREG32(cmdq, addr, val) DISP_REG_SET(cmdq, (unsigned long)addr, val)

static bool s_isDpiPowerOn;
static bool s_isDpiStart;
static bool s_isDpiConfig;
static int dpi_vsync_irq_count[DPI_INTERFACE_NUM];
static int dpi_undflow_irq_count[DPI_INTERFACE_NUM];
static DPI_REGS regBackup;
static PDPI_REGS DPI_REG[DPI_INTERFACE_NUM];
static LCM_UTIL_FUNCS lcm_utils_dpi;

#define APMIXEDSYS ((unsigned long)(0x10209000))
unsigned long APMIXEDSYS_VA;	/* = ioremap(AP_PLL_CON0,0x1000); */

#define TOP_CLOCK ((unsigned long)(0x10000000))
unsigned long TOP_CLOCK_VA;

const UINT32 BACKUP_DPI_REG_OFFSETS[] = {
	DPI_REG_OFFSET(INT_ENABLE),
	DPI_REG_OFFSET(CNTL),
	DPI_REG_OFFSET(SIZE),

	DPI_REG_OFFSET(TGEN_HWIDTH),
	DPI_REG_OFFSET(TGEN_HPORCH),
	DPI_REG_OFFSET(TGEN_VWIDTH_LODD),
	DPI_REG_OFFSET(TGEN_VPORCH_LODD),

	DPI_REG_OFFSET(BG_HCNTL),
	DPI_REG_OFFSET(BG_VCNTL),
	DPI_REG_OFFSET(BG_COLOR),

	DPI_REG_OFFSET(TGEN_VWIDTH_LEVEN),
	DPI_REG_OFFSET(TGEN_VPORCH_LEVEN),
	DPI_REG_OFFSET(TGEN_VWIDTH_RODD),

	DPI_REG_OFFSET(TGEN_VPORCH_RODD),
	DPI_REG_OFFSET(TGEN_VWIDTH_REVEN),

	DPI_REG_OFFSET(TGEN_VPORCH_REVEN),
	DPI_REG_OFFSET(ESAV_VTIM_LOAD),
	DPI_REG_OFFSET(ESAV_VTIM_ROAD),
	DPI_REG_OFFSET(ESAV_FTIM),
};

/*the static functions declare*/
static void lcm_udelay(UINT32 us)
{
	udelay(us);
}

static void lcm_mdelay(UINT32 ms)
{
	msleep(ms);
}

static void lcm_set_reset_pin(UINT32 value)
{
	/* DPI_OUTREG32(0, MMSYS_CONFIG_BASE+0x150, value); */
}

static void lcm_send_cmd(UINT32 cmd)
{
	/* DPI_OUTREG32(0, LCD_BASE+0x0F80, cmd); */
}

static void lcm_send_data(UINT32 data)
{
	/* DPI_OUTREG32(0, LCD_BASE+0x0F90, data); */
}

static void _BackupDPIRegisters(DISP_MODULE_ENUM module)
{
	UINT32 i;
	DPI_REGS *reg = &regBackup;

	for (i = 0; i < ARY_SIZE(BACKUP_DPI_REG_OFFSETS); ++i) {
		DPI_OUTREG32(0, REG_ADDR(reg, BACKUP_DPI_REG_OFFSETS[i]),
			     AS_UINT32(REG_ADDR
				       (DPI_REG[DPI_IDX(module)], BACKUP_DPI_REG_OFFSETS[i])));
	}
}

static void _RestoreDPIRegisters(DISP_MODULE_ENUM module)
{
	UINT32 i;
	DPI_REGS *reg = &regBackup;

	if (DPI_REG[DPI_IDX(module)] != 0) {
		for (i = 0; i < ARY_SIZE(BACKUP_DPI_REG_OFFSETS); ++i) {
			DPI_OUTREG32(0, REG_ADDR(DPI_REG[DPI_IDX(module)], BACKUP_DPI_REG_OFFSETS[i]),
				     AS_UINT32(REG_ADDR(reg, BACKUP_DPI_REG_OFFSETS[i])));
		}
	}
}

int Is_interlace_resolution(UINT32 resolution)
{
	if ((resolution == DPI_VIDEO_1920x1080i_60Hz) ||
	    (resolution == DPI_VIDEO_1920x1080i_50Hz) ||
#if defined(MTK_INTERNAL_MHL_SUPPORT)
	    (resolution == DPI_VIDEO_1920x1080i3d_sbs_60Hz) ||
	    (resolution == DPI_VIDEO_1920x1080i3d_sbs_50Hz) ||
#endif
	    (resolution == DPI_VIDEO_1920x1080i3d_60Hz) ||
	    (resolution == DPI_VIDEO_1920x1080i3d_60Hz))
		return true;
	else
		return false;
}

int Is_3d_resolution(UINT32 resolution)
{
	if ((resolution == DPI_VIDEO_1280x720p3d_60Hz) ||
	    (resolution == DPI_VIDEO_1280x720p3d_50Hz) ||
	    (resolution == DPI_VIDEO_1920x1080i3d_60Hz) ||
	    (resolution == DPI_VIDEO_1920x1080i3d_60Hz) ||
	    (resolution == DPI_VIDEO_1920x1080p3d_24Hz) ||
	    (resolution == DPI_VIDEO_1920x1080p3d_23Hz))
		return true;
	else
		return false;
}

/*the functions declare*/
/*DPI clock setting - use TVDPLL provide DPI clock*/
DPI_STATUS ddp_dpi_ConfigPclk(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, unsigned int clk_req,
			      DPI_POLARITY polarity)
{
	UINT32 dpickpol = 1, dpickoutdiv = 1, dpickdut = 1;
	DPI_REG_OUTPUT_SETTING ctrl = DPI_REG[DPI_IDX(module)]->OUTPUT_SETTING;
	DPI_REG_CLKCNTL clkcon = DPI_REG[DPI_IDX(module)]->DPI_CLKCON;
	UINT32 u4Feq = 0;
	struct clk *clksrc = NULL;
	unsigned long bPix;

	DDPMSG("ddp_dpi_ConfigPclk clk_req = %d\n", clk_req);

	switch (clk_req) {
	case DPI_VIDEO_720x480p_60Hz:
	case DPI_VIDEO_720x576p_50Hz:
	case DPI_VIDEO_1920x1080p3d_24Hz:
	case DPI_VIDEO_1280x720p_60Hz:
		dpickpol = 0;
		dpickdut = 0;
		break;

	case DPI_VIDEO_1920x1080p_30Hz:
	case DPI_VIDEO_1280x720p_50Hz:
	case DPI_VIDEO_1920x1080i_50Hz:
	case DPI_VIDEO_1920x1080p_25Hz:
	case DPI_VIDEO_1920x1080p_24Hz:
	case DPI_VIDEO_1920x1080p_50Hz:
	case DPI_VIDEO_1280x720p3d_50Hz:
	case DPI_VIDEO_1920x1080i3d_50Hz:
	case DPI_VIDEO_1920x1080i_60Hz:
	case DPI_VIDEO_1920x1080p_23Hz:
	case DPI_VIDEO_1920x1080p_29Hz:
	case DPI_VIDEO_1920x1080p_60Hz:
	case DPI_VIDEO_1280x720p3d_60Hz:
	case DPI_VIDEO_1920x1080i3d_60Hz:
	case DPI_VIDEO_1920x1080p3d_23Hz:
		break;

	default:
		DDPMSG("unknown clock frequency: %d\n", clk_req);
		break;
	}

	switch (clk_req) {
	case DPI_VIDEO_720x480p_60Hz:
	case DPI_VIDEO_720x576p_50Hz:
	default:
		bPix = 216000000 * 3;
		break;

	case DPI_VIDEO_1280x720p_60Hz:	/* 74.175M pixel clock */
	case DPI_VIDEO_1920x1080i_60Hz:
	case DPI_VIDEO_1920x1080p_23Hz:
	case DPI_VIDEO_1920x1080p_29Hz:
	case DPI_VIDEO_1920x1080p_60Hz:	/* 148.35M pixel clock */
	case DPI_VIDEO_1280x720p3d_60Hz:
	case DPI_VIDEO_1920x1080i3d_60Hz:
	case DPI_VIDEO_1920x1080p3d_23Hz:
	case DPI_VIDEO_2160P_23_976HZ:
	case DPI_VIDEO_2160P_29_97HZ:	/* 296.976m pixel clock */
		bPix = 593400000 * 3;
		break;

	case DPI_VIDEO_1280x720p_50Hz:	/* 74.25M pixel clock */
	case DPI_VIDEO_1920x1080i_50Hz:
	case DPI_VIDEO_1920x1080p_24Hz:
	case DPI_VIDEO_1920x1080p_25Hz:
	case DPI_VIDEO_1920x1080p_30Hz:
	case DPI_VIDEO_1920x1080p_50Hz:	/* 148.50M pixel clock */
	case DPI_VIDEO_1280x720p3d_50Hz:
	case DPI_VIDEO_1920x1080i3d_50Hz:
	case DPI_VIDEO_1920x1080p3d_24Hz:
	case DPI_VIDEO_2160P_24HZ:	/* 297m pixel clock */
	case DPI_VIDEO_2160P_25HZ:	/* 297m pixel clock */
	case DPI_VIDEO_2160P_30HZ:	/* 297m pixel clock */
	case DPI_VIDEO_2161P_24HZ:	/* 297m pixel clock */
		bPix = 594000000 * 3;
		break;
	}

	/* DISP_LOG_PRINT(ANDROID_LOG_WARN, "DPI", "TVDPLL clock setting clk %d, clksrc: %d\n", clk_req,  clksrc); */
	if ((clk_req == DPI_VIDEO_720x480p_60Hz) || (clk_req == DPI_VIDEO_720x576p_50Hz))
		u4Feq = 0;	/* 27M */
	else if ((clk_req == DPI_VIDEO_1920x1080p_60Hz)
		 || (clk_req == DPI_VIDEO_1920x1080p_50Hz)
		 || (clk_req == DPI_VIDEO_1280x720p3d_60Hz)
		 || (clk_req == DPI_VIDEO_1280x720p3d_50Hz)
		 || (clk_req == DPI_VIDEO_1920x1080i3d_60Hz)
		 || (clk_req == DPI_VIDEO_1920x1080i3d_50Hz)
		 || (clk_req == DPI_VIDEO_1920x1080p3d_24Hz)
		 || (clk_req == DPI_VIDEO_1920x1080p3d_23Hz))
		u4Feq = 2;	/* 148M */
	else
		u4Feq = 1;	/* 74M */

	if ((DPI_VIDEO_2160P_23_976HZ == clk_req) ||
	    (DPI_VIDEO_2160P_24HZ == clk_req) ||
	    (DPI_VIDEO_2160P_25HZ == clk_req) ||
	    (DPI_VIDEO_2160P_29_97HZ == clk_req) ||
	    (DPI_VIDEO_2160P_30HZ == clk_req) || (DPI_VIDEO_2161P_24HZ == clk_req)) {
		u4Feq = 3;	/* 297M no deepcolor */
	}

	if (u4Feq == 3)
		clksrc = ddp_clk_map[TOP_TVDPLL_D2];
	else if (u4Feq == 2)
		clksrc = ddp_clk_map[TOP_TVDPLL_D4];
	else if (u4Feq == 1)
		clksrc = ddp_clk_map[TOP_TVDPLL_D8];
	else if (u4Feq == 0)
		clksrc = ddp_clk_map[TOP_TVDPLL_D8];

	DDPMSG("u4Feq = 0x%x, bPix = 0x%lx\n", u4Feq, bPix);

#ifndef CONFIG_FPGA_EARLY_PORTING

	clk_prepare_enable(ddp_clk_map[MM_CLK_MUX_DPI0_SEL]);
	clk_set_parent(ddp_clk_map[MM_CLK_MUX_DPI0_SEL], clksrc);
	clk_disable_unprepare(ddp_clk_map[MM_CLK_MUX_DPI0_SEL]);

	DDPMSG("get dpi0 sel clk = 0x%x\n", INREG32(TOP_CLOCK_VA + 0xa0));

	clk_prepare_enable(ddp_clk_map[APMIXED_TVDPLL]);
	clk_set_rate(ddp_clk_map[APMIXED_TVDPLL], bPix);
	clk_disable_unprepare(ddp_clk_map[APMIXED_TVDPLL]);

	DDPMSG("get dpi clk 0x270=0x%x ,0x274=0x%x\n",
	       INREG32(APMIXEDSYS_VA + 0x270), INREG32(APMIXEDSYS_VA + 0x274));
#endif

	/*IO driving setting */
	MASKREG32(DISPSYS_IO_DRIVING, 0x3C00, 0x0);	/* 0x1400 for 8mA, 0x0 for 4mA */
	MASKREG32(APMIXEDSYS_VA + 0x40, 0x70000, 0x30000);

	/*DPI output clock polarity */
	ctrl.CLK_POL = (DPI_POLARITY_FALLING == polarity) ? 1 : 0;
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->OUTPUT_SETTING, AS_UINT32(&ctrl));

	clkcon.DPI_CKOUT_DIV = dpickoutdiv;
	clkcon.DPI_CK_POL = dpickpol;
	clkcon.DPI_CK_DUT = dpickdut;
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->DPI_CLKCON, AS_UINT32(&clkcon));

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_ConfigDE(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, DPI_POLARITY polarity)
{
	DPI_REG_OUTPUT_SETTING pol = DPI_REG[DPI_IDX(module)]->OUTPUT_SETTING;

	pol.DE_POL = (DPI_POLARITY_FALLING == polarity) ? 1 : 0;
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->OUTPUT_SETTING, AS_UINT32(&pol));

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_ConfigVsync(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, DPI_POLARITY polarity,
			       UINT32 pulseWidth, UINT32 backPorch, UINT32 frontPorch)
{
	DPI_REG_TGEN_VWIDTH_LODD vwidth_lodd = DPI_REG[DPI_IDX(module)]->TGEN_VWIDTH_LODD;
	DPI_REG_TGEN_VPORCH_LODD vporch_lodd = DPI_REG[DPI_IDX(module)]->TGEN_VPORCH_LODD;
	DPI_REG_OUTPUT_SETTING pol = DPI_REG[DPI_IDX(module)]->OUTPUT_SETTING;
	DPI_REG_CNTL VS = DPI_REG[DPI_IDX(module)]->CNTL;

	pol.VSYNC_POL = (DPI_POLARITY_FALLING == polarity) ? 1 : 0;
	vwidth_lodd.VPW_LODD = pulseWidth;
	vporch_lodd.VBP_LODD = backPorch;
	vporch_lodd.VFP_LODD = frontPorch;

	VS.VS_LODD_EN = 1;
	VS.VS_RODD_EN = 0;
	VS.VS_REVEN_EN = 0;

	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->OUTPUT_SETTING, AS_UINT32(&pol));
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->TGEN_VWIDTH_LODD, AS_UINT32(&vwidth_lodd));
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->TGEN_VPORCH_LODD, AS_UINT32(&vporch_lodd));
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->CNTL, AS_UINT32(&VS));

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_ConfigHsync(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, DPI_POLARITY polarity,
			       UINT32 pulseWidth, UINT32 backPorch, UINT32 frontPorch)
{
	DPI_REG_TGEN_HPORCH hporch = DPI_REG[DPI_IDX(module)]->TGEN_HPORCH;
	DPI_REG_OUTPUT_SETTING pol = DPI_REG[DPI_IDX(module)]->OUTPUT_SETTING;

	hporch.HBP = backPorch;
	hporch.HFP = frontPorch;
	pol.HSYNC_POL = (DPI_POLARITY_FALLING == polarity) ? 1 : 0;
	DPI_REG[DPI_IDX(module)]->TGEN_HWIDTH = pulseWidth;

	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->TGEN_HWIDTH, pulseWidth);
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->TGEN_HPORCH, AS_UINT32(&hporch));
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->OUTPUT_SETTING, AS_UINT32(&pol));

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_ConfigDualEdge(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, bool enable,
				  UINT32 mode)
{
	DPI_OUTREGBIT(cmdq, DPI_REG_OUTPUT_SETTING, DPI_REG[DPI_IDX(module)]->OUTPUT_SETTING,
		      DUAL_EDGE_SEL, enable);

#ifndef CONFIG_FPGA_EARLY_PORTING
	DPI_OUTREGBIT(cmdq, DPI_REG_DDR_SETTING, DPI_REG[DPI_IDX(module)]->DDR_SETTING, DDR_4PHASE,
		      1);
	DPI_OUTREGBIT(cmdq, DPI_REG_DDR_SETTING, DPI_REG[DPI_IDX(module)]->DDR_SETTING, DDR_EN, 1);
#endif

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_ConfigBG(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, bool enable, int BG_W,
			    int BG_H)
{
	if (enable == false) {
		DPI_OUTREGBIT(cmdq, DPI_REG_CNTL, DPI_REG[DPI_IDX(module)]->CNTL, BG_EN, 0);
	} else if (BG_W || BG_H) {
		DPI_OUTREGBIT(cmdq, DPI_REG_CNTL, DPI_REG[DPI_IDX(module)]->CNTL, BG_EN, 1);
		DPI_OUTREGBIT(cmdq, DPI_REG_BG_HCNTL, DPI_REG[DPI_IDX(module)]->BG_HCNTL, BG_RIGHT,
			      BG_W / 4);
		DPI_OUTREGBIT(cmdq, DPI_REG_BG_HCNTL, DPI_REG[DPI_IDX(module)]->BG_HCNTL, BG_LEFT,
			      BG_W - BG_W / 4);
		DPI_OUTREGBIT(cmdq, DPI_REG_BG_VCNTL, DPI_REG[DPI_IDX(module)]->BG_VCNTL, BG_BOT,
			      BG_H / 4);
		DPI_OUTREGBIT(cmdq, DPI_REG_BG_VCNTL, DPI_REG[DPI_IDX(module)]->BG_VCNTL, BG_TOP,
			      BG_H - BG_H / 4);
		DPI_OUTREGBIT(cmdq, DPI_REG_CNTL, DPI_REG[DPI_IDX(module)]->CNTL, BG_EN, 1);
		DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->BG_COLOR, 0);
	}

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_ConfigSize(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, UINT32 width,
			      UINT32 height)
{
	DPI_REG_SIZE size = DPI_REG[DPI_IDX(module)]->SIZE;

	size.WIDTH = width;
	size.HEIGHT = height;
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->SIZE, AS_UINT32(&size));

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_EnableColorBar(DISP_MODULE_ENUM module)
{
	/*enable internal pattern - color bar */
	if (module == DISP_MODULE_DPI0)
		DPI_OUTREG32(0, DISPSYS_DPI0_BASE + 0xF00, 0x41);
	else
		DPI_OUTREG32(0, DISPSYS_DPI1_BASE + 0xF00, 0x41);

	return DPI_STATUS_OK;
}

int ddp_dpi_power_on(DISP_MODULE_ENUM module, void *cmdq_handle)
{
	int ret = 0;

	DDPMSG("ddp_dpi_power_on, s_isDpiPowerOn %d\n", s_isDpiPowerOn);
	if (!s_isDpiPowerOn) {
#ifndef DISABLE_CLOCK_API
		if (module == DISP_MODULE_DPI0) {
			/* clk_prepare_enable(ddp_clk_map[MM_CLK_MUX_DPI0_SEL]); */

			/* ret += clk_prepare(ddp_clk_map[APMIXED_TVDPLL]); */
			/* ret += clk_enable(ddp_clk_map[APMIXED_TVDPLL]); */

			ret += clk_prepare(ddp_clk_map[MM_CLK_DPI_PIXEL]);
			ret += clk_enable(ddp_clk_map[MM_CLK_DPI_PIXEL]);
			ret += clk_prepare(ddp_clk_map[MM_CLK_DPI_ENGINE]);
			ret += clk_enable(ddp_clk_map[MM_CLK_DPI_ENGINE]);
			/*
			   ret += clk_prepare(ddp_clk_map[MM_CLK_HDMI_PIXEL]);
			   ret += clk_enable(ddp_clk_map[MM_CLK_HDMI_PIXEL]);
			   ret += clk_prepare(ddp_clk_map[MM_CLK_HDMI_PLLCK]);
			   ret += clk_enable(ddp_clk_map[MM_CLK_HDMI_PLLCK]);

			   ret += clk_prepare(ddp_clk_map[MM_CLK_HDMI_AUDIO]);
			   ret += clk_enable(ddp_clk_map[MM_CLK_HDMI_AUDIO]);
			   ret += clk_prepare(ddp_clk_map[MM_CLK_HDMI_SPDIF]);
			   ret += clk_enable(ddp_clk_map[MM_CLK_HDMI_SPDIF]);

			   ret += clk_prepare(ddp_clk_map[MM_CLK_HDMI_HDCP]);
			   ret += clk_enable(ddp_clk_map[MM_CLK_HDMI_HDCP]);
			   ret += clk_prepare(ddp_clk_map[MM_CLK_HDMI_HDCP_24M]);
			   ret += clk_enable(ddp_clk_map[MM_CLK_HDMI_HDCP_24M]); */
		} else {
			ret += clk_prepare(ddp_clk_map[MM_CLK_DPI1_PIXEL]);
			ret += clk_enable(ddp_clk_map[MM_CLK_DPI1_PIXEL]);
			ret += clk_prepare(ddp_clk_map[MM_CLK_DPI1_ENGINE]);
			ret += clk_enable(ddp_clk_map[MM_CLK_DPI1_ENGINE]);

			ret += clk_prepare(ddp_clk_map[MM_CLK_LVDS_PIXEL]);
			ret += clk_enable(ddp_clk_map[MM_CLK_LVDS_PIXEL]);
			ret += clk_prepare(ddp_clk_map[MM_CLK_LVDS_CTS]);
			ret += clk_enable(ddp_clk_map[MM_CLK_LVDS_CTS]);
		}
#endif
		if (ret > 0)
			DDPERR("power manager API return false\n");
		_RestoreDPIRegisters(module);
		s_isDpiPowerOn = true;
	}

	return 0;
}

int ddp_dpi_power_off(DISP_MODULE_ENUM module, void *cmdq_handle)
{
	int ret = 0;

	DDPMSG("ddp_dpi_power_off, s_isDpiPowerOn %d\n", s_isDpiPowerOn);
	if (s_isDpiPowerOn) {
#ifndef DISABLE_CLOCK_API
		if (module == DISP_MODULE_DPI0) {
			_BackupDPIRegisters(module);
			/* clk_disable_unprepare(ddp_clk_map[MM_CLK_MUX_DPI0_SEL]); */

			/* clk_disable(ddp_clk_map[APMIXED_TVDPLL]); */
			/* clk_unprepare(ddp_clk_map[APMIXED_TVDPLL]); */

			clk_disable(ddp_clk_map[MM_CLK_DPI_PIXEL]);
			clk_unprepare(ddp_clk_map[MM_CLK_DPI_PIXEL]);
			clk_disable(ddp_clk_map[MM_CLK_DPI_ENGINE]);
			clk_unprepare(ddp_clk_map[MM_CLK_DPI_ENGINE]);
			/*
			   clk_disable(ddp_clk_map[MM_CLK_HDMI_PIXEL]);
			   clk_unprepare(ddp_clk_map[MM_CLK_HDMI_PIXEL]);
			   clk_disable(ddp_clk_map[MM_CLK_HDMI_PLLCK]);
			   clk_unprepare(ddp_clk_map[MM_CLK_HDMI_PLLCK]);

			   clk_disable(ddp_clk_map[MM_CLK_HDMI_AUDIO]);
			   clk_unprepare(ddp_clk_map[MM_CLK_HDMI_AUDIO]);
			   clk_disable(ddp_clk_map[MM_CLK_HDMI_SPDIF]);
			   clk_unprepare(ddp_clk_map[MM_CLK_HDMI_SPDIF]);

			   clk_disable(ddp_clk_map[MM_CLK_HDMI_HDCP]);
			   clk_unprepare(ddp_clk_map[MM_CLK_HDMI_HDCP]);
			   clk_disable(ddp_clk_map[MM_CLK_HDMI_HDCP_24M]);
			   clk_unprepare(ddp_clk_map[MM_CLK_HDMI_HDCP_24M]);
			 */
		} else {
			clk_disable(ddp_clk_map[MM_CLK_DPI1_PIXEL]);
			clk_unprepare(ddp_clk_map[MM_CLK_DPI1_PIXEL]);
			clk_disable(ddp_clk_map[MM_CLK_DPI1_ENGINE]);
			clk_unprepare(ddp_clk_map[MM_CLK_DPI1_ENGINE]);

			clk_disable(ddp_clk_map[MM_CLK_LVDS_PIXEL]);
			clk_unprepare(ddp_clk_map[MM_CLK_LVDS_PIXEL]);
			clk_disable(ddp_clk_map[MM_CLK_LVDS_CTS]);
			clk_unprepare(ddp_clk_map[MM_CLK_LVDS_CTS]);
		}
#endif
		if (ret > 0)
			DDPERR("power manager API return false\n");
		s_isDpiPowerOn = false;
	}

	return 0;

}


DPI_STATUS ddp_dpi_3d_ctrl(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, bool fg3DFrame)
{
	DPI_REG_CNTL ctrl = DPI_REG[DPI_IDX(module)]->CNTL;

	ctrl.TDFP_EN = fg3DFrame;

	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->CNTL, AS_UINT32(&ctrl));

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_config_colorspace(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, UINT8 ColorSpace,
				     UINT8 HDMI_Res)
{
	UINT8 i = 0;
	DPI_REG_CNTL ctrl = DPI_REG[DPI_IDX(module)]->CNTL;

	DDPMSG("[DPI1] ddp_dpi_config_colorspace: ColorSpace[%d] / HDMI Resolution[%d]\n",
	       ColorSpace, HDMI_Res);

	if ((HDMI_Res == 0) || (HDMI_Res == 1)) {	/* SD */
		i = 1;
	} else {		/* HD */

		i = 0;
	}

	/* DPI1_MatrixCoef(dpi1_coef[i][0][0], dpi1_coef[i][0][1], dpi1_coef[i][0][2], */
	/* dpi1_coef[i][1][0], dpi1_coef[i][1][1], dpi1_coef[i][1][2], */
	/* dpi1_coef[i][2][0], dpi1_coef[i][2][1], dpi1_coef[i][2][2]); */
	/* DPI1_MatrixPreOffset(dpi1_coef[i][3][0], dpi1_coef[i][3][1], dpi1_coef[i][3][2]); */
	/* DPI1_MatrixPostOffset(dpi1_coef[i][4][1], dpi1_coef[i][4][0], dpi1_coef[i][4][2]); */

	ctrl.R601_SEL = i;
	if ((YCBCR_444 == ColorSpace) || (YCBCR_444_FULL == ColorSpace)) {	/* YUV444 */
		ctrl.YUV422_EN = 0;
		ctrl.RGB2YUV_EN = 1;
	} else if ((YCBCR_422 == ColorSpace) || (YCBCR_422_FULL == ColorSpace)) {	/* YUV422 */
		ctrl.YUV422_EN = 1;
		ctrl.RGB2YUV_EN = 1;
	} else {		/* RGB */

		ctrl.YUV422_EN = 0;
		ctrl.RGB2YUV_EN = 0;
		ctrl.R601_SEL = 0;
	}
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->CNTL, AS_UINT32(&ctrl));

	return DPI_STATUS_OK;
}

/*
DPI_VIDEO_1920x1080p_60Hz
DPI_VIDEO_1920x1080p_50Hz
DPI_VIDEO_1280x720p3d_60Hz
DPI_VIDEO_2160P_30HZ
*/
DPI_STATUS ddp_dpi_yuv422_setting(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, UINT32 uvsw)
{
	DPI_REG_YUV422_SETTING uvset = DPI_REG[DPI_IDX(module)]->YUV422_SETTING;

	uvset.UV_SWAP = uvsw;
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->YUV422_SETTING, AS_UINT32(&uvset));

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_clpf_setting(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, UINT8 clpfType,
				bool roundingEnable, UINT32 clpfen)
{
	DPI_REG_CLPF_SETTING setting = DPI_REG[DPI_IDX(module)]->CLPF_SETTING;
	DPI_REG_CNTL ctrl = DPI_REG[DPI_IDX(module)]->CNTL;

	setting.CLPF_TYPE = clpfType;
	setting.ROUND_EN = roundingEnable ? 1 : 0;
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->CLPF_SETTING, AS_UINT32(&setting));

	ctrl.CLPF_EN = clpfen;
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->CNTL, AS_UINT32(&ctrl));

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_vsync_lr_enable(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, UINT32 vs_lo_en,
				   UINT32 vs_le_en, UINT32 vs_ro_en, UINT32 vs_re_en)
{
	DPI_REG_CNTL ctrl = DPI_REG[DPI_IDX(module)]->CNTL;

	ctrl.VS_LODD_EN = vs_lo_en;
	ctrl.VS_LEVEN_EN = vs_le_en;
	ctrl.VS_RODD_EN = vs_ro_en;
	ctrl.VS_REVEN_EN = vs_re_en;

	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->CNTL, AS_UINT32(&ctrl));

	return DPI_STATUS_OK;
}

/*
DPI_VIDEO_1920x1080i_50Hz
DPI_VIDEO_1920x1080i_60Hz
*/
DPI_STATUS ddp_dpi_ConfigVsync_LEVEN(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, UINT32 pulseWidth,
				     UINT32 backPorch, UINT32 frontPorch, bool fgInterlace)
{
	DPI_REG_TGEN_VWIDTH_LEVEN vwidth_leven = DPI_REG[DPI_IDX(module)]->TGEN_VWIDTH_LEVEN;
	DPI_REG_TGEN_VPORCH_LEVEN vporch_leven = DPI_REG[DPI_IDX(module)]->TGEN_VPORCH_LEVEN;
	DPI_REG_CNTL VS = DPI_REG[DPI_IDX(module)]->CNTL;

	vwidth_leven.VPW_LEVEN = pulseWidth;
	vwidth_leven.VPW_HALF_LEVEN = fgInterlace;
	vporch_leven.VBP_LEVEN = (fgInterlace ? (backPorch + 1) : backPorch);
	vporch_leven.VFP_LEVEN = frontPorch;

	VS.INTL_EN = fgInterlace;
	VS.VS_LEVEN_EN = fgInterlace;
	VS.VS_LODD_EN = fgInterlace;
	VS.FAKE_DE_RODD = fgInterlace;

	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->TGEN_VWIDTH_LEVEN, AS_UINT32(&vwidth_leven));
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->TGEN_VPORCH_LEVEN, AS_UINT32(&vporch_leven));
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->CNTL, AS_UINT32(&VS));

	return DPI_STATUS_OK;
}

DPI_STATUS ddp_dpi_ConfigVsync_REVEN(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, UINT32 pulseWidth,
				     UINT32 backPorch, UINT32 frontPorch, bool fgInterlace)
{
	DPI_REG_TGEN_VWIDTH_REVEN vwidth_reven = DPI_REG[DPI_IDX(module)]->TGEN_VWIDTH_REVEN;
	DPI_REG_TGEN_VPORCH_REVEN vporch_reven = DPI_REG[DPI_IDX(module)]->TGEN_VPORCH_REVEN;

	vwidth_reven.VPW_REVEN = pulseWidth;
	vwidth_reven.VPW_HALF_REVEN = fgInterlace;
	vporch_reven.VBP_REVEN = backPorch;
	vporch_reven.VFP_REVEN = frontPorch;

	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->TGEN_VWIDTH_REVEN, AS_UINT32(&vwidth_reven));
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->TGEN_VPORCH_REVEN, AS_UINT32(&vporch_reven));

	return DPI_STATUS_OK;
}

/*
DPI_VIDEO_1280x720p3d_60Hz
DPI_VIDEO_1920x1080p3d_24Hz
*/
DPI_STATUS ddp_dpi_ConfigVsync_RODD(DISP_MODULE_ENUM module, cmdqRecHandle cmdq, UINT32 pulseWidth,
				    UINT32 backPorch, UINT32 frontPorch)
{
	DPI_REG_TGEN_VWIDTH_RODD vwidth_rodd = DPI_REG[DPI_IDX(module)]->TGEN_VWIDTH_RODD;
	DPI_REG_TGEN_VPORCH_RODD vporch_rodd = DPI_REG[DPI_IDX(module)]->TGEN_VPORCH_RODD;

	vwidth_rodd.VPW_RODD = pulseWidth;
	vporch_rodd.VBP_RODD = backPorch;
	vporch_rodd.VFP_RODD = frontPorch;

	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->TGEN_VWIDTH_RODD, AS_UINT32(&vwidth_rodd));
	DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->TGEN_VPORCH_RODD, AS_UINT32(&vporch_rodd));

	return DPI_STATUS_OK;
}

int ddp_dpi_config(DISP_MODULE_ENUM module, disp_ddp_path_config *config, void *cmdq_handle)
{
	if (s_isDpiConfig == false /*|| (module == DISP_MODULE_DPI0) */) {
		LCM_DPI_PARAMS *dpi_config = &(config->dispif_config.dpi);

		DDPMSG("ddp_dpi%x_config DPI status:%x, cmdq:%p\n",
		       module, INREG32(&DPI_REG[DPI_IDX(module)]->STATUS), cmdq_handle);

		switch (module) {
			/* hdmi case */
		case DISP_MODULE_DPI0:
			{
				ddp_dpi_ConfigPclk(module, cmdq_handle, dpi_config->dpi_clock,
						   dpi_config->clk_pol);
				ddp_dpi_ConfigSize(module, cmdq_handle, dpi_config->width,
						   Is_interlace_resolution(dpi_config->dpi_clock) ?
						   dpi_config->height / 2 : dpi_config->height);
				ddp_dpi_3d_ctrl(module, cmdq_handle,
						Is_3d_resolution(dpi_config->dpi_clock));
				ddp_dpi_ConfigVsync_LEVEN(module, cmdq_handle,
							  dpi_config->vsync_pulse_width,
							  dpi_config->vsync_back_porch,
							  dpi_config->vsync_front_porch,
							  Is_interlace_resolution
							  (dpi_config->dpi_clock));
			}
			break;

			/* lvds case */
		case DISP_MODULE_DPI1:
			{
				ddp_dpi_lvds_config(module, dpi_config, cmdq_handle);
				ddp_dpi_ConfigSize(module, cmdq_handle, dpi_config->width,
						   dpi_config->height);
			}
			break;

		default:
			DDPMSG("unknown clock interface: %d\n", module);
			return 0;
		}

		ddp_dpi_ConfigBG(module, cmdq_handle, true, dpi_config->bg_width,
				 dpi_config->bg_height);
		ddp_dpi_ConfigDE(module, cmdq_handle, dpi_config->de_pol);
		ddp_dpi_ConfigVsync(module, cmdq_handle, dpi_config->vsync_pol,
				    dpi_config->vsync_pulse_width, dpi_config->vsync_back_porch,
				    dpi_config->vsync_front_porch);
		ddp_dpi_ConfigHsync(module, cmdq_handle, dpi_config->hsync_pol,
				    dpi_config->hsync_pulse_width, dpi_config->hsync_back_porch,
				    dpi_config->hsync_front_porch);

		s_isDpiConfig = true;
		DDPMSG("ddp_dpi_config done\n");
	}

	return 0;
}

int ddp_dpi_reset(DISP_MODULE_ENUM module, void *cmdq_handle)
{
	DDPMSG("ddp_dpi_reset\n");

	DPI_OUTREGBIT(NULL, DPI_REG_RST, DPI_REG[DPI_IDX(module)]->DPI_RST, RST, 1);
	DPI_OUTREGBIT(NULL, DPI_REG_RST, DPI_REG[DPI_IDX(module)]->DPI_RST, RST, 0);

	return 0;
}

int ddp_dpi_start(DISP_MODULE_ENUM module, void *cmdq)
{
	return 0;
}

int ddp_dpi_trigger(DISP_MODULE_ENUM module, void *cmdq)
{
	if (s_isDpiStart == false) {
		DDPMSG("ddp_dpi_start\n");
		ddp_dpi_reset(module, cmdq);
		/*enable DPI */
		DPI_OUTREG32(cmdq, &DPI_REG[DPI_IDX(module)]->DPI_EN, 0x00000001);

		s_isDpiStart = true;
	}
	return 0;
}

int ddp_dpi_stop(DISP_MODULE_ENUM module, void *cmdq_handle)
{
	DDPMSG("ddp_dpi_stop\n");

	/*disable DPI and background, and reset DPI */
	DPI_OUTREG32(cmdq_handle, &DPI_REG[DPI_IDX(module)]->DPI_EN, 0x00000000);
	ddp_dpi_ConfigBG(module, cmdq_handle, false, 0, 0);
	ddp_dpi_reset(module, cmdq_handle);

	s_isDpiStart = false;
	s_isDpiConfig = false;
	dpi_vsync_irq_count[0] = 0;
	dpi_vsync_irq_count[1] = 0;
	dpi_undflow_irq_count[0] = 0;
	dpi_undflow_irq_count[1] = 0;

	return 0;
}

int ddp_dpi_is_busy(DISP_MODULE_ENUM module)
{
	unsigned int status = INREG32(&DPI_REG[DPI_IDX(module)]->STATUS);

	status = (status & (0x1 << 16) ? 1 : 0);

	return status;
}

int ddp_dpi_is_idle(DISP_MODULE_ENUM module)
{
	return !ddp_dpi_is_busy(module);
}

unsigned int ddp_dpi_get_cur_addr(bool rdma_mode, int layerid)
{
	if (rdma_mode)
		return INREG32(DISP_REG_RDMA_MEM_START_ADDR + DISP_INDEX_OFFSET * 2);

	if (INREG32(DISP_INDEX_OFFSET + DISP_REG_OVL_RDMA0_CTRL + layerid * 0x20) & 0x1)
		return INREG32(DISP_INDEX_OFFSET + DISP_REG_OVL_L0_ADDR + layerid * 0x20);

	return 0;
}


#if ENABLE_DPI_INTERRUPT
static irqreturn_t _DPI_InterruptHandler(DISP_MODULE_ENUM module, unsigned int param)
{
	DPI_REG_INTERRUPT status = DPI_REG[DPI_IDX(module)]->INT_STATUS;

	if (status.VSYNC) {
		dpi_vsync_irq_count[DPI_IDX(module)]++;
		if (dpi_vsync_irq_count[DPI_IDX(module)] > 30) {
			dpi_vsync_irq_count[DPI_IDX(module)] = 0;
		}
	}

	DPI_OUTREG32(0, &DPI_REG[DPI_IDX(module)]->INT_STATUS, 0);

	return IRQ_HANDLED;
}
#endif

int ddp_dpi_init(DISP_MODULE_ENUM module, void *cmdq)
{
	UINT32 i;

	DDPMSG("ddp_dpi_init- %p\n", cmdq);

#ifdef CONFIG_FPGA_EARLY_PORTING
#if 0
	/* DPI_OUTREG32(cmdq,  MMSYS_CONFIG_BASE+0x108, 0xffffffff); */
	/* DPI_OUTREG32(cmdq,  MMSYS_CONFIG_BASE+0x118, 0xffffffff); */
	/* DPI_OUTREG32(cmdq,  MMSYS_CONFIG_BASE+0xC08, 0xffffffff); */

	/* DPI_OUTREG32(cmdq,  LCD_BASE+0x001C, 0x00ffffff); */
	/* DPI_OUTREG32(cmdq,  LCD_BASE+0x0028, 0x010000C0); */
	/* DPI_OUTREG32(cmdq,  LCD_BASE+0x002C, 0x1); */
	/* DPI_OUTREG32(cmdq,  LCD_BASE+0x002C, 0x0); */

	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE + 0x14, 0x00000000);
	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE + 0x1C, 0x00000005);

	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE + 0x20, 0x0000001A);
	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE + 0x24, 0x001A001A);
	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE + 0x28, 0x0000000A);
	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE + 0x2C, 0x000A000A);
	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE + 0x08, 0x00000007);

	/* DPI_OUTREG32(cmdq, DISPSYS_DPI0_BASE+0x00, 0x00000000); */
	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE, 0x1);
	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE + 0xE0, 0x404);
#else
	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE, 0x1);
	DPI_OUTREG32(NULL, DISPSYS_DPI0_BASE + 0xE0, 0x404);
#endif
#endif

	DPI_REG[0] = (PDPI_REGS) (DISPSYS_DPI0_BASE);
	DPI_REG[1] = (PDPI_REGS) (DISPSYS_DPI1_BASE);

	APMIXEDSYS_VA = (unsigned long)ioremap(APMIXEDSYS, 0x1000);
	TOP_CLOCK_VA = (unsigned long)ioremap(TOP_CLOCK, 0x1000);
	DDPMSG("APMIXEDSYS=0x%lx => APMIXEDSYS_VA=0x%lx, TOP_CLOCK=0x%lx => TOP_CLOCK_VA=0x%lx\n",
	       APMIXEDSYS, APMIXEDSYS_VA, TOP_CLOCK, TOP_CLOCK_VA);

	ddp_dpi_power_on((module == DISP_MODULE_DPI0) ? DISP_MODULE_DPI0 : DISP_MODULE_DPI1, cmdq);
	/* ddp_dpi_power_on(DISP_MODULE_DPI1, cmdq); */


#if ENABLE_DPI_INTERRUPT

	disp_register_module_irq_callback((module == DISP_MODULE_DPI0) ?
					  DISP_MODULE_DPI0 : DISP_MODULE_DPI1,
					  (DDP_IRQ_CALLBACK) _DPI_InterruptHandler);

	if (module == DISP_MODULE_DPI0)
		DPI_OUTREGBIT(NULL, DPI_REG_INTERRUPT, DPI_REG[0]->INT_ENABLE, VSYNC, 1);
	else
		DPI_OUTREGBIT(NULL, DPI_REG_INTERRUPT, DPI_REG[1]->INT_ENABLE, VSYNC, 1);

#endif
	for (i = 0; i < DPI_INTERFACE_NUM; i++)
		DISPCHECK("dsi%d init finished\n", i);

	/* /_BackupDPIRegisters(); */

	return 0;
}

int ddp_dpi_deinit(DISP_MODULE_ENUM module, void *cmdq_handle)
{

	DDPMSG("ddp_dpi_deinit- %p\n", cmdq_handle);
	ddp_dpi_stop(module, cmdq_handle);
#if ENABLE_DPI_INTERRUPT

	if (module == DISP_MODULE_DPI0)
		DPI_OUTREGBIT(NULL, DPI_REG_INTERRUPT, DPI_REG[0]->INT_ENABLE, VSYNC, 0);
	else
		DPI_OUTREGBIT(NULL, DPI_REG_INTERRUPT, DPI_REG[1]->INT_ENABLE, VSYNC, 0);

	disp_unregister_module_irq_callback((module == DISP_MODULE_DPI0) ?
					    DISP_MODULE_DPI0 : DISP_MODULE_DPI1,
					    (DDP_IRQ_CALLBACK) _DPI_InterruptHandler);
#endif
	ddp_dpi_power_off(module, cmdq_handle);

	return 0;
}

int ddp_dpi_set_lcm_utils(DISP_MODULE_ENUM module, LCM_DRIVER *lcm_drv)
{
	LCM_UTIL_FUNCS *utils = NULL;

	DISPFUNC();

	if (lcm_drv == NULL) {
		DDPERR("lcm_drv is null!\n");
		return -1;
	}

	utils = &lcm_utils_dpi;

	utils->set_reset_pin = lcm_set_reset_pin;
	utils->udelay = lcm_udelay;
	utils->mdelay = lcm_mdelay;
	utils->send_cmd = lcm_send_cmd,
	    utils->send_data = lcm_send_data, lcm_drv->set_util_funcs(utils);

	return 0;
}

int ddp_dpi_build_cmdq(DISP_MODULE_ENUM module, void *cmdq_trigger_handle, CMDQ_STATE state)
{
	return 0;
}

int ddp_dpi_dump(DISP_MODULE_ENUM module, int level)
{
	UINT32 i;

	DDPDUMP("---------- Start dump DPI registers ----------\n");

	for (i = 0; i <= 0x40; i += 4)
		DDPDUMP("DPI+%04x : 0x%08x\n", i, INREG32(DISPSYS_DPI0_BASE + i));

	for (i = 0x68; i <= 0x7C; i += 4)
		DDPDUMP("DPI+%04x : 0x%08x\n", i, INREG32(DISPSYS_DPI0_BASE + i));

	DDPDUMP("DPI+Color Bar : %04x : 0x%08x\n", 0xF00, INREG32(DISPSYS_DPI0_BASE + 0xF00));
	DDPDUMP("DPI Addr IO Driving : 0x%08x\n", INREG32(DISPSYS_IO_DRIVING));
#if 0				/* ndef CONFIG_FPGA_EARLY_PORTING       //FOR BRING_UP */
	DDPDUMP("DPI TVDPLL CON0 : 0x%08x\n", INREG32(DDP_REG_TVDPLL_CON0));
	DDPDUMP("DPI TVDPLL CON1 : 0x%08x\n", INREG32(DDP_REG_TVDPLL_CON1));
#endif
	DDPDUMP("DPI TVDPLL CON6 : 0x%08x\n", INREG32(DDP_REG_TVDPLL_CON6));
	DDPDUMP("DPI MMSYS_CG_CON1:0x%08x\n", INREG32(DISP_REG_CONFIG_MMSYS_CG_CON1));
	return 0;
}

int ddp_dpi_ioctl(DISP_MODULE_ENUM module, void *cmdq_handle, unsigned int ioctl_cmd,
		  unsigned long *params)
{
	int ret = 0;
	enum DDP_IOCTL_NAME ioctl = (enum DDP_IOCTL_NAME)ioctl_cmd;

	DISPFUNC();
	DDPMSG("DPI ioctl: %d\n", ioctl);

	switch (ioctl) {
	case DDP_DPI_FACTORY_TEST:
		{
			disp_ddp_path_config *config_info = (disp_ddp_path_config *) params;

			ddp_dpi_power_on(module, NULL);
			ddp_dpi_stop(module, NULL);
			ddp_dpi_config(module, config_info, NULL);
			ddp_dpi_EnableColorBar(module);

			ddp_dpi_trigger(module, NULL);
			ddp_dpi_start(module, NULL);
			ddp_dpi_dump(module, 1);
			break;
		}
	default:
		break;
	}

	return ret;
}

DDP_MODULE_DRIVER ddp_driver_dpi0 = {
	.module = DISP_MODULE_DPI0,
#if HDMI_MAIN_PATH
#else
	.init = ddp_dpi_init,
	.deinit = ddp_dpi_deinit,
	.config = ddp_dpi_config,
	.build_cmdq = ddp_dpi_build_cmdq,
	.trigger = ddp_dpi_trigger,
	.start = ddp_dpi_start,
	.stop = ddp_dpi_stop,
	.reset = ddp_dpi_reset,
	.power_on = ddp_dpi_power_on,
	.power_off = ddp_dpi_power_off,
	.is_idle = ddp_dpi_is_idle,
	.is_busy = ddp_dpi_is_busy,
	.dump_info = ddp_dpi_dump,
	.set_lcm_utils = ddp_dpi_set_lcm_utils,
	.ioctl = ddp_dpi_ioctl
#endif
};

DDP_MODULE_DRIVER ddp_driver_dpi1 = {
	.module = DISP_MODULE_DPI1,
#if HDMI_MAIN_PATH

#else
	.init = ddp_dpi_init,
	.deinit = ddp_dpi_deinit,
	.config = ddp_dpi_config,
	.build_cmdq = ddp_dpi_build_cmdq,
	.trigger = ddp_dpi_trigger,
	.start = ddp_dpi_start,
	.stop = ddp_dpi_stop,
	.reset = ddp_dpi_reset,
	.power_on = ddp_dpi_power_on,
	.power_off = ddp_dpi_power_off,
	.is_idle = ddp_dpi_is_idle,
	.is_busy = ddp_dpi_is_busy,
	.dump_info = ddp_dpi_dump,
	.set_lcm_utils = ddp_dpi_set_lcm_utils,
	.ioctl = ddp_dpi_ioctl
#endif
};

PLVDS_REGS LVDS_REG;		/*=(PLVDS_REGS)(DISPSYS_DPI1_BASE + 0x2000);*/
PLVDS_TX1_REGS LVDS_TX1_REG;	/*=(PLVDS_TX1_REGS)(MIPITX0_BASE + 0x800);*/
PLVDS_TX2_REGS LVDS_TX2_REG;	/*=(PLVDS_TX2_REGS)(MIPITX1_BASE + 0x800);*/

void LVDS_Clk_Path_Config(void)
{
	/* dpi1 pixel/engine clock setting */
	MASKREG32(DDP_REG_BASE_MMSYS_CONFIG + 0x118, (0x01 << 10), (0x01 << 10));	/* bit10 */
	MASKREG32(DDP_REG_BASE_MMSYS_CONFIG + 0x118, (0x01 << 11), (0x01 << 11));	/* bit11 */
	MASKREG32(DDP_REG_BASE_MMSYS_CONFIG + 0x118, (0x01 << 17), (0x01 << 17));	/* bit17 */
	MASKREG32(DDP_REG_BASE_MMSYS_CONFIG + 0x118, (0x01 << 16), (0x01 << 16));	/* bit16 */
}

void LVDS_ANA_Init(DISP_MODULE_ENUM module, bool is_lvds_dual_tx, cmdqRecHandle cmdq)
{
#if 1
	/* OUTREG32(0x10216800 + 0x1c, 0x00000200); */
	/* OUTREG32(0x10215800 + 0x1c, 0x00000200); */

	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL3, LVDS_TX2_REG->VOPLL_CTL3, LVDS_ISO_EN, 0x00);
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL3, LVDS_TX2_REG->VOPLL_CTL3, DA_LVDSTX_PWR_ON, 0x01);

	DPI_OUTREGBIT(cmdq, LVDS_TX1_VOPLL_CTL3, LVDS_TX1_REG->VOPLL_CTL3, LVDS_ISO_EN, 0x00);
	DPI_OUTREGBIT(cmdq, LVDS_TX1_VOPLL_CTL3, LVDS_TX1_REG->VOPLL_CTL3, DA_LVDSTX_PWR_ON, 0x01);

	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL1, LVDS_TX2_REG->VOPLL_CTL1, RG_VPLL_FBKDIV, 0x1c);
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL1, LVDS_TX2_REG->VOPLL_CTL1, RG_VPLL_FBKSEL, 0x01);
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL1, LVDS_TX2_REG->VOPLL_CTL1, RG_VPLL_TXMUXDIV2_EN, 0x01);

	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_TXDIV1, 0x01);
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_TXDIV2,
		      (is_lvds_dual_tx ? 0x00 : 0x01));
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_LVDS_DPIX_DIV2,
		      0x01);
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_BIAS_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_LVDS_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_EN, 0x01);
	udelay(30);

	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL2, LVDS_TX2_REG->CTL2, RG_LVDSTX2_TVCM, 0x0b);
	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL2, LVDS_TX2_REG->CTL2, RG_LVDSTX2_TSTCLK_SEL, 0x03);
	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL2, LVDS_TX2_REG->CTL2, RG_LVDSTX2_TSTCLKDIV_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL2, LVDS_TX2_REG->CTL2, RG_LVDSTX2_TSTCLK_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL2, LVDS_TX2_REG->CTL2, RG_LVDSTX2_BIAS_SEL, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL2, LVDS_TX2_REG->CTL2, RG_LVDSTX2_LDO_EN, 0x01);

	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL2, LVDS_TX1_REG->CTL2, RG_LVDSTX1_TVCM, 0x0b);
	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL2, LVDS_TX1_REG->CTL2, RG_LVDSTX1_TSTCLK_SEL, 0x03);
	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL2, LVDS_TX1_REG->CTL2, RG_LVDSTX1_TSTCLKDIV_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL2, LVDS_TX1_REG->CTL2, RG_LVDSTX1_TSTCLK_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL2, LVDS_TX1_REG->CTL2, RG_LVDSTX1_BIAS_SEL, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL2, LVDS_TX1_REG->CTL2, RG_LVDSTX1_LDO_EN, 0x01);

	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_TXDIV5_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_TTLDIV,
		      (is_lvds_dual_tx ? 0x01 : 0x00));
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_TXDIV2,
		      (is_lvds_dual_tx ? 0x00 : 0x01));

	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL3, LVDS_TX2_REG->CTL3, RG_LVDSTX2_VOUTABIST_EN, 0x00);
	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL3, LVDS_TX2_REG->CTL3, RG_LVDSTX2_EXT_EN, 0x1f);

	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL3, LVDS_TX1_REG->CTL3, RG_LVDSTX1_VOUTABIST_EN, 0x00);
	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL3, LVDS_TX1_REG->CTL3, RG_LVDSTX1_EXT_EN, 0x1f);

	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL2, LVDS_TX1_REG->CTL2, RG_LVDSTX1_TVO, 0x07);
	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL2, LVDS_TX1_REG->CTL2, RG_LVDSTX1_LDO_EN, 0x01);

	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL2, LVDS_TX2_REG->CTL2, RG_LVDSTX2_TVO, 0x07);
	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL2, LVDS_TX2_REG->CTL2, RG_LVDSTX2_LDO_EN, 0x01);

	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL3, LVDS_TX2_REG->CTL3, RG_LVDSTX2_DRV_EN, 0x1f);
	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL3, LVDS_TX1_REG->CTL3, RG_LVDSTX1_DRV_EN, 0x1f);

	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL2, LVDS_TX1_REG->CTL2, RG_LVDSTX1_BIAS_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL2, LVDS_TX2_REG->CTL2, RG_LVDSTX2_BIAS_EN, 0x01);

	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_BIAS_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDS_VOPLL_CTL2, LVDS_TX2_REG->VOPLL_CTL2, RG_VPLL_BIASLPF_EN, 0x01);

	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL4, LVDS_TX2_REG->CTL4, RG_LVDSTX2_LDOLPF_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTX2_CTL4, LVDS_TX2_REG->CTL4, RG_LVDSTX2_BIASLPF_EN, 0x01);

	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL4, LVDS_TX1_REG->CTL4, RG_LVDSTX1_LDOLPF_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTX1_CTL4, LVDS_TX1_REG->CTL4, RG_LVDSTX1_BIASLPF_EN, 0x01);
#else
	OUTREG32(0x10216800 + 0x1c, 0x00000200);
	OUTREG32(0x10215800 + 0x1c, 0x00000200);
	OUTREG32(0x10216800 + 0x14, 0x0011c041);
	OUTREG32(0x10216800 + 0x18, 0x01003580);
	CTP_Wait_usec(30);

	OUTREG32(0x10216800 + 0x04, 0x00410fb0);
	OUTREG32(0x10215800 + 0x04, 0x00410fb0);

	OUTREG32(0x10216800 + 0x18, 0x01213180);

	OUTREG32(0x10216800 + 0x08, 0x000003e0);
	OUTREG32(0x10215800 + 0x08, 0x000003e0);

	OUTREG32(0x10215800 + 0x04, 0x00c10fb7);
	OUTREG32(0x10216800 + 0x04, 0x00c10fb7);

	OUTREG32(0x10216800 + 0x08, 0x00007fe0);
	OUTREG32(0x10215800 + 0x08, 0x00007fe0);

	OUTREG32(0x10216800 + 0x18, 0x03213180);
	OUTREG32(0x10216800 + 0x0c, 0x02800000);
	OUTREG32(0x10215800 + 0x0c, 0x02800000);
#endif
}

void LVDS_DIG_Init(DISP_MODULE_ENUM module, bool is_lvds_dual_tx, cmdqRecHandle cmdq)
{
	DPI_OUTREG32(cmdq, &LVDS_REG->TOP_REG02, 0xffffffff);

	/* clock control */
	DPI_OUTREGBIT(cmdq, LVDS_TOP_REG05, LVDS_REG->TOP_REG05, RG_FIFO_EN,
		      (is_lvds_dual_tx ? 0x03 : 0x01));
	DPI_OUTREGBIT(cmdq, LVDS_TOP_REG05, LVDS_REG->TOP_REG05, REG_FIFO_CTRL, 0x03);
	DPI_OUTREGBIT(cmdq, LVDS_TOP_REG05, LVDS_REG->TOP_REG05, LVDS_CLKDIV_CTRL,
		      (is_lvds_dual_tx ? 0x01 : 0x00));

	/* VESA model */
	DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL00, LVDS_REG->LVDS_CTRL00, RG_NS_VESA_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL00, LVDS_REG->LVDS_CTRL00, RG_DUAL,
		      (is_lvds_dual_tx ? 0x01 : 0x00));

	/* DSIM mode */
	/* DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL00, LVDS_REG->LVDS_CTRL00, RG_NS_VESA_EN, 0x00); */
	/* DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL00, LVDS_REG->LVDS_CTRL00, RG_DUAL, (is_lvds_dual_tx ? 0x01 : 0x00)); */

	DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL02, LVDS_REG->LVDS_CTRL02, RG_A_SW, 0x00);
	DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL02, LVDS_REG->LVDS_CTRL02, RG_B_SW, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL02, LVDS_REG->LVDS_CTRL02, RG_C_SW, 0x02);
	DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL02, LVDS_REG->LVDS_CTRL02, RG_D_SW, 0x03);
	DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL02, LVDS_REG->LVDS_CTRL02, RG_LPF_EN, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL02, LVDS_REG->LVDS_CTRL02, RG_C_LINE_EXT, 0x01);
	DPI_OUTREGBIT(cmdq, LVDSTXO_LVDS_CTRL02, LVDS_REG->LVDS_CTRL02, RG_LVDS_74FIFO_EN, 0x01);
#if 0
	/* pattern enable for 800 x 1280 */
	DPI_OUTREGBIT(cmdq, PANELB_PATGEN5, LVDS_REG->PATGEN5, RG_PTGEN_EN, 0x01);
	DPI_OUTREGBIT(cmdq, PANELB_PATGEN5, LVDS_REG->PATGEN5, RG_INTF_PTGEN_EN, 0x01);

	DPI_OUTREGBIT(cmdq, PANELB_PATGEN5, LVDS_REG->PATGEN5, RG_PTGEN_TYPE, 0x02);
	/* pattern width */
	DPI_OUTREGBIT(cmdq, PANELB_PATGEN0, LVDS_REG->PATGEN0, RG_PTGEN_H_TOTAL, 0x360);
	DPI_OUTREGBIT(cmdq, PANELB_PATGEN0, LVDS_REG->PATGEN0, RG_PTGEN_H_ACTIVE, 0x320);
	/* pattern height */
	DPI_OUTREGBIT(cmdq, PANELB_PATGEN1, LVDS_REG->PATGEN1, RG_PTGEN_V_TOTAL, 0x508);
	DPI_OUTREGBIT(cmdq, PANELB_PATGEN1, LVDS_REG->PATGEN1, RG_PTGEN_V_ACTIVE, 0x500);
	/* pattern start */
	DPI_OUTREGBIT(cmdq, PANELB_PATGEN2, LVDS_REG->PATGEN2, RG_PTGEN_V_START, 0x08);
	DPI_OUTREGBIT(cmdq, PANELB_PATGEN2, LVDS_REG->PATGEN2, RG_PTGEN_H_START, 0x20);
	/* pattern sync width */
	DPI_OUTREGBIT(cmdq, PANELB_PATGEN3, LVDS_REG->PATGEN3, RG_PTGEN_V_WIDTH, 0x01);
	DPI_OUTREGBIT(cmdq, PANELB_PATGEN3, LVDS_REG->PATGEN3, RG_PTGEN_H_WIDTH, 0x01);
#endif
}

void LVDS_PLL_Init(DISP_MODULE_ENUM module, unsigned int pixel_clock, cmdqRecHandle cmdq)
{
	unsigned int ck_div = 0, postdiv = 0;
	unsigned int pcw = 0, n_info = 0;

	if (pixel_clock > 250) {
		ASSERT(0);
	} else if (pixel_clock >= 125) {
		ck_div = 1;
		postdiv = 8;
	} else if (pixel_clock >= 63) {
		ck_div = 2;
		postdiv = 8;
	} else if (pixel_clock >= 32) {
		ck_div = 4;
		postdiv = 8;
	} else if (pixel_clock > 16) {
		ck_div = 8;
		postdiv = 8;
	} else if (pixel_clock >= 8) {
		ck_div = 1;
		postdiv = 16;
	} else {
		ASSERT(0);
	}

	n_info = pixel_clock * ck_div * postdiv / 26;
	pcw = ((n_info * (1 << 14)) | (1 << 31));

	switch (postdiv) {
	case 0:
	case 8:
		postdiv = 3;
		break;

	case 16:
		postdiv = 4;
		break;
	}

	switch (ck_div) {
	case 1:
		ck_div = 1;
		break;

	case 2:
		ck_div = 2;
		break;

	case 4:
		ck_div = 3;
		break;

	default:
	case 8:
		ck_div = 4;
		break;
	}

	DDPMSG("postdiv = 0x%x, pcw = 0x%x, ck_div = 0x%x\n", postdiv, pcw, ck_div);

	DPI_OUTREG32(cmdq, APMIXEDSYS_VA + 0x2d0, (1 << 8) | (postdiv << 4) | (1 << 0));	/* LVDSPLL_CON0 */
	DPI_OUTREG32(cmdq, APMIXEDSYS_VA + 0x2d4, pcw);	/* LVDSPLL_CON1 */
}

void ddp_dpi_lvds_config(DISP_MODULE_ENUM module, LCM_DPI_PARAMS *dpi_config, void *cmdq_handle)
{
	LVDS_REG = (PLVDS_REGS) (DISPSYS_DPI1_BASE + 0x2000);
	LVDS_TX1_REG = (PLVDS_TX1_REGS) (MIPITX0_BASE + 0x800);
	LVDS_TX2_REG = (PLVDS_TX2_REGS) (MIPITX1_BASE + 0x800);

	/*LVDS_ANA_Init(module, dpi_config->is_lvds_dual_tx, cmdq_handle);*/ /*kernel 3.18*/
	LVDS_Clk_Path_Config();
	LVDS_PLL_Init(module, dpi_config->PLL_CLOCK, cmdq_handle);
	/*LVDS_DIG_Init(module, dpi_config->is_lvds_dual_tx, cmdq_handle);*/ /*kernel 3.18*/
}

bool DPI0_IS_TOP_FIELD(void)
{
	if (DPI_REG[0]->STATUS.FIELD == 0)
		return true;
	else
		return false;

}
