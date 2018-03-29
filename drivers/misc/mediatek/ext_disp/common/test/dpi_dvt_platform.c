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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/err.h>
#include <linux/switch.h>


#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/timer.h>
/*#include <mach/mt_irq.h>*/
#include <linux/types.h>
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
/*
#include <mach/m4u.h>
#include <mach/m4u_port.h>
*/
#include "m4u.h"
#include "ddp_log.h"
/*#include "cmdq_record.h"*/
#include "dpi_dvt_platform.h"

#include "ddp_clkmgr.h"
#include "ddp_reg.h"

#undef kal_uint32
#define kal_uint32 unsigned int

/*static DPI_BOOL top_power_on;*/
#define SOF_DPI0 0xC3

/*
connect RDMA1(SOUT) -> DSC(SEL) -> DSC(MOUT) -> DPI(SEL)
DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN
DISP_REG_CONFIG_DISP_DSC_SEL_IN
DISP_REG_CONFIG_DISP_DSC_MOUT_EN
DISP_REG_CONFIG_DPI_SEL_IN
*/
int dvt_connect_path(void *handle)
{
	kal_uint32 value = 0;
	/*RDMA1 TO DISP_DSC*/
	value = 1;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN, value);
	value = 1;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DSC_SEL_IN, value);
	/*DISP_DSC TO DPI0*/
	value = 0x4;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DSC_MOUT_EN, value);
	value = 2;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DPI0_SEL_IN, value);

	return 0;
}

int dvt_disconnect_path(void *handle)
{
	kal_uint32 value = 0;

	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DSC_SEL_IN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DSC_MOUT_EN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DPI0_SEL_IN, value);


	return 0;
}

int dvt_acquire_mutex(void)
{
	return HW_MUTEX;
}

int dvt_ddp_path_top_clock_on(void)
{
#ifdef ENABLE_CLK_MGR
	DPI_DVT_LOG_W("ddp path top clock on\n");
#ifdef CONFIG_MTK_CLKMGR
	enable_clock(MT_CG_DISP0_SMI_COMMON, "DDP_SMI");
	enable_clock(MT_CG_DISP0_SMI_LARB0, "DDP_LARB0");
#else
	ddp_clk_enable(DISP_MTCMOS_CLK);
	ddp_clk_enable(DISP0_SMI_COMMON);
	ddp_clk_enable(DISP0_SMI_LARB0);
#endif
	/* enable_clock(MT_CG_DISP0_MUTEX_32K   , "DDP_MUTEX"); */
	DPI_DVT_LOG_W("ddp CG:%x\n", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
#endif
	return 0;
}

int dvt_ddp_path_top_clock_off(void)
{
#ifdef ENABLE_CLK_MGR
#ifdef CONFIG_MTK_CLKMGR
	DPI_DVT_LOG_W("ddp path top clock off\n");
	if (clk_is_force_on(MT_CG_DISP0_SMI_LARB0) || clk_is_force_on(MT_CG_DISP0_SMI_COMMON)) {
		DPI_DVT_LOG_W("clear SMI_LARB0 & SMI_COMMON forced on\n");
		clk_clr_force_on(MT_CG_DISP0_SMI_LARB0);
		clk_clr_force_on(MT_CG_DISP0_SMI_COMMON);
	}
	/* disable_clock(MT_CG_DISP0_MUTEX_32K   , "DDP_MUTEX"); */
	disable_clock(MT_CG_DISP0_SMI_LARB0, "DDP_LARB0");
	disable_clock(MT_CG_DISP0_SMI_COMMON, "DDP_SMI");
#else
	ddp_clk_disable(DISP0_SMI_LARB0);
	ddp_clk_disable(DISP0_SMI_COMMON);
	ddp_clk_disable(DISP_MTCMOS_CLK);
#endif
#endif
	return 0;
}

void dvt_mutex_dump_reg(void)
{

	DPI_DVT_LOG_W("==DISP MUTEX REGS==\n");
	DPI_DVT_LOG_W("MUTEX:0x000=0x%08x,0x004=0x%08x,0x020=0x%08x,0x028=0x%08x\n",
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTEN),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX0_EN),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX0_RST));

	DPI_DVT_LOG_W("MUTEX:0x02c=0x%08x,0x030=0x%08x,0x040=0x%08x,0x048=0x%08x\n",
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX0_MOD),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX0_SOF),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX1_EN),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX1_RST));

	DPI_DVT_LOG_W("MUTEX:0x04c=0x%08x,0x050=0x%08x,0x060=0x%08x,0x068=0x%08x\n",
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX1_MOD),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX1_SOF),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX2_EN),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX2_RST));

	DPI_DVT_LOG_W("MUTEX:0x06c=0x%08x,0x070=0x%08x,0x080=0x%08x,0x088=0x%08x\n",
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX2_MOD),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX2_SOF),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX3_EN),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX3_RST));

	DPI_DVT_LOG_W("MUTEX:0x08c=0x%08x,0x090=0x%08x,0x0a0=0x%08x,0x0a8=0x%08x\n",
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX3_MOD),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX3_SOF),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX4_EN),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX4_RST));

	DPI_DVT_LOG_W("MUTEX:0x0ac=0x%08x,0x0b0=0x%08x,0x0c0=0x%08x,0x0c8=0x%08x\n",
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX4_MOD),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX4_SOF),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX5_EN),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX5_RST));
	DPI_DVT_LOG_W("MUTEX:0x0cc=0x%08x,0x0d0=0x%08x,0x200=0x%08x\n",
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX5_MOD),
		      DISP_REG_GET(DISP_REG_CONFIG_MUTEX5_SOF),
		      DISP_REG_GET(DISP_REG_CONFIG_DEBUG_OUT_SEL));

}

