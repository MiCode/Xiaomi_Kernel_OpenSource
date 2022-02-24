/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

/*#include "mach/mt_typedefs.h"*/
#include <linux/types.h>

/*#include <mach/sync_write.h>*/
#include <mt-plat/sync_write.h>

#include "ddp_clkmgr.h"
/* #include <mach/irqs.h> */

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
#include "ddp_path.h"


#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
/*#include <mach/eint.h>*/

#ifndef LOG_TAG
#define LOG_TAG "DPI"
#endif

static void __iomem	*clk_apmixed_base;
#ifndef TVDPLL_CON0
#define TVDPLL_CON0             (clk_apmixed_base + 0x260)
#endif
#ifndef TVDPLL_CON1
#define TVDPLL_CON1             (clk_apmixed_base + 0x264)
#endif

#define ENABLE_DPI_INTERRUPT        0

#define K2_SMT

#undef LCD_BASE
#define LCD_BASE (0xF4024000)
#define DPI_REG_OFFSET(r)       offsetof(struct DPI_REGS, r)
#define REG_ADDR(base, offset)  (((BYTE *)(base)) + (offset))

#ifdef INREG32
#undef INREG32
#define INREG32(x) (__raw_readl((unsigned long *)(x)))
#endif

#define DPI_OUTREG32(cmdq, addr, val) \
		DISP_REG_SET(cmdq, addr, val)
#define DPI_OUTREGBIT(cmdq, TYPE, REG, bit, value)  \
	do {\
		TYPE r;\
		TYPE v;\
		if (cmdq) { \
			*(unsigned int *)(&r) = (0x00000000); \
			r.bit = ~(r.bit); \
			*(unsigned int *)(&v) = (0x00000000); \
			v.bit = value; \
			DISP_REG_MASK(cmdq, \
				&REG, AS_UINT32(&v), AS_UINT32(&r)); \
		} else { \
			mt_reg_sync_writel(INREG32(&REG), &r); \
			r.bit = (value); \
			DISP_REG_SET(cmdq, &REG, INREG32(&r)); \
		} \
	} while (0)

#define DPI_MASKREG32(cmdq, REG, MASK, VALUE) \
		DISP_REG_MASK((cmdq), (REG), (VALUE), (MASK))

static int cache_bg_parameter;
static unsigned char s_isDpiPowerOn;
static unsigned char s_isDpiStart;
static unsigned char s_isDpiConfig;

static int dpi_vsync_irq_count;
static int dpi_undflow_irq_count;

/*static DPI_REGS regBackup;*/
struct DPI_REGS *DPI_REG;
static const struct LCM_UTIL_FUNCS lcm_utils_dpi;

