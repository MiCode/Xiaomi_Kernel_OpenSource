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
#include <linux/wait.h>
#include <linux/slab.h>
#include <mt-plat/sync_write.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "inc/camera_fdvt.h"

#include <asm/cacheflush.h>

#define FDVT_USE_GCE

#ifdef FDVT_USE_GCE
#include <cmdq_core.h>
#include <cmdq_record.h>
#define CMDQ_REG_MASK 0xffffffff
#endif

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

#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#endif

struct wakeup_source *fdvt_wake_lock;
#define FDVT_SMI_READY
#ifdef FDVT_SMI_READY
#include <smi_public.h>
#endif
#define FDVT_DEVNAME     "camera-fdvt"

#define LOG_VRB(format, args...) \
pr_debug("FDVT [%s] " format, __func__, ##args)
#define LOG_DBG(format, args...) \
pr_debug("FDVT [%s] " format, __func__, ##args)
#define LOG_INF(format, args...) \
pr_info("FDVT [%s] " format, __func__, ##args)
#define LOG_WRN(format, args...) \
pr_info("FDVT [%s] WARNING: " format, __func__, ##args)
#define LOG_ERR(format, args...) \
pr_info("FDVT [%s, line%04d] ERROR: " format, __func__, __LINE__, ##args)
#define LOG_AST(format, args...) \
pr_info("FDVT [%s, line%04d] ASSERT: " format, __func__, __LINE__, ##args)

#define LDVT_EARLY_PORTING_NO_CCF 0
#if LDVT_EARLY_PORTING_NO_CCF
void __iomem *IMGSYS_CONFIG_BASE;
void __iomem *CAMSYS_CONFIG_BASE;
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

#define FDVT_BASE_HW   0x1502B000

static FDVTDBuffRegMap pFDVTReadBuffer;
static FDVTDBuffRegMap pFDVTWriteBuffer;
static FDVTSecureMeta  g_fdvt_secmeta;

/* register map */
#define FDVT_START                 (FDVT_ADDR+0x0)
#define FDVT_ENABLE                (FDVT_ADDR+0x4)
#define FDVT_RS                    (FDVT_ADDR+0x8)
#define FDVT_RSCON_BASE_ADR        (FDVT_ADDR+0xC)
#define FDVT_RGB2Y0                (FDVT_ADDR+0x10)
#define FDVT_RGB2Y1                (FDVT_ADDR+0x14)
#define FDVT_INVG0                 (FDVT_ADDR+0x18)
#define FDVT_INVG1                 (FDVT_ADDR+0x1C)
#define FDVT_INVG2                 (FDVT_ADDR+0x20)
#define FDVT_FNUM_0                (FDVT_ADDR+0x24)
#define FDVT_FNUM_1                (FDVT_ADDR+0x28)
#define FDVT_FNUM_2                (FDVT_ADDR+0x2C)
#define FDVT_FNUM_3                (FDVT_ADDR+0x30)
#define FDVT_T_FNUM_4              (FDVT_ADDR+0x34)
#define FDVT_T_FNUM_5              (FDVT_ADDR+0x38)
#define FDVT_T_FNUM_6              (FDVT_ADDR+0x3C)
#define FDVT_T_FNUM_7              (FDVT_ADDR+0x40)
#define FDVT_FF_NUM_0              (FDVT_ADDR+0x44)
#define FDVT_FF_NUM_1              (FDVT_ADDR+0x48)
#define FDVT_FF_NUM_2              (FDVT_ADDR+0x4C)
#define FDVT_FF_NUM_3              (FDVT_ADDR+0x50)
#define FDVT_FF_BASE_ADR_0         (FDVT_ADDR+0x54)
#define FDVT_FF_BASE_ADR_1         (FDVT_ADDR+0x58)
#define FDVT_FF_BASE_ADR_2         (FDVT_ADDR+0x5C)
#define FDVT_FF_BASE_ADR_3         (FDVT_ADDR+0x60)
#define FDVT_FF_BASE_ADR_4         (FDVT_ADDR+0x64)
#define FDVT_FF_BASE_ADR_5         (FDVT_ADDR+0x68)
#define FDVT_FF_BASE_ADR_6         (FDVT_ADDR+0x6C)
#define FDVT_FF_BASE_ADR_7         (FDVT_ADDR+0x70)
#define FDVT_RMAP_0                (FDVT_ADDR+0x74)
#define FDVT_RMAP_1                (FDVT_ADDR+0x78)
#define FDVT_FD                    (FDVT_ADDR+0x7C)
#define FDVT_FD_CON_BASE_ADR       (FDVT_ADDR+0x80)
#define FDVT_GFD                   (FDVT_ADDR+0x84)
#define FDVT_LFD                   (FDVT_ADDR+0x88)
#define FDVT_GFD_POS_0             (FDVT_ADDR+0x8C)
#define FDVT_GFD_POS_1             (FDVT_ADDR+0x90)
#define FDVT_GFD_DET0              (FDVT_ADDR+0x94)
#define FDVT_GFD_DET1              (FDVT_ADDR+0x98)
#define FDVT_FD_RLT_BASE_ADR       (FDVT_ADDR+0x9C)
#define FDVT_LFD_INFO_CTRL_0       (FDVT_ADDR+0xA4)
#define FDVT_LFD_INFO_XPOS_0       (FDVT_ADDR+0xA8)
#define FDVT_LFD_INFO_YPOS_0       (FDVT_ADDR+0xAC)
#define FDVT_LFD_INFO_CTRL_1       (FDVT_ADDR+0xB0)
#define FDVT_LFD_INFO_XPOS_1       (FDVT_ADDR+0xB4)
#define FDVT_LFD_INFO_YPOS_1       (FDVT_ADDR+0xB8)
#define FDVT_LFD_INFO_CTRL_2       (FDVT_ADDR+0xBC)
#define FDVT_LFD_INFO_XPOS_2       (FDVT_ADDR+0xC0)
#define FDVT_LFD_INFO_YPOS_2       (FDVT_ADDR+0xC4)
#define FDVT_LFD_INFO_CTRL_3       (FDVT_ADDR+0xC8)
#define FDVT_LFD_INFO_XPOS_3       (FDVT_ADDR+0xCC)
#define FDVT_LFD_INFO_YPOS_3       (FDVT_ADDR+0xD0)
#define FDVT_LFD_INFO_CTRL_4       (FDVT_ADDR+0xD4)
#define FDVT_LFD_INFO_XPOS_4       (FDVT_ADDR+0xD8)
#define FDVT_LFD_INFO_YPOS_4       (FDVT_ADDR+0xDC)
#define FDVT_LFD_INFO_CTRL_5       (FDVT_ADDR+0xE0)
#define FDVT_LFD_INFO_XPOS_5       (FDVT_ADDR+0xE4)
#define FDVT_LFD_INFO_YPOS_5       (FDVT_ADDR+0xE8)
#define FDVT_LFD_INFO_CTRL_6       (FDVT_ADDR+0xEC)
#define FDVT_LFD_INFO_XPOS_6       (FDVT_ADDR+0xF0)
#define FDVT_LFD_INFO_YPOS_6       (FDVT_ADDR+0xF4)
#define FDVT_LFD_INFO_CTRL_7       (FDVT_ADDR+0xF8)
#define FDVT_LFD_INFO_XPOS_7       (FDVT_ADDR+0xFC)
#define FDVT_LFD_INFO_YPOS_7       (FDVT_ADDR+0x100)
#define FDVT_LFD_INFO_CTRL_8       (FDVT_ADDR+0x104)
#define FDVT_LFD_INFO_XPOS_8       (FDVT_ADDR+0x108)
#define FDVT_LFD_INFO_YPOS_8       (FDVT_ADDR+0x10C)
#define FDVT_LFD_INFO_CTRL_9       (FDVT_ADDR+0x110)
#define FDVT_LFD_INFO_XPOS_9       (FDVT_ADDR+0x114)
#define FDVT_LFD_INFO_YPOS_9       (FDVT_ADDR+0x118)
#define FDVT_LFD_INFO_CTRL_10      (FDVT_ADDR+0x11C)
#define FDVT_LFD_INFO_XPOS_10      (FDVT_ADDR+0x120)
#define FDVT_LFD_INFO_YPOS_10      (FDVT_ADDR+0x124)
#define FDVT_LFD_INFO_CTRL_11      (FDVT_ADDR+0x128)
#define FDVT_LFD_INFO_XPOS_11      (FDVT_ADDR+0x12C)
#define FDVT_LFD_INFO_YPOS_11      (FDVT_ADDR+0x130)
#define FDVT_LFD_INFO_CTRL_12      (FDVT_ADDR+0x134)
#define FDVT_LFD_INFO_XPOS_12      (FDVT_ADDR+0x138)
#define FDVT_LFD_INFO_YPOS_12      (FDVT_ADDR+0x13C)
#define FDVT_LFD_INFO_CTRL_13      (FDVT_ADDR+0x140)
#define FDVT_LFD_INFO_XPOS_13      (FDVT_ADDR+0x144)
#define FDVT_LFD_INFO_YPOS_13      (FDVT_ADDR+0x148)
#define FDVT_LFD_INFO_CTRL_14      (FDVT_ADDR+0x14C)
#define FDVT_LFD_INFO_XPOS_14      (FDVT_ADDR+0x150)
#define FDVT_LFD_INFO_YPOS_14      (FDVT_ADDR+0x154)
#define FDVT_TC_ENABLE_RESULT      (FDVT_ADDR+0x158)
#define FDVT_INT_EN                (FDVT_ADDR+0x15C)
#define FDVT_SRC_WD_HT             (FDVT_ADDR+0x160)
#define FDVT_INT                   (FDVT_ADDR+0x168)
#define FDVT_DEBUG_INFO_1          (FDVT_ADDR+0x16C)
#define FDVT_DEBUG_INFO_2          (FDVT_ADDR+0x170)
#define FDVT_DEBUG_INFO_3          (FDVT_ADDR+0x174)
#define FDVT_RESULT                (FDVT_ADDR+0x178)
#define FDVT_SPARE_CELL	           (FDVT_ADDR+0x198)
#define FDVT_CTRL                  (FDVT_ADDR+0x19C)
#define FDVT_VERSION               (FDVT_ADDR+0x1A0)

#define FDVT_FF_NUM_4              (FDVT_ADDR+0x1C4)
#define FDVT_FF_NUM_5              (FDVT_ADDR+0x1C8)
#define FDVT_FF_NUM_6              (FDVT_ADDR+0x1CC)
#define FDVT_FF_NUM_7              (FDVT_ADDR+0x1D0)
#define FDVT_FF_NUM_8              (FDVT_ADDR+0x1D4)

#define FDVT_FF_BASE_ADR_8         (FDVT_ADDR+0x1A4)
#define FDVT_FF_BASE_ADR_9         (FDVT_ADDR+0x1A8)
#define FDVT_FF_BASE_ADR_10        (FDVT_ADDR+0x1AC)
#define FDVT_FF_BASE_ADR_11        (FDVT_ADDR+0x1B0)
#define FDVT_FF_BASE_ADR_12        (FDVT_ADDR+0x1B4)
#define FDVT_FF_BASE_ADR_13        (FDVT_ADDR+0x1B8)
#define FDVT_FF_BASE_ADR_14        (FDVT_ADDR+0x1BC)
#define FDVT_FF_BASE_ADR_15        (FDVT_ADDR+0x1C0)
#define FDVT_FF_BASE_ADR_16        (FDVT_ADDR+0x1D8)
#define FDVT_FF_BASE_ADR_17        (FDVT_ADDR+0x1DC)

/* register map for HW usage*/
#define FDVT_START_HW              (FDVT_BASE_HW+0x0)
#define FDVT_ENABLE_HW             (FDVT_BASE_HW+0x4)
#define FDVT_RS_HW                 (FDVT_BASE_HW+0x8)
#define FDVT_RSCON_BASE_ADR_HW     (FDVT_BASE_HW+0xC)
#define FDVT_RGB2Y0_HW             (FDVT_BASE_HW+0x10)
#define FDVT_RGB2Y1_HW             (FDVT_BASE_HW+0x14)
#define FDVT_INVG0_HW              (FDVT_BASE_HW+0x18)
#define FDVT_INVG1_HW              (FDVT_BASE_HW+0x1C)
#define FDVT_INVG2_HW              (FDVT_BASE_HW+0x20)
#define FDVT_FNUM_0_HW             (FDVT_BASE_HW+0x24)
#define FDVT_FNUM_1_HW             (FDVT_BASE_HW+0x28)
#define FDVT_FNUM_2_HW             (FDVT_BASE_HW+0x2C)
#define FDVT_FNUM_3_HW             (FDVT_BASE_HW+0x30)
#define FDVT_T_FNUM_4_HW           (FDVT_BASE_HW+0x34)
#define FDVT_T_FNUM_5_HW           (FDVT_BASE_HW+0x38)
#define FDVT_T_FNUM_6_HW           (FDVT_BASE_HW+0x3C)
#define FDVT_T_FNUM_7_HW           (FDVT_BASE_HW+0x40)
#define FDVT_FF_NUM_0_HW           (FDVT_BASE_HW+0x44)
#define FDVT_FF_NUM_1_HW           (FDVT_BASE_HW+0x48)
#define FDVT_FF_NUM_2_HW           (FDVT_BASE_HW+0x4C)
#define FDVT_FF_NUM_3_HW           (FDVT_BASE_HW+0x50)
#define FDVT_FF_BASE_ADR_0_HW      (FDVT_BASE_HW+0x54)
#define FDVT_FF_BASE_ADR_1_HW      (FDVT_BASE_HW+0x58)
#define FDVT_FF_BASE_ADR_2_HW      (FDVT_BASE_HW+0x5C)
#define FDVT_FF_BASE_ADR_3_HW      (FDVT_BASE_HW+0x60)
#define FDVT_FF_BASE_ADR_4_HW      (FDVT_BASE_HW+0x64)
#define FDVT_FF_BASE_ADR_5_HW      (FDVT_BASE_HW+0x68)
#define FDVT_FF_BASE_ADR_6_HW      (FDVT_BASE_HW+0x6C)
#define FDVT_FF_BASE_ADR_7_HW      (FDVT_BASE_HW+0x70)
#define FDVT_RMAP_0_HW             (FDVT_BASE_HW+0x74)
#define FDVT_RMAP_1_HW             (FDVT_BASE_HW+0x78)
#define FDVT_FD_HW                 (FDVT_BASE_HW+0x7C)
#define FDVT_FD_CON_BASE_ADR_HW    (FDVT_BASE_HW+0x80)
#define FDVT_GFD_HW                (FDVT_BASE_HW+0x84)
#define FDVT_LFD_HW                (FDVT_BASE_HW+0x88)
#define FDVT_GFD_POS_0_HW          (FDVT_BASE_HW+0x8C)
#define FDVT_GFD_POS_1_HW          (FDVT_BASE_HW+0x90)
#define FDVT_GFD_DET0_HW           (FDVT_BASE_HW+0x94)
#define FDVT_GFD_DET1_HW           (FDVT_BASE_HW+0x98)
#define FDVT_FD_RLT_BASE_ADR_HW    (FDVT_BASE_HW+0x9C)
#define FDVT_LFD_INFO_CTRL_0_HW    (FDVT_BASE_HW+0xA4)
#define FDVT_LFD_INFO_XPOS_0_HW    (FDVT_BASE_HW+0xA8)
#define FDVT_LFD_INFO_YPOS_0_HW    (FDVT_BASE_HW+0xAC)
#define FDVT_LFD_INFO_CTRL_1_HW    (FDVT_BASE_HW+0xB0)
#define FDVT_LFD_INFO_XPOS_1_HW    (FDVT_BASE_HW+0xB4)
#define FDVT_LFD_INFO_YPOS_1_HW    (FDVT_BASE_HW+0xB8)
#define FDVT_LFD_INFO_CTRL_2_HW    (FDVT_BASE_HW+0xBC)
#define FDVT_LFD_INFO_XPOS_2_HW    (FDVT_BASE_HW+0xC0)
#define FDVT_LFD_INFO_YPOS_2_HW    (FDVT_BASE_HW+0xC4)
#define FDVT_LFD_INFO_CTRL_3_HW    (FDVT_BASE_HW+0xC8)
#define FDVT_LFD_INFO_XPOS_3_HW    (FDVT_BASE_HW+0xCC)
#define FDVT_LFD_INFO_YPOS_3_HW    (FDVT_BASE_HW+0xD0)
#define FDVT_LFD_INFO_CTRL_4_HW    (FDVT_BASE_HW+0xD4)
#define FDVT_LFD_INFO_XPOS_4_HW    (FDVT_BASE_HW+0xD8)
#define FDVT_LFD_INFO_YPOS_4_HW    (FDVT_BASE_HW+0xDC)
#define FDVT_LFD_INFO_CTRL_5_HW    (FDVT_BASE_HW+0xE0)
#define FDVT_LFD_INFO_XPOS_5_HW    (FDVT_BASE_HW+0xE4)
#define FDVT_LFD_INFO_YPOS_5_HW    (FDVT_BASE_HW+0xE8)
#define FDVT_LFD_INFO_CTRL_6_HW    (FDVT_BASE_HW+0xEC)
#define FDVT_LFD_INFO_XPOS_6_HW    (FDVT_BASE_HW+0xF0)
#define FDVT_LFD_INFO_YPOS_6_HW    (FDVT_BASE_HW+0xF4)
#define FDVT_LFD_INFO_CTRL_7_HW    (FDVT_BASE_HW+0xF8)
#define FDVT_LFD_INFO_XPOS_7_HW    (FDVT_BASE_HW+0xFC)
#define FDVT_LFD_INFO_YPOS_7_HW    (FDVT_BASE_HW+0x100)
#define FDVT_LFD_INFO_CTRL_8_HW    (FDVT_BASE_HW+0x104)
#define FDVT_LFD_INFO_XPOS_8_HW    (FDVT_BASE_HW+0x108)
#define FDVT_LFD_INFO_YPOS_8_HW    (FDVT_BASE_HW+0x10C)
#define FDVT_LFD_INFO_CTRL_9_HW    (FDVT_BASE_HW+0x110)
#define FDVT_LFD_INFO_XPOS_9_HW    (FDVT_BASE_HW+0x114)
#define FDVT_LFD_INFO_YPOS_9_HW    (FDVT_BASE_HW+0x118)
#define FDVT_LFD_INFO_CTRL_10_HW   (FDVT_BASE_HW+0x11C)
#define FDVT_LFD_INFO_XPOS_10_HW   (FDVT_BASE_HW+0x120)
#define FDVT_LFD_INFO_YPOS_10_HW   (FDVT_BASE_HW+0x124)
#define FDVT_LFD_INFO_CTRL_11_HW   (FDVT_BASE_HW+0x128)
#define FDVT_LFD_INFO_XPOS_11_HW   (FDVT_BASE_HW+0x12C)
#define FDVT_LFD_INFO_YPOS_11_HW   (FDVT_BASE_HW+0x130)
#define FDVT_LFD_INFO_CTRL_12_HW   (FDVT_BASE_HW+0x134)
#define FDVT_LFD_INFO_XPOS_12_HW   (FDVT_BASE_HW+0x138)
#define FDVT_LFD_INFO_YPOS_12_HW   (FDVT_BASE_HW+0x13C)
#define FDVT_LFD_INFO_CTRL_13_HW   (FDVT_BASE_HW+0x140)
#define FDVT_LFD_INFO_XPOS_13_HW   (FDVT_BASE_HW+0x144)
#define FDVT_LFD_INFO_YPOS_13_HW   (FDVT_BASE_HW+0x148)
#define FDVT_LFD_INFO_CTRL_14_HW   (FDVT_BASE_HW+0x14C)
#define FDVT_LFD_INFO_XPOS_14_HW   (FDVT_BASE_HW+0x150)
#define FDVT_LFD_INFO_YPOS_14_HW   (FDVT_BASE_HW+0x154)
#define FDVT_TC_ENABLE_RESULT_HW   (FDVT_BASE_HW+0x158)
#define FDVT_INT_EN_HW             (FDVT_BASE_HW+0x15C)
#define FDVT_SRC_WD_HT_HW          (FDVT_BASE_HW+0x160)
#define FDVT_INT_HW                (FDVT_BASE_HW+0x168)
#define FDVT_DEBUG_INFO_1_HW       (FDVT_BASE_HW+0x16C)
#define FDVT_DEBUG_INFO_2_HW       (FDVT_BASE_HW+0x170)
#define FDVT_DEBUG_INFO_3_HW       (FDVT_BASE_HW+0x174)
#define FDVT_RESULT_HW             (FDVT_BASE_HW+0x178)
#define FDVT_SPARE_CELL_HW	   (FDVT_BASE_HW+0x198)
#define FDVT_CTRL_HW               (FDVT_BASE_HW+0x19C)
#define FDVT_VERSION_HW            (FDVT_BASE_HW+0x1A0)

#define FDVT_FF_NUM_4_HW           (FDVT_BASE_HW+0x1C4)
#define FDVT_FF_NUM_5_HW           (FDVT_BASE_HW+0x1C8)
#define FDVT_FF_NUM_6_HW           (FDVT_BASE_HW+0x1CC)
#define FDVT_FF_NUM_7_HW           (FDVT_BASE_HW+0x1D0)
#define FDVT_FF_NUM_8_HW           (FDVT_BASE_HW+0x1D4)

#define FDVT_FF_BASE_ADR_8_HW      (FDVT_BASE_HW+0x1A4)
#define FDVT_FF_BASE_ADR_9_HW      (FDVT_BASE_HW+0x1A8)
#define FDVT_FF_BASE_ADR_10_HW     (FDVT_BASE_HW+0x1AC)
#define FDVT_FF_BASE_ADR_11_HW     (FDVT_BASE_HW+0x1B0)
#define FDVT_FF_BASE_ADR_12_HW     (FDVT_BASE_HW+0x1B4)
#define FDVT_FF_BASE_ADR_13_HW     (FDVT_BASE_HW+0x1B8)
#define FDVT_FF_BASE_ADR_14_HW     (FDVT_BASE_HW+0x1BC)
#define FDVT_FF_BASE_ADR_15_HW     (FDVT_BASE_HW+0x1C0)
#define FDVT_FF_BASE_ADR_16_HW     (FDVT_BASE_HW+0x1D8)
#define FDVT_FF_BASE_ADR_17_HW     (FDVT_BASE_HW+0x1DC)

#define FDVT_MAX_OFFSET            0x1DC

#ifdef CONFIG_OF
struct fdvt_device {
	void __iomem *regs[FDVT_BASEADDR_NUM];
	struct device *dev;
	int irq[FDVT_IRQ_IDX_NUM];
};

static struct fdvt_device *fdvt_devs;
static int nr_fdvt_devs;
#endif

bool haveConfig;
bool g_isSecure;

void FDVT_basic_config(void)
{
	FDVT_WR32(0x00000111, FDVT_ENABLE);
	FDVT_WR32(0x0000040C, FDVT_RS);
	FDVT_WR32(0x0C000000, FDVT_RSCON_BASE_ADR);
	FDVT_WR32(0x02590132, FDVT_RGB2Y0);
	FDVT_WR32(0x00000075, FDVT_RGB2Y1);
	FDVT_WR32(0x66553520, FDVT_INVG0);
	FDVT_WR32(0xB8A28D79, FDVT_INVG1);
	FDVT_WR32(0xFFF4E7CF, FDVT_INVG2);
	FDVT_WR32(0x02A402EE, FDVT_FNUM_0);
	FDVT_WR32(0x02A402EE, FDVT_FNUM_1);
	FDVT_WR32(0x0DAC0DAC, FDVT_FNUM_2);
	FDVT_WR32(0x0DAC02A4, FDVT_FNUM_3);
	FDVT_WR32(0x00000000, FDVT_T_FNUM_4);
	FDVT_WR32(0x00000000, FDVT_T_FNUM_5);
	FDVT_WR32(0x00000000, FDVT_T_FNUM_6);
	FDVT_WR32(0x00000000, FDVT_T_FNUM_7);
	FDVT_WR32(0x02EE02A4, FDVT_FF_NUM_0);
	FDVT_WR32(0x02A402A4, FDVT_FF_NUM_1);
	FDVT_WR32(0x02A402EE, FDVT_FF_NUM_2);
	FDVT_WR32(0x02EE02A4, FDVT_FF_NUM_3);
	FDVT_WR32(0x000002A4, FDVT_FF_NUM_4);
	FDVT_WR32(0x00000000, FDVT_FF_NUM_5);
	FDVT_WR32(0x00000000, FDVT_FF_NUM_6);
	FDVT_WR32(0x00000000, FDVT_FF_NUM_7);
	FDVT_WR32(0x00000000, FDVT_FF_NUM_8);
	FDVT_WR32(0x0D000000, FDVT_FF_BASE_ADR_0);
	FDVT_WR32(0x0D010000, FDVT_FF_BASE_ADR_1);
	FDVT_WR32(0x0D020000, FDVT_FF_BASE_ADR_2);
	FDVT_WR32(0x0D030000, FDVT_FF_BASE_ADR_3);
	FDVT_WR32(0x0D040000, FDVT_FF_BASE_ADR_4);
	FDVT_WR32(0x0D050000, FDVT_FF_BASE_ADR_5);
	FDVT_WR32(0x0D060000, FDVT_FF_BASE_ADR_6);
	FDVT_WR32(0x0D070000, FDVT_FF_BASE_ADR_7);
	FDVT_WR32(0x0D080000, FDVT_FF_BASE_ADR_8);
	FDVT_WR32(0x0D090000, FDVT_FF_BASE_ADR_9);
	FDVT_WR32(0x0D0A0000, FDVT_FF_BASE_ADR_10);
	FDVT_WR32(0x0D0B0000, FDVT_FF_BASE_ADR_11);
	FDVT_WR32(0x0D0C0000, FDVT_FF_BASE_ADR_12);
	FDVT_WR32(0x0D0D0000, FDVT_FF_BASE_ADR_13);
	FDVT_WR32(0x0D0E0000, FDVT_FF_BASE_ADR_14);
	FDVT_WR32(0x0D0F0000, FDVT_FF_BASE_ADR_15);
	FDVT_WR32(0x0D110000, FDVT_FF_BASE_ADR_16);
	FDVT_WR32(0x0D120000, FDVT_FF_BASE_ADR_17);
	FDVT_WR32(0x00000000, FDVT_RMAP_0);
	FDVT_WR32(0x00000001, FDVT_RMAP_1);
	FDVT_WR32(0x0400000C, FDVT_FD);
	FDVT_WR32(0x0D100000, FDVT_FD_CON_BASE_ADR);
	FDVT_WR32(0x00000000, FDVT_GFD);
	FDVT_WR32(0x01130000, FDVT_LFD);
	FDVT_WR32(0x00000000, FDVT_GFD_POS_0);
	FDVT_WR32(0x012C0190, FDVT_GFD_POS_1);
	FDVT_WR32(0x00000207, FDVT_GFD_DET0);
	FDVT_WR32(0x00000000, FDVT_GFD_DET1);
	FDVT_WR32(0x11223300, FDVT_FD_RLT_BASE_ADR);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_0);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_0);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_0);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_1);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_1);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_1);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_2);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_2);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_2);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_3);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_3);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_3);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_4);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_4);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_4);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_5);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_5);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_5);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_6);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_6);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_6);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_7);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_7);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_7);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_8);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_8);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_8);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_9);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_9);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_9);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_10);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_10);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_10);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_11);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_11);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_11);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_12);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_12);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_12);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_13);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_13);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_13);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_CTRL_14);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_XPOS_14);
	FDVT_WR32(0x00000000, FDVT_LFD_INFO_YPOS_14);
	FDVT_WR32(0x00000000, FDVT_INT_EN);
	FDVT_WR32(0x0190012C, FDVT_SRC_WD_HT);
}