/*
static char *dvt_ddp_get_mutex_sof_name(MUTEX_SOF mode)
{
	switch (mode) {
	case SOF_SINGLE:
		return "single";
	case SOF_DSI0:
		return "dsi0";
	case SOF_DSI1:
		return "dsi1";
	case SOF_DPI0:
		return "dpi0";
	default:
		DPI_DVT_LOG_W("invalid sof =%d", mode);
		return "unknown";
	}
}
*/
static char *dvt_ddp_get_mutex_sof_name(unsigned int regval)
{
	if (regval == SOF_VAL_MUTEX0_SOF_SINGLE_MODE)
		return "single";
	else if (regval == SOF_VAL_MUTEX0_SOF_FROM_DSI0)
		return "dsi0";
	else if (regval == SOF_VAL_MUTEX0_SOF_FROM_DPI)
		return "dpi";

	DDPDUMP("%s, unknown reg=%d\n", __func__, regval);
	return "unknown";
}

static DPI_I32 dvt_mutex_enable_l(void *handle, DPI_U32 hwmutex_id)
{
	DPI_DVT_LOG_W("mutex %d enable\n", hwmutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_EN(hwmutex_id), 1);
	return 0;
}

DPI_I32 dvt_mutex_enable(void *handle, DPI_U32 hwmutex_id)
{
	return dvt_mutex_enable_l(handle, hwmutex_id);
}

DPI_I32 dvt_mutex_disenable(void *handle, DPI_U32 hwmutex_id)
{
	DPI_DVT_LOG_W("mutex %d disable\n", hwmutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_EN(hwmutex_id), 0);
	return 0;
}

DPI_I32 dvt_mutex_reset(void *handle, DPI_U32 hwmutex_id)
{
	DPI_DVT_LOG_W("mutex %d reset\n", hwmutex_id);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_RST(hwmutex_id), 1);
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_RST(hwmutex_id), 0);

	return 0;
}

static DPI_I32 dvt_mutex_mod(void *handle, DPI_U32 hwmutex_id, DPI_I32 mode)
{
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_MOD(hwmutex_id), mode);
	return 0;
}

static DPI_I32 dvt_mutex_sof(void *handle, DPI_U32 hwmutex_id, DPI_I32 sof)
{
	DISP_REG_SET(handle, DISP_REG_CONFIG_MUTEX_SOF(hwmutex_id), sof);
	return 0;
}

/*{DISP_MODULE_RDMA_SHORT    , 15}*/
int dvt_mutex_set(DPI_U32 mutex_id, void *handle)
{
	kal_uint32 mode = 0;
	DPI_I32 sof = SOF_DPI0;

	mode |= 1 << RDMA_MOD_FOR_MUTEX_SHORT;
	mode |= 1 << DISP_DSC_MOD_FOR_MUTEX;

	if (mutex_id < DISP_MUTEX_DDP_FIRST || mutex_id > DISP_MUTEX_DDP_LAST) {
		DDPERR("exceed mutex max (0 ~ %d)\n", DISP_MUTEX_DDP_LAST);
		return -1;
	}

	dvt_mutex_mod(handle, mutex_id, mode);
	dvt_mutex_sof(handle, mutex_id, sof);
	DPI_DVT_LOG_W("dvt_mutex_set %d mode=0x%x, sof=%s, sof=%x\n", mutex_id, mode,
		      dvt_ddp_get_mutex_sof_name(sof), sof);

	return 0;
}

#ifdef DPI_DVT_TEST_SUPPORT
/**********************************K2 LONG PATH Start***************************************/
/*OVL0 -> OVL0_MOUT -> COLOR0_SEL -> COLOR0 -> CCORRO -> AAL0 -> GAMMA0 -> DITHER0 -> DITHER0_MUT*/
/*DISP_RDMA0 -> RDMA0_SOUT -> DPI_SEL -> DPI */