const unsigned int BACKUP_DPI_REG_OFFSETS[] = {
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
static void lcm_udelay(unsigned int us)
{
	udelay(us);
}

static void lcm_mdelay(unsigned int ms)
{
	msleep(ms);
}

static void lcm_set_reset_pin(unsigned int value)
{
#ifndef K2_SMT
	DPI_OUTREG32(0, MMSYS_CONFIG_BASE + 0x150, value);
#endif
}

static void lcm_send_cmd(unsigned int cmd)
{
#ifndef K2_SMT
	DPI_OUTREG32(0, LCD_BASE + 0x0F80, cmd);
#endif
}

static void lcm_send_data(unsigned int data)
{
#ifndef K2_SMT
	DPI_OUTREG32(0, LCD_BASE + 0x0F90, data);
#endif
}

#if 0
static void _BackupDPIRegisters(void)
{
	UINT32 i;
	DPI_REGS *reg = &regBackup;

	for (i = 0; i < ARY_SIZE(BACKUP_DPI_REG_OFFSETS); ++i) {
		DPI_OUTREG32(0, REG_ADDR(reg, BACKUP_DPI_REG_OFFSETS[i]),
		AS_UINT32(REG_ADDR(DPI_REG, BACKUP_DPI_REG_OFFSETS[i])));
	}
}

static void _RestoreDPIRegisters(void)
{
	UINT32 i;
	DPI_REGS *reg = &regBackup;

	for (i = 0; i < ARY_SIZE(BACKUP_DPI_REG_OFFSETS); ++i) {
		DPI_OUTREG32(0, REG_ADDR(DPI_REG, BACKUP_DPI_REG_OFFSETS[i]),
		AS_UINT32(REG_ADDR(reg, BACKUP_DPI_REG_OFFSETS[i])));
	}
}
#endif

/*the functions declare*/
/*DPI clock setting - use TVDPLL provide DPI clock*/
enum DPI_STATUS ddp_dpi_ConfigPclk(struct cmdqRecStruct *cmdq,
			unsigned int clk_req, enum LCM_POLARITY polarity)
{
	unsigned int clksrc = 0;
	unsigned int con1 = 0;
	struct DPI_REG_OUTPUT_SETTING ctrl = DPI_REG->OUTPUT_SETTING;
	struct device_node *node;

	switch (clk_req) {
	case HDMI_VIDEO_720x480p_60Hz: /* pix clk: 27.027M, dpi clk: 54.054M */

		{
			clksrc = TVDPLL_D8;
			con1 = 0x83214395;
			break;
		}
	case HDMI_VIDEO_1280x720p_60Hz:	/* pix clk: 74.25M, dpi clk: 148.5M */
		{
			clksrc = TVDPLL_D2;
			con1 = 0x8316D89D;
			break;
		}
	case HDMI_VIDEO_1920x1080p_30Hz: /* pix clk: 74.25M, dpi clk: 148.5M */
		{
			clksrc = TVDPLL_D2;
			con1 = 0x8316D89D;
			break;
		}
	case HDMI_VIDEO_1920x1080p_60Hz: /* pix clk: 148.5M, dpi clk: 297M */
		{
			clksrc = TVDPLL_D2;
			con1 = 0x8216D89D;
			break;
		}
#if 0
	case HDMI_VIDEO_2160p_DSC_24Hz:		/*178.2M*/
		{
			clksrc = TVDPLL_D8;
			con1 = 0x901B6A56;
			break;
		}
	case HDMI_VIDEO_2160p_DSC_30Hz:		/*199M*/
		{
			clksrc = TVDPLL_D8;
			con1 = 0x901E9D89;
			break;
		}
#endif
	}

	DISPINFO("DISP/DPI,TVDPLL clock setting clk %d, clksrc: %d\n",
			clk_req, clksrc);

	ddp_clk_prepare_enable(MUX_DPI0);
	ddp_clk_set_parent(MUX_DPI0, clksrc);

	/* apmixed */
	node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
	if (!node)
		DISPERR("[CLK_APMIXED] find node failed\n");

	clk_apmixed_base = of_iomap(node, 0);
	if (!clk_apmixed_base)
		DISPERR("[CLK_APMIXED] base failed\n");
	else {
		/*enable TVDPLL */
		DPI_OUTREG32(NULL, TVDPLL_CON0, (INREG32(TVDPLL_CON0) | 0x01));
		/* set TVDPLL output clock frequency */
		DPI_OUTREG32(NULL, TVDPLL_CON1, con1);
	}
	DISPINFO("DISP/DPI,TVDPLL_CON0 0x%x, TVDPLL_CON1: 0x%x\n",
			INREG32(TVDPLL_CON0), INREG32(TVDPLL_CON1));

	/*DPI output clock polarity */
	ctrl.CLK_POL = (polarity == LCM_POLARITY_FALLING) ? 1 : 0;
	DPI_OUTREGBIT(cmdq, struct DPI_REG_OUTPUT_SETTING,
			DPI_REG->OUTPUT_SETTING, CLK_POL, ctrl.CLK_POL);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_ConfigDE(struct cmdqRecStruct *cmdq,
			enum LCM_POLARITY polarity)
{
	struct DPI_REG_OUTPUT_SETTING pol = DPI_REG->OUTPUT_SETTING;

	pol.DE_POL = (polarity == LCM_POLARITY_FALLING) ? 1 : 0;
	DPI_OUTREGBIT(cmdq, struct DPI_REG_OUTPUT_SETTING,
			DPI_REG->OUTPUT_SETTING, DE_POL, pol.DE_POL);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_ConfigVsync(struct cmdqRecStruct *cmdq,
			enum LCM_POLARITY polarity, unsigned int pulseWidth,
			unsigned int backPorch, unsigned int frontPorch)
{
	struct DPI_REG_TGEN_VWIDTH_LODD vwidth_lodd = DPI_REG->TGEN_VWIDTH_LODD;
	struct DPI_REG_TGEN_VPORCH_LODD vporch_lodd = DPI_REG->TGEN_VPORCH_LODD;
	struct DPI_REG_OUTPUT_SETTING pol = DPI_REG->OUTPUT_SETTING;
	struct DPI_REG_CNTL VS = DPI_REG->CNTL;

	pol.VSYNC_POL = (polarity == LCM_POLARITY_FALLING) ? 1 : 0;
	vwidth_lodd.VPW_LODD = pulseWidth;
	vporch_lodd.VBP_LODD = backPorch;
	vporch_lodd.VFP_LODD = frontPorch;

	VS.VS_LODD_EN = 1;
	VS.VS_LEVEN_EN = 0;
	VS.VS_RODD_EN = 0;
	VS.VS_REVEN_EN = 0;

	DPI_OUTREGBIT(cmdq, struct DPI_REG_OUTPUT_SETTING,
		DPI_REG->OUTPUT_SETTING, VSYNC_POL, pol.VSYNC_POL);

	DPI_OUTREGBIT(cmdq, struct DPI_REG_TGEN_VWIDTH_LODD,
		DPI_REG->TGEN_VWIDTH_LODD, VPW_LODD, vwidth_lodd.VPW_LODD);


	DPI_OUTREGBIT(cmdq, struct DPI_REG_TGEN_VPORCH_LODD,
		DPI_REG->TGEN_VPORCH_LODD, VBP_LODD, vporch_lodd.VBP_LODD);
	DPI_OUTREGBIT(cmdq, struct DPI_REG_TGEN_VPORCH_LODD,
		DPI_REG->TGEN_VPORCH_LODD, VFP_LODD, vporch_lodd.VFP_LODD);

	DPI_OUTREGBIT(cmdq, struct DPI_REG_CNTL, DPI_REG->CNTL,
		VS_LODD_EN, VS.VS_LODD_EN);
	DPI_OUTREGBIT(cmdq, struct DPI_REG_CNTL, DPI_REG->CNTL,
		VS_LEVEN_EN, VS.VS_LEVEN_EN);
	DPI_OUTREGBIT(cmdq, struct DPI_REG_CNTL, DPI_REG->CNTL,
		VS_RODD_EN, VS.VS_RODD_EN);
	DPI_OUTREGBIT(cmdq, struct DPI_REG_CNTL, DPI_REG->CNTL,
		VS_REVEN_EN, VS.VS_REVEN_EN);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_ConfigHsync(struct cmdqRecStruct *cmdq,
			enum LCM_POLARITY polarity, unsigned int pulseWidth,
			unsigned int backPorch, unsigned int frontPorch)
{
	struct DPI_REG_TGEN_HPORCH hporch = DPI_REG->TGEN_HPORCH;
	struct DPI_REG_OUTPUT_SETTING pol = DPI_REG->OUTPUT_SETTING;

	hporch.HBP = backPorch;
	hporch.HFP = frontPorch;
	pol.HSYNC_POL = (polarity == LCM_POLARITY_FALLING) ? 1 : 0;
	DPI_REG->TGEN_HWIDTH = pulseWidth;

	DPI_OUTREG32(cmdq, &DPI_REG->TGEN_HWIDTH, pulseWidth);

	DPI_OUTREGBIT(cmdq, struct DPI_REG_TGEN_HPORCH,
			DPI_REG->TGEN_HPORCH, HBP, hporch.HBP);
	DPI_OUTREGBIT(cmdq, struct DPI_REG_TGEN_HPORCH,
			DPI_REG->TGEN_HPORCH, HFP, hporch.HFP);

	DPI_OUTREGBIT(cmdq, struct DPI_REG_OUTPUT_SETTING,
			DPI_REG->OUTPUT_SETTING, HSYNC_POL, pol.HSYNC_POL);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_ConfigDualEdge(struct cmdqRecStruct *cmdq,
		bool enable, unsigned int mode)
{
	struct DPI_REG_OUTPUT_SETTING ctrl = DPI_REG->OUTPUT_SETTING;
	struct DPI_REG_DDR_SETTING ddr_setting = DPI_REG->DDR_SETTING;

	ctrl.DUAL_EDGE_SEL = enable;
	DPI_OUTREGBIT(cmdq, struct DPI_REG_OUTPUT_SETTING,
			DPI_REG->OUTPUT_SETTING, DUAL_EDGE_SEL,
			ctrl.DUAL_EDGE_SEL);

	ddr_setting.DDR_4PHASE = 1;
	ddr_setting.DDR_EN = 1;
	DPI_OUTREGBIT(cmdq, struct DPI_REG_DDR_SETTING,
			DPI_REG->DDR_SETTING, DDR_4PHASE,
			ddr_setting.DDR_4PHASE);
	DPI_OUTREGBIT(cmdq, struct DPI_REG_DDR_SETTING,
			DPI_REG->DDR_SETTING, DDR_EN, ddr_setting.DDR_EN);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_ConfigBG(struct cmdqRecStruct *cmdq, bool enable,
		int BG_W, int BG_H)
{
	if (enable == false) {
		struct DPI_REG_CNTL pol = DPI_REG->CNTL;

		pol.BG_EN = 0;
		DPI_OUTREGBIT(cmdq, struct DPI_REG_CNTL, DPI_REG->CNTL,
				BG_EN, pol.BG_EN);

	} else {
		struct DPI_REG_CNTL pol = DPI_REG->CNTL;
		struct DPI_REG_BG_HCNTL pol2 = DPI_REG->BG_HCNTL;
		struct DPI_REG_BG_VCNTL pol3 = DPI_REG->BG_VCNTL;
		struct DPI_REG_BG_COLOR pol4 = DPI_REG->BG_COLOR;

		pol.BG_EN = 1;
		DPI_OUTREGBIT(cmdq, struct DPI_REG_CNTL, DPI_REG->CNTL,
				BG_EN, pol.BG_EN);

		pol2.BG_RIGHT = BG_W / 2;
		pol2.BG_LEFT = BG_W - pol2.BG_RIGHT;
		DPI_OUTREGBIT(cmdq, struct DPI_REG_BG_HCNTL, DPI_REG->BG_HCNTL,
				BG_RIGHT, pol2.BG_RIGHT);
		DPI_OUTREGBIT(cmdq, struct DPI_REG_BG_HCNTL, DPI_REG->BG_HCNTL,
				BG_LEFT, pol2.BG_LEFT);

		pol3.BG_BOT = BG_H / 2;
		pol3.BG_TOP = BG_H - pol3.BG_BOT;
		DPI_OUTREGBIT(cmdq, struct DPI_REG_BG_VCNTL, DPI_REG->BG_VCNTL,
				BG_BOT, pol3.BG_BOT);
		DPI_OUTREGBIT(cmdq, struct DPI_REG_BG_VCNTL, DPI_REG->BG_VCNTL,
				BG_TOP, pol3.BG_TOP);

		pol4.BG_B = 0;
		pol4.BG_G = 0;
		pol4.BG_R = 0;
		DPI_OUTREGBIT(cmdq, struct DPI_REG_BG_COLOR, DPI_REG->BG_COLOR,
				BG_B, pol4.BG_B);
		DPI_OUTREGBIT(cmdq, struct DPI_REG_BG_COLOR, DPI_REG->BG_COLOR,
				BG_G, pol4.BG_G);
		DPI_OUTREGBIT(cmdq, struct DPI_REG_BG_COLOR, DPI_REG->BG_COLOR,
				BG_R, pol4.BG_R);
	}

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_ConfigSize(struct cmdqRecStruct *cmdq,
		unsigned int width, unsigned int height)
{
	struct DPI_REG_SIZE size = DPI_REG->SIZE;

	size.WIDTH = width;
	size.HEIGHT = height;
	DPI_OUTREGBIT(cmdq, struct DPI_REG_SIZE, DPI_REG->SIZE, WIDTH,
			size.WIDTH);
	DPI_OUTREGBIT(cmdq, struct DPI_REG_SIZE, DPI_REG->SIZE, HEIGHT,
			size.HEIGHT);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_EnableColorBar(void)
{
	/*enable internal pattern - color bar */
	DPI_OUTREG32(0, DISPSYS_DPI_BASE + 0xF00, 0x41);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_DisableColorBar(void)
{
	/*enable internal pattern - color bar */
	DPI_OUTREG32(0, DISPSYS_DPI_BASE + 0xF00, 0x0);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_EnableMux(void)
{
	int value = 0;

	/*DPI0_SEL_SOUT*/
	value = 0;
	DPI_OUTREG32(0, DISP_REG_CONFIG_DPI0_SEL_SOUT_SEL_IN, value);

	/*DBPI_SEL*/
	value = 0x1 << 1;
	DPI_OUTREG32(0, DISP_REG_CONFIG_DBPI_SEL, value);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_DisableMux(void)
{
	int value = 0;

	/*DPI0_SEL_SOUT*/
	DPI_OUTREG32(0, DISP_REG_CONFIG_DPI0_SEL_SOUT_SEL_IN, value);

	/*DBPI_SEL*/
	DPI_OUTREG32(0, DISP_REG_CONFIG_DBPI_SEL, value);

	return DPI_STATUS_OK;
}

int ddp_dpi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	int ret = 0;
	struct device_node *node;

	DISPINFO("DISP/DPI,%s, s_isDpiPowerOn %d\n",
			__func__, s_isDpiPowerOn);
	if (!s_isDpiPowerOn) {
		ddp_path_top_clock_on();

		ret += ddp_clk_prepare_enable(DISP1_DPI_MM_CLOCK);

		if (ret > 0)
			DISPERR("DPI power manager API return FALSE 1\n");

		ret += ddp_clk_prepare_enable(DISP1_DPI_INTERFACE_CLOCK);
		if (ret > 0)
			DISPERR("DPI power manager API return FALSE\n");

		s_isDpiPowerOn = TRUE;

		ddp_clk_check();

		ddp_clk_prepare_enable(MUX_DPI0);

		/* apmixed */
		node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
		if (!node)
			DISPERR("[CLK_APMIXED] find node failed\n");

		clk_apmixed_base = of_iomap(node, 0);
		if (!clk_apmixed_base)
			DISPERR("[CLK_APMIXED] base failed\n");
		else /* enable TVDPLL */
			DPI_OUTREG32(NULL, TVDPLL_CON0,
					(INREG32(TVDPLL_CON0) | 0x01));
		DISPINFO("DISP/DPI,TVDPLL_CON0 0x%x, TVDPLL_CON1: 0x%x\n",
				INREG32(TVDPLL_CON0), INREG32(TVDPLL_CON1));
	}
	return 0;
}

int ddp_dpi_power_off(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	int ret = 0;
	struct device_node *node;

	DISPINFO("DISP/DPI,%s, s_isDpiPowerOn %d\n",
			__func__, s_isDpiPowerOn);
	if (!s_isDpiPowerOn)
		return 0;

	/*_BackupDPIRegisters();*/
	ret += ddp_clk_disable_unprepare(DISP1_DPI_MM_CLOCK);
	if (ret > 0)
		DISPERR("DPI power manager API return FALSE 1\n");

	ret += ddp_clk_disable_unprepare(DISP1_DPI_INTERFACE_CLOCK);
	if (ret > 0)
		DISPERR("DPI power manager API return FALSE\n");

	/* apmixed */
	node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
	if (!node)
		DISPERR("[CLK_APMIXED] find node failed\n");

	clk_apmixed_base = of_iomap(node, 0);
	if (!clk_apmixed_base)
		DISPERR("[CLK_APMIXED] base failed\n");
	else /*disable TVDPLL */
		DPI_OUTREG32(NULL, TVDPLL_CON0,
				(INREG32(TVDPLL_CON0) | 0x00));
	DISPINFO("DISP/DPI,TVDPLL_CON0 0x%x, TVDPLL_CON1: 0x%x\n",
			INREG32(TVDPLL_CON0), INREG32(TVDPLL_CON1));

	ddp_path_top_clock_off();
	s_isDpiPowerOn = FALSE;

	return 0;
}

int ddp_dpi_config(enum DISP_MODULE_ENUM module,
		struct disp_ddp_path_config *config, void *cmdq_handle)
{
	struct LCM_DPI_PARAMS *dpi_config = &(config->dispif_config.dpi);
	int now_bg_parameters = dpi_config->bg_width << 16 |
		dpi_config->bg_height;

	if (s_isDpiConfig == FALSE) {

		DISPINFO("%s DPI status:%x, cmdq:%p\n", __func__,
				INREG32(&DPI_REG->STATUS), cmdq_handle);

		ddp_dpi_ConfigPclk(cmdq_handle, dpi_config->dpi_clock,
				dpi_config->clk_pol);
		ddp_dpi_ConfigSize(cmdq_handle, dpi_config->width,
				dpi_config->height);
		ddp_dpi_ConfigBG(cmdq_handle, true, dpi_config->bg_width,
				dpi_config->bg_height);
		cache_bg_parameter = dpi_config->bg_width << 16 |
			dpi_config->bg_height;
		DISPINFO("dpi_config->bg_width:%d, dpi_config->bg_height:%d\n",
				dpi_config->bg_width, dpi_config->bg_height);

		ddp_dpi_ConfigDE(cmdq_handle, dpi_config->de_pol);
		ddp_dpi_ConfigVsync(cmdq_handle, dpi_config->vsync_pol,
				dpi_config->vsync_pulse_width,
				dpi_config->vsync_back_porch,
				dpi_config->vsync_front_porch);
		ddp_dpi_ConfigHsync(cmdq_handle, dpi_config->hsync_pol,
				dpi_config->hsync_pulse_width,
				dpi_config->hsync_back_porch,
				dpi_config->hsync_front_porch);

		ddp_dpi_ConfigDualEdge(cmdq_handle, dpi_config->i2x_en,
				dpi_config->i2x_edge);

		s_isDpiConfig = TRUE;
		DISPINFO("DISP/DPI,%s done\n", __func__);
	}

	if (s_isDpiConfig != TRUE)
		return 0;

	if (now_bg_parameters != cache_bg_parameter) {
		DISPINFO("DISP/DPI,Need to rechange DPI BG\n");

		ddp_dpi_ConfigSize(cmdq_handle, dpi_config->width,
				dpi_config->height);
		ddp_dpi_ConfigBG(cmdq_handle, true, dpi_config->bg_width,
				dpi_config->bg_height);
		cache_bg_parameter = now_bg_parameters;
	}
	/*ddp_dpi_dump(DISP_MODULE_DPI, 1);*/

	return 0;
}

int ddp_dpi_reset(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	struct DPI_REG_RST reset;

	DISPINFO("DISP/DPI,%s\n", __func__);

	reset = DPI_REG->DPI_RST;
	reset.RST = 1;
	DPI_OUTREGBIT(cmdq_handle, struct DPI_REG_RST, DPI_REG->DPI_RST,
			RST, reset.RST);


	reset.RST = 0;
	DPI_OUTREGBIT(cmdq_handle, struct DPI_REG_RST, DPI_REG->DPI_RST,
			RST, reset.RST);

	return 0;
}

int ddp_dpi_start(enum DISP_MODULE_ENUM module, void *cmdq)
{
	return 0;
}

int ddp_dpi_trigger(enum DISP_MODULE_ENUM module, void *cmdq)
{
	if (s_isDpiStart == FALSE) {
		DISPINFO("DISP/DPI,%s\n", __func__);
		ddp_dpi_reset(module, cmdq);
		/*enable DPI */
		DPI_OUTREG32(cmdq, DISPSYS_DPI_BASE, 0x00000001);

		ddp_dpi_dump(module, 1);
		s_isDpiStart = TRUE;
	}
	return 0;
}

int ddp_dpi_stop(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	DISPINFO("DISP/DPI,%s\n", __func__);

	/*disable DPI and background, and reset DPI */

	DISPINFO("DISP/DPI,DISPSYS_DPI_BASE: 0x%x\n",
			INREG32(DISPSYS_DPI_BASE));
	DPI_OUTREG32(cmdq_handle, DISPSYS_DPI_BASE, 0x00000000);
	DISPINFO("DISP/DPI,DISPSYS_DPI_BASE: 0x%x\n",
			INREG32(DISPSYS_DPI_BASE));


	ddp_dpi_ConfigBG(cmdq_handle, false, 0, 0);
	ddp_dpi_reset(module, cmdq_handle);

	s_isDpiStart = FALSE;
	s_isDpiConfig = FALSE;
	dpi_vsync_irq_count = 0;
	dpi_undflow_irq_count = 0;

	return 0;
}

int ddp_dpi_is_busy(enum DISP_MODULE_ENUM module)
{
	unsigned int status = INREG32(DISPSYS_DPI_BASE + 0x40);

	return (status & (0x1 << 16) ? 1 : 0);
}

int ddp_dpi_is_idle(enum DISP_MODULE_ENUM module)
{
	return !ddp_dpi_is_busy(module);
}

#if ENABLE_DPI_INTERRUPT
irqreturn_t _DPI_InterruptHandler(int irq, void *dev_id)
{
	unsigned int status = (unsigned int)readDPIIntrStatus();

	if (status & 0x01) {
		unsigned int checkSumNum =
			(INREG32(DISPSYS_DPI_BASE + 0x48) & 0x00FFFFFF);

		dpi_vsync_irq_count++;
		if (dpi_vsync_irq_count > 30)
			dpi_vsync_irq_count = 0;
	} else if ((status >> 1) & 0x01) {
		/*DDPMSG("status.VDE  interrupt coming\n");*/
	} else if ((status >> 2) & 0x01) {
		/*DDPMSG("status.UNDERFLOW interrupt coming\n");*/
	}

	ClearDPIIntrStatus();
	return IRQ_HANDLED;
}

#endif

int ddp_dpi_init(enum DISP_MODULE_ENUM module, void *cmdq)
{
	struct device_node *node;

	DISPINFO("DISP/DPI,%s- %p\n", __func__, cmdq);
	/*_BackupDPIRegisters();*/
	ddp_dpi_power_on(DISP_MODULE_DPI, cmdq);

#if 0
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,APMIXED");
	if (!node)
		DISPERR("[CLK_APMIXED] find node failed\n");
	clk_apmixed_base = of_iomap(node, 0);
	if (!clk_apmixed_base)
		DISPERR("[CLK_APMIXED] base failed\n");
#endif

	/* apmixed */
	node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
	if (!node)
		DISPERR("[CLK_APMIXED] find node failed\n");

	clk_apmixed_base = of_iomap(node, 0);
	if (!clk_apmixed_base)
		DISPERR("[CLK_APMIXED] base failed\n");

	_Enable_Interrupt();
	DISPINFO("DISP/DPI,%s done %p\n", __func__, cmdq);

	return 0;
}

int ddp_dpi_deinit(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	DISPINFO("DISP/DPI,%s- %p\n", __func__, cmdq_handle);
	ddp_dpi_stop(DISP_MODULE_DPI, cmdq_handle);
	ddp_dpi_power_off(DISP_MODULE_DPI, cmdq_handle);

	return 0;
}

int ddp_dpi_set_lcm_utils(enum DISP_MODULE_ENUM module,
		struct LCM_DRIVER *lcm_drv)
{
	struct LCM_UTIL_FUNCS *utils = NULL;

	DISPFUNC();
	if (lcm_drv == NULL) {
		DISPERR("DISP/DPI,lcm_drv is null!\n");
		return -1;
	}

	utils = (struct LCM_UTIL_FUNCS *)&lcm_utils_dpi;

	utils->set_reset_pin = lcm_set_reset_pin;
	utils->udelay = lcm_udelay;
	utils->mdelay = lcm_mdelay;
	utils->send_cmd = lcm_send_cmd,
	utils->send_data = lcm_send_data, lcm_drv->set_util_funcs(utils);

	return 0;
}

int ddp_dpi_build_cmdq(enum DISP_MODULE_ENUM module, void *cmdq_trigger_handle,
		enum CMDQ_STATE state)
{
	return 0;
}

int ddp_dpi_dump(enum DISP_MODULE_ENUM module, int level)
{
	unsigned int i;

	DDPDUMP("---------- Start dump DPI registers ----------\n");

	for (i = 0; i <= 0x50; i += 4)
		DDPDUMP("DPI+%04x: 0x%08x\n", i, INREG32(DISPSYS_DPI_BASE + i));
	for (i = 0x68; i <= 0xd8; i += 4)
		DDPDUMP("DPI+%04x: 0x%08x\n", i, INREG32(DISPSYS_DPI_BASE + i));

	DDPDUMP("DPI+Color Bar : %04x : 0x%08x\n", 0xF00,
			INREG32(DISPSYS_DPI_BASE + 0xF00));

/* DDPDUMP("DPI TVDPLL CON0 : 0x%08x\n",  INREG32(DDP_REG_TVDPLL_CON0));*/
/* DDPDUMP("DPI TVDPLL CON1 : 0x%08x\n",  INREG32(DDP_REG_TVDPLL_CON1));*/
/* DDPDUMP("DPI TVDPLL CON6 : 0x%08x\n",  INREG32(DDP_REG_TVDPLL_CON6));*/
/* DDPDUMP("io_driving1:0x:%08x\n", INREG32(DISPSYS_IO_DRIVING1));*/
/* DDPDUMP("io_driving2:0x:%08x\n", INREG32(DISPSYS_IO_DRIVING2));*/


	return 0;
}


static void __iomem *io_driving_base;
static void __iomem *gpio_base;
void ddp_dpi_change_io_driving(enum LCM_DRIVING_CURRENT io_driving)
{
	enum LCM_DRIVING_CURRENT vsync_io_driving = (io_driving >> 8) & 0xFF;
	enum LCM_DRIVING_CURRENT data_io_driving = io_driving & 0xFF;
	struct device_node *node;

	DDPDUMP("vsync_io_driving: 0x%x, data_io_driving: 0x%x\n",
			vsync_io_driving,
			data_io_driving);
	node = of_find_compatible_node(NULL, NULL, "mediatek,pctl-1-syscfg");
	if (!node)
		DISPERR("[DISP_IO_DRIVING] find device node failed!\n");
	io_driving_base = of_iomap(node, 0);
	if (!io_driving_base) {
		DISPERR("[DISP_IO_DRIVING] map base failed!\n");
	} else {

		DISPINFO("IO_DRIVING: DATA_REGISTER BEFORE WRITE is 0x%x\n",
			(INREG32(io_driving_base + 0xA0) & 0xfff00) >> 8);
		DISPINFO("IO_DRIVING: VSYNC_REGISTER BEFORE WRITE is 0x%x\n",
			(INREG32(io_driving_base + 0xA0) & 0xf00000) >> 20);
		switch (data_io_driving) {
		case LCM_DRIVING_CURRENT_2MA:/*2ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(12, 8),
				(unsigned long)(io_driving_base + 0xA0), 0x0);
			break;
		case LCM_DRIVING_CURRENT_4MA:/*4ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(12, 8),
				(unsigned long)(io_driving_base + 0xA0), 0x111);
			break;
		case LCM_DRIVING_CURRENT_6MA:/*6ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(12, 8),
				(unsigned long)(io_driving_base + 0xA0), 0x222);
			break;
		case LCM_DRIVING_CURRENT_8MA:/*8ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(12, 8),
				(unsigned long)(io_driving_base + 0xA0), 0x333);
			break;
		case LCM_DRIVING_CURRENT_10MA:/*10ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(12, 8),
				(unsigned long)(io_driving_base + 0xA0), 0x444);
			break;
		case LCM_DRIVING_CURRENT_12MA:/*12ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(12, 8),
				(unsigned long)(io_driving_base + 0xA0), 0x555);
			break;
		case LCM_DRIVING_CURRENT_14MA:/*14ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(12, 8),
				(unsigned long)(io_driving_base + 0xA0), 0x666);
			break;
		case LCM_DRIVING_CURRENT_16MA:/*16ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(12, 8),
				(unsigned long)(io_driving_base + 0xA0), 0x777);
			break;
		default:
			break;
		}

		switch (vsync_io_driving) {
		case LCM_DRIVING_CURRENT_2MA:/*2ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(4, 20),
				(unsigned long)(io_driving_base + 0xA0), 0x0);
			break;
		case LCM_DRIVING_CURRENT_4MA:/*4ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(4, 20),
				(unsigned long)(io_driving_base + 0xA0), 0x1);
			break;
		case LCM_DRIVING_CURRENT_6MA:/*6ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(4, 20),
				(unsigned long)(io_driving_base + 0xA0), 0x2);
			break;
		case LCM_DRIVING_CURRENT_8MA:/*8ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(4, 20),
				(unsigned long)(io_driving_base + 0xA0), 0x3);
			break;
		case LCM_DRIVING_CURRENT_10MA:/*10ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(4, 20),
				(unsigned long)(io_driving_base + 0xA0), 0x4);
			break;
		case LCM_DRIVING_CURRENT_12MA:/*12ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(4, 20),
				(unsigned long)(io_driving_base + 0xA0), 0x5);
			break;
		case LCM_DRIVING_CURRENT_14MA:/*14ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(4, 20),
				(unsigned long)(io_driving_base + 0xA0), 0x6);
			break;
		case LCM_DRIVING_CURRENT_16MA:/*16ma*/
			DISP_REG_SET_FIELD(NULL, REG_FLD(4, 20),
				(unsigned long)(io_driving_base + 0xA0), 0x7);
			break;
		default:
			break;
		}
		DISPINFO("IO_DRIVING: DATA_REGISTER BEFORE WRITE is 0x%x\n",
			(INREG32(io_driving_base + 0xA0) & 0xfff00) >> 8);
		DISPINFO("IO_DRIVING: VSYNC_REGISTER BEFORE WRITE is 0x%x\n",
			(INREG32(io_driving_base + 0xA0) & 0xf00000) >> 20);
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,gpio");
	if (!node)
		DISPERR("[DISP_GPIO] find device node failed!\n");
	gpio_base = of_iomap(node, 0);
	if (!gpio_base) {
		DISPERR("[DISP_GPIO] map base failed!\n");
	} else {
		DISPINFO("DISP_GPIO: DATA_REGISTER VALUE0 is 0x%x\n",
			(INREG32(gpio_base + 0x310) & 0xfff00000) >> 20);
		DISPINFO("DISP_GPIO: DATA_REGISTER VALUE1 is 0x%x\n",
			(INREG32(gpio_base + 0x320) & 0xffffffff) >> 0);
		DISPINFO("DISP_GPIO: DATA_REGISTER VALUE2 is 0x%x\n",
			(INREG32(gpio_base + 0x330) & 0xfffff) >> 0);
	}


}

int ddp_dpi_ioctl(enum DISP_MODULE_ENUM module, void *cmdq_handle,
		enum DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = 0;
	enum DDP_IOCTL_NAME ioctl = (enum DDP_IOCTL_NAME) ioctl_cmd;

	switch (ioctl) {
	case DDP_DPI_FACTORY_TEST:
		{
			struct disp_ddp_path_config *config_info =
				(struct disp_ddp_path_config *) params;
			struct LCM_DPI_PARAMS *dpi_cfg =
				&config_info->dispif_config.dpi;

			ddp_dpi_power_on(module, NULL);
			ddp_dpi_stop(module, NULL);
			ddp_dpi_reset(module, NULL);
			ddp_dpi_config(module, config_info, NULL);
			ddp_dpi_EnableMux();
			ddp_dpi_EnableColorBar();

			if (dpi_cfg->io_driving_current !=
					LCM_DRIVING_CURRENT_DEFAULT) {
				ddp_dpi_change_io_driving(
						dpi_cfg->io_driving_current);

			}
			ddp_dpi_trigger(module, NULL);
			ddp_dpi_start(module, NULL);
			ddp_dpi_dump(module, 1);

			break;
		}
	case DDP_DPI_FACTORY_RESET:
		{
			ddp_dpi_stop(module, NULL);
			ddp_dpi_DisableColorBar();
			ddp_dpi_DisableMux();
			ddp_dpi_power_off(module, NULL);
			break;
		}
	default:
		break;
	}

	return ret;
}

struct DDP_MODULE_DRIVER ddp_driver_dpi = {
	.module = DISP_MODULE_DPI,
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
};

/*****************************DPI DVT Case Start******************************/
enum DPI_STATUS DPI_EnableColorBar(unsigned int pattern)
{
	/*OUTREG32(DISPSYS_DPI_BASE + 0xF00, 0x41);*/
	OUTREG32(DISPSYS_DPI_BASE + 0xF00, pattern);

	return DPI_STATUS_OK;
}

enum DPI_STATUS DPI_DisableColorBar(void)
{
	OUTREG32(DISPSYS_DPI_BASE + 0xF00, 0);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_EnableColorBar_16(void)
{
	/*enable internal pattern - color bar */
	DPI_OUTREG32(0, DISPSYS_DPI_BASE + 0xF00, 0x10101051);

	return DPI_STATUS_OK;
}

enum DPI_STATUS ddp_dpi_EnableColorBar_0(void)
{
	/*enable internal pattern - color bar */
	DPI_OUTREG32(0, DISPSYS_DPI_BASE + 0xF00, 0x51);

	return DPI_STATUS_OK;
}

/****************************Platform IRQ*****************************/
#if ENABLE_DPI_INTERRUPT
static int dpi_irq;		/*dpi  IRQ*/
int get_eint_dpi_irq(void)
{
	dpi_irq = 0;
	if (dpi_irq < 1) {
		struct device_node *node;

		node = of_find_compatible_node(NULL, NULL, "mediatek,DPI0");
		if (node) {
			/*this step will set irq feature by dtsi*/
			dpi_irq = irq_of_parse_and_map(node, 0);
			if (!dpi_irq) {
				DISPERR("can't irq_of_parse_and_map!\n");
				dpi_irq = 0;
			} else {
				DISPINFO("DISP/DPI,DPI Irq Num: %d\n", dpi_irq);
			}
		} else {
			DISPERR("DISP/DPI,mediatek,DPI0 not find\n");
		}
	}

	return dpi_irq;
}
#endif

int _Enable_Interrupt(void)
{
#if ENABLE_DPI_INTERRUPT
	int ret;
	int dpi_irq_num = get_eint_dpi_irq();

	if (dpi_irq_num > 0) {
		ret =
			request_irq(dpi_irq_num, &_DPI_InterruptHandler,
					IRQF_TRIGGER_NONE,
					"mediatek,DPI0", NULL);
		if (ret > 0)
			DISPERR("eint irq %d not available\n", dpi_irq_num);
	} else {
		DISPERR("%s,%d Error: DPI intr is not defined\n",
				__func__, __LINE__);
	}
#else
#if 0
	if (request_irq(DPI0_IRQ_BIT_ID,
		_DPI_InterruptHandler, IRQF_TRIGGER_LOW, "mtkdpi", NULL) < 0) {
		DISPERR("DISP/DPI [ERROR] fail to request DPI irq\n");
		return DPI_STATUS_ERROR;
	}
#endif
#endif

	struct DPI_REG_INTERRUPT enInt = DPI_REG->INT_ENABLE;

	enInt.VSYNC = 1;
	enInt.VDE = 1;
	enInt.UNDERFLOW = 1;
	OUTREG32(&DPI_REG->INT_ENABLE, AS_UINT32(&enInt));

	return 0;
}