/***********************************************************
 * Clock to ms
 ************************************************************/

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

	/* smi_bus_enable(SMI_LARB_IMGSYS1, "camera_fdvt"); */
	smi_bus_prepare_enable(SMI_LARB2, "camera_fdvt");

	ret = clk_prepare_enable(fd_clk.CG_IMGSYS_FDVT);
	if (ret)
		LOG_ERR("cannot prepare and enable CG_IMGSYS_FDVT clock\n");


}

static inline void FD_Disable_Unprepare_ccf_clock(void)
{
	clk_disable_unprepare(fd_clk.CG_IMGSYS_FDVT);
	/* smi_bus_disable(SMI_LARB_IMGSYS1, "camera_fdvt"); */
	smi_bus_disable_unprepare(SMI_LARB2, "camera_fdvt");
}
#endif
#endif

static int mt_fdvt_clk_ctrl(int en)
{
#if LDVT_EARLY_PORTING_NO_CCF
	if (en) {
		unsigned int setReg = 0xFFFFFFFF;

		FDVT_WR32(setReg, CAMSYS_CONFIG_BASE+0x8);
		FDVT_WR32(setReg, IMGSYS_CONFIG_BASE+0x8);
	} else {
		unsigned int setReg = 0xFFFFFFFF;

		FDVT_WR32(setReg, IMGSYS_CONFIG_BASE+0x4);
		FDVT_WR32(setReg, CAMSYS_CONFIG_BASE+0x4);
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

/*****************************************************************************
 *
 *****************************************************************************/
#if (MTK_SECURE_FD_SUPPORT == 1)
static inline int FDVT_switchCmdqToSecure(void *handle)
{
	enum CMDQ_ENG_ENUM cmdq_engine;
	/*enum CMDQ_EVENT_ENUM cmdq_event;*/

	/*cmdq_engine = module_to_cmdq_engine(module);*/
	cmdq_engine = CMDQ_ENG_FDVT;
	/*cmdq_event	= CMDQ_EVENT_DVE_EOF;*/

	cmdqRecSetSecure(handle, 1);
	/* set engine as sec */
	cmdqRecSecureEnablePortSecurity(handle, (1LL << cmdq_engine));
	//cmdqRecSecureEnableDAPC(handle, (1LL << cmdq_engine));
	/* Set fdvt with mtee Task */
	cmdq_task_set_mtee(handle, true);

	return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/

static inline int FDVT_switchPortToNonSecure(void)
{
	struct cmdqRecStruct *handle;
	uint64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_FDVT);

	cmdq_task_create(CMDQ_SCENARIO_ISP_FDVT_OFF, &handle);
	cmdq_task_set_secure(handle, true);
	cmdqRecSetEngine(handle, engineFlag);
	//cmdq_task_secure_enable_dapc(handle, engineFlag);
	cmdq_task_secure_enable_port_security(handle, engineFlag);
	/* Set fdvt with mtee Task */
	cmdq_task_set_mtee(handle, true);
	cmdq_task_flush(handle);

	return 0;
}
#endif
/***********************************************************
 * Set FDVT Meta Data
 ************************************************************/
static int FDVT_SetMetaData(FDVTMetaData *pMetaData)
{
	int ret = 0;
	/* int i = 0; */
	/* FDVTMetaData fdvt_meta; */

	if (pMetaData == NULL) {
		LOG_DBG("Null input argrment\n");
		return -EINVAL;
	}

	if (copy_from_user(&g_fdvt_secmeta, (void *)pMetaData->SecureMeta,
		sizeof(FDVTSecureMeta)) != 0) {
		LOG_DBG("secmeta copy_from_user failed\n");
		return -EFAULT;
	}

	/*
	 *LOG_INF("SecureMeta->fd_mode: %d\n", g_fdvt_secmeta.fd_mode);
	 *for (i = 0; i < 15; i ++) {
	 *LOG_INF("SecureMeta->source img width/height[%d]: (%d, %d)\n",
	 *	i,
	 *	g_fdvt_secmeta.source_img_width[i],
	 *	g_fdvt_secmeta.source_img_height[i]);
	 *}
	 *LOG_INF("SecureMeta->Learning_Type: %d\n",
	 *	g_fdvt_secmeta.Learning_Type);
	 *LOG_INF("SecureMeta->RIP_feature: %d\n",
	 *	g_fdvt_secmeta.RIP_feature);
	 *LOG_INF("SecureMeta->GFD_skip: %d\n",
	 *	g_fdvt_secmeta.GFD_skip);
	 *LOG_INF("SecureMeta->GFD_skip_V: %d\n",
	 *	g_fdvt_secmeta.GFD_skip_V);
	 *LOG_INF("SecureMeta->feature_threshold: %d\n",
	 *	g_fdvt_secmeta.feature_threshold);
	 *LOG_INF("SecureMeta->source_img_fmt: %d\n",
	 *	g_fdvt_secmeta.source_img_fmt);
	 *LOG_INF("SecureMeta->scale_from_original: %d\n",
	 *	g_fdvt_secmeta.scale_from_original);
	 *LOG_INF("SecureMeta->scale_manual_mode: %d\n",
	 *	g_fdvt_secmeta.scale_manual_mode);
	 *LOG_INF("SecureMeta->scale_num_from_user: %d\n",
	 *	g_fdvt_secmeta.scale_num_from_user);
	 *LOG_INF("SecureMeta->SecMemType: %d\n",
	 *	g_fdvt_secmeta.SecMemType);
	 *LOG_INF("SecureMeta->ImgSrcY_Handler: 0x%x, Size: %d\n",
	 *	g_fdvt_secmeta.ImgSrcY_Handler,
	 *	g_fdvt_secmeta.ImgSrc_Y_Size);
	 *LOG_INF("SecureMeta->ImgSrcUV_Handler: 0x%x, Size: %d\n",
	 *	g_fdvt_secmeta.ImgSrcUV_Handler,
	 *	g_fdvt_secmeta.ImgSrc_UV_Size);
	 *LOG_INF("SecureMeta->RSConfig_Handler: 0x%x, Size: %d\n",
	 *	g_fdvt_secmeta.RSConfig_Handler,
	 *	g_fdvt_secmeta.RSConfigSize);
	 *LOG_INF("SecureMeta->FDConfig_Handler: 0x%x, Size: %d\n",
	 *	g_fdvt_secmeta.FDConfig_Handler,
	 *	g_fdvt_secmeta.FDConfigSize);
	 *LOG_INF("SecureMeta->FDResultBuf_PA: 0x%llx, Size: %d\n",
	 *	g_fdvt_secmeta.FDResultBuf_PA,
	 *	g_fdvt_secmeta.FDResultBufSize);
	 *for (i = 0; i < 18; i ++) {
	 *LOG_INF("SecureMeta->Learning_Data_Handler[%d]: 0x%x\n",
	 *	i,
	 *	g_fdvt_secmeta.Learning_Data_Handler[i]);
	 *LOG_INF("SecureMeta->Learning_Data_Handler_Extra[%d]: 0x%x\n",
	 *	i,
	 *	g_fdvt_secmeta.Extra_Learning_Data_Handler[i]);
	 *LOG_INF("SecureMeta->Learning_Data_Size[%d]: %d\n",
	 *	i,
	 *	g_fdvt_secmeta.Learning_Data_Size[i]);
	 *LOG_INF("SecureMeta->dynamic_change_model[%d]: %d\n",
	 *	i,
	 *	g_fdvt_secmeta.dynamic_change_model[i]);
	 *}
	 */

	return ret;
}

/*=======================================================================*/
/* FDVT FD Config Registers */
/*=======================================================================*/

void FaceDetecteConfig(void)
{
	FDVT_WR32(0x02A402EE, FDVT_FNUM_0);
	FDVT_WR32(0x02A402EE, FDVT_FNUM_1);
	FDVT_WR32(0x0DAC0DAC, FDVT_FNUM_2);
	FDVT_WR32(0x0DAC02A4, FDVT_FNUM_3);
	FDVT_WR32(0x00000000, FDVT_T_FNUM_4);
	FDVT_WR32(0x00000000, FDVT_T_FNUM_5);
	FDVT_WR32(0x00000000, FDVT_T_FNUM_6);
	FDVT_WR32(0x00000000, FDVT_T_FNUM_7);
	FDVT_WR32(0x02EE02A4, FDVT_FF_NUM_0);
	FDVT_WR32(0x02A402A4, FDVT_FF_NUM_1);
	FDVT_WR32(0x02A402EE, FDVT_FF_NUM_2);
	FDVT_WR32(0x02EE02A4, FDVT_FF_NUM_3);
	FDVT_WR32(0x00C802A4, FDVT_FF_NUM_4);/*0x000002A4*/
	FDVT_WR32(0x00640064, FDVT_FF_NUM_5);/*0x00000000*/
	FDVT_WR32(0x00DC00DC, FDVT_FF_NUM_6);/*0x00000000*/
	FDVT_WR32(0x00000000, FDVT_FF_NUM_7);
	FDVT_WR32(0x00000000, FDVT_FF_NUM_8);
	/* FDVT_WR32(0x000F010B,FDVT_FD);   //LDVT Disable */
}
/*=======================================================================*/
/* FDVT SD Config Registers */
/*=======================================================================*/
void SmileDetecteConfig(void)
{
	FDVT_WR32(0x01210171, FDVT_FNUM_0);
	FDVT_WR32(0x00D10081, FDVT_FNUM_1);
	FDVT_WR32(0x00F100D1, FDVT_FNUM_2);
	FDVT_WR32(0x00F100D1, FDVT_FNUM_3);
	FDVT_WR32(0x00E70127, FDVT_T_FNUM_4);
	FDVT_WR32(0x00A70067, FDVT_T_FNUM_5);
	FDVT_WR32(0x00C000A7, FDVT_T_FNUM_6);
	FDVT_WR32(0x00C000A7, FDVT_T_FNUM_7);
	FDVT_WR32(0x00180018, FDVT_FF_NUM_0);
	FDVT_WR32(0x00180018, FDVT_FF_NUM_1);
	FDVT_WR32(0x00180018, FDVT_FF_NUM_2);
	FDVT_WR32(0x00180018, FDVT_FF_NUM_3);
	FDVT_WR32(0x000F010B, FDVT_FD);
}

/*=======================================================================*/
/* FDVT Dump Registers */
/*=======================================================================*/
void FDVT_DUMPREG(void)
{
	unsigned int u4RegValue = 0;
	unsigned int u4Index = 0;

	LOG_INF("FDVT REG:\n ********************\n");

	/* for(u4Index = 0; u4Index < 0x180; u4Index += 4) { */
	for (u4Index = 0x0; u4Index < FDVT_MAX_OFFSET; u4Index += 4) {
		u4RegValue = ioread32((void *)(FDVT_ADDR + u4Index));
		LOG_INF("+0x%x 0x%x\n", u4Index, u4RegValue);
	}
}

/*=======================================================================*/
/* FDVT set reg to HW buffer */
/*=======================================================================*/
static int FDVT_SetRegHW(FDVTRegIO *a_pstCfg)
{
	#ifdef FDVT_USE_GCE
		struct cmdqRecStruct *handle;
		uint64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_FDVT);
	#endif
	FDVTRegIO *pREGIO = NULL;
	u32 i = 0;

	if (a_pstCfg == NULL) {
		LOG_DBG("Null input argrment\n");
		return -EINVAL;
	}

	pREGIO = (FDVTRegIO *)a_pstCfg;

	if (pREGIO == NULL) {
		LOG_DBG("pREGIO is NULL!\n");
		return -EFAULT;
	}

	if ((pREGIO->u4Count == 0) || (pREGIO->u4Count > FDVT_DRAM_REGCNT)) {
		LOG_DBG("Abnormal Register Count!\n");
		return -EFAULT;
	}

	if (copy_from_user(
		(void *)pFDVTWriteBuffer.u4Addr,
		(void *) pREGIO->pAddr,
		pREGIO->u4Count * sizeof(u32))) {
		LOG_DBG("ioctl copy from user failed\n");
		return -EFAULT;
	}

	if (copy_from_user(
		(void *)pFDVTWriteBuffer.u4Data,
		(void *) pREGIO->pData,
		pREGIO->u4Count * sizeof(u32)) != 0) {
		LOG_DBG("ioctl copy from user failed\n");
		return -EFAULT;
	}

#ifdef FDVT_USE_GCE
	cmdqRecCreate(CMDQ_SCENARIO_ISP_FDVT, &handle);

	LOG_DBG("FDVT isSecure: %d\n", g_isSecure);

#if (MTK_SECURE_FD_SUPPORT == 1)
	if (g_isSecure != 0)
		FDVT_switchCmdqToSecure(handle);
#endif

	cmdqRecSetEngine(handle, engineFlag);

	for (i = 0; i < pREGIO->u4Count; i++) {
		if ((FDVT_BASE_HW + pFDVTWriteBuffer.u4Addr[i]) >=
		FDVT_BASE_HW &&
		(FDVT_BASE_HW + pFDVTWriteBuffer.u4Addr[i]) <=
		(FDVT_BASE_HW + FDVT_MAX_OFFSET)) {
		/*LOG_DBG("Write: FDVT[0x%03lx](0x%08lx) = 0x%08lx\n",*/
		/*(unsigned long)pFDVTWriteBuffer.u4Addr[i], */
		/*(unsigned long)(FDVT_ADDR + pFDVTWriteBuffer.u4Addr[i]),*/
		/*(unsigned long)pFDVTWriteBuffer.u4Data[i]); */
			cmdqRecWrite(
				handle,
				FDVT_BASE_HW + pFDVTWriteBuffer.u4Addr[i],
				pFDVTWriteBuffer.u4Data[i],
				CMDQ_REG_MASK);
		} else {
		/* LOG_DBG("Error: Writing Addr(0x%8x) Excess FDVT Range!*/
		/* FD Offset: 0x%x\n",*/
		/* FDVT_ADDR + pFDVTWriteBuffer.u4Addr[i],*/
		/* pFDVTWriteBuffer.u4Addr[i]);*/
		}
	}

#if (MTK_SECURE_FD_SUPPORT == 1)
	if (g_isSecure == 1) {
		unsigned int LearningData_Chosen[18];
		unsigned int secMemType2MVA = 1;
		int ret = 0;

		for (i = 0; i < 18; i++) {
			LearningData_Chosen[i] =
			(g_fdvt_secmeta.dynamic_change_model[i] == 1) ?
			(g_fdvt_secmeta.Extra_Learning_Data_Handler[i]) :
			(g_fdvt_secmeta.Learning_Data_Handler[i]);
		}

		if (g_fdvt_secmeta.SecMemType == 2)
			secMemType2MVA = CMDQ_SAM_H_2_MVA;
		else if (g_fdvt_secmeta.SecMemType == 1)
			secMemType2MVA = CMDQ_SAM_PH_2_MVA;
		else
			LOG_INF("Unknown Sec Mem Type\n");

		cmdqRecWriteSecure(handle,
			FDVT_RSCON_BASE_ADR_HW,
			secMemType2MVA,
			g_fdvt_secmeta.RSConfig_Handler,
			0,
			g_fdvt_secmeta.RSConfigSize,
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FD_CON_BASE_ADR_HW,
			secMemType2MVA,
			g_fdvt_secmeta.FDConfig_Handler,
			0,
			g_fdvt_secmeta.FDConfigSize,
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_0_HW,
			secMemType2MVA,
			LearningData_Chosen[0],
			0,
			g_fdvt_secmeta.Learning_Data_Size[0],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_1_HW,
			secMemType2MVA,
			LearningData_Chosen[1],
			0,
			g_fdvt_secmeta.Learning_Data_Size[1],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_2_HW,
			secMemType2MVA,
			LearningData_Chosen[2],
			0,
			g_fdvt_secmeta.Learning_Data_Size[2],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_3_HW,
			secMemType2MVA,
			LearningData_Chosen[3],
			0,
			g_fdvt_secmeta.Learning_Data_Size[3],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_4_HW,
			secMemType2MVA,
			LearningData_Chosen[4],
			0,
			g_fdvt_secmeta.Learning_Data_Size[4],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_5_HW,
			secMemType2MVA,
			LearningData_Chosen[5],
			0,
			g_fdvt_secmeta.Learning_Data_Size[5],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_6_HW,
			secMemType2MVA,
			LearningData_Chosen[6],
			0,
			g_fdvt_secmeta.Learning_Data_Size[6],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_7_HW,
			secMemType2MVA,
			LearningData_Chosen[7],
			0,
			g_fdvt_secmeta.Learning_Data_Size[7],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_8_HW,
			secMemType2MVA,
			LearningData_Chosen[8],
			0,
			g_fdvt_secmeta.Learning_Data_Size[8],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_9_HW,
			secMemType2MVA,
			LearningData_Chosen[9],
			0,
			g_fdvt_secmeta.Learning_Data_Size[9],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_10_HW,
			secMemType2MVA,
			LearningData_Chosen[10],
			0,
			g_fdvt_secmeta.Learning_Data_Size[10],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_11_HW,
			secMemType2MVA,
			LearningData_Chosen[11],
			0,
			g_fdvt_secmeta.Learning_Data_Size[11],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_12_HW,
			secMemType2MVA,
			LearningData_Chosen[12],
			0,
			g_fdvt_secmeta.Learning_Data_Size[12],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_13_HW,
			secMemType2MVA,
			LearningData_Chosen[13],
			0,
			g_fdvt_secmeta.Learning_Data_Size[13],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_14_HW,
			secMemType2MVA,
			LearningData_Chosen[14],
			0,
			g_fdvt_secmeta.Learning_Data_Size[14],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_15_HW,
			secMemType2MVA,
			LearningData_Chosen[15],
			0,
			g_fdvt_secmeta.Learning_Data_Size[15],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_16_HW,
			secMemType2MVA,
			LearningData_Chosen[16],
			0,
			g_fdvt_secmeta.Learning_Data_Size[16],
			M4U_PORT_CAM_FD_RB);
		cmdqRecWriteSecure(handle,
			FDVT_FF_BASE_ADR_17_HW,
			secMemType2MVA,
			LearningData_Chosen[17],
			0,
			g_fdvt_secmeta.Learning_Data_Size[17],
			M4U_PORT_CAM_FD_RB);

		ret = cmdq_task_set_secure_meta(
			handle,
			CMDQ_SEC_METAEX_FD,
			&(g_fdvt_secmeta),
			sizeof(FDVTSecureMeta));
	}
#endif
	cmdqRecWrite(handle, FDVT_INT_EN_HW, 0x1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_START_HW, 0x0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_START_HW, 0x1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_START_HW, 0x0, CMDQ_REG_MASK);

	cmdq_task_flush_async_destroy(handle);	/* flush and destroy in cmdq */
#else
	/* pFDVTWriteBuffer.u4Counter=pREGIO->u4Count; */
	/* LOG_DBG("Count = %d\n", pREGIO->u4Count); */

	for (i = 0; i < pREGIO->u4Count; i++) {
		if ((FDVT_ADDR + pFDVTWriteBuffer.u4Addr[i]) >=
		FDVT_ADDR &&
		(FDVT_ADDR + pFDVTWriteBuffer.u4Addr[i]) <=
		(FDVT_ADDR + FDVT_MAX_OFFSET)) {
		/*LOG_DBG("Write: FDVT[0x%03lx](0x%08lx) = 0x%08lx\n",*/
		/*(unsigned long)pFDVTWriteBuffer.u4Addr[i], */
		/*(unsigned long)(FDVT_ADDR + pFDVTWriteBuffer.u4Addr[i]),*/
		/*(unsigned long)pFDVTWriteBuffer.u4Data[i]); */
			FDVT_WR32(pFDVTWriteBuffer.u4Data[i],
			FDVT_ADDR + pFDVTWriteBuffer.u4Addr[i]);
		} else {
		/* LOG_DBG("Error: Writing Addr(0x%8x) Excess FDVT Range!*/
		/* FD Offset: 0x%x\n",*/
		/* FDVT_ADDR + pFDVTWriteBuffer.u4Addr[i],*/
		/* pFDVTWriteBuffer.u4Addr[i]);*/
		}
	}
#endif
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
		LOG_DBG("Null input argrment\n");
		return -EINVAL;
	}

	if (a_pstCfg->u4Count > FDVT_DRAM_REGCNT) {
		LOG_DBG("Buffer Size Exceeded!\n");
		return -EFAULT;
	}

	size = a_pstCfg->u4Count * 4;

	if (copy_from_user(pFDVTReadBuffer.u4Addr, a_pstCfg->pAddr,
		size) != 0) {
		LOG_DBG("copy_from_user failed\n");
		ret = -EFAULT;
		goto mt_FDVT_read_reg_exit;
	}

	for (i = 0; i < a_pstCfg->u4Count; i++) {
		if ((FDVT_ADDR + pFDVTReadBuffer.u4Addr[i]) >= FDVT_ADDR &&
			(FDVT_ADDR + pFDVTReadBuffer.u4Addr[i]) <=
			(FDVT_ADDR + FDVT_MAX_OFFSET)) {
			pFDVTReadBuffer.u4Data[i] =
			ioread32(
			(void *)(FDVT_ADDR + pFDVTReadBuffer.u4Addr[i]));
			/*LOG_DBG("Read  addr/val: 0x%08x/0x%08x\n",*/
			/*(u32) (FDVT_ADDR + pFDVTReadBuffer.u4Addr[i]),*/
			/*(u32) pFDVTReadBuffer.u4Data[i]);*/
		} else {
		/*LOG_DBG("Error: Reading Addr(0x%8x) Excess FDVT Range!*/
		/* FD Offset: 0x%x\n",*/
		/*FDVT_ADDR + pFDVTReadBuffer.u4Addr[i], */
		/*pFDVTReadBuffer.u4Addr[i]);*/
			ret = -EFAULT;
			goto mt_FDVT_read_reg_exit;
		}
	}
	if (copy_to_user(a_pstCfg->pData, pFDVTReadBuffer.u4Data, size) != 0) {
		LOG_DBG("copy_to_user failed\n");
		ret = -EFAULT;
		goto mt_FDVT_read_reg_exit;
	}
mt_FDVT_read_reg_exit:

	return ret;
}