int dvt_connect_ovl0_dpi(void *handle)
{
	kal_uint32 value = 0;

	/* OVL0 out to COLOR0 */
	value = 0x1;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_OVL0_MOUT_EN, value);

	/* COLOR0 input from OVL0 */
	value = 1;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_COLOR0_SEL_IN, value);

	/* dither0 out to RDMA0 */
	value = 0x1;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DITHER_MOUT_EN, value);

	/* RDMA0 out to dpi0 */
	value = 3;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN, value);

	/* DPI in from RDMA0 */
	value = 1;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DPI0_SEL_IN, value);

	return 0;
}

int dvt_disconnect_ovl0_dpi(void *handle)
{
	kal_uint32 value = 0;

	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_OVL0_MOUT_EN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_COLOR0_SEL_IN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DITHER_MOUT_EN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DPI0_SEL_IN, value);

	return 0;
}

int dvt_mutex_set_ovl0_dpi(DPI_U32 mutex_id, void *handle)
{
	kal_uint32 mode = 0;
	DPI_I32 sof = SOF_DPI0;

	/*OVL0 -> OVL0_MOUT -> COLOR0_SEL -> COLOR0 -> CCORRO -> AAL0 -> GAMMA0 -> DITHER0 -> DITHER0_MUT */
	/*DISP_RDMA0 -> RDMA0_SOUT -> DPI_SEL -> DPI */
	/*6,11,12,13,14,15,8 */
	mode |= 1 << 6;
	mode |= 1 << 11;
	mode |= 1 << 12;
	mode |= 1 << 13;
	mode |= 1 << 14;
	mode |= 1 << 15;
	mode |= 1 << 8;

	if (mutex_id < DISP_MUTEX_DDP_FIRST || mutex_id > DISP_MUTEX_DDP_LAST) {
		DDPERR("exceed mutex max (0 ~ %d)\n", DISP_MUTEX_DDP_LAST);
		return -1;
	}

	dvt_mutex_mod(handle, mutex_id, mode);
	dvt_mutex_sof(handle, mutex_id, sof);
	DPI_DVT_LOG_W("dvt_mutex_set %d mode=0x%x, sof=%s\n", mutex_id, mode,
		      dvt_ddp_get_mutex_sof_name(sof));

	return 0;
}


/*OVL1 -> OVL1_MOUT -> DISP_RDMA1 -> RDMA1_SOUT -> DPI_SEL -> DPI*/
/*ovl1 out, rdma1 sel, dpi sel*/
int dvt_connect_ovl1_dpi(void *handle)
{
	kal_uint32 value = 0;

	/*OVL1 out to RDMA1 */
	value = 0x1;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_OVL1_MOUT_EN, value);

	/*RDMA1 TO DISP_DSC*/
	value = 1;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN, value);
	value = 1;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DSC_SEL_IN, value);

	/*DISP_DSC TO DPI0*/
	value = 0x4;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DSC_MOUT_EN, value);
	value = 2;
	DISP_REG_SET(handle, DISP_REG_CONFIG_DPI0_SEL_IN, value);

	return 0;
}

int dvt_disconnect_ovl1_dpi(void *handle)
{
	kal_uint32 value = 0;

	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_OVL1_MOUT_EN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DSC_SEL_IN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DISP_DSC_MOUT_EN, value);
	DISP_REG_SET(handle, DISP_REG_CONFIG_DPI0_SEL_IN, value);

	return 0;
}

/*OVL1 -> OVL1_MOUT -> DISP_RDMA1 -> RDMA1_SOUT -> DPI_SEL -> DPI*/
int dvt_mutex_set_ovl1_dpi(DPI_U32 mutex_id, void *handle)
{
	kal_uint32 mode = 0;
	DPI_I32 sof = SOF_DPI0;

	mode |= 1 << OVL1_MOD_FOR_MUTEX;
	mode |= 1 << RDMA_MOD_FOR_MUTEX_SHORT;
	mode |= 1 << DISP_DSC_MOD_FOR_MUTEX;

	if (mutex_id < DISP_MUTEX_DDP_FIRST || mutex_id > DISP_MUTEX_DDP_LAST) {
		DDPERR("exceed mutex max (0 ~ %d)\n", DISP_MUTEX_DDP_LAST);
		return -1;
	}

	dvt_mutex_mod(handle, mutex_id, mode);
	dvt_mutex_sof(handle, mutex_id, sof);
	DPI_DVT_LOG_W("dvt_mutex_set %d mode=0x%x, sof=%s\n", mutex_id, mode,
		      dvt_ddp_get_mutex_sof_name(sof));

	return 0;
}

#endif				/*DPI_DVT_TEST_SUPPORT */
#endif				/*#if defined(RDMA_DPI_PATH_SUPPORT) || defined(DPI_DVT_TEST_SUPPORT) */
