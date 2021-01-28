// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

/*****************************************************************************
 * camera_fdvt.c - Linux FDVT Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers FDVT relative functions
 *
 *****************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
/*#include <mach/irqs.h>*/
#include <linux/wait.h>
#include <linux/slab.h>
/*#include <linux/xlog.h>*/
#include <mt-plat/sync_write.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "inc/camera_fdvt.h"
/*#include <mach/mt_reg_base.h>*/
/*#include <mach/mt_clkmgr.h>*/
/* #include <mach/mt_clock_manager.h> */

#include <asm/cacheflush.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#ifndef CONFIG_MTK_CLKMGR
#include <linux/clk.h>
#endif

#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif

#include <linux/pm_wakeup.h>

struct wakeup_source *fdvt_wake_lock;

/* #define FDVT_SMI_READY */
#ifdef FDVT_SMI_READY
#include <smi_public.h>
#endif
#define FDVT_DEVNAME     "camera-fdvt"

#define log_vrb(format, args...) \
pr_debug("FDVT [%s] " format, __func__, ##args)
#define log_dbg(format, args...) \
pr_debug("FDVT [%s] " format, __func__, ##args)
#define log_inf(format, args...) \
pr_info("FDVT [%s] " format, __func__, ##args)
#define log_wrn(format, args...) \
pr_info("FDVT [%s] WARNING: " format, __func__, ##args)
#define log_err(format, args...) \
pr_info("FDVT [%s, line%04d] ERROR: " format, __func__, __LINE__, ##args)
#define log_ast(format, args...) \
pr_info("FDVT [%s, line%04d] ASSERT: " format, __func__, __LINE__, ##args)


#define LDVT_EARLY_PORTING_NO_CCF 0
#if LDVT_EARLY_PORTING_NO_CCF
void __iomem *IMGSYS_CONFIG_BASE;
#endif

static dev_t FDVT_devno;
static struct cdev *FDVT_cdev;
static struct class *FDVT_class;
static wait_queue_head_t g_FDVTWQ;
static u32 g_FDVTIRQ = 0, g_FDVTIRQMSK = 0x00000001;
static DEFINE_SPINLOCK(g_spinLock);
static unsigned int g_drvOpened;
static unsigned int g_isSuspend;

static u8 *pBuff;
static u8 *pread_buf;
static u32 buf_size = 1024;

#define FDVT_DRAM_REGCNT 208

#define FDVT_WR32(data, addr)    mt_reg_sync_writel(data, addr)

struct FDVTDBuffRegMap {
	unsigned int u4Addr[FDVT_DRAM_REGCNT];
	unsigned int u4Data[FDVT_DRAM_REGCNT];
	unsigned int u4Counter;
};
#define FDVTDBuffRegMap struct FDVTDBuffRegMap

#ifdef CONFIG_OF

enum {
	FDVT_IRQ_IDX = 0,
	FDVT_IRQ_IDX_NUM
};

enum {
	FDVT_BASE_ADDR = 0,
	FDVT_BASEADDR_NUM
};

#if LDVT_EARLY_PORTING_NO_CCF
#else
struct FD_CLK_STRUCT {
	struct clk *CG_IMGSYS_FDVT;
} FD_CLK_STRUCT;
struct FD_CLK_STRUCT fd_clk;
#endif

static unsigned long gFDVT_Irq[FDVT_IRQ_IDX_NUM];
static unsigned long gFDVT_Reg[FDVT_BASEADDR_NUM];

/* static void __iomem *g_isp_base_dase; */
/* static void __iomem *g_isp_inner_base_dase; */
/* static void __iomem *g_imgsys_config_base_dase; */


#define FDVT_ADDR                        (gFDVT_Reg[FDVT_BASE_ADDR])

#else
#define FDVT_ADDR                        FDVT_BASE
#endif

static FDVTDBuffRegMap pFDVTReadBuffer;
static FDVTDBuffRegMap pFDVTWriteBuffer;

/* register map */
#define FDVT_START                 (FDVT_ADDR+0x0)
#define FDVT_ENABLE                (FDVT_ADDR+0x4)
#define FDVT_RS                    (FDVT_ADDR+0x8)
#define FDVT_RSCON_ADR             (FDVT_ADDR+0xC)
#define FDVT_RGB2Y0                (FDVT_ADDR+0x10)
#define FDVT_RGB2Y1                (FDVT_ADDR+0x14)
#define FDVT_INVG0                 (FDVT_ADDR+0x18)
#define FDVT_INVG1                 (FDVT_ADDR+0x1C)
#define FDVT_INVG2                 (FDVT_ADDR+0x20)
#define FD_FNUM_0                  (FDVT_ADDR+0x24)
#define FD_FNUM_1                  (FDVT_ADDR+0x28)
#define FD_FNUM_2                  (FDVT_ADDR+0x2C)
#define FD_FNUM_3                  (FDVT_ADDR+0x30)
#define VT_FNUM_0                  (FDVT_ADDR+0x34)
#define VT_FNUM_1                  (FDVT_ADDR+0x38)
#define VT_FNUM_2                  (FDVT_ADDR+0x3C)
#define VT_FNUM_3                  (FDVT_ADDR+0x40)
#define FE_FNUM_0                  (FDVT_ADDR+0x44)
#define FE_FNUM_1                  (FDVT_ADDR+0x48)
#define FE_FNUM_2                  (FDVT_ADDR+0x4C)
#define FE_FNUM_3                  (FDVT_ADDR+0x50)
#define LD_BADR_0                  (FDVT_ADDR+0x54)
#define LD_BADR_1                  (FDVT_ADDR+0x58)
#define LD_BADR_2                  (FDVT_ADDR+0x5C)
#define LD_BADR_3                  (FDVT_ADDR+0x60)
#define LD_BADR_4                  (FDVT_ADDR+0x64)
#define LD_BADR_5                  (FDVT_ADDR+0x68)
#define LD_BADR_6                  (FDVT_ADDR+0x6C)
#define LD_BADR_7                  (FDVT_ADDR+0x70)
#define FDVT_RMAP_0                (FDVT_ADDR+0x74)
#define FDVT_RMAP_1                (FDVT_ADDR+0x78)
#define FDVT_FD_SET                (FDVT_ADDR+0x7C)
#define FDVT_FD_CON_BASE_ADR       (FDVT_ADDR+0x80)
#define FDVT_GFD_SKIP              (FDVT_ADDR+0x84)
#define FDVT_LFD                   (FDVT_ADDR+0x88)
#define FDVT_GFD_POSITION_0        (FDVT_ADDR+0x8C)
#define FDVT_GFD_POSITION_1        (FDVT_ADDR+0x90)
#define FDVT_GFD_DET0              (FDVT_ADDR+0x94)
#define FDVT_GFD_DET1              (FDVT_ADDR+0x98)
#define FDVT_FD_RLT_BASE_ADR       (FDVT_ADDR+0x9C)

#define LFD_CTRL_0                 (FDVT_ADDR+0xA4)
#define LFD_TL_0                   (FDVT_ADDR+0xA8)
#define LFD_BR_0                   (FDVT_ADDR+0xAC)
#define LFD_CTRL_1                 (FDVT_ADDR+0xB0)
#define LFD_TL_1                   (FDVT_ADDR+0xB4)
#define LFD_BR_1                   (FDVT_ADDR+0xB8)
#define LFD_CTRL_2                 (FDVT_ADDR+0xBC)
#define LFD_TL_2                   (FDVT_ADDR+0xC0)
#define LFD_BR_2                   (FDVT_ADDR+0xC4)
#define LFD_CTRL_3                 (FDVT_ADDR+0xC8)
#define LFD_TL_3                   (FDVT_ADDR+0xCC)
#define LFD_BR_3                   (FDVT_ADDR+0xD0)
#define LFD_CTRL_4                 (FDVT_ADDR+0xD4)
#define LFD_TL_4                   (FDVT_ADDR+0xD8)
#define LFD_BR_4                   (FDVT_ADDR+0xDC)
#define LFD_CTRL_5                 (FDVT_ADDR+0xE0)
#define LFD_TL_5                   (FDVT_ADDR+0xE4)
#define LFD_BR_5                   (FDVT_ADDR+0xE8)
#define LFD_CTRL_6                 (FDVT_ADDR+0xEC)
#define LFD_TL_6                   (FDVT_ADDR+0xF0)
#define LFD_BR_6                   (FDVT_ADDR+0xF4)
#define LFD_CTRL_7                 (FDVT_ADDR+0xF8)
#define LFD_TL_7                   (FDVT_ADDR+0xFC)
#define LFD_BR_7                   (FDVT_ADDR+0x100)
#define LFD_CTRL_8                 (FDVT_ADDR+0x104)
#define LFD_TL_8                   (FDVT_ADDR+0x108)
#define LFD_BR_8                   (FDVT_ADDR+0x10C)
#define LFD_CTRL_9                 (FDVT_ADDR+0x110)
#define LFD_TL_9                   (FDVT_ADDR+0x114)
#define LFD_BR_9                   (FDVT_ADDR+0x118)
#define LFD_CTRL_10                (FDVT_ADDR+0x11C)
#define LFD_TL_10                  (FDVT_ADDR+0x120)
#define LFD_BR_10                  (FDVT_ADDR+0x124)
#define LFD_CTRL_11                (FDVT_ADDR+0x128)
#define LFD_TL_11                  (FDVT_ADDR+0x12C)
#define LFD_BR_11                  (FDVT_ADDR+0x130)
#define LFD_CTRL_12                (FDVT_ADDR+0x134)
#define LFD_TL_12                  (FDVT_ADDR+0x138)
#define LFD_BR_12                  (FDVT_ADDR+0x13C)
#define LFD_CTRL_13                (FDVT_ADDR+0x140)
#define LFD_TL_13                  (FDVT_ADDR+0x144)
#define LFD_BR_13                  (FDVT_ADDR+0x148)
#define LFD_CTRL_14                (FDVT_ADDR+0x14C)
#define LFD_TL_14                  (FDVT_ADDR+0x150)
#define LFD_BR_14                  (FDVT_ADDR+0x154)
#define FDVT_RESULT                (FDVT_ADDR+0x158)
#define FDVT_INT_EN                (FDVT_ADDR+0x15C)
#define FDVT_SRC_WD_HT             (FDVT_ADDR+0x160)
/* #define FDVT_SRC_IMG_BASE_ADDR     (FDVT_ADDR+0x164) */
#define FDVT_INT                   (FDVT_ADDR+0x168)
#define DEBUG_INFO_1               (FDVT_ADDR+0x16C)
#define DEBUG_INFO_2               (FDVT_ADDR+0x170)
#define DEBUG_INFO_3               (FDVT_ADDR+0x174)
#define FDVT_RESULTNUM             (FDVT_ADDR+0x178)
#define FDVT_MAX_OFFSET            0x178

#ifdef CONFIG_OF
struct fdvt_device {
	void __iomem *regs[FDVT_BASEADDR_NUM];
	struct device *dev;
	int irq[FDVT_IRQ_IDX_NUM];
#ifndef CONFIG_MTK_CLKMGR /* CCF */
	struct clk *clk_SCP_SYS_DIS;
	struct clk *clk_SCP_SYS_ISP;
	struct clk *clk_SMI_COMMON;
	struct clk *clk_FD;
#endif
};

static struct fdvt_device *fdvt_devs;
static int nr_fdvt_devs;
#endif

bool haveConfig;

void FDVT_basic_config(void)
{
	FDVT_WR32(0x00000111, FDVT_ENABLE);
	FDVT_WR32(0x0000040B, FDVT_RS);
	FDVT_WR32(0x00000000, FDVT_RSCON_ADR);
	FDVT_WR32(0x02590132, FDVT_RGB2Y0);
	FDVT_WR32(0x00000075, FDVT_RGB2Y1);
	FDVT_WR32(0x66553520, FDVT_INVG0);
	FDVT_WR32(0xB8A28D79, FDVT_INVG1);
	FDVT_WR32(0xFFF4E7CF, FDVT_INVG2);
	FDVT_WR32(0x02310231, FD_FNUM_0);
	FDVT_WR32(0x02A10213, FD_FNUM_1);
	FDVT_WR32(0x03F102B1, FD_FNUM_2);
	FDVT_WR32(0x046103C1, FD_FNUM_3);
	FDVT_WR32(0x01450168, VT_FNUM_0);
	FDVT_WR32(0x01D70173, VT_FNUM_1);
	FDVT_WR32(0x02C201E2, VT_FNUM_2);
	FDVT_WR32(0x031002A0, VT_FNUM_3);
	FDVT_WR32(0x00180018, FE_FNUM_0);
	FDVT_WR32(0x00000000, FE_FNUM_1);
	FDVT_WR32(0x00000000, FE_FNUM_2);
	FDVT_WR32(0x00000000, FE_FNUM_3);
	FDVT_WR32(0x00000000, LD_BADR_0);
	FDVT_WR32(0x00000000, LD_BADR_1);
	FDVT_WR32(0x00000000, LD_BADR_2);
	FDVT_WR32(0x00000000, LD_BADR_3);
	FDVT_WR32(0x00000000, LD_BADR_4);
	FDVT_WR32(0x00000000, LD_BADR_5);
	FDVT_WR32(0x00000000, LD_BADR_6);
	FDVT_WR32(0x00000000, LD_BADR_7);
	FDVT_WR32(0x3F000000, FDVT_RMAP_0);
	FDVT_WR32(0x05230401, FDVT_RMAP_1);
	FDVT_WR32(0x000F010B, FDVT_FD_SET);
	FDVT_WR32(0x00000000, FDVT_FD_CON_BASE_ADR);
	FDVT_WR32(0x00000011, FDVT_GFD_SKIP);
	FDVT_WR32(0x01130000, FDVT_LFD);
	FDVT_WR32(0x00000000, FDVT_GFD_POSITION_0);
	FDVT_WR32(0x00F00140, FDVT_GFD_POSITION_1);
	FDVT_WR32(0x00000001, FDVT_GFD_DET0);
	FDVT_WR32(0x00000000, FDVT_GFD_DET1);
	FDVT_WR32(0x00000000, FDVT_FD_RLT_BASE_ADR);
	FDVT_WR32(0x00000000, LFD_CTRL_0);
	FDVT_WR32(0x00000000, LFD_TL_0);
	FDVT_WR32(0x00000000, LFD_BR_0);
	FDVT_WR32(0x00000000, LFD_CTRL_1);
	FDVT_WR32(0x00000000, LFD_TL_1);
	FDVT_WR32(0x00000000, LFD_BR_1);
	FDVT_WR32(0x00000000, LFD_CTRL_2);
	FDVT_WR32(0x00000000, LFD_TL_2);
	FDVT_WR32(0x00000000, LFD_BR_2);
	FDVT_WR32(0x00000000, LFD_CTRL_3);
	FDVT_WR32(0x00000000, LFD_TL_3);
	FDVT_WR32(0x00000000, LFD_BR_3);
	FDVT_WR32(0x00000000, LFD_CTRL_4);
	FDVT_WR32(0x00000000, LFD_TL_4);
	FDVT_WR32(0x00000000, LFD_BR_4);
	FDVT_WR32(0x00000000, LFD_CTRL_5);
	FDVT_WR32(0x00000000, LFD_TL_5);
	FDVT_WR32(0x00000000, LFD_BR_5);
	FDVT_WR32(0x00000000, LFD_CTRL_6);
	FDVT_WR32(0x00000000, LFD_TL_6);
	FDVT_WR32(0x00000000, LFD_BR_6);
	FDVT_WR32(0x00000000, LFD_CTRL_7);
	FDVT_WR32(0x00000000, LFD_TL_7);
	FDVT_WR32(0x00000000, LFD_BR_7);
	FDVT_WR32(0x00000000, LFD_CTRL_8);
	FDVT_WR32(0x00000000, LFD_TL_8);
	FDVT_WR32(0x00000000, LFD_BR_8);
	FDVT_WR32(0x00000000, LFD_CTRL_9);
	FDVT_WR32(0x00000000, LFD_TL_9);
	FDVT_WR32(0x00000000, LFD_BR_9);
	FDVT_WR32(0x00000000, LFD_CTRL_10);
	FDVT_WR32(0x00000000, LFD_TL_10);
	FDVT_WR32(0x00000000, LFD_BR_10);
	FDVT_WR32(0x00000000, LFD_CTRL_11);
	FDVT_WR32(0x00000000, LFD_TL_11);
	FDVT_WR32(0x00000000, LFD_BR_11);
	FDVT_WR32(0x00000000, LFD_CTRL_12);
	FDVT_WR32(0x00000000, LFD_TL_12);
	FDVT_WR32(0x00000000, LFD_BR_12);
	FDVT_WR32(0x00000000, LFD_CTRL_13);
	FDVT_WR32(0x00000000, LFD_TL_13);
	FDVT_WR32(0x00000000, LFD_BR_13);
	FDVT_WR32(0x00000000, LFD_CTRL_14);
	FDVT_WR32(0x00000000, LFD_TL_14);
	FDVT_WR32(0x00000000, LFD_BR_14);
	FDVT_WR32(0x00000000, FDVT_INT_EN);
	FDVT_WR32(0x014000F0, FDVT_SRC_WD_HT);
}

/*
 *static unsigned long ms_to_jiffies(unsigned long ms)
 *{
 *	return (ms * HZ + 512) >> 10;
 *}
 */

static unsigned long us_to_jiffies(unsigned long us)
{
	return (((us/1000) * HZ + 512) >> 10);
}

/*=======================================================================*/
/* FDVT Clock control Registers */
/*=======================================================================*/
#if LDVT_EARLY_PORTING_NO_CCF
#else
#ifdef FDVT_SMI_READY
static inline void FD_Prepare_Enable_ccf_clock(void)
{
	int ret;

	/*smi_bus_enable(SMI_LARB_IMGSYS0, "camera_fdvt");*/
#if (MTK_FD_LARB == 1)
	smi_bus_prepare_enable(SMI_LARB1_REG_INDX, "camera_fdvt", true);
#else
	smi_bus_prepare_enable(SMI_LARB2_REG_INDX, "camera_fdvt", true);
#endif
	ret = clk_prepare_enable(fd_clk.CG_IMGSYS_FDVT);
	if (ret)
		log_err("cannot prepare and enable CG_IMGSYS_FDVT clock\n");


}

static inline void FD_Disable_Unprepare_ccf_clock(void)
{
	clk_disable_unprepare(fd_clk.CG_IMGSYS_FDVT);
	/*smi_bus_disable(SMI_LARB_IMGSYS0, "camera_fdvt");*/
#if (MTK_FD_LARB == 1)
	smi_bus_disable_unprepare(SMI_LARB1_REG_INDX, "camera_fdvt", true);
#else
	smi_bus_disable_unprepare(SMI_LARB2_REG_INDX, "camera_fdvt", true);
#endif
}
#endif
#endif

static int mt_fdvt_clk_ctrl(int en)
{
#if LDVT_EARLY_PORTING_NO_CCF
	if (en) {
		unsigned int setReg = 0xFFFFFFFF;

		FDVT_WR32(setReg, IMGSYS_CONFIG_BASE+0x8);
	} else {
		unsigned int setReg = 0xFFFFFFFF;

		FDVT_WR32(setReg, IMGSYS_CONFIG_BASE+0x4);
	}
#else
#ifdef FDVT_SMI_READY
	if (en)
		FD_Prepare_Enable_ccf_clock();
	else
		FD_Disable_Unprepare_ccf_clock();
#else
#endif
#endif
	return 0;
}

/*=======================================================================*/
/* FDVT FD Config Registers */
/*=======================================================================*/

void FaceDetecteConfig(void)
{
	FDVT_WR32(0x02310231, FD_FNUM_0);
	FDVT_WR32(0x02A10213, FD_FNUM_1);
	FDVT_WR32(0x03F102B1, FD_FNUM_2);
	FDVT_WR32(0x046103C1, FD_FNUM_3);
	FDVT_WR32(0x01450168, VT_FNUM_0);
	FDVT_WR32(0x01D70173, VT_FNUM_1);
	FDVT_WR32(0x02C201E2, VT_FNUM_2);
	FDVT_WR32(0x031002A0, VT_FNUM_3);
	FDVT_WR32(0x00180018, FE_FNUM_0);
	FDVT_WR32(0x00000000, FE_FNUM_1);
	FDVT_WR32(0x00000000, FE_FNUM_2);
	FDVT_WR32(0x00000000, FE_FNUM_3);
	/* FDVT_WR32(0x000F010B,FDVT_FD_SET);   //LDVT Disable */
}
/*=======================================================================*/
/* FDVT SD Config Registers */
/*=======================================================================*/
void SmileDetecteConfig(void)
{
	FDVT_WR32(0x01210171, FD_FNUM_0);
	FDVT_WR32(0x00D10081, FD_FNUM_1);
	FDVT_WR32(0x00F100D1, FD_FNUM_2);
	FDVT_WR32(0x00F100D1, FD_FNUM_3);
	FDVT_WR32(0x00E70127, VT_FNUM_0);
	FDVT_WR32(0x00A70067, VT_FNUM_1);
	FDVT_WR32(0x00C000A7, VT_FNUM_2);
	FDVT_WR32(0x00C000A7, VT_FNUM_3);
	FDVT_WR32(0x00180018, FE_FNUM_0);
	FDVT_WR32(0x00180018, FE_FNUM_1);
	FDVT_WR32(0x00180018, FE_FNUM_2);
	FDVT_WR32(0x00180018, FE_FNUM_3);
	FDVT_WR32(0x000F010B, FDVT_FD_SET);
}

/*=======================================================================*/
/* FDVT Dump Registers */
/*=======================================================================*/
void FDVT_DUMPREG(void)
{
	unsigned int u4RegValue = 0;
	unsigned int u4Index = 0;

	log_dbg("FDVT REG:\n ********************\n");

	/* for(u4Index = 0; u4Index < 0x180; u4Index += 4) { */
	for (u4Index = 0x158; u4Index < 0x180; u4Index += 4) {
		u4RegValue = ioread32((void *)(FDVT_ADDR + u4Index));
		log_dbg("+0x%x 0x%x\n", u4Index, u4RegValue);
	}
}

/*=======================================================================*/
/* FDVT set reg to HW buffer */
/*=======================================================================*/
static int FDVT_SetRegHW(FDVTRegIO *a_pstCfg)
{
	FDVTRegIO *pREGIO = NULL;
	u32 i = 0;

	if (a_pstCfg == NULL) {
		log_dbg("Null input argrment\n");
		return -EINVAL;
	}

	pREGIO = (FDVTRegIO *)a_pstCfg;

	if (pREGIO->u4Count > FDVT_DRAM_REGCNT) {
		log_dbg("Buffer Size Exceeded!\n");
		return -EFAULT;
	}

	if (copy_from_user(
		(void *)pFDVTWriteBuffer.u4Addr,
		(void *) pREGIO->pAddr,
		pREGIO->u4Count * sizeof(u32))) {
		log_dbg("ioctl copy from user failed\n");
		return -EFAULT;
	}

	if (copy_from_user(
		(void *)pFDVTWriteBuffer.u4Data,
		(void *) pREGIO->pData,
		pREGIO->u4Count * sizeof(u32))) {
		log_dbg("ioctl copy from user failed\n");
		return -EFAULT;
	}

	/* pFDVTWriteBuffer.u4Counter=pREGIO->u4Count; */
	/* log_dbg("Count = %d\n", pREGIO->u4Count); */

	for (i = 0; i < pREGIO->u4Count; i++) {
		if (((FDVT_ADDR + pFDVTWriteBuffer.u4Addr[i]) >= FDVT_ENABLE) &&
			((pFDVTWriteBuffer.u4Addr[i]) <= FDVT_MAX_OFFSET) &&
			((pFDVTWriteBuffer.u4Addr[i] & 0x3) == 0)) {
			/* log_dbg("Write: FDVT[0x%03lx](0x%08lx) = 0x%08lx\n",
			 * (unsigned long)pFDVTWriteBuffer.u4Addr[i],
			 * (unsigned long)(FDVT_ADDR +
			 *		pFDVTWriteBuffer.u4Addr[i]),
			 * (unsigned long)pFDVTWriteBuffer.u4Data[i]);
			 */
			FDVT_WR32(pFDVTWriteBuffer.u4Data[i],
				FDVT_ADDR + pFDVTWriteBuffer.u4Addr[i]);
		} else {
			log_err("Error: Writing Memory(0x%lx) Excess FDVT Range!  FD Offset: 0x%lx, FD Base: 0x%lx\n",
			(unsigned long)(FDVT_ADDR + pFDVTReadBuffer.u4Addr[i]),
			(unsigned long)pFDVTReadBuffer.u4Addr[i],
			(unsigned long)FDVT_ADDR);
		}
	}

	return 0;
}

/***********************************************************
 *
 ************************************************************/
static int FDVT_ReadRegHW(FDVTRegIO *a_pstCfg)
{
	int ret = 0;
	int size = 0;
	int i = 0;

	if (a_pstCfg == NULL) {
		log_dbg("Null input argrment\n");
		return -EINVAL;
	}

	if (a_pstCfg->u4Count > FDVT_DRAM_REGCNT) {
		log_dbg("Buffer Size Exceeded!\n");
		return -EFAULT;
	}

	size = a_pstCfg->u4Count * 4;

	if (copy_from_user(
		pFDVTReadBuffer.u4Addr,
		a_pstCfg->pAddr,
		size) != 0) {
		log_dbg("copy_from_user failed\n");
		ret = -EFAULT;
		goto mt_FDVT_read_reg_exit;
	}

	for (i = 0; i < a_pstCfg->u4Count; i++) {
		if (((FDVT_ADDR + pFDVTReadBuffer.u4Addr[i]) >= FDVT_ADDR) &&
			((pFDVTReadBuffer.u4Addr[i]) <= FDVT_MAX_OFFSET) &&
			((pFDVTWriteBuffer.u4Addr[i] & 0x3) == 0)) {
			pFDVTReadBuffer.u4Data[i] =
				ioread32(
					(void *)(FDVT_ADDR +
					pFDVTReadBuffer.u4Addr[i]));
			/*log_dbg("Read  addr/val: 0x%08x/0x%08x\n",
			 *	(u32) (FDVT_ADDR + pFDVTReadBuffer.u4Addr[i]),
			 *	(u32) pFDVTReadBuffer.u4Data[i]);
			 */
		} else {
			log_err("Error: Reading Memory(0x%lx) Excess FDVT Range!  FD Offset: 0x%lx, FD Base: 0x%lx\n",
			(unsigned long)(FDVT_ADDR + pFDVTReadBuffer.u4Addr[i]),
			(unsigned long)pFDVTReadBuffer.u4Addr[i],
			(unsigned long)FDVT_ADDR);
			ret = -EFAULT;
			goto mt_FDVT_read_reg_exit;
		}
	}
	if (copy_to_user(a_pstCfg->pData, pFDVTReadBuffer.u4Data, size) != 0) {
		log_dbg("copy_to_user failed\n");
		ret = -EFAULT;
		goto mt_FDVT_read_reg_exit;
	}
mt_FDVT_read_reg_exit:

	return ret;
}

/*=======================================================================*/
/* Wait IRQ, for user space program to wait interrupt */
/* wait for timeout 500ms */
/*=======================================================================*/
static int FDVT_WaitIRQ(u32 *u4IRQMask)
{
	int timeout;

	timeout = wait_event_interruptible_timeout(
		g_FDVTWQ, (g_FDVTIRQMSK & g_FDVTIRQ),
	us_to_jiffies(15 * 1000000));

	if (timeout == 0) {
		log_err("wait_event_interruptible_timeout timeout, %d, %d\n",
			g_FDVTIRQMSK, g_FDVTIRQ);
		FDVT_WR32(0x00030000, FDVT_START);  /* LDVT Disable */
		FDVT_WR32(0x00000000, FDVT_START);  /* LDVT Disable */
		return -EAGAIN;
	}

	*u4IRQMask = g_FDVTIRQ;
	/*log_dbg("[FDVT] IRQ : 0x%8x\n",g_FDVTIRQ);*/

	/* check if user is interrupted by system signal */
	if (timeout != 0 && !(g_FDVTIRQMSK & g_FDVTIRQ)) {
		log_err("interrupted by system signal,return value(%d)\n",
			timeout);
		return -ERESTARTSYS; /* actually it should be -ERESTARTSYS */
	}

	if (!(g_FDVTIRQMSK & g_FDVTIRQ)) {
		log_err("wait_event_interruptible Not FDVT, %d, %d\n",
			g_FDVTIRQMSK, g_FDVTIRQ);
		FDVT_DUMPREG();
		return -1;
	}

	g_FDVTIRQ = 0;

	return 0;
}

static irqreturn_t FDVT_irq(int irq, void *dev_id)
{
	/*g_FDVT_interrupt_handler = VAL_TRUE;*/
	/*FDVT_ISR();*/
	g_FDVTIRQ = ioread32((void *)FDVT_INT);

	wake_up_interruptible(&g_FDVTWQ);

	return IRQ_HANDLED;
}

static long FDVT_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	if (_IOC_SIZE(cmd) > buf_size) {
		log_dbg("Buffer Size Exceeded!\n");
		return -EFAULT;
	}

	if (_IOC_DIR(cmd) != _IOC_NONE) {
		/* IO write */
		if (_IOC_WRITE & _IOC_DIR(cmd)) {
			if (copy_from_user(
				pBuff,
				(void *)arg,
				_IOC_SIZE(cmd))) {
				log_dbg(" ioctl copy from user failed\n");
				return -EFAULT;
			}
		}
		/* else */
			/* log_dbg(" ioctlnot write command\n"); */
	}
	/* else */
		/* log_dbg(" ioctl command = NONE\n"); */

	switch (cmd) {
	case FDVT_IOC_INIT_SETPARA_CMD:
		log_dbg("[FDVT] FDVT_INIT_CMD\n");
		haveConfig = 0;
		FDVT_basic_config();
		break;
	case FDVT_IOC_STARTFD_CMD:
		/* log_dbg("[FDVT] FDVTIOC_STARTFD_CMD\n"); */
		if (haveConfig) {
			FDVT_WR32(0x00000001, FDVT_INT_EN);
			FDVT_WR32(0x00000000, FDVT_START);
			FDVT_WR32(0x00000001, FDVT_START);
			FDVT_WR32(0x00000000, FDVT_START);
			haveConfig = 0;
		}
		/* FDVT_DUMPREG(); */
		break;
	case FDVT_IOC_G_WAITIRQ:
		/* log_dbg("[FDVT] FDVT_WaitIRQ\n"); */
		ret = FDVT_WaitIRQ((unsigned int *)pBuff);
		FDVT_WR32(0x00000000, FDVT_INT_EN);
		break;
	case FDVT_IOC_T_SET_FDCONF_CMD:
		/* log_dbg("[FDVT] FDVT set FD config\n"); */
		FaceDetecteConfig();
		FDVT_SetRegHW((FDVTRegIO *)pBuff);
		haveConfig = 1;
		break;
	case FDVT_IOC_T_SET_SDCONF_CMD:
		/* log_dbg("[FDVT] FDVT set SD config\n"); */
		SmileDetecteConfig();
		FDVT_SetRegHW((FDVTRegIO *)pBuff);
		haveConfig = 1;
		break;
	case FDVT_IOC_G_READ_FDREG_CMD:
		/* log_dbg("[FDVT] FDVT read FD config\n"); */
		FDVT_ReadRegHW((FDVTRegIO *)pBuff);
		break;
	case FDVT_IOC_T_DUMPREG:
		log_dbg("[FDVT] FDVT_DUMPREG\n");
		FDVT_DUMPREG();
		break;
	default:
		log_dbg("[FDVT][ERROR] %s default case\n", __func__);
		break;
	}
	/* NOT_REFERENCED(ret); */
	/* log_dbg("[FDVT] FDVT IOCtrol out\n"); */
	if (_IOC_READ & _IOC_DIR(cmd)) {
		if (copy_to_user(
			(void __user *)arg,
			pBuff,
			_IOC_SIZE(cmd)) != 0) {
			log_dbg("copy_to_user failed\n");
			return -EFAULT;
		}
	}

	return ret;
}

#ifdef CONFIG_COMPAT

/***********************************************************
 *
 ************************************************************/

static int compat_FD_get_register_data(
		compat_FDVTRegIO __user *data32,
		FDVTRegIO __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr_Addr;
	compat_uptr_t uptr_Data;
	int err;

	err = get_user(uptr_Addr, &data32->pAddr);
	err |= put_user(compat_ptr(uptr_Addr), &data->pAddr);
	err |= get_user(uptr_Data, &data32->pData);
	err |= put_user(compat_ptr(uptr_Data), &data->pData);
	err |= get_user(count, &data32->u4Count);
	err |= put_user(count, &data->u4Count);

	return err;
}

static int compat_FD_put_register_data(
		compat_FDVTRegIO __user *data32,
		FDVTRegIO __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr_Addr;*/
	/*compat_uptr_t uptr_Data;*/
	int err;

	/* Assume data pointer is unchanged. */
	/* err = get_user(uptr_Addr, &data->pAddr); */
	/* err |= put_user(compat_ptr(uptr_Addr), data32->pAddr); */
	/* err |= get_user(uptr_Data, &data->pData); */
	/* err |= put_user(compat_ptr(uptr_Data), &data32->pData); */
	err = get_user(count, &data->u4Count);
	err |= put_user(count, &data32->u4Count);

	return err;
}

static long compat_FD_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_FDVT_IOC_INIT_SETPARA_CMD:
	{
		ret = file->f_op->unlocked_ioctl
			(file,
			cmd,
			(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_FDVT_IOC_STARTFD_CMD:
	{
		ret = file->f_op->unlocked_ioctl
			(file,
			cmd,
			(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_FDVT_IOC_G_WAITIRQ:
	{
		compat_FDVTRegIO __user *data32;
		FDVTRegIO __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_FD_get_register_data(data32, data);
		if (err)
			return err;
		ret = file->f_op->unlocked_ioctl(
			file,
			FDVT_IOC_G_WAITIRQ,
			(unsigned long)data);
		err = compat_FD_put_register_data(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_FDVT_IOC_T_SET_FDCONF_CMD:
	{
		compat_FDVTRegIO __user *data32;
		FDVTRegIO __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_FD_get_register_data(data32, data);
		if (err)
			return err;
		ret = file->f_op->unlocked_ioctl(
			file,
			FDVT_IOC_T_SET_FDCONF_CMD,
			(unsigned long)data);
		return ret ? ret : err;
	}
	case COMPAT_FDVT_IOC_G_READ_FDREG_CMD:
	{
		compat_FDVTRegIO __user *data32;
		FDVTRegIO __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_FD_get_register_data(data32, data);
		if (err)
			return err;
		ret = file->f_op->unlocked_ioctl
			(file,
			FDVT_IOC_G_READ_FDREG_CMD,
			(unsigned long)data);
		err = compat_FD_put_register_data(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_FDVT_IOC_T_SET_SDCONF_CMD:
	{
		compat_FDVTRegIO __user *data32;
		FDVTRegIO __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_FD_get_register_data(data32, data);
		if (err)
			return err;
		ret = file->f_op->unlocked_ioctl
			(file,
			FDVT_IOC_T_SET_SDCONF_CMD,
			(unsigned long)data);
		return ret ? ret : err;
	}
	case COMPAT_FDVT_IOC_T_DUMPREG:
	{
		ret = file->f_op->unlocked_ioctl
			(file,
			cmd,
			(unsigned long)compat_ptr(arg));
		return ret;
	}
	default:
		return -ENOIOCTLCMD;
		/* return ISP_ioctl(filep, cmd, arg); */
	}
}

#endif


static int FDVT_open(struct inode *inode, struct file *file)
{
	signed int ret = 0;

	log_dbg("[FDVT_DEBUG] %s\n", __func__);

	spin_lock(&g_spinLock);
	if (g_drvOpened) {
		spin_unlock(&g_spinLock);
		log_dbg("Opened, return -EBUSY\n");
		return -EBUSY;
	}
	g_drvOpened = 1;
	spin_unlock(&g_spinLock);

	__pm_stay_awake(fdvt_wake_lock);

	mt_fdvt_clk_ctrl(1);

	__pm_relax(fdvt_wake_lock);

	if (pBuff != NULL)
		log_dbg("pBuff is not null\n");
	if (pread_buf != NULL)
		log_dbg("pread_buf is not null\n");

	pBuff = kmalloc(buf_size, GFP_KERNEL);
	if (pBuff == NULL) {
		/* log_dbg(" ioctl allocate mem failed\n"); */
		ret = -ENOMEM;
	} else {
		log_dbg(" ioctl allocate mem ok\n");
	}

	pread_buf = kmalloc(buf_size, GFP_KERNEL);
	if (pread_buf == NULL) {
		/* log_dbg(" ioctl allocate mem failed\n"); */
		ret = -ENOMEM;
	} else {
		log_dbg(" ioctl allocate mem ok\n");
	}

	if (ret < 0) {
		/*if (pBuff) {*/
		kfree(pBuff);
		pBuff = NULL;
		/*}*/
		/*if (pread_buf) {*/
		kfree(pread_buf);
		pread_buf = NULL;
		/*}*/
	}

	return 0;
}
/*
 * static int FDVT_flush(struct file *file, fl_owner_t id)
 * {
 *	log_dbg("[FDVT_DEBUG] FDVT_flush\n");
 *	return 0;
 * }
 */
static int FDVT_release(struct inode *inode, struct file *file)
{
	log_dbg("[FDVT_DEBUG] %s\n", __func__);
	/*if (pBuff) {*/
		kfree(pBuff);
		pBuff = NULL;
	/*}*/
	/*if (pread_buf) {*/
		kfree(pread_buf);
		pread_buf = NULL;
	/*}*/

	FDVT_WR32(0x00000000, FDVT_INT_EN);
	g_FDVTIRQ = ioread32((void *)FDVT_INT);

	__pm_stay_awake(fdvt_wake_lock);

	mt_fdvt_clk_ctrl(0);

	__pm_relax(fdvt_wake_lock);

	spin_lock(&g_spinLock);
	g_drvOpened = 0;
	spin_unlock(&g_spinLock);

	return 0;
}

static const struct file_operations FDVT_fops = {
	.owner               = THIS_MODULE,
	.unlocked_ioctl      = FDVT_ioctl,
	.open                = FDVT_open,
	.release             = FDVT_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl        = compat_FD_ioctl,
#endif
};

static int FDVT_probe(struct platform_device *dev)
{
#if LDVT_EARLY_PORTING_NO_CCF
	struct device_node *node = NULL;
#endif
	struct class_device;
	int ret;
	int i = 0;
	int new_count;
	struct class_device *class_dev = NULL;
	struct fdvt_device *tempFdvt;
#ifdef CONFIG_OF
	struct fdvt_device *fdvt_dev;
#endif

#if LDVT_EARLY_PORTING_NO_CCF
	node = of_find_compatible_node(NULL, NULL, "mediatek,imgsys_config");
	if (!node) {
		log_err("find IMGSYS_CONFIG node failed!!!\n");
		return -ENODEV;
	}
	IMGSYS_CONFIG_BASE = of_iomap(node, 0);
	if (!IMGSYS_CONFIG_BASE) {
		log_err("unable to map IMGSYS_CONFIG_BASE registers!!!\n");
		return -ENODEV;
	}

	log_dbg("[FDVT_DEBUG] IMGSYS_CONFIG_BASE: %lx\n",
		(unsigned long)IMGSYS_CONFIG_BASE);
#endif

	nr_fdvt_devs = 0;

	log_inf("FDVT PROBE!!!\n");
#ifdef CONFIG_OF

	log_dbg("[FDVT_DEBUG] %s\n", __func__);

	if (dev == NULL) {
		dev_info(&dev->dev, "dev is NULL");
		return -ENXIO;
	}

	new_count = nr_fdvt_devs + 1;
	tempFdvt = krealloc
		(fdvt_devs,
		sizeof(struct fdvt_device) * new_count,
		GFP_KERNEL);
	if (!tempFdvt) {
		dev_info(&dev->dev, "Unable to realloc fdvt_devs\n");
		return -ENOMEM;
	}
	fdvt_devs = tempFdvt;
	fdvt_dev = &(fdvt_devs[nr_fdvt_devs]);
	fdvt_dev->dev = &dev->dev;

	/* iomap registers and irq*/
	for (i = 0; i < FDVT_BASEADDR_NUM; i++)	{
		fdvt_dev->regs[i] = of_iomap(dev->dev.of_node, i);
		if (!fdvt_dev->regs[i]) {
			dev_info(&dev->dev,
				"Unable to ioremap registers, of_iomap fail, i=%d\n",
				i);
			return -ENOMEM;
		}
		gFDVT_Reg[i] = (unsigned long)fdvt_dev->regs[i];
		log_inf("DT, i=%d, map_addr=0x%lx\n", i, gFDVT_Reg[i]);
	}

#ifdef CONFIG_MTK_CLKMGR
#else
	/*of_property_read_u32(
	 *	pdev->dev.of_node,
	 *	"clock-frequency",
	 *	&speed_hz);
	 */
	/*of_property_read_u32(pdev->dev.of_node, "clock-div", &clk_src_div);*/
	fdvt_devs->clk_SCP_SYS_DIS =
		devm_clk_get(fdvt_dev->dev, "FD-SCP_SYS_DIS");
	if (!fdvt_devs->clk_SCP_SYS_DIS) {
		log_err("cannot get clock: FD-SCP_SYS_DIS\n");
		return -ENOMEM;
	}
	fdvt_devs->clk_SCP_SYS_ISP =
		devm_clk_get(fdvt_dev->dev, "FD-SCP_SYS_ISP");
	if (!fdvt_devs->clk_SCP_SYS_ISP) {
		log_err("cannot get clock: FD-SCP_SYS_ISP\n");
		return -ENOMEM;
	}
	fdvt_devs->clk_SMI_COMMON =
		devm_clk_get(fdvt_dev->dev, "FD-MM_DISP0_SMI_COMMON");
	if (!fdvt_devs->clk_SMI_COMMON) {
		log_err("cannot get clock: FD-MM_DISP0_SMI_COMMON\n");
		return -ENOMEM;
	}
	fdvt_devs->clk_FD =
		devm_clk_get(fdvt_dev->dev, "FD-IMG_IMAGE_FD");
	if (!fdvt_devs->clk_FD) {
		log_err("cannot get clock: FD-IMG_IMAGE_FD\n");
		return -ENOMEM;
	}
#endif

	/* get IRQ ID and request IRQ */

	for (i = 0; i < FDVT_IRQ_IDX_NUM; i++) {
		fdvt_dev->irq[i] = irq_of_parse_and_map(dev->dev.of_node, i);
		gFDVT_Irq[i] = fdvt_dev->irq[i];
		if (i == FDVT_IRQ_IDX) {
			/* IRQF_TRIGGER_NONE dose not take effect here,
			 * real trigger mode set in dts file
			 */
			ret = request_irq
			(fdvt_dev->irq[i],
			(irq_handler_t)FDVT_irq,
			IRQF_TRIGGER_NONE,
			FDVT_DEVNAME,
			NULL);
			/* request_irq
			 * (FD_IRQ_BIT_ID,
			 * (irq_handler_t)FDVT_irq,
			 * IRQF_TRIGGER_LOW,
			 * FDVT_DEVNAME,
			 * NULL)
			 */
		}
		if (ret) {
			dev_info(&dev->dev, "Unable to request IRQ, request_irq fail, i=%d, irq=%d\n",
				i, fdvt_dev->irq[i]);
			return ret;
		}
		log_inf("DT, i=%d, map_irq=%d\n", i, fdvt_dev->irq[i]);
	}

	nr_fdvt_devs = new_count;

#endif

	ret = alloc_chrdev_region(&FDVT_devno, 0, 1, FDVT_DEVNAME);

	if (ret)
		log_dbg("[FDVT_DEBUG][ERROR] Can't Get Major number for FDVT Device\n");

	FDVT_cdev = cdev_alloc();

	FDVT_cdev->owner = THIS_MODULE;
	FDVT_cdev->ops = &FDVT_fops;

	cdev_init(FDVT_cdev, &FDVT_fops);

	ret = cdev_add(FDVT_cdev, FDVT_devno, 1);
	if (ret < 0) {
		log_dbg("[FDVT_DEBUG] Attatch file operation failed\n");
		return -EFAULT;
	}

#ifndef CONFIG_OF
	/* Register Interrupt */
	if (request_irq
		(FD_IRQ_BIT_ID,
		(irq_handler_t)FDVT_irq,
		IRQF_TRIGGER_LOW,
		FDVT_DEVNAME,
		NULL) < 0)
		log_dbg("[FDVT_DEBUG][ERROR] error to request FDVT irq\n");
	else
		log_dbg("[FDVT_DEBUG] success to request FDVT irq\n");
#endif

#if LDVT_EARLY_PORTING_NO_CCF
#else
    /*CCF: Grab clock pointer (struct clk*) */
#if IS_ENABLED(CONFIG_MACH_MT6763)
	fd_clk.CG_IMGSYS_FDVT = devm_clk_get(&dev->dev, "IMG_FDVT");
#else
	fd_clk.CG_IMGSYS_FDVT = devm_clk_get(&dev->dev, "FD_CLK_CAM_FDVT");
#endif
	if (IS_ERR(fd_clk.CG_IMGSYS_FDVT)) {
		log_err("cannot get CG_IMGSYS_FDVT clock\n");
		return PTR_ERR(fd_clk.CG_IMGSYS_FDVT);
	}

#endif

	FDVT_class = class_create(THIS_MODULE, FDVT_DEVNAME);
	class_dev = (struct class_device *)device_create(FDVT_class,
							NULL,
							FDVT_devno,
							NULL,
							FDVT_DEVNAME
							);
	/* Initialize waitqueue */
	init_waitqueue_head(&g_FDVTWQ);

	fdvt_wake_lock = wakeup_source_register(NULL, "fdvt_lock_wakelock");

	log_dbg("[FDVT_DEBUG] %s Done\n", __func__);

	return 0;
}

static int FDVT_remove(struct platform_device *dev)
{
	int i4IRQ = 0;

	log_dbg("[FDVT_DEBUG] FDVT_driver_exit\n");
	FDVT_WR32(0x00000000, FDVT_INT_EN);
	g_FDVTIRQ = ioread32((void *)FDVT_INT);

	__pm_stay_awake(fdvt_wake_lock);

	mt_fdvt_clk_ctrl(0);

	__pm_relax(fdvt_wake_lock);

	device_destroy(FDVT_class, FDVT_devno);
	class_destroy(FDVT_class);

	cdev_del(FDVT_cdev);
	unregister_chrdev_region(FDVT_devno, 1);

	i4IRQ = platform_get_irq(dev, 0);
	free_irq(i4IRQ, NULL);

	/*if (pBuff) {*/
		kfree(pBuff);
		pBuff = NULL;
	/*}*/
	/*if (pread_buf) {*/
		kfree(pread_buf);
		pread_buf = NULL;
	/*}*/
	return 0;
}

static int FDVT_suspend(struct platform_device *dev, pm_message_t state)
{
	spin_lock(&g_spinLock);
	if (g_drvOpened > 0) {
		log_inf("[FDVT_DEBUG] %s\n", __func__);
		mt_fdvt_clk_ctrl(0);
		g_isSuspend = 1;
	}
	spin_unlock(&g_spinLock);
	return 0;
}

static int FDVT_resume(struct platform_device *dev)
{
	spin_lock(&g_spinLock);
	if (g_isSuspend) {
		log_inf("[FDVT_DEBUG] %s\n", __func__);
		mt_fdvt_clk_ctrl(1);
		g_isSuspend = 0;
	}
	spin_unlock(&g_spinLock);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id fdvt_of_ids[] = {
	{ .compatible = "mediatek,fdvt", },
	{}
};
#endif

static struct platform_driver FDVT_driver = {
	.driver      = {
	.name        = FDVT_DEVNAME,
	.owner       = THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = fdvt_of_ids,
#endif
	},
	.probe       = FDVT_probe,
	.remove      = FDVT_remove,
	.suspend     = FDVT_suspend,
	.resume      = FDVT_resume,
};

/* Device Tree Architecture Don't Use This Struct
 *static struct platform_device FDVT_device = {
 *	.name    = FDVT_DEVNAME,
 *	.id         = 0,
 *};
 */

#ifdef CONFIG_HAS_EARLYSUSPEND
static void FDVT_early_suspend(struct early_suspend *h)
{
	/* log_dbg("[FDVT_DEBUG] FDVT_suspend\n"); */
	/* mt_fdvt_clk_ctrl(0); */

}

static void FDVT_early_resume(struct early_suspend *h)
{
	/* log_dbg("[FDVT_DEBUG] FDVT_suspend\n"); */
	/* mt_fdvt_clk_ctrl(1); */
}

static struct early_suspend FDVT_early_suspend_desc = {
	.level		= EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend	= FDVT_early_suspend,
	.resume		= FDVT_early_resume,
};
#endif

static int __init FDVT_driver_init(void)
{
	int ret;

	log_dbg("[FDVT_DEBUG] %s\n", __func__);

	if (platform_driver_register(&FDVT_driver)) {
		log_dbg("[FDVT_DEBUG][ERROR] failed to register FDVT Driver\n");
		ret = -ENODEV;
		return ret;
	}
	#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&FDVT_early_suspend_desc);
	#endif

	log_dbg("[FDVT_DEBUG] %s Done\n", __func__);

	return 0;
}

static void __exit FDVT_driver_exit(void)
{
	log_dbg("[FDVT_DEBUG] %s\n", __func__);

	device_destroy(FDVT_class, FDVT_devno);
	class_destroy(FDVT_class);

	cdev_del(FDVT_cdev);
	unregister_chrdev_region(FDVT_devno, 1);

	platform_driver_unregister(&FDVT_driver);

	log_dbg("[FDVT_DEBUG] %s Done\n", __func__);
}


module_init(FDVT_driver_init);
module_exit(FDVT_driver_exit);
MODULE_AUTHOR("MM/MM3/SW5");
MODULE_DESCRIPTION("FDVT Driver");
MODULE_LICENSE("GPL");