/*===============================================*/
/* Wait IRQ, for user space program to wait interrupt */
/* wait for timeout 500ms */
/*===============================================*/
static int FDVT_WaitIRQ(u32 *u4IRQMask)
{
	int timeout;
	/*timeout = wait_event_interruptible_timeout*/
	/*(g_FDVTWQ, (g_FDVTIRQMSK & g_FDVTIRQ),*/
	/*ms_to_jiffies(500));*/
	timeout = wait_event_interruptible_timeout
		(g_FDVTWQ,
		(g_FDVTIRQMSK & g_FDVTIRQ),
		us_to_jiffies(1000000));

	if (timeout == 0) {
		LOG_ERR("wait_event_interruptible_timeout timeout, %d, %d\n",
			g_FDVTIRQMSK,
			g_FDVTIRQ);
		FDVT_DUMPREG();
		FDVT_WR32(0x00030000, FDVT_START);  /* LDVT Disable */
		FDVT_WR32(0x00000000, FDVT_START);  /* LDVT Disable */
		return -EAGAIN;
	}

	*u4IRQMask = g_FDVTIRQ;
	LOG_INF("[FDVT] Receive IRQ : 0x%x\n", g_FDVTIRQ);

	/* check if user is interrupted by system signal */
	if (timeout != 0 && !(g_FDVTIRQMSK & g_FDVTIRQ)) {
		LOG_ERR("interrupted by system signal,return value(%d)\n",
			timeout);
		return -ERESTARTSYS; /* actually it should be -ERESTARTSYS */
	}

	if (!(g_FDVTIRQMSK & g_FDVTIRQ)) {
		LOG_ERR("wait_event_interruptible Not FDVT, %d, %d\n",
			g_FDVTIRQMSK,
			g_FDVTIRQ);
		FDVT_DUMPREG();
		return -1;
	}
#if (MTK_SECURE_FD_SUPPORT == 1)
	if (g_isSecure != 0)
		FDVT_switchPortToNonSecure();
#endif
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
		LOG_DBG("Buffer Size Exceeded!\n");
		return -EFAULT;
	}

	if (_IOC_DIR(cmd) != _IOC_NONE) {
		/* IO write */
		if (_IOC_WRITE & _IOC_DIR(cmd)) {
			if (copy_from_user(
				pBuff,
				(void *)arg,
				_IOC_SIZE(cmd))) {
				LOG_DBG(" ioctl copy from user failed\n");
				return -EFAULT;
			}
		}
		/* else */
			/* LOG_DBG(" ioctlnot write command\n"); */
	}
	/* else */
		/* LOG_DBG(" ioctl command = NONE\n"); */

	switch (cmd) {
	case FDVT_IOC_INIT_SETPARA_CMD:
		LOG_DBG("[FDVT] FDVT_INIT_CMD\n");
		haveConfig = 0;
		FDVT_basic_config();
		break;
	case FDVT_IOC_INIT_SETNORMAL_CMD:
		LOG_DBG("[FDVT] FDVT_INIT_SETNORMAL_CMD\n");
		g_isSecure = 0;
		break;
	case FDVT_IOC_INIT_SETSECURE_CMD:
		LOG_DBG("[FDVT] FDVT_INIT_SETSECURE_CMD\n");
		g_isSecure = 1;
		break;
	case FDVT_IOC_STARTFD_CMD:
		/* LOG_DBG("[FDVT] FDVTIOC_STARTFD_CMD\n"); */
		if (haveConfig) {
			#ifdef FDVT_USE_GCE
			#else
			FDVT_WR32(0x00000001, FDVT_INT_EN);
			FDVT_WR32(0x00000000, FDVT_START);
			FDVT_WR32(0x00000001, FDVT_START);
			FDVT_WR32(0x00000000, FDVT_START);
			#endif
			haveConfig = 0;
		}
		/* FDVT_DUMPREG(); */
		break;
	case FDVT_IOC_G_WAITIRQ:
		/* LOG_DBG("[FDVT] FDVT_WaitIRQ\n"); */
		ret = FDVT_WaitIRQ((unsigned int *)pBuff);
		FDVT_WR32(0x00000000, FDVT_INT_EN);
		break;
	case FDVT_IOC_T_SET_FDCONF_CMD:
		/* LOG_DBG("[FDVT] FDVT set FD config\n"); */
		FaceDetecteConfig();
		ret = FDVT_SetRegHW((FDVTRegIO *)pBuff);
		if (ret == 0)
			haveConfig = 1;
		else
			LOG_DBG("Set FD HW register fail");
		break;
	case FDVT_IOC_T_SET_SDCONF_CMD:
		/* LOG_DBG("[FDVT] FDVT set SD config\n"); */
		SmileDetecteConfig();
		ret = FDVT_SetRegHW((FDVTRegIO *)pBuff);
		if (ret == 0)
			haveConfig = 1;
		else
			LOG_DBG("Set SD HW register fail");
		break;
	case FDVT_IOC_G_READ_FDREG_CMD:
		/* LOG_DBG("[FDVT] FDVT read FD config\n"); */
		ret = FDVT_ReadRegHW((FDVTRegIO *)pBuff);
		break;
	case FDVT_IOC_SETMETA_CMD:
		LOG_DBG("[FDVT] FDVT set meta data\n");
		ret = FDVT_SetMetaData((FDVTMetaData *)pBuff);
		break;
	case FDVT_IOC_T_DUMPREG:
		LOG_DBG("[FDVT] FDVT_DUMPREG\n");
		FDVT_DUMPREG();
		break;
	default:
		LOG_DBG("[FDVT][ERROR] default case\n");
		break;
	}
	/* NOT_REFERENCED(ret); */
	/* LOG_DBG("[FDVT] FDVT IOCtrol out\n"); */
	if (_IOC_READ & _IOC_DIR(cmd)) {
		if (copy_to_user(
			(void __user *)arg,
			pBuff,
			_IOC_SIZE(cmd)) != 0) {
			LOG_DBG("copy_to_user failed\n");
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

static int compat_FD_get_meta_data(compat_FDVTMetaData __user *data32,
					      FDVTMetaData __user *data)
{
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->SecureMeta);
	err |= put_user(compat_ptr(uptr), &data->SecureMeta);
	return err;
}

static long compat_FD_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_FDVT_IOC_INIT_SETSECURE_CMD:
	{
	return
	file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
	}
	case COMPAT_FDVT_IOC_INIT_SETNORMAL_CMD:
	{
	return
	file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
	}
	case COMPAT_FDVT_IOC_INIT_SETPARA_CMD:
	{
	return
	file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
	}
	case COMPAT_FDVT_IOC_STARTFD_CMD:
	{
	return
	file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
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
		ret = file->f_op->unlocked_ioctl(
			file,
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
		ret = file->f_op->unlocked_ioctl(
			file,
			FDVT_IOC_T_SET_SDCONF_CMD,
			(unsigned long)data);
		return ret ? ret : err;
	}
	case COMPAT_FDVT_IOC_SETMETA_CMD:
	{
		compat_FDVTMetaData __user *data32;
		FDVTMetaData __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_FD_get_meta_data(data32, data);
		if (err)
			return err;
		ret = file->f_op->unlocked_ioctl(
			file,
			FDVT_IOC_SETMETA_CMD,
			(unsigned long)data);
		return ret ? ret : err;
	}
	case COMPAT_FDVT_IOC_T_DUMPREG:
	{
	return
	file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
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

	LOG_DBG("[FDVT_DEBUG]\n");

	spin_lock(&g_spinLock);
	if (g_drvOpened) {
		spin_unlock(&g_spinLock);
		LOG_DBG("Opened, return -EBUSY\n");
		return -EBUSY;
	}
	g_drvOpened = 1;
	spin_unlock(&g_spinLock);

	__pm_stay_awake(fdvt_wake_lock);

	mt_fdvt_clk_ctrl(1);

	__pm_relax(fdvt_wake_lock);

	if (pBuff != NULL)
		LOG_DBG("pBuff is not null\n");
	if (pread_buf != NULL)
		LOG_DBG("pread_buf is not null\n");

	pBuff = kmalloc(buf_size, GFP_KERNEL);
	if (pBuff == NULL) {
		LOG_DBG(" ioctl allocate mem failed\n");
		ret = -ENOMEM;
	} else {
		LOG_DBG(" ioctl allocate mem ok\n");
	}

	pread_buf = kmalloc(buf_size, GFP_KERNEL);
	if (pread_buf == NULL) {
		LOG_DBG(" ioctl allocate mem failed\n");
		ret = -ENOMEM;
	} else {
		LOG_DBG(" ioctl allocate mem ok\n");
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
 *static int FDVT_flush(struct file *file, fl_owner_t id)
 *{
 *	LOG_DBG("[FDVT_DEBUG] FDVT_flush\n");
 *	return 0;
 *}
 */
static int FDVT_release(struct inode *inode, struct file *file)
{
	LOG_DBG("[FDVT_DEBUG]\n");
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
		LOG_ERR("find IMGSYS_CONFIG node failed!!!\n");
		return -ENODEV;
	}
	IMGSYS_CONFIG_BASE = of_iomap(node, 0);
	if (!IMGSYS_CONFIG_BASE) {
		LOG_ERR("unable to map IMGSYS_CONFIG_BASE registers!!!\n");
		return -ENODEV;
	}

	LOG_DBG("[FDVT_DEBUG] IMGSYS_CONFIG_BASE: %lx\n",
		(unsigned long)IMGSYS_CONFIG_BASE);

	node = of_find_compatible_node(NULL, NULL, "mediatek,camsys_config");
	if (!node) {
		LOG_ERR("find CAMSYS_CONFIG node failed!!!\n");
		return -ENODEV;
	}
	CAMSYS_CONFIG_BASE = of_iomap(node, 0);
	if (!CAMSYS_CONFIG_BASE) {
		LOG_ERR("unable to map CAMSYS_CONFIG_BASE registers!!!\n");
		return -ENODEV;
	}
	LOG_DBG("[FDVT_DEBUG] CAMSYS_CONFIG_BASE: %lx\n",
		(unsigned long)CAMSYS_CONFIG_BASE);
#endif

	nr_fdvt_devs = 0;

	LOG_INF("FDVT PROBE!!!\n");
#ifdef CONFIG_OF

	LOG_DBG("[FDVT_DEBUG]\n");

	if (dev == NULL) {
		dev_info(&dev->dev, "dev is NULL");
		return -ENXIO;
	}

	new_count = nr_fdvt_devs + 1;
	tempFdvt = krealloc(
		fdvt_devs,
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
			dev_info(&dev->dev, "of_iomap fail, i=%d\n", i);
			return -ENOMEM;
		}
		gFDVT_Reg[i] = (unsigned long)fdvt_dev->regs[i];
		LOG_INF("DT, i=%d, map_addr=0x%lx\n", i, gFDVT_Reg[i]);
	}

	/* get IRQ ID and request IRQ */

	for (i = 0; i < FDVT_IRQ_IDX_NUM; i++) {
		fdvt_dev->irq[i] = irq_of_parse_and_map(dev->dev.of_node, i);
		gFDVT_Irq[i] = fdvt_dev->irq[i];
		if (i == FDVT_IRQ_IDX) {
			/* IRQF_TRIGGER_NONE dose not take effect here*/
			/* real trigger mode set in dts file */
			ret = request_irq(
				fdvt_dev->irq[i],
				(irq_handler_t)FDVT_irq,
				IRQF_TRIGGER_NONE,
				FDVT_DEVNAME,
				NULL);
			/* request_irq( */
			/* FD_IRQ_BIT_ID, */
			/* (irq_handler_t)FDVT_irq, */
			/* IRQF_TRIGGER_LOW, */
			/* FDVT_DEVNAME, */
			/* NULL) */
		}
		if (ret) {
			dev_info(&dev->dev, "request_irq fail, i=%d, irq=%d\n",
				i, fdvt_dev->irq[i]);
			return ret;
		}
		LOG_INF("DT, i=%d, map_irq=%d\n", i, fdvt_dev->irq[i]);
	}

	nr_fdvt_devs = new_count;

#endif

	ret = alloc_chrdev_region(&FDVT_devno, 0, 1, FDVT_DEVNAME);

	if (ret)
		LOG_DBG("[FDVT_DEBUG]Can't get major number for FDVT device\n");

	FDVT_cdev = cdev_alloc();

	FDVT_cdev->owner = THIS_MODULE;
	FDVT_cdev->ops = &FDVT_fops;

	cdev_init(FDVT_cdev, &FDVT_fops);

	ret = cdev_add(FDVT_cdev, FDVT_devno, 1);
	if (ret < 0) {
		LOG_DBG("[FDVT_DEBUG] Attatch file operation failed\n");
		return -EFAULT;
	}

#ifndef CONFIG_OF
	/* Register Interrupt */
	if (request_irq(
		FD_IRQ_BIT_ID,
		(irq_handler_t)FDVT_irq,
		IRQF_TRIGGER_LOW,
		FDVT_DEVNAME,
		NULL) < 0)
		LOG_DBG("[FDVT_DEBUG][ERROR] error to request FDVT irq\n");
	else
		LOG_DBG("[FDVT_DEBUG] success to request FDVT irq\n");
#endif

#if LDVT_EARLY_PORTING_NO_CCF
#else
    /*CCF: Grab clock pointer (struct clk*) */
	fd_clk.CG_IMGSYS_FDVT = devm_clk_get(&dev->dev, "FD_CLK_IMG_FDVT");

	if (IS_ERR(fd_clk.CG_IMGSYS_FDVT)) {
		LOG_ERR("cannot get CG_IMGSYS_FDVT clock\n");
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

	fdvt_wake_lock =
		wakeup_source_register(NULL, "fdvt_lock_wakelock");

	LOG_DBG("[FDVT_DEBUG] Done\n");

	return 0;
}

static int FDVT_remove(struct platform_device *dev)
{
	int i4IRQ = 0;

	LOG_DBG("[FDVT_DEBUG] FDVT_driver_exit\n");
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
		LOG_INF("[FDVT_DEBUG]\n");
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
		LOG_INF("[FDVT_DEBUG]\n");
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
	/* LOG_DBG("[FDVT_DEBUG]\n"); */
	/* mt_fdvt_clk_ctrl(0); */

}

static void FDVT_early_resume(struct early_suspend *h)
{
	/* LOG_DBG("[FDVT_DEBUG]\n"); */
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

	LOG_DBG("[FDVT_DEBUG]\n");

	if (platform_driver_register(&FDVT_driver)) {
		LOG_DBG("[FDVT_DEBUG][ERROR] failed to register FDVT Driver\n");
		ret = -ENODEV;
		return ret;
	}
	#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&FDVT_early_suspend_desc);
	#endif

	LOG_DBG("[FDVT_DEBUG] FDVT driver init Done\n");

	return 0;
}

static void __exit FDVT_driver_exit(void)
{
	LOG_DBG("[FDVT_DEBUG]\n");

	device_destroy(FDVT_class, FDVT_devno);
	class_destroy(FDVT_class);

	cdev_del(FDVT_cdev);
	unregister_chrdev_region(FDVT_devno, 1);

	platform_driver_unregister(&FDVT_driver);
}


module_init(FDVT_driver_init);
module_exit(FDVT_driver_exit);
MODULE_AUTHOR("MM/MM3/SW5");
MODULE_DESCRIPTION("FDVT Driver");
MODULE_LICENSE("GPL");
