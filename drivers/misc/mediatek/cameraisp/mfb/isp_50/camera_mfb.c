/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/******************************************************************************
 * camera_MFB.c - Linux MFB Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers MFB relative functions
 *
 ******************************************************************************/
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
/* #include <asm/io.h> */
/* #include <asm/tcm.h> */
#include <linux/proc_fs.h>	/* proc file use */
/*  */
#include <linux/slab.h>
#include <linux/spinlock.h>
/* #include <linux/io.h> */
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/mm.h>
#include <linux/seq_file.h>

/*#include <linux/xlog.h>		 For xlog_printk(). */
/*  */
/*#include <mach/hardware.h>*/
/* #include <mach/mt6593_pll.h> */
#include "camera_mfb.h"
/*#include <mach/irqs.h>*/
/* #include <mach/mt_reg_base.h> */
/* #if defined(CONFIG_MTK_LEGACY) */
/* For clock mgr APIS. enable_clock()/disable_clock(). */
/* #include <mach/mt_clkmgr.h> */
/* #endif */
#include <mt-plat/sync_write.h>	/* For mt65xx_reg_sync_writel(). */
/* For spm_enable_sodi()/spm_disable_sodi(). */
/* #include <mach/mt_spm_idle.h> */
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <m4u.h>
#include <cmdq_core.h>
#include <cmdq_record.h>
#include <smi_public.h>

/* Measure the kernel performance
 * #define __MFB_KERNEL_PERFORMANCE_MEASURE__
 */
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
#endif
#if 0
/* Another Performance Measure Usage */
#include <linux/kallsyms.h>
#include <linux/ftrace_event.h>
static unsigned long __read_mostly tracing_mark_write_addr;
#define _kernel_trace_begin(name) {\
	tracing_mark_write_addr =\
		kallsyms_lookup_name("tracing_mark_write");\
	event_trace_printk(tracing_mark_write_addr,\
		"B|%d|%s\n", current->tgid, name);\
}
#define _kernel_trace_end() {\
	event_trace_printk(tracing_mark_write_addr,  "E\n");\
}
/* How to Use */
/* char strName[128]; */
/* sprintf(strName, "TAG_K_WAKEUP (%d)",sof_count[_PASS1]); */
/* _kernel_trace_begin(strName); */
/* _kernel_trace_end(); */
#endif


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*  #include "smi_common.h" */

#include <linux/pm_wakeup.h>

/* MFB Command Queue */
/* #include "../../cmdq/mt6797/cmdq_record.h" */
/* #include "../../cmdq/mt6797/cmdq_core.h" */

/* CCF */
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#include <linux/clk.h>
struct MFB_CLK_STRUCT {
#define SMI_CLK
#ifndef SMI_CLK
	struct clk *CG_SCP_SYS_MM0;
	struct clk *CG_MM_SMI_COMMON;
	struct clk *CG_MM_SMI_COMMON_2X;
	struct clk *CG_MM_SMI_COMMON_GALS_M0_2X;
	struct clk *CG_MM_SMI_COMMON_GALS_M1_2X;
	struct clk *CG_MM_SMI_COMMON_UPSZ0;
	struct clk *CG_MM_SMI_COMMON_UPSZ1;
	struct clk *CG_MM_SMI_COMMON_FIFO0;
	struct clk *CG_MM_SMI_COMMON_FIFO1;
	struct clk *CG_MM_LARB5;
	struct clk *CG_SCP_SYS_ISP;
	struct clk *CG_IMGSYS_LARB;
#endif
	struct clk *CG_IMGSYS_MFB;
};
struct MFB_CLK_STRUCT mfb_clk;
#endif
/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */

#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define MFB_DEV_NAME                "camera-mfb"
/* #define EP_NO_CLKMGR */
#define BYPASS_REG         (0)
/* #define MFB_WAITIRQ_LOG  */
#define MFB_USE_GCE
/* #define MFB_DEBUG_USE */
#define DUMMY_MFB	   (0)
/* #define MFB_MULTIPROCESS_TIMEING_ISSUE  */
/*I can' test the situation in FPGA due to slow FPGA. */
#define MyTag "[MFB]"
#define IRQTag "KEEPER"

#define log_vrb(format,	args...)    pr_debug(MyTag format, ##args)

#ifdef MFB_DEBUG_USE
#define log_dbg(format, args...)    pr_info(MyTag format, ##args)
#else
#define log_dbg(format, args...)
#endif

#define log_inf(format, args...)    pr_info(MyTag format,  ##args)
#define log_notice(format, args...) pr_notice(MyTag format,  ##args)
#define log_wrn(format, args...)    pr_debug(MyTag format,  ##args)
#define log_err(format, args...)    pr_debug(MyTag format,  ##args)
#define log_ast(format, args...)    pr_debug(MyTag format, ##args)


/******************************************************************************
 *
 ******************************************************************************/
/* #define MFB_WR32(addr, data)  iowrite32(data, addr) // For other projects. */
/* For 89 Only.   // NEED_TUNING_BY_PROJECT */
#define MFB_WR32(addr, data)    mt_reg_sync_writel(data, addr)
#define MFB_RD32(addr)          ioread32(addr)
/******************************************************************************
 *
 ******************************************************************************/
/* dynamic log level */
#define MFB_DBG_DBGLOG              (0x00000001)
#define MFB_DBG_INFLOG              (0x00000002)
#define MFB_DBG_INT                 (0x00000004)
#define MFB_DBG_READ_REG            (0x00000008)
#define MFB_DBG_WRITE_REG           (0x00000010)
#define MFB_DBG_TASKLET             (0x00000020)

/*
 *    CAM interrupt status
 */

/* normal siganl : happens to be the same bit as register bit*/
/*#define MFB_INT_ST           (1<<0)*/


/*
 *   IRQ signal mask
 */

#define INT_ST_MASK_MFB     ( \
			MFB_INT_ST)

#define CMDQ_REG_MASK 0xffffffff
#define MFB_START      0x1

#define MFB_ENABLE     0x1

#define MFB_IS_BUSY    0x2


/* static irqreturn_t MFB_Irq_CAM_A(signed int  Irq,void *DeviceId); */
static irqreturn_t ISP_Irq_MFB(signed int Irq, void *DeviceId);
static bool ConfigMFB(void);
static signed int ConfigMFBHW(MFB_Config *pMfbConfig);
static void MFB_ScheduleWork(struct work_struct *data);



typedef irqreturn_t(*IRQ_CB) (signed int, void *);

struct ISR_TABLE {
	IRQ_CB isr_fp;
	unsigned int int_number;
	char device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE MFB_IRQ_CB_TBL[MFB_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_MFB, MFB_IRQ_BIT_ID, "mfb"},
};

#else
/* int number is got from kernel api */
const struct ISR_TABLE MFB_IRQ_CB_TBL[MFB_IRQ_TYPE_AMOUNT] = {
#if DUMMY_MFB
	{ISP_Irq_MFB, 0, "mfb-dummy"},
#else
	{ISP_Irq_MFB, 0, "mfb"},
#endif
};
#endif
/* ////////////////////////////////////////////////////////////////////////// */
/*  */
typedef void (*tasklet_cb) (unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct *pMFB_tkt;
};

struct tasklet_struct Mfbtkt[MFB_IRQ_TYPE_AMOUNT];

static void ISP_TaskletFunc_MFB(unsigned long data);

static struct Tasklet_table MFB_tasklet[MFB_IRQ_TYPE_AMOUNT] = {
	{ISP_TaskletFunc_MFB, &Mfbtkt[MFB_IRQ_TYPE_INT_MFB_ST]},
};

struct wakeup_source MFB_wake_lock;


static DEFINE_MUTEX(gMfbMutex);
static DEFINE_MUTEX(gMfbDequeMutex);

#ifdef CONFIG_OF

struct MFB_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct MFB_device *MFB_devs;
static int nr_MFB_devs;

/* Get HW modules' base address from device nodes */
#define MFB_DEV_NODE_IDX 0
#define IMGSYS_DEV_MODE_IDX 1
/* static unsigned long gISPSYS_Reg[MFB_IRQ_TYPE_AMOUNT]; */


#define ISP_MFB_BASE                  (MFB_devs[MFB_DEV_NODE_IDX].regs)
#define ISP_IMGSYS_BASE               (MFB_devs[IMGSYS_DEV_MODE_IDX].regs)

/* #define ISP_MFB_BASE                  (gISPSYS_Reg[MFB_DEV_NODE_IDX]) */



#else
#define ISP_MFB_BASE                        (IMGSYS_BASE + 0xe000)

#endif


static unsigned int g_u4EnableClockCount;
static unsigned int g_u4MfbCnt;

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32


enum MFB_FRAME_STATUS_ENUM {
	MFB_FRAME_STATUS_EMPTY,	/* 0 */
	MFB_FRAME_STATUS_ENQUE,	/* 1 */
	MFB_FRAME_STATUS_RUNNING,	/* 2 */
	MFB_FRAME_STATUS_FINISHED,	/* 3 */
	MFB_FRAME_STATUS_TOTAL
};


enum MFB_REQUEST_STATE_ENUM {
	MFB_REQUEST_STATE_EMPTY,	/* 0 */
	MFB_REQUEST_STATE_PENDING,	/* 1 */
	MFB_REQUEST_STATE_RUNNING,	/* 2 */
	MFB_REQUEST_STATE_FINISHED,	/* 3 */
	MFB_REQUEST_STATE_TOTAL
};

struct MFB_REQUEST_STRUCT {
	enum MFB_REQUEST_STATE_ENUM State;
	pid_t processID;	/* caller process ID */
	unsigned int callerID;	/* caller thread ID */
	unsigned int enqueReqNum;/* to judge it belongs to which frame package*/
	signed int FrameWRIdx;	/* Frame write Index */
	signed int RrameRDIdx;	/* Frame read Index */
	enum MFB_FRAME_STATUS_ENUM
		MfbFrameStatus[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
	MFB_Config MfbFrameConfig[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
};

struct MFB_REQUEST_RING_STRUCT {
	signed int WriteIdx;	/* enque how many request  */
	signed int ReadIdx;		/* read which request index */
	signed int HWProcessIdx;	/* HWWriteIdx */
	struct MFB_REQUEST_STRUCT
		MFBReq_Struct[_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_];
};

struct  MFB_CONFIG_STRUCT {
	MFB_Config MfbFrameConfig[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
};

static struct MFB_REQUEST_RING_STRUCT g_MFB_ReqRing;
static struct MFB_CONFIG_STRUCT g_MfbEnqueReq_Struct;
static struct MFB_CONFIG_STRUCT g_MfbDequeReq_Struct;


/******************************************************************************
 *
 ******************************************************************************/
struct  MFB_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};
enum MFB_PROCESS_ID_ENUM {
	MFB_PROCESS_ID_NONE,
	MFB_PROCESS_ID_MFB,
	MFB_PROCESS_ID_AMOUNT
};


/******************************************************************************
 *
 ******************************************************************************/
struct MFB_IRQ_INFO_STRUCT {
	unsigned int Status[MFB_IRQ_TYPE_AMOUNT];
	signed int MfbIrqCnt;
	pid_t ProcessID[MFB_PROCESS_ID_AMOUNT];
	unsigned int Mask[MFB_IRQ_TYPE_AMOUNT];
};


struct MFB_INFO_STRUCT {
	spinlock_t SpinLockMFBRef;
	spinlock_t SpinLockMFB;
	spinlock_t SpinLockIrq[MFB_IRQ_TYPE_AMOUNT];
	wait_queue_head_t WaitQueueHead;
	struct work_struct ScheduleMfbWork;
	unsigned int UserCount;	/* User Count */
	unsigned int DebugMask;	/* Debug Mask */
	signed int IrqNum;
	struct MFB_IRQ_INFO_STRUCT IrqInfo;
	signed int WriteReqIdx;
	signed int ReadReqIdx;
	pid_t ProcessID[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
};


static struct MFB_INFO_STRUCT MFBInfo;

enum eLOG_TYPE {
	_LOG_DBG = 0,/* currently, only used at ipl_buf_ctrl for
		      * critical section
		      */
	_LOG_INF = 1,
	_LOG_ERR = 2,
	_LOG_MAX = 3,
};

#define NORMAL_STR_LEN (512)
#define ERR_PAGE 2
#define DBG_PAGE 2
#define INF_PAGE 4
/* #define SV_LOG_STR_LEN NORMAL_STR_LEN */

#define LOG_PPNUM 2
static unsigned int m_CurrentPPB;
struct SV_LOG_STR {
	unsigned int _cnt[LOG_PPNUM][_LOG_MAX];
	/* char   _str[_LOG_MAX][SV_LOG_STR_LEN]; */
	char *_str[LOG_PPNUM][_LOG_MAX];
} *MFB_PSV_LOG_STR;

static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[MFB_IRQ_TYPE_AMOUNT];

/*
 *   for irq used,keep log until IRQ_LOG_PRINTER being involked,
 *   limited:
 *   each log must shorter than 512 bytes
 *  total log length in each irq/logtype can't over 1024 bytes
 */
#if 1
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) do {\
	char *ptr; \
	char *pDes;\
	unsigned int *ptr2 = &gSvLog[irq]._cnt[ppb][logT];\
	unsigned int str_leng;\
	if (logT == _LOG_ERR) {\
		str_leng = NORMAL_STR_LEN*ERR_PAGE; \
	} else if (logT == _LOG_DBG) {\
		str_leng = NORMAL_STR_LEN*DBG_PAGE; \
	} else if (logT == _LOG_INF) {\
		str_leng = NORMAL_STR_LEN*INF_PAGE;\
	} else {\
		str_leng = 0;\
	} \
	ptr = pDes = (char *)&(\
	    gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]); \
	sprintf((char *)(pDes), fmt, ##__VA_ARGS__);   \
	if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) {\
		log_err("log str over flow(%d)", irq);\
	} \
	while (*ptr++ != '\0') {        \
		(*ptr2)++;\
	}     \
} while (0)
#else
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...)\
		xlog_printk(ANDROID_LOG_DEBUG,\
		"KEEPER", "[%s] " fmt, __func__, ##__VA_ARGS__)
#endif

#if 1
#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) do {\
	struct SV_LOG_STR *pSrc = &gSvLog[irq];\
	char *ptr;\
	unsigned int i;\
	signed int ppb = 0;\
	signed int logT = 0;\
	if (ppb_in > 1) {\
		ppb = 1;\
	} else{\
		ppb = ppb_in;\
	} \
	if (logT_in > _LOG_ERR) {\
		logT = _LOG_ERR;\
	} else{\
		logT = logT_in;\
	} \
	ptr = pSrc->_str[ppb][logT];\
	if (pSrc->_cnt[ppb][logT] != 0) {\
		if (logT == _LOG_DBG) {\
			for (i = 0; i < DBG_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					log_dbg("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else{\
					log_dbg("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
	else if (logT == _LOG_INF) {\
		for (i = 0; i < INF_PAGE; i++) {\
			if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
				ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
				log_inf("%s", &ptr[NORMAL_STR_LEN*i]);\
			} else{\
				log_inf("%s", &ptr[NORMAL_STR_LEN*i]);\
				break;\
			} \
		} \
	} \
	else if (logT == _LOG_ERR) {\
		for (i = 0; i < ERR_PAGE; i++) {\
			if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
				ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
				log_err("%s", &ptr[NORMAL_STR_LEN*i]);\
			} else{\
				log_err("%s", &ptr[NORMAL_STR_LEN*i]);\
				break;\
			} \
		} \
	} \
	else {\
		log_err("N.S.%d", logT);\
	} \
		ptr[0] = '\0';\
		pSrc->_cnt[ppb][logT] = 0;\
	} \
} while (0)


#else
#define IRQ_LOG_PRINTER(irq, ppb, logT)
#endif

#define IMGSYS_REG_CG_CON             (ISP_IMGSYS_BASE + 0x0)
#define IMGSYS_REG_CG_SET             (ISP_IMGSYS_BASE + 0x4)
#define IMGSYS_REG_CG_CLR             (ISP_IMGSYS_BASE + 0x8)

/* MFB unmapped base address macro for GCE to access */
#define MFB_CON_HW                       (MFB_BASE_HW)
#define MFB_LL_CON1_HW                   (MFB_BASE_HW + 0x04)
#define MFB_LL_CON2_HW                   (MFB_BASE_HW + 0x08)
#define MFB_LL_CON3_HW                   (MFB_BASE_HW + 0x0C)
#define MFB_LL_CON4_HW                   (MFB_BASE_HW + 0x10)
#define MFB_EDGE_HW                      (MFB_BASE_HW + 0x14)
#define MFB_LL_CON5_HW                   (MFB_BASE_HW + 0x18)
#define MFB_LL_CON6_HW                   (MFB_BASE_HW + 0x1C)
#define MFB_LL_CON7_HW                   (MFB_BASE_HW + 0x20)
#define MFB_LL_CON8_HW                   (MFB_BASE_HW + 0x24)
#define MFB_LL_CON9_HW                   (MFB_BASE_HW + 0x28)
#define MFB_LL_CON10_HW                  (MFB_BASE_HW + 0x2c)
#define MFB_MBD_CON0_HW                  (MFB_BASE_HW + 0x30)
#define MFB_MBD_CON1_HW                  (MFB_BASE_HW + 0x34)
#define MFB_MBD_CON2_HW                  (MFB_BASE_HW + 0x38)
#define MFB_MBD_CON3_HW                  (MFB_BASE_HW + 0x3c)
#define MFB_MBD_CON4_HW                  (MFB_BASE_HW + 0x40)
#define MFB_MBD_CON5_HW                  (MFB_BASE_HW + 0x44)
#define MFB_MBD_CON6_HW                  (MFB_BASE_HW + 0x48)
#define MFB_MBD_CON7_HW                  (MFB_BASE_HW + 0x4C)
#define MFB_MBD_CON8_HW                  (MFB_BASE_HW + 0x50)
#define MFB_MBD_CON9_HW                  (MFB_BASE_HW + 0x54)
#define MFB_MBD_CON10_HW                 (MFB_BASE_HW + 0x58)
#define MFB_TOP_CFG0_HW                  (MFB_BASE_HW + 0x5C)
#define MFB_TOP_CFG1_HW                  (MFB_BASE_HW + 0x60)
#define MFB_TOP_CFG2_HW                  (MFB_BASE_HW + 0x64)
#define MFB_INT_CTL_HW                   (MFB_BASE_HW + 0x80)
#define MFB_INT_STATUS_HW                (MFB_BASE_HW + 0x84)
#define MFB_SW_RST_HW                    (MFB_BASE_HW + 0x88)
#define MFB_MAIN_DCM_ST_HW               (MFB_BASE_HW + 0x8C)
#define MFB_DMA_DCM_ST_HW                (MFB_BASE_HW + 0x90)
#define MFB_MAIN_DCM_DIS_HW              (MFB_BASE_HW + 0x94)
#define MFB_DMA_DCM_DIS_HW               (MFB_BASE_HW + 0x98)
#define MFB_DBG_CTL0_HW                  (MFB_BASE_HW + 0x9C)
#define MFB_DBG_OUT0_HW                  (MFB_BASE_HW + 0xA0)
#define MFB_DBG_OUT1_HW                  (MFB_BASE_HW + 0xA4)
#define MFB_DBG_OUT2_HW                  (MFB_BASE_HW + 0xA8)
#define MFB_DBG_OUT3_HW                  (MFB_BASE_HW + 0xAC)
#define MFB_DBG_OUT4_HW                  (MFB_BASE_HW + 0xB0)

#define MFB_DMA_SOFT_RSTSTAT_HW          (MFB_BASE_HW + 0x100)
#define MFB_TDRI_BASE_ADDR_HW            (MFB_BASE_HW + 0x104)
#define MFB_TDRI_OFST_ADDR_HW            (MFB_BASE_HW + 0x108)
#define MFB_TDRI_XSIZE_HW                (MFB_BASE_HW + 0x10C)
#define MFB_VERTICAL_FLIP_EN_HW          (MFB_BASE_HW + 0x110)
#define MFB_DMA_SOFT_RESET_HW            (MFB_BASE_HW + 0x114)
#define MFB_LAST_ULTRA_EN_HW             (MFB_BASE_HW + 0x118)
#define MFB_SPECIAL_FUN_EN_HW            (MFB_BASE_HW + 0x11C)
#define MFB_MFBO_BASE_ADDR_HW            (MFB_BASE_HW + 0x130)
#define MFB_MFBO_OFST_ADDR_HW            (MFB_BASE_HW + 0x138)
#define MFB_MFBO_XSIZE_HW                (MFB_BASE_HW + 0x140)
#define MFB_MFBO_YSIZE_HW                (MFB_BASE_HW + 0x144)
#define MFB_MFBO_STRIDE_HW               (MFB_BASE_HW + 0x148)
#define MFB_MFBO_CON_HW                  (MFB_BASE_HW + 0x14C)
#define MFB_MFBO_CON2_HW                 (MFB_BASE_HW + 0x150)
#define MFB_MFBO_CON3_HW                 (MFB_BASE_HW + 0x154)
#define MFB_MFBO_CROP_HW                 (MFB_BASE_HW + 0x158)
#define MFB_MFB2O_BASE_ADDR_HW           (MFB_BASE_HW + 0x160)
#define MFB_MFB2O_OFST_ADDR_HW           (MFB_BASE_HW + 0x168)
#define MFB_MFB2O_XSIZE_HW               (MFB_BASE_HW + 0x170)
#define MFB_MFB2O_YSIZE_HW               (MFB_BASE_HW + 0x174)
#define MFB_MFB2O_STRIDE_HW              (MFB_BASE_HW + 0x178)
#define MFB_MFB2O_CON_HW                 (MFB_BASE_HW + 0x17C)
#define MFB_MFB2O_CON2_HW                (MFB_BASE_HW + 0x180)
#define MFB_MFB2O_CON3_HW                (MFB_BASE_HW + 0x184)
#define MFB_MFB2O_CROP_HW                (MFB_BASE_HW + 0x188)
#define MFB_MFBI_BASE_ADDR_HW            (MFB_BASE_HW + 0x190)
#define MFB_MFBI_OFST_ADDR_HW            (MFB_BASE_HW + 0x198)
#define MFB_MFBI_XSIZE_HW                (MFB_BASE_HW + 0x1A0)
#define MFB_MFBI_YSIZE_HW                (MFB_BASE_HW + 0x1A4)
#define MFB_MFBI_STRIDE_HW               (MFB_BASE_HW + 0x1A8)
#define MFB_MFBI_CON_HW                  (MFB_BASE_HW + 0x1AC)
#define MFB_MFBI_CON2_HW                 (MFB_BASE_HW + 0x1B0)
#define MFB_MFBI_CON3_HW                 (MFB_BASE_HW + 0x1B4)
#define MFB_MFB2I_BASE_ADDR_HW           (MFB_BASE_HW + 0x1C0)
#define MFB_MFB2I_OFST_ADDR_HW           (MFB_BASE_HW + 0x1C8)
#define MFB_MFB2I_XSIZE_HW               (MFB_BASE_HW + 0x1D0)
#define MFB_MFB2I_YSIZE_HW               (MFB_BASE_HW + 0x1D4)
#define MFB_MFB2I_STRIDE_HW              (MFB_BASE_HW + 0x1D8)
#define MFB_MFB2I_CON_HW                 (MFB_BASE_HW + 0x1DC)
#define MFB_MFB2I_CON2_HW                (MFB_BASE_HW + 0x1E0)
#define MFB_MFB2I_CON3_HW                (MFB_BASE_HW + 0x1E4)
#define MFB_MFB3I_BASE_ADDR_HW           (MFB_BASE_HW + 0x1F0)
#define MFB_MFB3I_OFST_ADDR_HW           (MFB_BASE_HW + 0x1F8)
#define MFB_MFB3I_XSIZE_HW               (MFB_BASE_HW + 0x200)
#define MFB_MFB3I_YSIZE_HW               (MFB_BASE_HW + 0x204)
#define MFB_MFB3I_STRIDE_HW              (MFB_BASE_HW + 0x208)
#define MFB_MFB3I_CON_HW                 (MFB_BASE_HW + 0x20C)
#define MFB_MFB3I_CON2_HW                (MFB_BASE_HW + 0x210)
#define MFB_MFB3I_CON3_HW                (MFB_BASE_HW + 0x214)
#define MFB_MFB4I_BASE_ADDR_HW           (MFB_BASE_HW + 0x220)
#define MFB_MFB4I_OFST_ADDR_HW           (MFB_BASE_HW + 0x228)
#define MFB_MFB4I_XSIZE_HW               (MFB_BASE_HW + 0x230)
#define MFB_MFB4I_YSIZE_HW               (MFB_BASE_HW + 0x234)
#define MFB_MFB4I_STRIDE_HW              (MFB_BASE_HW + 0x238)
#define MFB_MFB4I_CON_HW                 (MFB_BASE_HW + 0x23C)
#define MFB_MFB4I_CON2_HW                (MFB_BASE_HW + 0x240)
#define MFB_MFB4I_CON3_HW                (MFB_BASE_HW + 0x244)
#define MFB_DMA_ERR_CTRL_HW              (MFB_BASE_HW + 0x250)
#define MFB_MFBO_ERR_STAT_HW             (MFB_BASE_HW + 0x254)
#define MFB_MFB2O_ERR_STAT_HW            (MFB_BASE_HW + 0x258)
#define MFB_MFBO_B_ERR_STAT_HW           (MFB_BASE_HW + 0x25C)
#define MFB_MFBI_ERR_STAT_HW             (MFB_BASE_HW + 0x260)
#define MFB_MFB2I_ERR_STAT_HW            (MFB_BASE_HW + 0x264)
#define MFB_MFB3I_ERR_STAT_HW            (MFB_BASE_HW + 0x268)
#define MFB_MFB4I_ERR_STAT_HW            (MFB_BASE_HW + 0x26C)
#define MFB_MFBI_B_ERR_STAT_HW           (MFB_BASE_HW + 0x270)
#define MFB_MFB2I_B_ERR_STAT_HW          (MFB_BASE_HW + 0x274)
#define MFB_DMA_DEBUG_ADDR_HW            (MFB_BASE_HW + 0x278)
#define MFB_DMA_RSV1_HW                  (MFB_BASE_HW + 0x27C)
#define MFB_DMA_RSV2_HW                  (MFB_BASE_HW + 0x280)
#define MFB_DMA_RSV3_HW                  (MFB_BASE_HW + 0x284)
#define MFB_DMA_RSV4_HW                  (MFB_BASE_HW + 0x288)
#define MFB_DMA_DEBUG_SEL_HW             (MFB_BASE_HW + 0x28C)
#define MFB_DMA_BW_SELF_TEST_HW          (MFB_BASE_HW + 0x290)
#define MFB_MFBO_B_BASE_ADDR_HW          (MFB_BASE_HW + 0x2A0)
#define MFB_MFBO_B_OFST_ADDR_HW          (MFB_BASE_HW + 0x2A8)
#define MFB_MFBO_B_XSIZE_HW              (MFB_BASE_HW + 0x2B0)
#define MFB_MFBO_B_YSIZE_HW              (MFB_BASE_HW + 0x2B4)
#define MFB_MFBO_B_STRIDE_HW             (MFB_BASE_HW + 0x2B8)
#define MFB_MFBO_B_CON_HW                (MFB_BASE_HW + 0x2BC)
#define MFB_MFBO_B_CON2_HW               (MFB_BASE_HW + 0x2C0)
#define MFB_MFBO_B_CON3_HW               (MFB_BASE_HW + 0x2C4)
#define MFB_MFBO_B_CROP_HW               (MFB_BASE_HW + 0x2C8)
#define MFB_MFBI_B_BASE_ADDR_HW          (MFB_BASE_HW + 0x2D0)
#define MFB_MFBI_B_OFST_ADDR_HW          (MFB_BASE_HW + 0x2D8)
#define MFB_MFBI_B_XSIZE_HW              (MFB_BASE_HW + 0x2E0)
#define MFB_MFBI_B_YSIZE_HW              (MFB_BASE_HW + 0x2E4)
#define MFB_MFBI_B_STRIDE_HW             (MFB_BASE_HW + 0x2E8)
#define MFB_MFBI_B_CON_HW                (MFB_BASE_HW + 0x2EC)
#define MFB_MFBI_B_CON2_HW               (MFB_BASE_HW + 0x2F0)
#define MFB_MFBI_B_CON3_HW               (MFB_BASE_HW + 0x2F4)
#define MFB_MFB2I_B_BASE_ADDR_HW         (MFB_BASE_HW + 0x300)
#define MFB_MFB2I_B_OFST_ADDR_HW         (MFB_BASE_HW + 0x308)
#define MFB_MFB2I_B_XSIZE_HW             (MFB_BASE_HW + 0x310)
#define MFB_MFB2I_B_YSIZE_HW             (MFB_BASE_HW + 0x314)
#define MFB_MFB2I_B_STRIDE_HW            (MFB_BASE_HW + 0x318)
#define MFB_MFB2I_B_CON_HW               (MFB_BASE_HW + 0x31C)
#define MFB_MFB2I_B_CON2_HW              (MFB_BASE_HW + 0x320)
#define MFB_MFB2I_B_CON3_HW              (MFB_BASE_HW + 0x324)

#define SRZ_CONTROL_HW                   (MFB_BASE_HW + 0x340)
#define SRZ_IN_IMG_HW                    (MFB_BASE_HW + 0x344)
#define SRZ_OUT_IMG_HW                   (MFB_BASE_HW + 0x348)
#define SRZ_HORI_STEP_HW                 (MFB_BASE_HW + 0x34C)
#define SRZ_VERT_STEP_HW                 (MFB_BASE_HW + 0x350)
#define SRZ_HORI_INT_OFST_HW             (MFB_BASE_HW + 0x354)
#define SRZ_HORI_SUB_OFST_HW             (MFB_BASE_HW + 0x358)
#define SRZ_VERT_INT_OFST_HW             (MFB_BASE_HW + 0x35C)
#define SRZ_VERT_SUB_OFST_HW             (MFB_BASE_HW + 0x360)

#define C02A_CON_HW                      (MFB_BASE_HW + 0x380)
#define C02A_CROP_CON1_HW                (MFB_BASE_HW + 0x384)
#define C02A_CROP_CON2_HW                (MFB_BASE_HW + 0x388)
#define C02B_CON_HW                      (MFB_BASE_HW + 0x3A0)
#define C02B_CROP_CON1_HW                (MFB_BASE_HW + 0x3A4)
#define C02B_CROP_CON2_HW                (MFB_BASE_HW + 0x3A8)

#define CRSP_CTRL_HW                     (MFB_BASE_HW + 0x3C0)
#define CRSP_OUT_IMG_HW                  (MFB_BASE_HW + 0x3C8)
#define CRSP_STEP_OFST_HW                (MFB_BASE_HW + 0x3CC)
#define CRSP_CROP_X_HW                   (MFB_BASE_HW + 0x3D0)
#define CRSP_CROP_Y_HW                   (MFB_BASE_HW + 0x3D4)

/*SW Access Registers : using mapped base address from DTS*/
#define MFB_CON_REG                       (ISP_MFB_BASE)
#define MFB_LL_CON1_REG                   (ISP_MFB_BASE + 0x04)
#define MFB_LL_CON2_REG                   (ISP_MFB_BASE + 0x08)
#define MFB_LL_CON3_REG                   (ISP_MFB_BASE + 0x0C)
#define MFB_LL_CON4_REG                   (ISP_MFB_BASE + 0x10)
#define MFB_EDGE_REG                      (ISP_MFB_BASE + 0x14)
#define MFB_LL_CON5_REG                   (ISP_MFB_BASE + 0x18)
#define MFB_LL_CON6_REG                   (ISP_MFB_BASE + 0x1C)
#define MFB_LL_CON7_REG                   (ISP_MFB_BASE + 0x20)
#define MFB_LL_CON8_REG                   (ISP_MFB_BASE + 0x24)
#define MFB_LL_CON9_REG                   (ISP_MFB_BASE + 0x28)
#define MFB_LL_CON10_REG                  (ISP_MFB_BASE + 0x2c)
#define MFB_MBD_CON0_REG                  (ISP_MFB_BASE + 0x30)
#define MFB_MBD_CON1_REG                  (ISP_MFB_BASE + 0x34)
#define MFB_MBD_CON2_REG                  (ISP_MFB_BASE + 0x38)
#define MFB_MBD_CON3_REG                  (ISP_MFB_BASE + 0x3c)
#define MFB_MBD_CON4_REG                  (ISP_MFB_BASE + 0x40)
#define MFB_MBD_CON5_REG                  (ISP_MFB_BASE + 0x44)
#define MFB_MBD_CON6_REG                  (ISP_MFB_BASE + 0x48)
#define MFB_MBD_CON7_REG                  (ISP_MFB_BASE + 0x4C)
#define MFB_MBD_CON8_REG                  (ISP_MFB_BASE + 0x50)
#define MFB_MBD_CON9_REG                  (ISP_MFB_BASE + 0x54)
#define MFB_MBD_CON10_REG                 (ISP_MFB_BASE + 0x58)
#define MFB_TOP_CFG0_REG                  (ISP_MFB_BASE + 0x5C)
#define MFB_TOP_CFG1_REG                  (ISP_MFB_BASE + 0x60)
#define MFB_TOP_CFG2_REG                  (ISP_MFB_BASE + 0x64)
#define MFB_INT_CTL_REG                   (ISP_MFB_BASE + 0x80)
#define MFB_INT_STATUS_REG                (ISP_MFB_BASE + 0x84)
#define MFB_SW_RST_REG                    (ISP_MFB_BASE + 0x88)
#define MFB_MAIN_DCM_ST_REG               (ISP_MFB_BASE + 0x8C)
#define MFB_DMA_DCM_ST_REG                (ISP_MFB_BASE + 0x90)
#define MFB_MAIN_DCM_DIS_REG              (ISP_MFB_BASE + 0x94)
#define MFB_DMA_DCM_DIS_REG               (ISP_MFB_BASE + 0x98)
#define MFB_DBG_CTL0_REG                  (ISP_MFB_BASE + 0x9C)
#define MFB_DBG_OUT0_REG                  (ISP_MFB_BASE + 0xA0)
#define MFB_DBG_OUT1_REG                  (ISP_MFB_BASE + 0xA4)
#define MFB_DBG_OUT2_REG                  (ISP_MFB_BASE + 0xA8)
#define MFB_DBG_OUT3_REG                  (ISP_MFB_BASE + 0xAC)
#define MFB_DBG_OUT4_REG                  (ISP_MFB_BASE + 0xB0)

#define MFB_DMA_SOFT_RSTSTAT_REG          (ISP_MFB_BASE + 0x100)
#define MFB_TDRI_BASE_ADDR_REG            (ISP_MFB_BASE + 0x104)
#define MFB_TDRI_OFST_ADDR_REG            (ISP_MFB_BASE + 0x108)
#define MFB_TDRI_XSIZE_REG                (ISP_MFB_BASE + 0x10C)
#define MFB_VERTICAL_FLIP_EN_REG          (ISP_MFB_BASE + 0x110)
#define MFB_DMA_SOFT_RESET_REG            (ISP_MFB_BASE + 0x114)
#define MFB_LAST_ULTRA_EN_REG             (ISP_MFB_BASE + 0x118)
#define MFB_SPECIAL_FUN_EN_REG            (ISP_MFB_BASE + 0x11C)
#define MFB_MFBO_BASE_ADDR_REG            (ISP_MFB_BASE + 0x130)
#define MFB_MFBO_OFST_ADDR_REG            (ISP_MFB_BASE + 0x138)
#define MFB_MFBO_XSIZE_REG                (ISP_MFB_BASE + 0x140)
#define MFB_MFBO_YSIZE_REG                (ISP_MFB_BASE + 0x144)
#define MFB_MFBO_STRIDE_REG               (ISP_MFB_BASE + 0x148)
#define MFB_MFBO_CON_REG                  (ISP_MFB_BASE + 0x14C)
#define MFB_MFBO_CON2_REG                 (ISP_MFB_BASE + 0x150)
#define MFB_MFBO_CON3_REG                 (ISP_MFB_BASE + 0x154)
#define MFB_MFBO_CROP_REG                 (ISP_MFB_BASE + 0x158)
#define MFB_MFB2O_BASE_ADDR_REG           (ISP_MFB_BASE + 0x160)
#define MFB_MFB2O_OFST_ADDR_REG           (ISP_MFB_BASE + 0x168)
#define MFB_MFB2O_XSIZE_REG               (ISP_MFB_BASE + 0x170)
#define MFB_MFB2O_YSIZE_REG               (ISP_MFB_BASE + 0x174)
#define MFB_MFB2O_STRIDE_REG              (ISP_MFB_BASE + 0x178)
#define MFB_MFB2O_CON_REG                 (ISP_MFB_BASE + 0x17C)
#define MFB_MFB2O_CON2_REG                (ISP_MFB_BASE + 0x180)
#define MFB_MFB2O_CON3_REG                (ISP_MFB_BASE + 0x184)
#define MFB_MFB2O_CROP_REG                (ISP_MFB_BASE + 0x188)
#define MFB_MFBI_BASE_ADDR_REG            (ISP_MFB_BASE + 0x190)
#define MFB_MFBI_OFST_ADDR_REG            (ISP_MFB_BASE + 0x198)
#define MFB_MFBI_XSIZE_REG                (ISP_MFB_BASE + 0x1A0)
#define MFB_MFBI_YSIZE_REG                (ISP_MFB_BASE + 0x1A4)
#define MFB_MFBI_STRIDE_REG               (ISP_MFB_BASE + 0x1A8)
#define MFB_MFBI_CON_REG                  (ISP_MFB_BASE + 0x1AC)
#define MFB_MFBI_CON2_REG                 (ISP_MFB_BASE + 0x1B0)
#define MFB_MFBI_CON3_REG                 (ISP_MFB_BASE + 0x1B4)
#define MFB_MFB2I_BASE_ADDR_REG           (ISP_MFB_BASE + 0x1C0)
#define MFB_MFB2I_OFST_ADDR_REG           (ISP_MFB_BASE + 0x1C8)
#define MFB_MFB2I_XSIZE_REG               (ISP_MFB_BASE + 0x1D0)
#define MFB_MFB2I_YSIZE_REG               (ISP_MFB_BASE + 0x1D4)
#define MFB_MFB2I_STRIDE_REG              (ISP_MFB_BASE + 0x1D8)
#define MFB_MFB2I_CON_REG                 (ISP_MFB_BASE + 0x1DC)
#define MFB_MFB2I_CON2_REG                (ISP_MFB_BASE + 0x1E0)
#define MFB_MFB2I_CON3_REG                (ISP_MFB_BASE + 0x1E4)
#define MFB_MFB3I_BASE_ADDR_REG           (ISP_MFB_BASE + 0x1F0)
#define MFB_MFB3I_OFST_ADDR_REG           (ISP_MFB_BASE + 0x1F8)
#define MFB_MFB3I_XSIZE_REG               (ISP_MFB_BASE + 0x200)
#define MFB_MFB3I_YSIZE_REG               (ISP_MFB_BASE + 0x204)
#define MFB_MFB3I_STRIDE_REG              (ISP_MFB_BASE + 0x208)
#define MFB_MFB3I_CON_REG                 (ISP_MFB_BASE + 0x20C)
#define MFB_MFB3I_CON2_REG                (ISP_MFB_BASE + 0x210)
#define MFB_MFB3I_CON3_REG                (ISP_MFB_BASE + 0x214)
#define MFB_MFB4I_BASE_ADDR_REG           (ISP_MFB_BASE + 0x220)
#define MFB_MFB4I_OFST_ADDR_REG           (ISP_MFB_BASE + 0x228)
#define MFB_MFB4I_XSIZE_REG               (ISP_MFB_BASE + 0x230)
#define MFB_MFB4I_YSIZE_REG               (ISP_MFB_BASE + 0x234)
#define MFB_MFB4I_STRIDE_REG              (ISP_MFB_BASE + 0x238)
#define MFB_MFB4I_CON_REG                 (ISP_MFB_BASE + 0x23C)
#define MFB_MFB4I_CON2_REG                (ISP_MFB_BASE + 0x240)
#define MFB_MFB4I_CON3_REG                (ISP_MFB_BASE + 0x244)
#define MFB_DMA_ERR_CTRL_REG              (ISP_MFB_BASE + 0x250)
#define MFB_MFBO_ERR_STAT_REG             (ISP_MFB_BASE + 0x254)
#define MFB_MFB2O_ERR_STAT_REG            (ISP_MFB_BASE + 0x258)
#define MFB_MFBO_B_ERR_STAT_REG           (ISP_MFB_BASE + 0x25C)
#define MFB_MFBI_ERR_STAT_REG             (ISP_MFB_BASE + 0x260)
#define MFB_MFB2I_ERR_STAT_REG            (ISP_MFB_BASE + 0x264)
#define MFB_MFB3I_ERR_STAT_REG            (ISP_MFB_BASE + 0x268)
#define MFB_MFB4I_ERR_STAT_REG            (ISP_MFB_BASE + 0x26C)
#define MFB_MFBI_B_ERR_STAT_REG           (ISP_MFB_BASE + 0x270)
#define MFB_MFB2I_B_ERR_STAT_REG          (ISP_MFB_BASE + 0x274)
#define MFB_DMA_DEBUG_ADDR_REG            (ISP_MFB_BASE + 0x278)
#define MFB_DMA_RSV1_REG                  (ISP_MFB_BASE + 0x27C)
#define MFB_DMA_RSV2_REG                  (ISP_MFB_BASE + 0x280)
#define MFB_DMA_RSV3_REG                  (ISP_MFB_BASE + 0x284)
#define MFB_DMA_RSV4_REG                  (ISP_MFB_BASE + 0x288)
#define MFB_DMA_DEBUG_SEL_REG             (ISP_MFB_BASE + 0x28C)
#define MFB_DMA_BW_SELF_TEST_REG          (ISP_MFB_BASE + 0x290)
#define MFB_MFBO_B_BASE_ADDR_REG          (ISP_MFB_BASE + 0x2A0)
#define MFB_MFBO_B_OFST_ADDR_REG          (ISP_MFB_BASE + 0x2A8)
#define MFB_MFBO_B_XSIZE_REG              (ISP_MFB_BASE + 0x2B0)
#define MFB_MFBO_B_YSIZE_REG              (ISP_MFB_BASE + 0x2B4)
#define MFB_MFBO_B_STRIDE_REG             (ISP_MFB_BASE + 0x2B8)
#define MFB_MFBO_B_CON_REG                (ISP_MFB_BASE + 0x2BC)
#define MFB_MFBO_B_CON2_REG               (ISP_MFB_BASE + 0x2C0)
#define MFB_MFBO_B_CON3_REG               (ISP_MFB_BASE + 0x2C4)
#define MFB_MFBO_B_CROP_REG               (ISP_MFB_BASE + 0x2C8)
#define MFB_MFBI_B_BASE_ADDR_REG          (ISP_MFB_BASE + 0x2D0)
#define MFB_MFBI_B_OFST_ADDR_REG          (ISP_MFB_BASE + 0x2D8)
#define MFB_MFBI_B_XSIZE_REG              (ISP_MFB_BASE + 0x2E0)
#define MFB_MFBI_B_YSIZE_REG              (ISP_MFB_BASE + 0x2E4)
#define MFB_MFBI_B_STRIDE_REG             (ISP_MFB_BASE + 0x2E8)
#define MFB_MFBI_B_CON_REG                (ISP_MFB_BASE + 0x2EC)
#define MFB_MFBI_B_CON2_REG               (ISP_MFB_BASE + 0x2F0)
#define MFB_MFBI_B_CON3_REG               (ISP_MFB_BASE + 0x2F4)
#define MFB_MFB2I_B_BASE_ADDR_REG         (ISP_MFB_BASE + 0x300)
#define MFB_MFB2I_B_OFST_ADDR_REG         (ISP_MFB_BASE + 0x308)
#define MFB_MFB2I_B_XSIZE_REG             (ISP_MFB_BASE + 0x310)
#define MFB_MFB2I_B_YSIZE_REG             (ISP_MFB_BASE + 0x314)
#define MFB_MFB2I_B_STRIDE_REG            (ISP_MFB_BASE + 0x318)
#define MFB_MFB2I_B_CON_REG               (ISP_MFB_BASE + 0x31C)
#define MFB_MFB2I_B_CON2_REG              (ISP_MFB_BASE + 0x320)
#define MFB_MFB2I_B_CON3_REG              (ISP_MFB_BASE + 0x324)

#define SRZ_CONTROL_REG                   (ISP_MFB_BASE + 0x340)
#define SRZ_IN_IMG_REG                    (ISP_MFB_BASE + 0x344)
#define SRZ_OUT_IMG_REG                   (ISP_MFB_BASE + 0x348)
#define SRZ_HORI_STEP_REG                 (ISP_MFB_BASE + 0x34C)
#define SRZ_VERT_STEP_REG                 (ISP_MFB_BASE + 0x350)
#define SRZ_HORI_INT_OFST_REG             (ISP_MFB_BASE + 0x354)
#define SRZ_HORI_SUB_OFST_REG             (ISP_MFB_BASE + 0x358)
#define SRZ_VERT_INT_OFST_REG             (ISP_MFB_BASE + 0x35C)
#define SRZ_VERT_SUB_OFST_REG             (ISP_MFB_BASE + 0x360)

#define C02A_CON_REG                      (ISP_MFB_BASE + 0x380)
#define C02A_CROP_CON1_REG                (ISP_MFB_BASE + 0x384)
#define C02A_CROP_CON2_REG                (ISP_MFB_BASE + 0x388)
#define C02B_CON_REG                      (ISP_MFB_BASE + 0x3A0)
#define C02B_CROP_CON1_REG                (ISP_MFB_BASE + 0x3A4)
#define C02B_CROP_CON2_REG                (ISP_MFB_BASE + 0x3A8)

#define CRSP_CTRL_REG                     (ISP_MFB_BASE + 0x3C0)
#define CRSP_OUT_IMG_REG                  (ISP_MFB_BASE + 0x3C8)
#define CRSP_STEP_OFST_REG                (ISP_MFB_BASE + 0x3CC)
#define CRSP_CROP_X_REG                   (ISP_MFB_BASE + 0x3D0)
#define CRSP_CROP_Y_REG                   (ISP_MFB_BASE + 0x3D4)

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int MFB_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int MFB_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int MFB_GetIRQState(
	unsigned int type, unsigned int userNumber, unsigned int stus,
	enum MFB_PROCESS_ID_ENUM whichReq, int ProcessID)
{
	unsigned int ret = 0;
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */

	/*  */
	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[type]), flags);
#ifdef MFB_USE_GCE

#ifdef MFB_MULTIPROCESS_TIMEING_ISSUE
	if (stus & MFB_INT_ST) {
		ret = ((MFBInfo.IrqInfo.MfbIrqCnt > 0)
		       && (MFBInfo.ProcessID[MFBInfo.ReadReqIdx] == ProcessID));
	} else {
		log_err(
		    " WaitIRQ StatusErr, type:%d, userNum:%d, status:%d, whichReq:%d,ProcessID:0x%x, ReadReqIdx:%d\n",
		    type, userNumber, stus, whichReq,
		    ProcessID, MFBInfo.ReadReqIdx);
	}

#else
	if (stus & MFB_INT_ST) {
		ret = ((MFBInfo.IrqInfo.MfbIrqCnt > 0)
		       && (MFBInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
	} else {
		log_err(
		    "WaitIRQ Status Error, type:%d, userNumber:%d, status:%d, whichReq:%d, ProcessID:0x%x\n",
		    type, userNumber, stus, whichReq, ProcessID);
	}
#endif
#else
	ret = ((MFBInfo.IrqInfo.Status[type] & stus)
	       && (MFBInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
#endif
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[type]), flags);
	/*  */
	return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
static inline unsigned int MFB_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}


#define RegDump(start, end) {\
	unsigned int i;\
	for (i = start; i <= end; i += 0x10) {\
		log_dbg(\
	    "[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]",\
	    (unsigned int)(ISP_MFB_BASE + i),\
	    (unsigned int)MFB_RD32(ISP_MFB_BASE + i),\
	    (unsigned int)(ISP_MFB_BASE + i+0x4),\
	    (unsigned int)MFB_RD32(ISP_MFB_BASE + i+0x4),\
	    (unsigned int)(ISP_MFB_BASE + i+0x8),\
	    (unsigned int)MFB_RD32(ISP_MFB_BASE + i+0x8),\
	    (unsigned int)(ISP_MFB_BASE + i+0xc),\
	    (unsigned int)MFB_RD32(ISP_MFB_BASE + i+0xc));\
	} \
}


static bool ConfigMFBRequest(signed int ReqIdx)
{
#ifdef MFB_USE_GCE
	unsigned int j;
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */


	spin_lock_irqsave(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]), flags);
	if (g_MFB_ReqRing.MFBReq_Struct[ReqIdx].State ==
	    MFB_REQUEST_STATE_PENDING) {
		g_MFB_ReqRing.MFBReq_Struct[ReqIdx].State =
		    MFB_REQUEST_STATE_RUNNING;
		for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
			if (MFB_FRAME_STATUS_ENQUE ==
			    g_MFB_ReqRing.MFBReq_Struct[ReqIdx]
			    .MfbFrameStatus[j]) {
				g_MFB_ReqRing.MFBReq_Struct[ReqIdx]
				    .MfbFrameStatus[j] =
				MFB_FRAME_STATUS_RUNNING;
				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[
					    MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
				ConfigMFBHW(&g_MFB_ReqRing
					.MFBReq_Struct[ReqIdx]
					.MfbFrameConfig[j]);
				spin_lock_irqsave(
					&(MFBInfo.SpinLockIrq[
					    MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
			}
		}
	} else {
		log_err("%s state machine error!!, ReqIdx:%d, State:%d\n",
			__func__, ReqIdx,
			g_MFB_ReqRing.MFBReq_Struct[ReqIdx].State);
	}
	spin_unlock_irqrestore(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]), flags);


	return MTRUE;
#else
	log_err("%s don't support this mode.!!\n", __func__);
	return MFALSE;
#endif
}


static bool ConfigMFB(void)
{
#ifdef MFB_USE_GCE

	unsigned int i, j, k;
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */


	spin_lock_irqsave(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]), flags);
	for (k = 0; k < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; k++) {
		i = (g_MFB_ReqRing.HWProcessIdx + k) %
		    _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
		if (g_MFB_ReqRing.MFBReq_Struct[i].State ==
		    MFB_REQUEST_STATE_PENDING) {
			g_MFB_ReqRing.MFBReq_Struct[i].State =
			    MFB_REQUEST_STATE_RUNNING;
			for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
				if (MFB_FRAME_STATUS_ENQUE ==
				    g_MFB_ReqRing.MFBReq_Struct[i]
				    .MfbFrameStatus[j]) {
					/* break; */
					g_MFB_ReqRing.MFBReq_Struct[i]
					    .MfbFrameStatus[j] =
					    MFB_FRAME_STATUS_RUNNING;
					spin_unlock_irqrestore(
						&(MFBInfo.SpinLockIrq[
						    MFB_IRQ_TYPE_INT_MFB_ST]),
						flags);
					ConfigMFBHW(
						&g_MFB_ReqRing.MFBReq_Struct[i]
						.MfbFrameConfig[j]);
					spin_lock_irqsave(
						&(MFBInfo.SpinLockIrq[
						    MFB_IRQ_TYPE_INT_MFB_ST]),
						flags);
				}
			}
			/* log_dbg("ConfigMFB idx j:%d\n",j); */
			if (j != _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
				log_err(
				    "MFB Config State is wrong!  idx j(%d), HWProcessIdx(%d), State(%d)\n",
				    j, g_MFB_ReqRing.HWProcessIdx,
				    g_MFB_ReqRing.MFBReq_Struct[i].State);
				/*
				 *g_MFB_ReqRing.MFBReq_Struct[i]
				 *	.MfbFrameStatus[j] =
				 *	MFB_FRAME_STATUS_RUNNING;
				 *spin_unlock_irqrestore(
				 *	&(MFBInfo.SpinLockIrq[
				 *	    MFB_IRQ_TYPE_INT_MFB_ST]),
				 *	flags);
				 *ConfigMFBHW(
				 *	&g_MFB_ReqRing.MFBReq_Struct[i]
				 *	.MfbFrameConfig[j]);
				 *return MTRUE;
				 */
				return MFALSE;
			}
			#if 0
			else {
				g_MFB_ReqRing.MFBReq_Struct[i]
					.State = MFB_REQUEST_STATE_RUNNING;
				log_err(
					"MFB Config State is wrong! HWProcessIdx(%d), State(%d)\n",
					g_MFB_ReqRing.HWProcessIdx,
					g_MFB_ReqRing.MFBReq_Struct[i].State);
				g_MFB_ReqRing.HWProcessIdx =
					(g_MFB_ReqRing.HWProcessIdx+1)
					%_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
			}
			#endif

		}
	}
	spin_unlock_irqrestore(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]), flags);
	if (k == _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_)
		log_dbg("No any MFB Request in Ring!!\n");

	return MTRUE;


#else				/* #ifdef MFB_USE_GCE */

	unsigned int i, j, k;
	unsigned int flags;

	for (k = 0; k < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; k++) {
		i = (g_MFB_ReqRing.HWProcessIdx + k) %
		    _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
		if (g_MFB_ReqRing.MFBReq_Struct[i].State ==
		    MFB_REQUEST_STATE_PENDING) {
			for (j = 0; j <
			    _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
				if (MFB_FRAME_STATUS_ENQUE ==
					g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[j]) {
					break;
				}
			}
			log_dbg("%s idx j:%d\n", __func__, j);
			if (j != _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
				g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[j] =
					MFB_FRAME_STATUS_RUNNING;
				ConfigMFBHW(
					&g_MFB_ReqRing.MFBReq_Struct[i]
					 .MfbFrameConfig[j]);
				return MTRUE;
			}
			/*else {*/
			log_err(
			    "MFB Config State is wrong! HWProcessIdx(%d), State(%d)\n",
			    g_MFB_ReqRing.HWProcessIdx,
			    g_MFB_ReqRing.MFBReq_Struct[i].State);
			g_MFB_ReqRing.HWProcessIdx =
			   (g_MFB_ReqRing.HWProcessIdx +
			    1) % _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
			/*}*/
		}
	}
	if (k == _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_)
		log_dbg("No any MFB Request in Ring!!\n");

	return MFALSE;

#endif				/* #ifdef MFB_USE_GCE */



}


static bool UpdateMFB(pid_t *ProcessID)
{
#ifdef MFB_USE_GCE
	unsigned int i, j, next_idx;
	bool bFinishRequest = MFALSE;

	for (i = g_MFB_ReqRing.HWProcessIdx;
	    i < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; i++) {
		if (g_MFB_ReqRing.MFBReq_Struct[i].State ==
		    MFB_REQUEST_STATE_RUNNING) {
			for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
				if (MFB_FRAME_STATUS_RUNNING ==
					g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[j]) {
					break;
				}
			}
			IRQ_LOG_KEEPER(
				MFB_IRQ_TYPE_INT_MFB_ST,
				m_CurrentPPB,
				_LOG_DBG,
				"%s idx j:%d\n",
				__func__, j);
			if (j != _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
				next_idx = j + 1;
				g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[j] =
					MFB_FRAME_STATUS_FINISHED;
				if ((_SUPPORT_MAX_MFB_FRAME_REQUEST_ ==
				    (next_idx)) ||
				    ((_SUPPORT_MAX_MFB_FRAME_REQUEST_ >
				    (next_idx)) &&
				    (MFB_FRAME_STATUS_EMPTY ==
				     g_MFB_ReqRing.MFBReq_Struct[i]
				    .MfbFrameStatus[next_idx]))) {
					bFinishRequest = MTRUE;
					(*ProcessID) =
					    g_MFB_ReqRing.MFBReq_Struct[i]
					    .processID;
					g_MFB_ReqRing.MFBReq_Struct[i]
					    .State =
					    MFB_REQUEST_STATE_FINISHED;
					g_MFB_ReqRing.HWProcessIdx =
					    (g_MFB_ReqRing.HWProcessIdx + 1) %
					    _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
					IRQ_LOG_KEEPER(
						MFB_IRQ_TYPE_INT_MFB_ST,
						m_CurrentPPB,
						_LOG_INF,
						"Finish MFB Request i:%d, j:%d, HWProcessIdx:%d\n",
						i, j,
						g_MFB_ReqRing.HWProcessIdx);
				} else {
					IRQ_LOG_KEEPER(
						MFB_IRQ_TYPE_INT_MFB_ST,
						m_CurrentPPB,
						_LOG_DBG,
						"Finish MFB Frame i:%d, j:%d, HWProcessIdx:%d\n",
						i, j,
						g_MFB_ReqRing.HWProcessIdx);
				}
				break;
			}
			/*else {*/
			IRQ_LOG_KEEPER(
				MFB_IRQ_TYPE_INT_MFB_ST,
				m_CurrentPPB, _LOG_ERR,
				"MFB State Machine is wrong! HWProcessIdx(%d), State(%d)\n",
				g_MFB_ReqRing.HWProcessIdx,
				g_MFB_ReqRing.MFBReq_Struct[i].State);
			g_MFB_ReqRing.MFBReq_Struct[i].State =
			    MFB_REQUEST_STATE_FINISHED;
			g_MFB_ReqRing.HWProcessIdx =
			    (g_MFB_ReqRing.HWProcessIdx +
			     1) % _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
			break;
			/*}*/
		}
	}

	return bFinishRequest;


#else				/* #ifdef MFB_USE_GCE */
	unsigned int i, j, next_idx;
	bool bFinishRequest = MFALSE;

	for (i = g_MFB_ReqRing.HWProcessIdx;
	    i < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; i++) {
		if (g_MFB_ReqRing.MFBReq_Struct[i].State ==
		    MFB_REQUEST_STATE_PENDING) {
			for (j = 0;
			    j < _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
				if (MFB_FRAME_STATUS_RUNNING ==
				    g_MFB_ReqRing.MFBReq_Struct[i]
				    .MfbFrameStatus[j]) {
					break;
				}
			}
			IRQ_LOG_KEEPER(
				MFB_IRQ_TYPE_INT_MFB_ST,
				m_CurrentPPB, _LOG_DBG,
				"%s idx j:%d\n",
				__func__, j);
			if (j != _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
				next_idx = j + 1;
				g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[j] =
					MFB_FRAME_STATUS_FINISHED;
				if ((_SUPPORT_MAX_MFB_FRAME_REQUEST_ ==
				    (next_idx)) ||
				    ((_SUPPORT_MAX_MFB_FRAME_REQUEST_ >
				    (next_idx)) && (MFB_FRAME_STATUS_EMPTY ==
				    g_MFB_ReqRing.MFBReq_Struct[i]
				    .MfbFrameStatus[next_idx]))) {
					bFinishRequest = MTRUE;
					(*ProcessID) =
					    g_MFB_ReqRing.MFBReq_Struct[i]
					    .processID;
					g_MFB_ReqRing.MFBReq_Struct[i].State =
					    MFB_REQUEST_STATE_FINISHED;
					g_MFB_ReqRing.HWProcessIdx =
					    (g_MFB_ReqRing.HWProcessIdx + 1) %
					    _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
					IRQ_LOG_KEEPER(
						MFB_IRQ_TYPE_INT_MFB_ST,
						m_CurrentPPB,
						_LOG_INF,
						"Finish MFB Request i:%d, j:%d, HWProcessIdx:%d\n",
						i, j,
						g_MFB_ReqRing.HWProcessIdx);
				} else {
					IRQ_LOG_KEEPER(
						MFB_IRQ_TYPE_INT_MFB_ST,
						m_CurrentPPB,
						_LOG_DBG,
						"Finish MFB Frame i:%d, j:%d, HWProcessIdx:%d\n",
						i, j,
						g_MFB_ReqRing.HWProcessIdx);
				}
				break;
			}
			/*else {*/
			IRQ_LOG_KEEPER(
				MFB_IRQ_TYPE_INT_MFB_ST,
				m_CurrentPPB, _LOG_ERR,
				"MFB State Machine is wrong! HWProcessIdx(%d), State(%d)\n",
				g_MFB_ReqRing.HWProcessIdx,
				g_MFB_ReqRing.MFBReq_Struct[i].State);
			g_MFB_ReqRing.MFBReq_Struct[i].State =
				MFB_REQUEST_STATE_FINISHED;
			g_MFB_ReqRing.HWProcessIdx =
				(g_MFB_ReqRing.HWProcessIdx + 1) %
				_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
			break;
			/*}*/
		}
	}

	return bFinishRequest;

#endif				/* #ifdef MFB_USE_GCE */


}

static signed int ConfigMFBHW(MFB_Config *pMfbConfig)
#if !BYPASS_REG
{
#ifdef MFB_USE_GCE
		struct cmdqRecStruct *handle;
		uint64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_MFB);
#endif

	if (MFB_DBG_DBGLOG == (MFB_DBG_DBGLOG & MFBInfo.DebugMask)) {

		log_dbg("ConfigMFBHW Start!\n");
		log_dbg("MFB_TOP_CFG0:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_TOP_CFG0);
		log_dbg("MFB_TOP_CFG2:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_TOP_CFG2);
		log_dbg("MFB_MFBI_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFBI_ADDR);
		log_dbg("MFB_MFBI_B_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFBI_B_ADDR);
		log_dbg("MFB_MFB2I_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFB2I_ADDR);
		log_dbg("MFB_MFB2I_B_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFB2I_B_ADDR);
		log_dbg("MFB_MFB3I_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFB3I_ADDR);
		log_dbg("MFB_MFB4I_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFB4I_ADDR);
		log_dbg("MFB_MFBO_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFBO_ADDR);
		log_dbg("MFB_MFBO_B_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFBO_B_ADDR);
		log_dbg("MFB_MFB2O_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFB2O_ADDR);
		log_dbg("MFB_TDRI_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_TDRI_ADDR);

		log_dbg("MFB_TOP_CFG0:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_TOP_CFG0);
		log_dbg("MFB_TOP_CFG2:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_TOP_CFG2);
		log_dbg("MFB_MFBI_STRIDE:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFBI_STRIDE);
		log_dbg("MFB_MFB2I_STRIDE:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFB2I_STRIDE);
		log_dbg("MFB_MFBO_STRIDE:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFBO_STRIDE);
		log_dbg("MFB_CON:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_CON);

	}

#ifdef MFB_USE_GCE

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("ConfigMFBHW");
#endif

	cmdqRecCreate(CMDQ_SCENARIO_KERNEL_CONFIG_GENERAL, &handle);
	/* CMDQ driver dispatches CMDQ HW thread and HW thread's
	 * priority according to scenario
	 */

	cmdqRecSetEngine(handle, engineFlag);

	cmdqRecReset(handle);

#if 1
	/* Use command queue to write register */
	/* BIT0 for INT_EN, BIT20 for CHROMA_INT_EN, BIT21 for WEIGHT_INT_EN */
	cmdqRecWrite(handle, MFB_INT_CTL_HW, 0x300001, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_TOP_CFG0_HW,
		pMfbConfig->MFB_TOP_CFG0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_TOP_CFG2_HW,
		pMfbConfig->MFB_TOP_CFG2, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_MFBI_BASE_ADDR_HW,
		pMfbConfig->MFB_MFBI_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBI_STRIDE_HW,
		pMfbConfig->MFB_MFBI_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBI_XSIZE_HW,
		(pMfbConfig->MFB_MFBI_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBI_YSIZE_HW,
		pMfbConfig->MFB_MFBI_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBI_CON_HW,
		0x80000040, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_MFBI_B_BASE_ADDR_HW,
		pMfbConfig->MFB_MFBI_B_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBI_B_STRIDE_HW,
		pMfbConfig->MFB_MFBI_B_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBI_B_XSIZE_HW,
		(pMfbConfig->MFB_MFBI_B_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBI_B_YSIZE_HW,
		pMfbConfig->MFB_MFBI_B_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBI_B_CON_HW,
		0x80000040, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_MFB2I_BASE_ADDR_HW,
		pMfbConfig->MFB_MFB2I_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2I_STRIDE_HW,
		pMfbConfig->MFB_MFB2I_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2I_XSIZE_HW,
		(pMfbConfig->MFB_MFB2I_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2I_YSIZE_HW,
		pMfbConfig->MFB_MFB2I_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2I_CON_HW,
		0x80000040, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_MFB2I_B_BASE_ADDR_HW,
		pMfbConfig->MFB_MFB2I_B_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2I_B_STRIDE_HW,
		pMfbConfig->MFB_MFB2I_B_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2I_B_XSIZE_HW,
		(pMfbConfig->MFB_MFB2I_B_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2I_B_YSIZE_HW,
		pMfbConfig->MFB_MFB2I_B_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2I_B_CON_HW,
		0x80000040, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_MFB3I_BASE_ADDR_HW,
		pMfbConfig->MFB_MFB3I_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB3I_STRIDE_HW,
		pMfbConfig->MFB_MFB3I_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB3I_XSIZE_HW,
		(pMfbConfig->MFB_MFB3I_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB3I_YSIZE_HW,
		pMfbConfig->MFB_MFB3I_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB3I_CON_HW,
		0x80000040, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_MFB4I_BASE_ADDR_HW,
		pMfbConfig->MFB_MFB4I_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB4I_STRIDE_HW,
		pMfbConfig->MFB_MFB4I_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB4I_XSIZE_HW,
		(pMfbConfig->MFB_MFB4I_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB4I_YSIZE_HW,
		pMfbConfig->MFB_MFB4I_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB4I_CON_HW,
		0x80000040, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_MFBO_BASE_ADDR_HW,
		pMfbConfig->MFB_MFBO_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBO_STRIDE_HW,
		pMfbConfig->MFB_MFBO_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBO_XSIZE_HW,
		(pMfbConfig->MFB_MFBO_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBO_YSIZE_HW,
		pMfbConfig->MFB_MFBO_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBO_CON_HW,
		0x80000040, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_MFBO_B_BASE_ADDR_HW,
		pMfbConfig->MFB_MFBO_B_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBO_B_STRIDE_HW,
		pMfbConfig->MFB_MFBO_B_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBO_B_XSIZE_HW,
		(pMfbConfig->MFB_MFBO_B_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBO_B_YSIZE_HW,
		pMfbConfig->MFB_MFBO_B_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFBO_B_CON_HW,
		0x80000020, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_MFB2O_BASE_ADDR_HW,
		pMfbConfig->MFB_MFB2O_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2O_STRIDE_HW,
		pMfbConfig->MFB_MFB2O_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2O_XSIZE_HW,
		(pMfbConfig->MFB_MFB2O_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2O_YSIZE_HW,
		pMfbConfig->MFB_MFB2O_YSIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB2O_CON_HW,
		0x80000020, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_TDRI_BASE_ADDR_HW,
		pMfbConfig->MFB_TDRI_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_TDRI_XSIZE_HW,
		pMfbConfig->MFB_TDRI_XSIZE, CMDQ_REG_MASK);

	cmdqRecWrite(handle, SRZ_CONTROL_HW,
		pMfbConfig->MFB_SRZ_CTRL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_IN_IMG_HW,
		pMfbConfig->MFB_SRZ_IN_IMG, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_OUT_IMG_HW,
		pMfbConfig->MFB_SRZ_OUT_IMG, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_HORI_STEP_HW,
		pMfbConfig->MFB_SRZ_HORI_STEP, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_VERT_STEP_HW,
		pMfbConfig->MFB_SRZ_VERT_STEP, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_HORI_INT_OFST_HW,
		pMfbConfig->MFB_SRZ_HORI_INT_OFST, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_HORI_SUB_OFST_HW,
		pMfbConfig->MFB_SRZ_HORI_SUB_OFST, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_VERT_INT_OFST_HW,
		pMfbConfig->MFB_SRZ_VERT_INT_OFST, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_VERT_SUB_OFST_HW,
		pMfbConfig->MFB_SRZ_VERT_SUB_OFST, CMDQ_REG_MASK);

	cmdqRecWrite(handle, C02A_CON_HW,
		pMfbConfig->MFB_C02A_CON, CMDQ_REG_MASK);
	cmdqRecWrite(handle, C02B_CON_HW,
		pMfbConfig->MFB_C02B_CON, CMDQ_REG_MASK);

	cmdqRecWrite(handle, CRSP_CTRL_HW,
		pMfbConfig->MFB_CRSP_CTRL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, CRSP_OUT_IMG_HW,
		pMfbConfig->MFB_CRSP_OUT_IMG, CMDQ_REG_MASK);
	cmdqRecWrite(handle, CRSP_STEP_OFST_HW,
		pMfbConfig->MFB_CRSP_STEP_OFST, CMDQ_REG_MASK);

#ifdef MFB_TUNABLE
	cmdqRecWrite(handle, MFB_CON_HW,
		pMfbConfig->MFB_CON, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON1_HW,
		pMfbConfig->MFB_LL_CON1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON2_HW,
		pMfbConfig->MFB_LL_CON2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON3_HW,
		pMfbConfig->MFB_LL_CON3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON4_HW,
		pMfbConfig->MFB_LL_CON4, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_EDGE_HW,
		pMfbConfig->MFB_EDGE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON5_HW,
		pMfbConfig->MFB_LL_CON5, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON6_HW,
		pMfbConfig->MFB_LL_CON6, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON7_HW,
		pMfbConfig->MFB_LL_CON7, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON8_HW,
		pMfbConfig->MFB_LL_CON8, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON9_HW,
		pMfbConfig->MFB_LL_CON9, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON10_HW,
		pMfbConfig->MFB_LL_CON10, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON0_HW,
		pMfbConfig->MFB_MBD_CON0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON1_HW,
		pMfbConfig->MFB_MBD_CON1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON2_HW,
		pMfbConfig->MFB_MBD_CON2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON3_HW,
		pMfbConfig->MFB_MBD_CON3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON4_HW,
		pMfbConfig->MFB_MBD_CON4, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON5_HW,
		pMfbConfig->MFB_MBD_CON5, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON6_HW,
		pMfbConfig->MFB_MBD_CON6, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON7_HW,
		pMfbConfig->MFB_MBD_CON7, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON8_HW,
		pMfbConfig->MFB_MBD_CON8, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON9_HW,
		pMfbConfig->MFB_MBD_CON9, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON10_HW,
		pMfbConfig->MFB_MBD_CON10, CMDQ_REG_MASK);
#endif

	/* Disable MFB DCM if necessary */
	/* cmdqRecWrite(handle, MFB_MAIN_DCM_DIS_HW, */
	/*	0xFFFFFFFF, CMDQ_REG_MASK); */
	/* cmdqRecWrite(handle, MFB_DMA_DCM_DIS_HW, */
	/*	0xFFFFFFFF, CMDQ_REG_MASK); */

	cmdqRecWrite(handle, MFB_TOP_CFG1_HW, 0x1, CMDQ_REG_MASK);
	cmdqRecWait(handle, CMDQ_EVENT_MFB_DONE);
	cmdqRecWrite(handle, MFB_TOP_CFG1_HW, 0x0, CMDQ_REG_MASK);
#endif
	/* non-blocking API, Please  use cmdqRecFlushAsync() */
	cmdqRecFlushAsync(handle);
	cmdqRecReset(handle);	/* if you want to re-use the handle,
				 * please reset the handle
				 */
	cmdqRecDestroy(handle);	/* recycle the memory */

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif

#else

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("ConfigMFBHW");
#endif
#if 0
	/* MFB Interrupt enabled in read-clear mode */
	MFB_WR32(MFB_INT_CTL_REG, 0x1);

	MFB_WR32(MFB_CTRL_REG, pMfbConfig->MFB_CTRL);
	MFB_WR32(MFB_SIZE_REG, pMfbConfig->MFB_SIZE);

	MFB_WR32(MFB_APLI_C_BASE_ADDR_REG, pMfbConfig->MFB_APLI_C_BASE_ADDR);
	MFB_WR32(MFB_APLI_P_BASE_ADDR_REG, pMfbConfig->MFB_APLI_P_BASE_ADDR);
	MFB_WR32(MFB_IMGI_C_BASE_ADDR_REG, pMfbConfig->MFB_IMGI_C_BASE_ADDR);
	MFB_WR32(MFB_IMGI_P_BASE_ADDR_REG, pMfbConfig->MFB_IMGI_P_BASE_ADDR);
	MFB_WR32(MFB_IMGI_C_STRIDE_REG, pMfbConfig->MFB_IMGI_C_STRIDE);
	MFB_WR32(MFB_IMGI_P_STRIDE_REG, pMfbConfig->MFB_IMGI_P_STRIDE);

	MFB_WR32(MFB_MVI_BASE_ADDR_REG, pMfbConfig->MFB_MVI_BASE_ADDR);
	MFB_WR32(MFB_MVI_STRIDE_REG, pMfbConfig->MFB_MVI_STRIDE);

	MFB_WR32(MFB_MVO_BASE_ADDR_REG, pMfbConfig->MFB_MVO_BASE_ADDR);
	MFB_WR32(MFB_MVO_STRIDE_REG, pMfbConfig->MFB_MVO_STRIDE);
	MFB_WR32(MFB_BVO_BASE_ADDR_REG, pMfbConfig->MFB_BVO_BASE_ADDR);
	MFB_WR32(MFB_BVO_STRIDE_REG, pMfbConfig->MFB_BVO_STRIDE);

	MFB_WR32(MFB_START_REG, 0x1);	/* MFB Interrupt read-clear mode */
#endif
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif

#endif
	return 0;
}
#else
{
	return 0;
}
#endif

#define MFB_IS_BUSY    0x2

#ifndef MFB_USE_GCE

static bool Check_MFB_Is_Busy(void)
#if !BYPASS_REG
{
	unsigned int Ctrl _Fsm;
	unsigned int Mfb_Start;

	Ctrl_Fsm = MFB_RD32(MFB_DBG_INFO_00_REG);
	Mfb_Start = MFB_RD32(MFB_START_REG);
	if ((MFB_IS_BUSY == (Ctrl_Fsm & MFB_IS_BUSY)) ||
		(MFB_START == (Mfb_Start & MFB_START)))
		return MTRUE;

	return MFALSE;
}
#else
{
	return MFLASE;
}
#endif
#endif


/*
 *
 */
static signed int MFB_DumpReg(void)
{
#if 1
	signed int Ret = 0;
	unsigned int i, j;

	log_inf("- E.");

	log_inf("MFB Config Info\n");

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_CON_HW),
		(unsigned int)MFB_RD32(MFB_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_TOP_CFG0_HW),
		(unsigned int)MFB_RD32(MFB_TOP_CFG0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_TOP_CFG1_HW),
		(unsigned int)MFB_RD32(MFB_TOP_CFG1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_TOP_CFG2_HW),
		(unsigned int)MFB_RD32(MFB_TOP_CFG2_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_INT_CTL_HW),
		(unsigned int)MFB_RD32(MFB_INT_CTL_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_INT_STATUS_HW),
		(unsigned int)MFB_RD32(MFB_INT_STATUS_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_SW_RST_HW),
		(unsigned int)MFB_RD32(MFB_SW_RST_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MAIN_DCM_ST_HW),
		(unsigned int)MFB_RD32(MFB_MAIN_DCM_ST_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MAIN_DCM_DIS_HW),
		(unsigned int)MFB_RD32(MFB_MAIN_DCM_DIS_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DMA_DCM_ST_HW),
		(unsigned int)MFB_RD32(MFB_DMA_DCM_ST_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DMA_DCM_DIS_HW),
		(unsigned int)MFB_RD32(MFB_DMA_DCM_DIS_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DBG_CTL0_HW),
		(unsigned int)MFB_RD32(MFB_DBG_CTL0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DBG_OUT0_HW),
		(unsigned int)MFB_RD32(MFB_DBG_OUT0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DBG_OUT0_HW),
		(unsigned int)MFB_RD32(MFB_DBG_OUT0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DBG_OUT1_HW),
		(unsigned int)MFB_RD32(MFB_DBG_OUT1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DBG_OUT2_HW),
		(unsigned int)MFB_RD32(MFB_DBG_OUT2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DBG_OUT3_HW),
		(unsigned int)MFB_RD32(MFB_DBG_OUT3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DBG_OUT4_HW),
		(unsigned int)MFB_RD32(MFB_DBG_OUT4_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DMA_ERR_CTRL_HW),
		(unsigned int)MFB_RD32(MFB_DMA_ERR_CTRL_REG));

	log_inf("MFB:HWProcessIdx:%d, WriteIdx:%d, ReadIdx:%d\n",
		g_MFB_ReqRing.HWProcessIdx,
		g_MFB_ReqRing.WriteIdx,
		g_MFB_ReqRing.ReadIdx);

	for (i = 0; i < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; i++) {
		log_inf(
		"MFB Req:State:%d, procID:0x%08X, callerID:0x%08X, enqueReqNum:%d, FrameWRIdx:%d, RrameRDIdx:%d\n",
		g_MFB_ReqRing.MFBReq_Struct[i].State,
		g_MFB_ReqRing.MFBReq_Struct[i].processID,
		g_MFB_ReqRing.MFBReq_Struct[i].callerID,
		g_MFB_ReqRing.MFBReq_Struct[i].enqueReqNum,
		g_MFB_ReqRing.MFBReq_Struct[i].FrameWRIdx,
		g_MFB_ReqRing.MFBReq_Struct[i].RrameRDIdx);

		for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_;) {
			log_inf(
				"MFB:FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d\n",
				j,
				g_MFB_ReqRing.MFBReq_Struct[i]
				    .MfbFrameStatus[j],
				j + 1,
				g_MFB_ReqRing.MFBReq_Struct[i]
				    .MfbFrameStatus[j + 1],
				j + 2,
				g_MFB_ReqRing.MFBReq_Struct[i]
				    .MfbFrameStatus[j + 2],
				j + 3,
				g_MFB_ReqRing.MFBReq_Struct[i]
				    .MfbFrameStatus[j + 3]);
			j = j + 4;
		}

	}



	log_inf("- X.");
	/*  */
	return Ret;
#endif
return 0;
}

#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
static inline void MFB_Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order:
	 * CG_SCP_SYS_MM0-> CG_MM_SMI_COMMON -> CG_SCP_SYS_ISP -> MFB clk
	 */
#ifndef SMI_CLK
	ret = clk_prepare_enable(mfb_clk.CG_SCP_SYS_MM0);
	if (ret)
		log_err("cannot prepare and enable CG_SCP_SYS_MM0 clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_MM_SMI_COMMON);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_MM_SMI_COMMON_2X);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_2X clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_MM_SMI_COMMON_GALS_M0_2X);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_GALS_M0_2X clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_MM_SMI_COMMON_GALS_M1_2X);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_GALS_M1_2X clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_MM_SMI_COMMON_UPSZ0);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_UPSZ0 clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_MM_SMI_COMMON_UPSZ1);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_UPSZ1 clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_MM_SMI_COMMON_FIFO0);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_FIFO0 clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_MM_SMI_COMMON_FIFO1);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_FIFO1 clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_MM_LARB5);
	if (ret)
		log_err("cannot prepare and enable CG_MM_LARB5 clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_SCP_SYS_ISP);
	if (ret)
		log_err("cannot prepare and enable CG_SCP_SYS_ISP clock\n");

	ret = clk_prepare_enable(mfb_clk.CG_IMGSYS_LARB);
	if (ret)
		log_err("cannot prepare and enable CG_IMGSYS_LARB clock\n");
#else
	smi_bus_prepare_enable(SMI_LARB5, "camera_mfb");
#endif

	ret = clk_prepare_enable(mfb_clk.CG_IMGSYS_MFB);
	if (ret)
		log_err("cannot prepare and enable CG_IMGSYS_MFB clock\n");

}

static inline void MFB_Disable_Unprepare_ccf_clock(void)
{
	/* must keep this clk close order:
	 * MFB clk -> CG_SCP_SYS_ISP -> CG_MM_SMI_COMMON -> CG_SCP_SYS_MM0
	 */
	clk_disable_unprepare(mfb_clk.CG_IMGSYS_MFB);
#ifndef SMI_CLK
	clk_disable_unprepare(mfb_clk.CG_IMGSYS_LARB);
	clk_disable_unprepare(mfb_clk.CG_SCP_SYS_ISP);
	clk_disable_unprepare(mfb_clk.CG_MM_LARB5);
	clk_disable_unprepare(mfb_clk.CG_MM_SMI_COMMON_FIFO1);
	clk_disable_unprepare(mfb_clk.CG_MM_SMI_COMMON_FIFO0);
	clk_disable_unprepare(mfb_clk.CG_MM_SMI_COMMON_UPSZ1);
	clk_disable_unprepare(mfb_clk.CG_MM_SMI_COMMON_UPSZ0);
	clk_disable_unprepare(mfb_clk.CG_MM_SMI_COMMON_GALS_M1_2X);
	clk_disable_unprepare(mfb_clk.CG_MM_SMI_COMMON_GALS_M0_2X);
	clk_disable_unprepare(mfb_clk.CG_MM_SMI_COMMON_2X);
	clk_disable_unprepare(mfb_clk.CG_MM_SMI_COMMON);
	clk_disable_unprepare(mfb_clk.CG_SCP_SYS_MM0);
#else
	smi_bus_disable_unprepare(SMI_LARB5, "camera_mfb");
#endif
}
#endif

/******************************************************************************
 *
 ******************************************************************************/
static void MFB_EnableClock(bool En)
{
#if defined(EP_NO_CLKMGR)
	unsigned int setReg;
#endif

	if (En) {		/* Enable clock. */
		/* log_dbg("Dpe clock enbled. g_u4EnableClockCount: %d.", */
		/*	g_u4EnableClockCount); */
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			MFB_Prepare_Enable_ccf_clock();
#else
			/* Enable clock by hardcode:
			 * 1. CAMSYS_CG_CLR (0x1A000008) = 0xffffffff;
			 * 2. IMG_CG_CLR (0x15000008) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			MFB_WR32(IMGSYS_REG_CG_CLR, setReg);
#endif
#else
			enable_clock(MT_CG_DMFB0_SMI_COMMON, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			/* enable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			enable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
#endif	/* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
			break;
		default:
			break;
		}
		spin_lock(&(MFBInfo.SpinLockMFB));
		g_u4EnableClockCount++;
		spin_unlock(&(MFBInfo.SpinLockMFB));
	} else {		/* Disable clock. */

		/* log_dbg("Dpe clock disabled. g_u4EnableClockCount: %d.", */
		/*	g_u4EnableClockCount); */
		spin_lock(&(MFBInfo.SpinLockMFB));
		g_u4EnableClockCount--;
		spin_unlock(&(MFBInfo.SpinLockMFB));
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			MFB_Disable_Unprepare_ccf_clock();
#else
			/* Disable clock by hardcode:
			 *  1. CAMSYS_CG_SET (0x1A000004) = 0xffffffff;
			 *  2. IMG_CG_SET (0x15000004) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			MFB_WR32(IMGSYS_REG_CG_SET, setReg);
#endif
#else
			/* do disable clock */
			disable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			/* disable_clock(MT_CG_IMAGE_FD, "CAMERA"); */
			disable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
			disable_clock(MT_CG_DMFB0_SMI_COMMON, "CAMERA");
#endif	/* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) */
			break;
		default:
			break;
		}
	}
}

/******************************************************************************
 *
 ******************************************************************************/
static inline void MFB_Reset(void)
{
	log_dbg("- E.");

	log_dbg(" MFB Reset start!\n");
	spin_lock(&(MFBInfo.SpinLockMFBRef));

	if (MFBInfo.UserCount > 1) {
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
		log_dbg("Curr UserCount(%d) users exist", MFBInfo.UserCount);
	} else {
		spin_unlock(&(MFBInfo.SpinLockMFBRef));

		/* Reset MFB flow */
		MFB_WR32(MFB_SW_RST_REG, 0x001);
		while ((MFB_RD32(MFB_SW_RST_REG) && 0x00000002 != 0x00000002))
			log_dbg("MFB resetting...\n");
		MFB_WR32(MFB_SW_RST_REG, 0x101);
		MFB_WR32(MFB_SW_RST_REG, 0x100);
		MFB_WR32(MFB_SW_RST_REG, 0x0);
		log_dbg(" MFB Reset end!\n");
	}

}

/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_ReadReg(MFB_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;
	/*  */
	MFB_REG_STRUCT reg;
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	MFB_REG_STRUCT *pData = (MFB_REG_STRUCT *) pRegIo->pData;

	for (i = 0; i < pRegIo->Count; i++) {
		if (get_user(reg.Addr, (unsigned int *) &pData->Addr) != 0) {
			log_err("get_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		/* pData++; */
		/*  */
		if ((ISP_MFB_BASE + reg.Addr >= ISP_MFB_BASE)
		    && (reg.Addr < MFB_REG_RANGE)
			&& ((reg.Addr & 0x3) == 0)) {
			reg.Val = MFB_RD32(ISP_MFB_BASE + reg.Addr);
		} else {
			log_err(
			    "Wrong address(0x%p), MFB_BASE(0x%p), Addr(0x%lx)",
			    (ISP_MFB_BASE + reg.Addr),
			    ISP_MFB_BASE,
			    (unsigned long)reg.Addr);
			reg.Val = 0;
		}
		/*  */
		/* printk("[KernelRDReg]addr(0x%x), */
		/*	value()0x%x\n",MFB_ADDR_CAMINF + reg.Addr,reg.Val); */

		if (put_user(reg.Val, (unsigned int *) &(pData->Val)) != 0) {
			log_err("put_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		pData++;
		/*  */
	}
	/*  */
EXIT:
	return Ret;
}


/******************************************************************************
 *
 ******************************************************************************/
/* Can write sensor's test model only, if need write to other modules,
 * need modify current code flow
 */
static signed int MFB_WriteRegToHw(MFB_REG_STRUCT *pReg, unsigned int Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;

	/* Use local variable to store MFBInfo.DebugMask & MFB_DBG_WRITE_REG
	 * for saving lock time
	 */
	spin_lock(&(MFBInfo.SpinLockMFB));
	dbgWriteReg = MFBInfo.DebugMask & MFB_DBG_WRITE_REG;
	spin_unlock(&(MFBInfo.SpinLockMFB));

	/*  */
	if (dbgWriteReg)
		log_dbg("- E.\n");

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg) {
			log_dbg("Addr(0x%lx), Val(0x%x)\n",
				(unsigned long)(ISP_MFB_BASE + pReg[i].Addr),
				(unsigned int) (pReg[i].Val));
		}

		if ((pReg[i].Addr < MFB_REG_RANGE) &&
		    ((pReg[i].Addr & 0x3) == 0)) {
			MFB_WR32(ISP_MFB_BASE + pReg[i].Addr, pReg[i].Val);
		} else {
			log_err(
			  "wrong address(0x%p), MFB_BASE(0x%p), Addr(0x%lx)\n",
			  (ISP_MFB_BASE + pReg[i].Addr),
			  ISP_MFB_BASE,
			  (unsigned long)pReg[i].Addr);
		}
	}

	/*  */
	return Ret;
}



/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_WriteReg(MFB_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	/* unsigned char* pData = NULL; */
	MFB_REG_STRUCT *pData = NULL;
	/*	*/
	if (MFBInfo.DebugMask & MFB_DBG_WRITE_REG)
		log_dbg("Data(0x%p), Count(%d)\n",
			(pRegIo->pData), (pRegIo->Count));

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
	    (pRegIo->Count > (MFB_REG_RANGE>>2))) {
		log_err("ERROR: pRegIo->pData is NULL or Count:%d\n",
		    pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	/* pData = (unsigned char*)kmalloc(
	 *	(pRegIo->Count)*sizeof(MFB_REG_STRUCT), GFP_ATOMIC);
	 */
	pData = kmalloc((pRegIo->Count) * sizeof(MFB_REG_STRUCT), GFP_KERNEL);
	if (pData == NULL) {
		#if 0
		log_err(
		  "ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
		  current->comm,
		  current->pid,
		  current->tgid);
		#endif
		Ret = -ENOMEM;
		goto EXIT;
	}
	if (copy_from_user(pData, (void __user *)(pRegIo->pData),
	    pRegIo->Count * sizeof(MFB_REG_STRUCT)) != 0) {
		log_err("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	Ret = MFB_WriteRegToHw(pData, pRegIo->Count);
	/*  */
EXIT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return Ret;
}



/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_WaitIrq(MFB_WAIT_IRQ_STRUCT *WaitIrq)
{

	signed int Ret = 0;
	signed int Timeout = WaitIrq->Timeout;
	enum MFB_PROCESS_ID_ENUM whichReq = MFB_PROCESS_ID_NONE;

	/*unsigned int i;*/
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */
	unsigned int irqStatus;
	/*int cnt = 0;*/
	struct timeval time_getrequest;
	unsigned long long sec = 0;
	unsigned long usec = 0;

	/* do_gettimeofday(&time_getrequest); */
	sec = cpu_clock(0);	/* ns */
	do_div(sec, 1000);	/* usec */
	usec = do_div(sec, 1000000);	/* sec and usec */
	time_getrequest.tv_usec = usec;
	time_getrequest.tv_sec = sec;


	/* Debug interrupt */
	if (MFBInfo.DebugMask & MFB_DBG_INT) {
		if (WaitIrq->Status & MFBInfo.IrqInfo.Mask[WaitIrq->Type]) {
			if (WaitIrq->UserKey > 0) {
				log_dbg("+WaitIrq clr(%d), Type(%d), Stat(0x%08X), Timeout(%d),usr(%d), ProcID(%d)\n",
				WaitIrq->Clear,
				WaitIrq->Type,
				WaitIrq->Status,
				WaitIrq->Timeout,
				WaitIrq->UserKey,
				WaitIrq->ProcessID);
			}
		}
	}


	/* 1. wait type update */
	if (WaitIrq->Clear == MFB_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		#if 0
		log_dbg(
		  "WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X) has been cleared",
		  WaitIrq->EventInfo.Clear, WaitIrq->Type,
		  MFBInfo.IrqInfo.Status[WaitIrq->Type]);
		  MFBInfo.IrqInfo.Status[WaitIrq->Type][
		      WaitIrq->EventInfo.UserKey] &=
		      (~WaitIrq->EventInfo.Status);
		#endif

		MFBInfo.IrqInfo.Status[WaitIrq->Type] &= (~WaitIrq->Status);
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		return Ret;
	}

	if (WaitIrq->Clear == MFB_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		if (MFBInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
				(~WaitIrq->Status);

		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
	} else if (WaitIrq->Clear == MFB_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

		MFBInfo.IrqInfo.Status[WaitIrq->Type] = 0;
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
	}
	/* MFB_IRQ_WAIT_CLEAR ==> do nothing */


	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(
		&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
	spin_unlock_irqrestore(
		&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (WaitIrq->Status & MFB_INT_ST) {
		whichReq = MFB_PROCESS_ID_MFB;
	} else {
		log_err("No Such Stats can be waited!! irq Type/User/Sts/Pid(0x%x/%d/0x%x/%d)\n",
			WaitIrq->Type, WaitIrq->UserKey,
			WaitIrq->Status, WaitIrq->ProcessID);
	}


#ifdef MFB_WAITIRQ_LOG
	log_inf(
	    "before wait_event:Tout(%d), Clear(%d), Type(%d), IrqStat(0x%08X), WaitStat(0x%08X), usrKey(%d)\n",
	    WaitIrq->Timeout, WaitIrq->Clear, WaitIrq->Type,
	    irqStatus, WaitIrq->Status, WaitIrq->UserKey);
	log_inf(
	    "before wait_event:ProcID(%d), MfbIrq(0x%08X), WriteReq(0x%08X), ReadReq(0x%08X), whichReq(%d)\n",
	    WaitIrq->ProcessID, MFBInfo.IrqInfo.MfbIrqCnt,
	    MFBInfo.WriteReqIdx, MFBInfo.ReadReqIdx, whichReq);
#endif

	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(MFBInfo.WaitQueueHead,
			MFB_GetIRQState(WaitIrq->Type, WaitIrq->UserKey,
			WaitIrq->Status, whichReq,
			WaitIrq->ProcessID),
			MFB_MsToJiffies(WaitIrq->Timeout));

	/* check if user is interrupted by system signal */
	if ((Timeout != 0) && (!MFB_GetIRQState(WaitIrq->Type, WaitIrq->UserKey,
	    WaitIrq->Status, whichReq, WaitIrq->ProcessID))) {
		log_dbg("interrupted by system, timeout(%d),irq Type/User/Sts/whichReq/Pid(0x%x/%d/0x%x/%d/%d)\n",
		Timeout, WaitIrq->Type, WaitIrq->UserKey,
		WaitIrq->Status, whichReq, WaitIrq->ProcessID);
		Ret = -ERESTARTSYS;	/* actually it should be -ERESTARTSYS */
		goto EXIT;
	}
	/* timeout */
	if (Timeout == 0) {
		/* Store irqinfo status in here to redeuce time of
		 * spin_lock_irqsave
		 */
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

		log_err(
		    "WaitIrq Timeout:Tout(%d) clr(%d) Type(%d) IrqStat(0x%08X) WaitStat(0x%08X) usrKey(%d)\n",
		    WaitIrq->Timeout, WaitIrq->Clear, WaitIrq->Type,
		    irqStatus, WaitIrq->Status, WaitIrq->UserKey);
		log_err(
		    "WaitIrq Timeout:whichReq(%d),ProcID(%d) MfbIrqCnt(0x%08X) WriteReq(0x%08X) ReadReq(0x%08X)\n",
		    whichReq, WaitIrq->ProcessID, MFBInfo.IrqInfo.MfbIrqCnt,
		    MFBInfo.WriteReqIdx, MFBInfo.ReadReqIdx);

		if (WaitIrq->bDumpReg)
			MFB_DumpReg();

		Ret = -EFAULT;
		goto EXIT;
	} else {
		/* Store irqinfo status in here to redeuce time of
		 * spin_lock_irqsave
		 */
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("MFB_WaitIrq");
#endif

		spin_lock_irqsave(&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

		if (WaitIrq->Clear == MFB_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
#ifdef MFB_USE_GCE

#ifdef MFB_MULTIPROCESS_TIMEING_ISSUE
			MFBInfo.ReadReqIdx = (MFBInfo.ReadReqIdx + 1) %
						_SUPPORT_MAX_MFB_FRAME_REQUEST_;
			/* actually, it doesn't happen the timging issue!! */
			/* wake_up_interruptible(&MFBInfo.WaitQueueHead); */
#endif
			if (WaitIrq->Status & MFB_INT_ST) {
				MFBInfo.IrqInfo.MfbIrqCnt--;
				if (MFBInfo.IrqInfo.MfbIrqCnt == 0)
					MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
						(~WaitIrq->Status);
			} else {
				log_err(
				    "MFB_IRQ_WAIT_CLEAR Error, Type(%d), WaitStatus(0x%08X)",
				    WaitIrq->Type, WaitIrq->Status);
			}
#else
			if (MFBInfo.IrqInfo.Status[WaitIrq->Type] &
			    WaitIrq->Status)
				MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
				    (~WaitIrq->Status);
#endif
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
		}

#ifdef MFB_WAITIRQ_LOG
		log_inf(
		    "no Timeout:Tout(%d), clr(%d), Type(%d), IrqStat(0x%08X), WaitStat(0x%08X), usrKey(%d)\n",
		    WaitIrq->Timeout, WaitIrq->Clear, WaitIrq->Type,
		    irqStatus, WaitIrq->Status, WaitIrq->UserKey);
		log_inf(
		    "no Timeout:ProcID(%d),MfbIrq(0x%08X), WriteReq(0x%08X), ReadReq(0x%08X),whichReq(%d)\n",
		    WaitIrq->ProcessID, MFBInfo.IrqInfo.MfbIrqCnt,
		    MFBInfo.WriteReqIdx, MFBInfo.ReadReqIdx, whichReq);
#endif

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif

	}


EXIT:


	return Ret;
}


/******************************************************************************
 *
 ******************************************************************************/
static long MFB_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;

	/*unsigned int pid = 0;*/
	MFB_REG_IO_STRUCT RegIo;
	MFB_WAIT_IRQ_STRUCT IrqInfo;
	MFB_CLEAR_IRQ_STRUCT ClearIrq;
	MFB_Config mfb_MfbConfig;
	MFB_Request mfb_MfbReq;
	signed int MfbWriteIdx = 0;
	int idx;
	struct MFB_USER_INFO_STRUCT *pUserInfo;
	int enqueNum;
	int dequeNum;
	unsigned long flags; /* old: unsigned int flags;*/
			     /* FIX to avoid build warning */



	/*  */
	if (pFile->private_data == NULL) {
		log_wrn(
		    "private_data is NULL,(process, pid, tgid)=(%s, %d, %d)",
		    current->comm,
		    current->pid,
		    current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct MFB_USER_INFO_STRUCT *) (pFile->private_data);
	/*  */
	switch (Cmd) {
	case MFB_RESET:
		{
			spin_lock(&(MFBInfo.SpinLockMFB));
			MFB_Reset();
			spin_unlock(&(MFBInfo.SpinLockMFB));
			break;
		}

		/*  */
	case MFB_DUMP_REG:
		{
			Ret = MFB_DumpReg();
			break;
		}
	case MFB_DUMP_ISR_LOG:
		{
			unsigned int currentPPB = m_CurrentPPB;

			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
				flags);
			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
				flags);

			IRQ_LOG_PRINTER(
				MFB_IRQ_TYPE_INT_MFB_ST, currentPPB, _LOG_INF);
			IRQ_LOG_PRINTER(
				MFB_IRQ_TYPE_INT_MFB_ST, currentPPB, _LOG_ERR);
			break;
		}
	case MFB_READ_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
			    sizeof(MFB_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from user is
				 * implemented in MFB_ReadReg(...)
				 */
				Ret = MFB_ReadReg(&RegIo);
			} else {
				log_err("MFB_READ_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_WRITE_REGISTER:
		{
			if (copy_from_user(&RegIo,
			    (void *)Param, sizeof(MFB_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir of copy from user is
				 * implemented in MFB_WriteReg(...)
				 */
				Ret = MFB_WriteReg(&RegIo);
			} else {
				log_err("MFB_WRITE_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_WAIT_IRQ:
		{
			if (copy_from_user(&IrqInfo, (void *)Param,
			    sizeof(MFB_WAIT_IRQ_STRUCT)) == 0) {
				/*  */
				if ((IrqInfo.Type >= MFB_IRQ_TYPE_AMOUNT)
				    || (IrqInfo.Type < 0)) {
					Ret = -EFAULT;
					log_err("invalid type(%d)",
					    IrqInfo.Type);
					goto EXIT;
				}

				if ((IrqInfo.UserKey >= IRQ_USER_NUM_MAX)
				    || (IrqInfo.UserKey < 0)) {
					log_err(
					    "invalid userKey(%d), max(%d), force userkey = 0\n",
					    IrqInfo.UserKey,
					    IRQ_USER_NUM_MAX);
					IrqInfo.UserKey = 0;
				}

				log_inf(
				    "IRQ clear(%d), type(%d), userKey(%d), timeout(%d), status(%d)\n",
				    IrqInfo.Clear, IrqInfo.Type,
				    IrqInfo.UserKey, IrqInfo.Timeout,
				    IrqInfo.Status);
				IrqInfo.ProcessID = pUserInfo->Pid;
				Ret = MFB_WaitIrq(&IrqInfo);

				if (copy_to_user((void *)Param, &IrqInfo,
				    sizeof(MFB_WAIT_IRQ_STRUCT)) != 0) {
					log_err("copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				log_err("MFB_WAIT_IRQ copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_CLEAR_IRQ:
		{
			if (copy_from_user(&ClearIrq, (void *)Param,
			    sizeof(MFB_CLEAR_IRQ_STRUCT)) == 0) {
				log_dbg("MFB_CLEAR_IRQ Type(%d)",
					ClearIrq.Type);

				if ((ClearIrq.Type >= MFB_IRQ_TYPE_AMOUNT) ||
				    (ClearIrq.Type < 0)) {
					Ret = -EFAULT;
					log_err("invalid type(%d)",
						ClearIrq.Type);
					goto EXIT;
				}

				/*  */
				if ((ClearIrq.UserKey >= IRQ_USER_NUM_MAX)
				    || (ClearIrq.UserKey < 0)) {
					log_err("errUserEnum(%d)",
						ClearIrq.UserKey);
					Ret = -EFAULT;
					goto EXIT;
				}

				log_dbg("MFB_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)\n",
					ClearIrq.Type, ClearIrq.Status,
					MFBInfo.IrqInfo.Status[ClearIrq.Type]);
				spin_lock_irqsave(
					&(MFBInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
				MFBInfo.IrqInfo.Status[ClearIrq.Type] &=
					(~ClearIrq.Status);
				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[ClearIrq.Type]),
					flags);
			} else {
				log_err("MFB_CLEAR_IRQ copy_from_user failed\n");
				Ret = -EFAULT;
			}
			break;
		}
	case MFB_ENQNUE_NUM:
		{
			/* enqueNum */
			if (copy_from_user(&enqueNum,
			    (void *)Param, sizeof(int)) == 0) {
				if (MFB_REQUEST_STATE_EMPTY ==
				    g_MFB_ReqRing.MFBReq_Struct[
				    g_MFB_ReqRing.WriteIdx].State) {
					spin_lock_irqsave(
						&(MFBInfo.SpinLockIrq[
						    MFB_IRQ_TYPE_INT_MFB_ST]),
						flags);
					g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.WriteIdx]
						.processID =
						pUserInfo->Pid;
					g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing
						.WriteIdx].enqueReqNum =
						enqueNum;
					spin_unlock_irqrestore(
						&(MFBInfo.SpinLockIrq[
						    MFB_IRQ_TYPE_INT_MFB_ST]),
						flags);
					if (enqueNum >
					    _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
						log_err(
						    "MFB Enque Num is bigger than enqueNum:%d\n",
						    enqueNum);
					}
					log_dbg("MFB_ENQNUE_NUM:%d\n",
						enqueNum);
				} else {
					log_err(
					    "WFME Enque request state is not empty:%d, writeIdx:%d, readIdx:%d\n",
					    g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.WriteIdx].
					    State, g_MFB_ReqRing.WriteIdx,
					    g_MFB_ReqRing.ReadIdx);
				}
			} else {
				log_err(
				    "MFB_EQNUE_NUM copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
		/* MFB_Config */
	case MFB_ENQUE:
		{
			if (copy_from_user(&mfb_MfbConfig,
				(void *)Param, sizeof(MFB_Config)) == 0) {
				/* log_dbg("MFB_CLEAR_IRQ:Type(%d),
				 *	Status(0x%08X),IrqStatus(0x%08X)",
				 *	ClearIrq.Type, ClearIrq.Status,
				 *	MFBInfo.IrqInfo.Status[ClearIrq.Type]);
				 */
				spin_lock_irqsave(
					&(MFBInfo.SpinLockIrq[
					    MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
				if ((MFB_REQUEST_STATE_EMPTY ==
				    g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.WriteIdx].State) &&
				    (g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.WriteIdx].FrameWRIdx <
				    g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.WriteIdx].enqueReqNum)) {
					g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.WriteIdx]
					    .MfbFrameStatus[
					    g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.WriteIdx]
					    .FrameWRIdx] =
					MFB_FRAME_STATUS_ENQUE;

					memcpy(
					    &g_MFB_ReqRing
						.MFBReq_Struct[g_MFB_ReqRing
						.WriteIdx].MfbFrameConfig[
						g_MFB_ReqRing
						.MFBReq_Struct[g_MFB_ReqRing
						.WriteIdx].FrameWRIdx++],
					    &mfb_MfbConfig,
					    sizeof(MFB_Config));
					if (g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.WriteIdx]
						.FrameWRIdx ==
						g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.WriteIdx]
						.enqueReqNum) {
						g_MFB_ReqRing
						    .MFBReq_Struct[
						    g_MFB_ReqRing.WriteIdx]
						    .State =
						MFB_REQUEST_STATE_PENDING;

						g_MFB_ReqRing.WriteIdx =
						  (g_MFB_ReqRing.WriteIdx + 1) %
					    _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
						log_dbg("MFB enque done!!\n");
					} else {
						log_dbg("MFB enque frame!!\n");
					}
				} else {
					log_err(
					    "No Buffer! WriteIdx(%d), Stat(%d), FrameWRIdx(%d), enqueReqNum(%d)\n",
					    g_MFB_ReqRing.WriteIdx,
					    g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.WriteIdx]
						.State,
					    g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.WriteIdx]
						.FrameWRIdx,
					    g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.WriteIdx].
						enqueReqNum);
				}
#ifdef MFB_USE_GCE
				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
				log_dbg("ConfigMFB!!\n");
				ConfigMFB();
#else
				/* check the hw is running or not ? */
				if (Check_MFB_Is_Busy() == MFALSE) {
					/* config the mfb hw and run */
					log_dbg("ConfigMFB\n");
					ConfigMFB();
				} else {
					log_inf("MFB HW is busy!!\n");
				}
				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
#endif


			} else {
				log_err("MFB_ENQUE copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
	case MFB_ENQUE_REQ:
		{
			if (copy_from_user(&mfb_MfbReq,
			    (void *)Param, sizeof(MFB_Request)) == 0) {
				log_dbg("MFB_ENQNUE_NUM:%d, pid:%d\n",
					mfb_MfbReq.m_ReqNum,
					pUserInfo->Pid);
				if (mfb_MfbReq.m_ReqNum >
				    _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
					log_err("MFB Enque Num is bigger than enqueNum:%d\n",
						mfb_MfbReq.m_ReqNum);
					Ret = -EFAULT;
					goto EXIT;
				}
				if (copy_from_user(
				    g_MfbEnqueReq_Struct.MfbFrameConfig,
				     (void *)mfb_MfbReq.m_pMfbConfig,
				     mfb_MfbReq.m_ReqNum * sizeof(MFB_Config))
				     != 0) {
					log_err("copy MFBConfig from request is fail!!\n");
					Ret = -EFAULT;
					goto EXIT;
				}

				/* Protect the Multi Process */
				mutex_lock(&gMfbMutex);

				spin_lock_irqsave(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
				if (MFB_REQUEST_STATE_EMPTY ==
				    g_MFB_ReqRing.MFBReq_Struct[
				    g_MFB_ReqRing.WriteIdx]
				    .State) {
					g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.WriteIdx].processID =
					pUserInfo->Pid;
					g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.WriteIdx].enqueReqNum =
					mfb_MfbReq.m_ReqNum;

					for (idx = 0;
					    idx < mfb_MfbReq.m_ReqNum; idx++) {
						g_MFB_ReqRing
						    .MFBReq_Struct[
						    g_MFB_ReqRing.WriteIdx]
						    .MfbFrameStatus[
						    g_MFB_ReqRing.MFBReq_Struct[
						    g_MFB_ReqRing.WriteIdx].
						    FrameWRIdx] =
						MFB_FRAME_STATUS_ENQUE;

						memcpy(
						  &g_MFB_ReqRing.MFBReq_Struct[
						      g_MFB_ReqRing.WriteIdx]
						      .MfbFrameConfig[
						      g_MFB_ReqRing
						      .MFBReq_Struct[
						      g_MFB_ReqRing
						      .WriteIdx].FrameWRIdx++],
						  &g_MfbEnqueReq_Struct
						      .MfbFrameConfig[idx],
						  sizeof(MFB_Config));
					}
					g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.WriteIdx].State =
					MFB_REQUEST_STATE_PENDING;
					MfbWriteIdx = g_MFB_ReqRing.WriteIdx;
					g_MFB_ReqRing.WriteIdx =
					    (g_MFB_ReqRing.WriteIdx + 1) %
					    _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
					log_dbg("MFB request enque done!!\n");
				} else {
					log_err("Enque req NG: WriteIdx(%d) Stat(%d) FrameWRIdx(%d) enqueReqNum(%d)\n",
					     g_MFB_ReqRing.WriteIdx,
					     g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.WriteIdx]
						.State,
					     g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.WriteIdx]
						.FrameWRIdx,
					     g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.WriteIdx]
						.enqueReqNum);
				}
				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
				log_dbg("ConfigMFB Request!!\n");
				ConfigMFBRequest(MfbWriteIdx);

				mutex_unlock(&gMfbMutex);
			} else {
				log_err(
				    "MFB_ENQUE_REQ copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
	case MFB_DEQUE_NUM:
		{
			if (MFB_REQUEST_STATE_FINISHED ==
			    g_MFB_ReqRing.MFBReq_Struct[
			    g_MFB_ReqRing.ReadIdx].State) {
				dequeNum =
				    g_MFB_ReqRing.MFBReq_Struct[
				    g_MFB_ReqRing.ReadIdx]
				    .enqueReqNum;
				log_dbg("MFB_DEQUE_NUM(%d)\n", dequeNum);
			} else {
				dequeNum = 0;
				log_err("DEQUE_NUM:No Buffer: ReadIdx(%d) State(%d) RrameRDIdx(%d) enqueReqNum(%d)\n",
				     g_MFB_ReqRing.ReadIdx,
				     g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx].State,
				     g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx].RrameRDIdx,
				     g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx].enqueReqNum);
			}
			if (copy_to_user((void *)Param, &dequeNum,
			    sizeof(unsigned int)) != 0) {
				log_err("MFB_DEQUE_NUM copy_to_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}

	case MFB_DEQUE:
		{
			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
				flags);
			if ((MFB_REQUEST_STATE_FINISHED ==
			    g_MFB_ReqRing.MFBReq_Struct[
			    g_MFB_ReqRing.ReadIdx].State) &&
			    (g_MFB_ReqRing.MFBReq_Struct[
			    g_MFB_ReqRing.ReadIdx].RrameRDIdx <
			    g_MFB_ReqRing.MFBReq_Struct[
			    g_MFB_ReqRing.ReadIdx].enqueReqNum)) {
				/* dequeNum = g_DVE_RequestRing.DVEReq_Struct[*/
				/*     g_DVE_RequestRing.ReadIdx].enqueReqNum;*/
				if (MFB_FRAME_STATUS_FINISHED ==
				    g_MFB_ReqRing.MFBReq_Struct[
				    g_MFB_ReqRing.ReadIdx]
				    .MfbFrameStatus[
				    g_MFB_ReqRing.MFBReq_Struct[
				    g_MFB_ReqRing.ReadIdx].RrameRDIdx]) {
					memcpy(
					    &mfb_MfbConfig,
					    &g_MFB_ReqRing
						.MFBReq_Struct[
						g_MFB_ReqRing.ReadIdx]
						.MfbFrameConfig[
						g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.ReadIdx]
						.RrameRDIdx],
					     sizeof(MFB_Config));

					g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.ReadIdx].
					    MfbFrameStatus[
					    g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.ReadIdx]
					    .RrameRDIdx++] =
					MFB_FRAME_STATUS_EMPTY;
				}
				if (g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx].RrameRDIdx ==
				    g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx].enqueReqNum) {
					g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.ReadIdx]
					    .State =
					    MFB_REQUEST_STATE_EMPTY;
					g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.ReadIdx]
					    .FrameWRIdx = 0;
					g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.ReadIdx]
					    .RrameRDIdx = 0;
					g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.ReadIdx]
					    .enqueReqNum = 0;
					g_MFB_ReqRing.ReadIdx =
					    (g_MFB_ReqRing.ReadIdx + 1) %
					    _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
					log_dbg("MFB ReadIdx(%d)\n",
						g_MFB_ReqRing.ReadIdx);
				}
				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
				if (copy_to_user((void *)Param,
				    &g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx]
					.MfbFrameConfig[g_MFB_ReqRing
					.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
					.RrameRDIdx], sizeof(MFB_Config))
				    != 0) {
					log_err(
					    "MFB_DEQUE copy_to_user failed\n");
					Ret = -EFAULT;
				}

			} else {
				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
				log_err("MFB_DEQUE No Buffer: ReadIdx(%d) State(%d) RrameRDIdx(%d), enqueReqNum(%d)\n",
				     g_MFB_ReqRing.ReadIdx,
				     g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx].State,
				     g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx].RrameRDIdx,
				     g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx].enqueReqNum);
			}

			break;
		}
	case MFB_DEQUE_REQ:
		{
			if (copy_from_user(&mfb_MfbReq, (void *)Param,
			    sizeof(MFB_Request)) == 0) {
				/* Protect the Multi Process */
				mutex_lock(&gMfbDequeMutex);

				spin_lock_irqsave(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);

				if (MFB_REQUEST_STATE_FINISHED ==
				    g_MFB_ReqRing.MFBReq_Struct[
				    g_MFB_ReqRing.ReadIdx].State) {
					dequeNum =
					    g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.ReadIdx].enqueReqNum;
					log_dbg("MFB_DEQUE_REQ(%d)\n",
					    dequeNum);
				} else {
					dequeNum = 0;
					log_err("DEQUE_REQ no buf:RIdx(%d) Stat(%d) RrameRDIdx(%d) enqueReqNum(%d)\n",
					     g_MFB_ReqRing.ReadIdx,
					     g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.ReadIdx]
						.State,
					     g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.ReadIdx]
						.RrameRDIdx,
					     g_MFB_ReqRing.MFBReq_Struct[
						g_MFB_ReqRing.ReadIdx]
						.enqueReqNum);
				}
				mfb_MfbReq.m_ReqNum = dequeNum;

				for (idx = 0; idx < dequeNum; idx++) {
					if (MFB_FRAME_STATUS_FINISHED ==
					    g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.ReadIdx]
					    .MfbFrameStatus[
					    g_MFB_ReqRing.MFBReq_Struct[
					    g_MFB_ReqRing.ReadIdx]
					    .RrameRDIdx]) {

						memcpy(
						    &g_MfbDequeReq_Struct
							.MfbFrameConfig[idx],
						    &g_MFB_ReqRing
							.MFBReq_Struct[
							g_MFB_ReqRing.ReadIdx]
							.MfbFrameConfig[
							g_MFB_ReqRing
							.MFBReq_Struct[
							g_MFB_ReqRing.ReadIdx]
							.RrameRDIdx],
						    sizeof(MFB_Config));

						g_MFB_ReqRing
						    .MFBReq_Struct[
						    g_MFB_ReqRing.ReadIdx]
						    .MfbFrameStatus[
						    g_MFB_ReqRing.MFBReq_Struct[
						    g_MFB_ReqRing.ReadIdx]
						    .RrameRDIdx++] =
						MFB_FRAME_STATUS_EMPTY;
					} else {
						log_err("deq err idx(%d) dequNum(%d) Rd(%d) RrameRD(%d) FrmStat(%d)\n",
						idx, dequeNum,
						g_MFB_ReqRing.ReadIdx,
						g_MFB_ReqRing.MFBReq_Struct[
						    g_MFB_ReqRing.ReadIdx]
						    .RrameRDIdx,
						g_MFB_ReqRing.MFBReq_Struct[
						    g_MFB_ReqRing.ReadIdx]
						    .MfbFrameStatus[
						    g_MFB_ReqRing.MFBReq_Struct[
						    g_MFB_ReqRing.ReadIdx]
						    .RrameRDIdx]);
					}
				}
				g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx].State =
					MFB_REQUEST_STATE_EMPTY;
				g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx]
					.FrameWRIdx = 0;
				g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx]
					.RrameRDIdx = 0;
				g_MFB_ReqRing.MFBReq_Struct[
					g_MFB_ReqRing.ReadIdx]
					.enqueReqNum = 0;
				g_MFB_ReqRing.ReadIdx =
					(g_MFB_ReqRing.ReadIdx + 1) %
					_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
				log_dbg("MFB Request ReadIdx(%d)\n",
					g_MFB_ReqRing.ReadIdx);

				spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq[
						MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);

				mutex_unlock(&gMfbDequeMutex);

				if (mfb_MfbReq.m_pMfbConfig == NULL) {
					log_err("NULL pointer:mfb_MfbReq.m_pMfbConfig");
					Ret = -EFAULT;
					goto EXIT;
				}

				if (copy_to_user
				    ((void *)mfb_MfbReq.m_pMfbConfig,
				     &g_MfbDequeReq_Struct.MfbFrameConfig[0],
				     dequeNum * sizeof(MFB_Config)) != 0) {
					log_err(
					    "MFB_DEQUE_REQ copy_to_user frameconfig failed\n");
					Ret = -EFAULT;
				}
				if (copy_to_user((void *)Param,
				    &mfb_MfbReq, sizeof(MFB_Request)) != 0) {
					log_err("MFB_DEQUE_REQ copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				log_err("MFB_CMD_MFB_DEQUE_REQ copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
	default:
		{
			log_err("Unknown Cmd(%d)", Cmd);
			log_err("Fail, Cmd(%d), Dir(%d), Type(%d), Nr(%d),Size(%d)\n",
			    Cmd, _IOC_DIR(Cmd),
			    _IOC_TYPE(Cmd), _IOC_NR(Cmd), _IOC_SIZE(Cmd));
			Ret = -EPERM;
			break;
		}
	}
	/*  */
EXIT:
	if (Ret != 0) {
		log_err(
		    "Fail, Cmd(%d), Pid(%d), (process, pid, tgid)=(%s, %d, %d)",
		    Cmd, pUserInfo->Pid, current->comm,
		    current->pid, current->tgid);
	}
	/*  */
	return Ret;
}

#ifdef CONFIG_COMPAT

/******************************************************************************
 *
 ******************************************************************************/
static int compat_get_MFB_read_register_data(
	compat_MFB_REG_IO_STRUCT __user *data32,
	MFB_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err;

	err = get_user(uptr, &data32->pData);
	err |= put_user(compat_ptr(uptr), &data->pData);
	err |= get_user(count, &data32->Count);
	err |= put_user(count, &data->Count);
	return err;
}

static int compat_put_MFB_read_register_data(
	compat_MFB_REG_IO_STRUCT __user *data32,
	MFB_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->pData); */
	/* err |= put_user(uptr, &data32->pData); */
	err |= get_user(count, &data->Count);
	err |= put_user(count, &data32->Count);
	return err;
}

static int compat_get_MFB_enque_req_data(
	compat_MFB_Request __user *data32,
	MFB_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pMfbConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pMfbConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_MFB_enque_req_data(
	compat_MFB_Request __user *data32, MFB_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pDpeConfig); */
	/* err |= put_user(uptr, &data32->m_pDpeConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}


static int compat_get_MFB_deque_req_data(
	compat_MFB_Request __user *data32, MFB_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pMfbConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pMfbConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_MFB_deque_req_data(
	compat_MFB_Request __user *data32, MFB_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pDpeConfig); */
	/* err |= put_user(uptr, &data32->m_pDpeConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static long MFB_ioctl_compat(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;


	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		log_err("no f_op !!!\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case COMPAT_MFB_READ_REGISTER:
		{
			compat_MFB_REG_IO_STRUCT __user *data32;
			MFB_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_read_register_data(data32, data);
			if (err) {
				log_inf(
				    "compat_get_MFB_read_register_data error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, MFB_READ_REGISTER,
						       (unsigned long)data);
			err = compat_put_MFB_read_register_data(data32, data);
			if (err) {
				log_inf(
				    "compat_put_MFB_read_register_data error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_MFB_WRITE_REGISTER:
		{
			compat_MFB_REG_IO_STRUCT __user *data32;
			MFB_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_read_register_data(data32, data);
			if (err) {
				log_inf("COMPAT_MFB_WRITE_REGISTER error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, MFB_WRITE_REGISTER,
						       (unsigned long)data);
			return ret;
		}
	case COMPAT_MFB_ENQUE_REQ:
		{
			compat_MFB_Request __user *data32;
			MFB_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_enque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_MFB_ENQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, MFB_ENQUE_REQ,
						       (unsigned long)data);
			err = compat_put_MFB_enque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_MFB_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_MFB_DEQUE_REQ:
		{
			compat_MFB_Request __user *data32;
			MFB_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_deque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_MFB_DEQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, MFB_DEQUE_REQ,
						       (unsigned long)data);
			err = compat_put_MFB_deque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_MFB_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}

	case MFB_WAIT_IRQ:
	case MFB_CLEAR_IRQ:	/* structure (no pointer) */
	case MFB_ENQNUE_NUM:
	case MFB_ENQUE:
	case MFB_DEQUE_NUM:
	case MFB_DEQUE:
	case MFB_RESET:
	case MFB_DUMP_REG:
	case MFB_DUMP_ISR_LOG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return MFB_ioctl(filep, cmd, arg); */
	}
}

#endif

/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i, j;
	/*int q = 0, p = 0;*/
	struct MFB_USER_INFO_STRUCT *pUserInfo;

	log_dbg("- E. UserCount: %d.", MFBInfo.UserCount);


	/*  */
	spin_lock(&(MFBInfo.SpinLockMFBRef));

	pFile->private_data = NULL;
	pFile->private_data = kmalloc(
		sizeof(struct MFB_USER_INFO_STRUCT), GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		/*
		 *log_dbg(
		 *   "ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)",
		 *   current->comm,
		 *    current->pid,
		 *   current->tgid);
		 */
		Ret = -ENOMEM;
	} else {
		pUserInfo = (struct MFB_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (MFBInfo.UserCount > 0) {
		MFBInfo.UserCount++;
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
		log_dbg("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			MFBInfo.UserCount, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else {
		MFBInfo.UserCount++;
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
		log_dbg("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), first user",
			MFBInfo.UserCount, current->comm,
			current->pid, current->tgid);
	}

	/* do wait queue head init when re-enter in camera */
	/*  */
	for (i = 0; i < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; i++) {
		/* MFB */
		g_MFB_ReqRing.MFBReq_Struct[i].processID = 0x0;
		g_MFB_ReqRing.MFBReq_Struct[i].callerID = 0x0;
		g_MFB_ReqRing.MFBReq_Struct[i].enqueReqNum = 0x0;
		/* g_MFB_ReqRing.MFBReq_Struct[i].enqueIdx = 0x0; */
		g_MFB_ReqRing.MFBReq_Struct[i].State = MFB_REQUEST_STATE_EMPTY;
		g_MFB_ReqRing.MFBReq_Struct[i].FrameWRIdx = 0x0;
		g_MFB_ReqRing.MFBReq_Struct[i].RrameRDIdx = 0x0;
		for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
			g_MFB_ReqRing.MFBReq_Struct[i].MfbFrameStatus[j] =
			    MFB_FRAME_STATUS_EMPTY;
		}

	}
	g_MFB_ReqRing.WriteIdx = 0x0;
	g_MFB_ReqRing.ReadIdx = 0x0;
	g_MFB_ReqRing.HWProcessIdx = 0x0;

	/* Enable clock */
	MFB_EnableClock(MTRUE);
	g_u4MfbCnt = 0;
	log_dbg("MFB open g_u4EnableClockCount: %d", g_u4EnableClockCount);
	/*  */

	for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++)
		MFBInfo.IrqInfo.Status[i] = 0;

	for (i = 0; i < _SUPPORT_MAX_MFB_FRAME_REQUEST_; i++)
		MFBInfo.ProcessID[i] = 0;

	MFBInfo.WriteReqIdx = 0;
	MFBInfo.ReadReqIdx = 0;
	MFBInfo.IrqInfo.MfbIrqCnt = 0;

/*#define KERNEL_LOG*/
#ifdef KERNEL_LOG
    /* In EP, Add MFB_DBG_WRITE_REG for debug. Should remove it after EP */
	MFBInfo.DebugMask = (MFB_DBG_INT | MFB_DBG_DBGLOG | MFB_DBG_WRITE_REG);
#endif
	/*  */
EXIT:




	log_dbg("- X. Ret: %d. UserCount: %d.", Ret, MFBInfo.UserCount);
	return Ret;

}

/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_release(struct inode *pInode, struct file *pFile)
{
	struct MFB_USER_INFO_STRUCT *pUserInfo;
	/*unsigned int Reg;*/

	log_dbg("- E. UserCount: %d.", MFBInfo.UserCount);

	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo =
			(struct  MFB_USER_INFO_STRUCT *) pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&(MFBInfo.SpinLockMFBRef));
	MFBInfo.UserCount--;

	if (MFBInfo.UserCount > 0) {
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
		log_dbg("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			MFBInfo.UserCount, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
	/*  */
	log_dbg("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), last user",
		MFBInfo.UserCount, current->comm, current->pid, current->tgid);


	/* Disable clock. */
	MFB_EnableClock(MFALSE);
	log_dbg("MFB release g_u4EnableClockCount: %d", g_u4EnableClockCount);

	/*  */
EXIT:


	log_dbg("- X. UserCount: %d.", MFBInfo.UserCount);
	return 0;
}


/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	length = pVma->vm_end - pVma->vm_start;
	/*  */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;

	log_inf(
	    "%s:vm_pgoff(0x%lx) pfn(0x%x) phy(0x%lx) vm_start(0x%lx) vm_end(0x%lx) length(0x%lx)",
	    __func__,
	    pVma->vm_pgoff,
	    pfn, pVma->vm_pgoff << PAGE_SHIFT,
	    pVma->vm_start, pVma->vm_end, length);

	switch (pfn) {
	case MFB_BASE_HW:
		if (length > MFB_REG_RANGE) {
			log_err("mmap range error :module:0x%x length(0x%lx),MFB_REG_RANGE(0x%x)!",
				pfn, length, MFB_REG_RANGE);
			return -EAGAIN;
		}
		break;
	default:
		log_err("Illegal starting HW addr for mmap!");
		return -EAGAIN;
	}
	if (remap_pfn_range(
		pVma, pVma->vm_start, pVma->vm_pgoff,
		pVma->vm_end - pVma->vm_start,
		pVma->vm_page_prot)) {
		return -EAGAIN;
	}
	/*  */
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/

static dev_t MFBDevNo;
static struct cdev *pMFBCharDrv;
static struct class *pMFBClass;

static const struct file_operations MFBFileOper = {
	.owner = THIS_MODULE,
	.open = MFB_open,
	.release = MFB_release,
	/* .flush   = mt_MFB_flush, */
	.mmap = MFB_mmap,
	.unlocked_ioctl = MFB_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = MFB_ioctl_compat,
#endif
};

/******************************************************************************
 *
 ******************************************************************************/
static inline void MFB_UnregCharDev(void)
{
	log_dbg("- E.");
	/*  */
	/* Release char driver */
	if (pMFBCharDrv != NULL) {
		cdev_del(pMFBCharDrv);
		pMFBCharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(MFBDevNo, 1);
}

/******************************************************************************
 *
 ******************************************************************************/
static inline signed int MFB_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	log_dbg("- E.");
	/*  */
	Ret = alloc_chrdev_region(&MFBDevNo, 0, 1, MFB_DEV_NAME);
	if (Ret < 0) {
		log_err("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pMFBCharDrv = cdev_alloc();
	if (pMFBCharDrv == NULL) {
		log_err("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pMFBCharDrv, &MFBFileOper);
	/*  */
	pMFBCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pMFBCharDrv, MFBDevNo, 1);
	if (Ret < 0) {
		log_err("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		MFB_UnregCharDev();

	/*  */

	log_dbg("- X.");
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*struct resource *pRes = NULL;*/
	signed int i = 0;
	unsigned char n;
	unsigned int irq_info[3];  /* Record interrupts info from device tree */
	struct device *dev = NULL;
	struct MFB_device *_mfb_dev;


#ifdef CONFIG_OF
	struct MFB_device *MFB_dev;
#endif

	log_inf("- E. MFB driver probe.\n");

	/* Check platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		dev_dbg(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_MFB_devs += 1;
	_mfb_dev = krealloc(MFB_devs,
		sizeof(struct MFB_device) * nr_MFB_devs, GFP_KERNEL);
	if (!_mfb_dev) {
		dev_dbg(&pDev->dev, "Unable to allocate MFB_devs\n");
		return -ENOMEM;
	}
	MFB_devs = _mfb_dev;

	MFB_dev = &(MFB_devs[nr_MFB_devs - 1]);
	MFB_dev->dev = &pDev->dev;

	/* iomap registers */
	MFB_dev->regs = of_iomap(pDev->dev.of_node, 0);
	/* gISPSYS_Reg[nr_MFB_devs - 1] = MFB_dev->regs; */

	if (!MFB_dev->regs) {
		dev_dbg(&pDev->dev,
			"Unable to ioremap registers, of_iomap fail, nr_MFB_devs=%d, devnode(%s).\n",
			nr_MFB_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

	log_inf("nr_MFB_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_MFB_devs,
		pDev->dev.of_node->name, (unsigned long)MFB_dev->regs);

	/* get IRQ ID and request IRQ */
	MFB_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (MFB_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array(
		    pDev->dev.of_node, "interrupts",
		    irq_info, ARRAY_SIZE(irq_info))) {
			dev_dbg(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++) {
			if (strcmp(pDev->dev.of_node->name,
			    MFB_IRQ_CB_TBL[i].device_name) == 0) {
				Ret = request_irq(MFB_dev->irq,
				    (irq_handler_t) MFB_IRQ_CB_TBL[i].isr_fp,
				    irq_info[2],
				    (const char *)MFB_IRQ_CB_TBL[i]
					.device_name,
				    NULL);
				if (Ret) {
					dev_dbg(
					    &pDev->dev,
					    "Unable to request IRQ, request_irq fail, nr_MFB_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
					    nr_MFB_devs,
					    pDev->dev.of_node->name,
					    MFB_dev->irq,
					    MFB_IRQ_CB_TBL[i].device_name);
					return Ret;
				}

				log_inf(
				    "nr_MFB_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
				    nr_MFB_devs,
				    pDev->dev.of_node->name,
				    MFB_dev->irq,
				    MFB_IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= MFB_IRQ_TYPE_AMOUNT) {
			log_inf(
			    "No corresponding ISR!!: nr_MFB_devs=%d, devnode(%s), irq=%d\n",
			    nr_MFB_devs,
			    pDev->dev.of_node->name,
			    MFB_dev->irq);
		}


	} else {
		log_inf("No IRQ!!: nr_MFB_devs=%d, devnode(%s), irq=%d\n",
			nr_MFB_devs,
			pDev->dev.of_node->name,
			MFB_dev->irq);
	}


#endif

	/* Only register char driver in the 1st time */
	if (nr_MFB_devs == 1) {

		/* Register char driver */
		Ret = MFB_RegCharDev();
		if (Ret) {
			dev_dbg(&pDev->dev, "register char failed");
			return Ret;
		}
#ifndef EP_NO_CLKMGR
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
		    /*CCF: Grab clock pointer (struct clk*) */
#ifndef SMI_CLK
		mfb_clk.CG_SCP_SYS_MM0 = devm_clk_get(
			&pDev->dev, "MFB_SCP_SYS_MM0");
		mfb_clk.CG_MM_SMI_COMMON = devm_clk_get(
			&pDev->dev, "MFB_CLK_MM_CG2_B11");
		mfb_clk.CG_MM_SMI_COMMON_2X = devm_clk_get(
			&pDev->dev, "MFB_CLK_MM_CG2_B12");
		mfb_clk.CG_MM_SMI_COMMON_GALS_M0_2X = devm_clk_get(
			&pDev->dev, "MFB_CLK_MM_CG1_B12");
		mfb_clk.CG_MM_SMI_COMMON_GALS_M1_2X = devm_clk_get(
			&pDev->dev, "MFB_CLK_MM_CG1_B13");
		mfb_clk.CG_MM_SMI_COMMON_UPSZ0 = devm_clk_get(
			&pDev->dev, "MFB_CLK_MM_CG1_B14");
		mfb_clk.CG_MM_SMI_COMMON_UPSZ1 = devm_clk_get(
			&pDev->dev, "MFB_CLK_MM_CG1_B15");
		mfb_clk.CG_MM_SMI_COMMON_FIFO0 = devm_clk_get(
			&pDev->dev, "MFB_CLK_MM_CG1_B16");
		mfb_clk.CG_MM_SMI_COMMON_FIFO1 = devm_clk_get(
			&pDev->dev, "MFB_CLK_MM_CG1_B17");
		mfb_clk.CG_MM_LARB5 = devm_clk_get(
			&pDev->dev, "MFB_CLK_MM_CG1_B10");
		mfb_clk.CG_SCP_SYS_ISP = devm_clk_get(
			&pDev->dev, "MFB_SCP_SYS_ISP");
		mfb_clk.CG_IMGSYS_LARB = devm_clk_get(
			&pDev->dev, "MFB_CLK_IMG_LARB");
#endif
		mfb_clk.CG_IMGSYS_MFB = devm_clk_get(
			&pDev->dev, "MFB_CLK_IMG_MFB");

#ifndef SMI_CLK
		if (IS_ERR(mfb_clk.CG_SCP_SYS_MM0)) {
			log_err("cannot get CG_SCP_SYS_MM0 clock\n");
			return PTR_ERR(mfb_clk.CG_SCP_SYS_MM0);
		}
		if (IS_ERR(mfb_clk.CG_MM_SMI_COMMON)) {
			log_err("cannot get CG_MM_SMI_COMMON clock\n");
			return PTR_ERR(mfb_clk.CG_MM_SMI_COMMON);
		}
		if (IS_ERR(mfb_clk.CG_MM_SMI_COMMON_2X)) {
			log_err("cannot get CG_MM_SMI_COMMON_2X clock\n");
			return PTR_ERR(mfb_clk.CG_MM_SMI_COMMON_2X);
		}
		if (IS_ERR(mfb_clk.CG_MM_SMI_COMMON_GALS_M0_2X)) {
			log_err(
			    "cannot get CG_MM_SMI_COMMON_GALS_M0_2X clock\n");
			return PTR_ERR(mfb_clk.CG_MM_SMI_COMMON_GALS_M0_2X);
		}
		if (IS_ERR(mfb_clk.CG_MM_SMI_COMMON_GALS_M1_2X)) {
			log_err(
			    "cannot get CG_MM_SMI_COMMON_GALS_M1_2X clock\n");
			return PTR_ERR(mfb_clk.CG_MM_SMI_COMMON_GALS_M1_2X);
		}
		if (IS_ERR(mfb_clk.CG_MM_SMI_COMMON_UPSZ0)) {
			log_err(
			    "cannot get CG_MM_SMI_COMMON_UPSZ0 clock\n");
			return PTR_ERR(mfb_clk.CG_MM_SMI_COMMON_UPSZ0);
		}
		if (IS_ERR(mfb_clk.CG_MM_SMI_COMMON_UPSZ1)) {
			log_err("cannot get CG_MM_SMI_COMMON_UPSZ1 clock\n");
			return PTR_ERR(mfb_clk.CG_MM_SMI_COMMON_UPSZ1);
		}
		if (IS_ERR(mfb_clk.CG_MM_SMI_COMMON_FIFO0)) {
			log_err("cannot get CG_MM_SMI_COMMON_FIFO0 clock\n");
			return PTR_ERR(mfb_clk.CG_MM_SMI_COMMON_FIFO0);
		}
		if (IS_ERR(mfb_clk.CG_MM_SMI_COMMON_FIFO1)) {
			log_err("cannot get CG_MM_SMI_COMMON_FIFO1 clock\n");
			return PTR_ERR(mfb_clk.CG_MM_SMI_COMMON_FIFO1);
		}
		if (IS_ERR(mfb_clk.CG_MM_LARB5)) {
			log_err("cannot get CG_MM_LARB5 clock\n");
			return PTR_ERR(mfb_clk.CG_MM_LARB5);
		}
		if (IS_ERR(mfb_clk.CG_SCP_SYS_ISP)) {
			log_err("cannot get CG_SCP_SYS_ISP clock\n");
			return PTR_ERR(mfb_clk.CG_SCP_SYS_ISP);
		}
		if (IS_ERR(mfb_clk.CG_IMGSYS_LARB)) {
			log_err("cannot get CG_IMGSYS_LARB clock\n");
			return PTR_ERR(mfb_clk.CG_IMGSYS_LARB);
		}
#endif
		if (IS_ERR(mfb_clk.CG_IMGSYS_MFB)) {
			log_err("cannot get CG_IMGSYS_MFB clock\n");
			return PTR_ERR(mfb_clk.CG_IMGSYS_MFB);
		}
#endif	/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
#endif

		/* Create class register */
		pMFBClass = class_create(THIS_MODULE, "MFBdrv");
		if (IS_ERR(pMFBClass)) {
			Ret = PTR_ERR(pMFBClass);
			log_err("Unable to create class, err = %d", Ret);
			goto EXIT;
		}

		dev = device_create(pMFBClass, NULL,
			MFBDevNo, NULL, MFB_DEV_NAME);
		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			dev_dbg(
			    &pDev->dev,
			    "Failed to create device: /dev/%s, err = %d",
			    MFB_DEV_NAME, Ret);
			goto EXIT;
		}

		/* Init spinlocks */
		spin_lock_init(&(MFBInfo.SpinLockMFBRef));
		spin_lock_init(&(MFBInfo.SpinLockMFB));
		for (n = 0; n < MFB_IRQ_TYPE_AMOUNT; n++)
			spin_lock_init(&(MFBInfo.SpinLockIrq[n]));

		/*  */
		init_waitqueue_head(&MFBInfo.WaitQueueHead);
		INIT_WORK(&MFBInfo.ScheduleMfbWork, MFB_ScheduleWork);

		wakeup_source_init(&MFB_wake_lock, "mfb_lock_wakelock");

		for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(MFB_tasklet[i].pMFB_tkt,
			MFB_tasklet[i].tkt_cb, 0);




		/* Init MFBInfo */
		spin_lock(&(MFBInfo.SpinLockMFBRef));
		MFBInfo.UserCount = 0;
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
		/*  */
		MFBInfo.IrqInfo.Mask[MFB_IRQ_TYPE_INT_MFB_ST] = INT_ST_MASK_MFB;

	}

EXIT:
	if (Ret < 0)
		MFB_UnregCharDev();


	log_inf("- X. MFB driver probe.");

	return Ret;
}

/******************************************************************************
 * Called when the device is being detached from the driver
 ******************************************************************************/
static signed int MFB_remove(struct platform_device *pDev)
{
	/*struct resource *pRes;*/
	signed int IrqNum;
	int i;
	/*  */
	log_dbg("- E.");
	/* unregister char driver. */
	MFB_UnregCharDev();

	/* Release IRQ */
	disable_irq(MFBInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(MFB_tasklet[i].pMFB_tkt);
#if 0
	/* free all registered irq(child nodes) */
	MFB_UnRegister_AllregIrq();
	/* free father nodes of irq user list */
	struct my_list_head *head;
	struct my_list_head *father;

	head = ((struct my_list_head *)(&SupIrqUserListHead.list));
	while (1) {
		father = head;
		if (father->nextirq != father) {
			father = father->nextirq;
			REG_IRQ_NODE *accessNode;

			typeof(((REG_IRQ_NODE *) 0)->list) * __mptr = (father);
			accessNode =
			    ((REG_IRQ_NODE *) ((char *)__mptr -
			    offsetof(REG_IRQ_NODE, list)));
			log_inf("free father,reg_T(%d)\n", accessNode->reg_T);
			if (father->nextirq != father) {
				head->nextirq = father->nextirq;
				father->nextirq = father;
			} else {	/* last father node */
				head->nextirq = head;
				log_inf("break\n");
				break;
			}
			kfree(accessNode);
		}
	}
#endif
	/*  */
	device_destroy(pMFBClass, MFBDevNo);
	/*  */
	class_destroy(pMFBClass);
	pMFBClass = NULL;
	/*  */
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int bPass1_On_In_Resume_TG1;

static signed int MFB_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	/*signed int ret = 0;*/

	log_dbg("bPass1_On_In_Resume_TG1(%d)\n", bPass1_On_In_Resume_TG1);

	bPass1_On_In_Resume_TG1 = 0;

	if (g_u4EnableClockCount > 0) {
		MFB_EnableClock(MFALSE);
		g_u4MfbCnt++;
	}
	return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
static signed int MFB_resume(struct platform_device *pDev)
{
	log_dbg("bPass1_On_In_Resume_TG1(%d).\n", bPass1_On_In_Resume_TG1);

	if (g_u4MfbCnt > 0) {
		MFB_EnableClock(MTRUE);
		g_u4MfbCnt--;
	}
	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int MFB_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/* pr_debug("calling %s()\n", __func__); */
	log_inf("MFB suspend g_u4EnableClockCount: %d, g_u4MfbCnt: %d",
		g_u4EnableClockCount, g_u4MfbCnt);

	return MFB_suspend(pdev, PMSG_SUSPEND);
}

int MFB_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/* pr_debug("calling %s()\n", __func__); */
	log_inf("MFB resume g_u4EnableClockCount: %d, g_u4MfbCnt: %d",
		g_u4EnableClockCount, g_u4MfbCnt);

	return MFB_resume(pdev);
}
#ifndef CONFIG_OF
/*extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);*/
/*extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);*/
#endif
int MFB_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
/*	mt_irq_set_sens(MFB_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);*/
/*	mt_irq_set_polarity(MFB_IRQ_BIT_ID, MT_POLARITY_LOW);*/
#endif
	return 0;

}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define MFB_pm_suspend NULL
#define MFB_pm_resume  NULL
#define MFB_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_OF
/*
 * Note!!! The order and member of .compatible must be the
 * same with MFB_DEV_NODE_IDX
 */
static const struct of_device_id MFB_of_ids[] = {
/*	{.compatible = "mediatek,imgsyscq",},*/
	{.compatible = "mediatek,mfb",},
	{}
};
#endif

const struct dev_pm_ops MFB_pm_ops = {
	.suspend = MFB_pm_suspend,
	.resume = MFB_pm_resume,
	.freeze = MFB_pm_suspend,
	.thaw = MFB_pm_resume,
	.poweroff = MFB_pm_suspend,
	.restore = MFB_pm_resume,
	.restore_noirq = MFB_pm_restore_noirq,
};


/******************************************************************************
 *
 ******************************************************************************/
static struct platform_driver MFBDriver = {
	.probe = MFB_probe,
	.remove = MFB_remove,
	.suspend = MFB_suspend,
	.resume = MFB_resume,
	.driver = {
		   .name = MFB_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = MFB_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &MFB_pm_ops,
#endif
	}
};


static int mfb_dump_read(struct seq_file *m, void *v)
{
	int i, j;

	seq_puts(m, "\n============ mfb dump register============\n");
	seq_puts(m, "MFB Config Info\n");

	if (MFBInfo.UserCount > 0) {
		for (i = 0x2C; i < 0x8C; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(MFB_BASE_HW + i),
				(unsigned int)MFB_RD32(ISP_MFB_BASE + i));
		}
		seq_puts(m, "MFB Debug Info\n");
		for (i = 0x120; i < 0x148; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(MFB_BASE_HW + i),
				(unsigned int)MFB_RD32(ISP_MFB_BASE + i));
		}

		seq_puts(m, "MFB Config Info\n");
		for (i = 0x230; i < 0x2D8; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(MFB_BASE_HW + i),
				(unsigned int)MFB_RD32(ISP_MFB_BASE + i));
		}
		seq_puts(m, "MFB Debug Info\n");
		for (i = 0x2F4; i < 0x30C; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				(unsigned int)(MFB_BASE_HW + i),
				(unsigned int)MFB_RD32(ISP_MFB_BASE + i));
		}
	}
	seq_puts(m, "\n");
	seq_printf(m, "Mfb Clock Count:%d\n", g_u4EnableClockCount);

	seq_printf(m, "MFB:HWProcessIdx:%d, WriteIdx:%d, ReadIdx:%d\n",
		   g_MFB_ReqRing.HWProcessIdx, g_MFB_ReqRing.WriteIdx,
		   g_MFB_ReqRing.ReadIdx);

	for (i = 0; i < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; i++) {
		seq_printf(m,
			   "MFB:State:%d, processID:0x%08X, callerID:0x%08X, enqueReqNum:%d, FrameWRIdx:%d, RrameRDIdx:%d\n",
			   g_MFB_ReqRing.MFBReq_Struct[i].State,
			   g_MFB_ReqRing.MFBReq_Struct[i].processID,
			   g_MFB_ReqRing.MFBReq_Struct[i].callerID,
			   g_MFB_ReqRing.MFBReq_Struct[i].enqueReqNum,
			   g_MFB_ReqRing.MFBReq_Struct[i].FrameWRIdx,
			   g_MFB_ReqRing.MFBReq_Struct[i].RrameRDIdx);

		for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_;) {
			seq_printf(m,
				   "MFB:FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d\n",
				   j,
				   g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[j],
				   j + 1,
				   g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[j + 1],
				   j + 2,
				   g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[j + 2],
				   j + 3,
				   g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[j + 3]);
			j = j + 4;
		}
	}

	seq_puts(m, "\n============ mfb dump debug ============\n");

	return 0;
}


static int proc_mfb_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mfb_dump_read, NULL);
}

static const struct file_operations mfb_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_mfb_dump_open,
	.read = seq_read,
};


static int mfb_reg_read(struct seq_file *m, void *v)
{
	unsigned int i;

	seq_puts(m, "======== read mfb register ========\n");

	if (MFBInfo.UserCount > 0) {
		for (i = 0x0; i <= 0x308; i = i + 4) {
			seq_printf(m, "[0x%08X 0x%08X]\n",
				(unsigned int)(MFB_BASE_HW + i),
				(unsigned int)MFB_RD32(ISP_MFB_BASE + i));
		}
	}
	seq_printf(m, "[0x%08X 0x%08X]\n",
		(unsigned int)(MFB_DMA_DEBUG_ADDR_HW),
		(unsigned int)MFB_RD32(MFB_DMA_DEBUG_ADDR_REG));


	return 0;
}

/*static int mfb_reg_write(struct file *file, const char __user *buffer, */
/*	size_t count, loff_t *data)*/

static ssize_t mfb_reg_write(
	struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;
	/*char *pEnd;*/
	char addrSzBuf[24];
	char valSzBuf[24];
	char *pszTmp;
	int addr = 0, val = 0;
	long int tempval;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	if (MFBInfo.UserCount <= 0)
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%23s %23s", addrSzBuf, valSzBuf) == 2) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
			if (kstrtol(addrSzBuf, 10,
			    (long int *)&tempval) != 0)
				log_err("scan decimal addr is wrong !!:%s",
				    addrSzBuf);
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					log_err(
					    "scan hexadecimal addr is wrong !!:%s",
					    addrSzBuf);
			} else {
				log_inf("MFB Write Addr Error!!:%s", addrSzBuf);
			}
		}

		pszTmp = strstr(valSzBuf, "0x");
		if (pszTmp == NULL) {
			if (kstrtol(valSzBuf, 10, (long int *)&tempval) != 0)
				log_err("scan decimal value is wrong !!:%s",
				    valSzBuf);
		} else {
			if (strlen(valSzBuf) > 2) {
				if (sscanf(valSzBuf + 2, "%x", &val) != 1)
					log_err(
					    "scan hexadecimal value is wrong !!:%s",
					    valSzBuf);
			} else {
				log_inf(
				    "MFB Write Value Error!!:%s\n",
				    valSzBuf);
			}
		}

		if ((addr >= MFB_BASE_HW) && (addr <= CRSP_CROP_Y_HW)
			&& ((addr & 0x3) == 0)) {
			log_inf("Write Request - addr:0x%x, value:0x%x\n",
			    addr, val);
			MFB_WR32((ISP_MFB_BASE + (addr - MFB_BASE_HW)), val);
		} else {
			log_inf(
			  "Write-Address Range exceeds the size of hw mfb!! addr:0x%x, value:0x%x\n",
			   addr, val);
		}

	} else if (sscanf(desc, "%23s", addrSzBuf) == 1) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
			if (kstrtol(addrSzBuf, 10, (long int *)&tempval) != 0)
				log_err("scan decimal addr is wrong !!:%s",
				    addrSzBuf);
			else
				addr = tempval;
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					log_err("scan hexadecimal addr is wrong !!:%s",
					    addrSzBuf);
			} else {
				log_inf("MFB Read Addr Error!!:%s", addrSzBuf);
			}
		}

		if ((addr >= MFB_BASE_HW) && (addr <= CRSP_CROP_Y_HW)
			&& ((addr & 0x3) == 0)) {
			val = MFB_RD32((ISP_MFB_BASE + (addr - MFB_BASE_HW)));
			log_inf("Read Request - addr:0x%x,value:0x%x\n",
			     addr, val);
		} else {
			log_inf(
			  "Read-Address Range exceeds the size of hw mfb!! addr:0x%x, value:0x%x\n",
			  addr, val);
		}

	}


	return count;
}

static int proc_mfb_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, mfb_reg_read, NULL);
}

static const struct file_operations mfb_reg_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_mfb_reg_open,
	.read = seq_read,
	.write = mfb_reg_write,
};


/******************************************************************************
 *
 ******************************************************************************/

int32_t MFB_ClockOnCallback(uint64_t engineFlag)
{
	/* log_dbg("MFB_ClockOnCallback"); */
	/* log_dbg("+CmdqEn:%d", g_u4EnableClockCount); */
	/* MFB_EnableClock(MTRUE); */

	return 0;
}

int32_t MFB_DumpCallback(uint64_t engineFlag, int level)
{
	log_dbg("MFB_DumpCallback");

	MFB_DumpReg();

	return 0;
}

int32_t MFB_ResetCallback(uint64_t engineFlag)
{
	log_dbg("MFB_ResetCallback");
	MFB_Reset();

	return 0;
}

int32_t MFB_ClockOffCallback(uint64_t engineFlag)
{
	/* log_dbg("MFB_ClockOffCallback"); */
	/* MFB_EnableClock(MFALSE); */
	/* log_dbg("-CmdqEn:%d", g_u4EnableClockCount); */
	return 0;
}


static signed int __init MFB_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	/* FIX-ME: linux-3.10 procfs API changed */
	/* use proc_create */
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *isp_mfb_dir;


	int i;
	/*  */
	log_dbg("- E.");
	/*  */
	Ret = platform_driver_register(&MFBDriver);
	if (Ret < 0) {
		log_err("platform_driver_register fail");
		return Ret;
	}

#if 0
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,MFB");
	if (!node) {
		log_err("find mediatek,MFB node failed!!!\n");
		return -ENODEV;
	}
	ISP_MFB_BASE = of_iomap(node, 0);
	if (!ISP_MFB_BASE) {
		log_err("unable to map ISP_MFB_BASE registers!!!\n");
		return -ENODEV;
	}
	log_dbg("ISP_MFB_BASE: %lx\n", ISP_MFB_BASE);
#endif

	isp_mfb_dir = proc_mkdir("mfb", NULL);
	if (!isp_mfb_dir) {
		log_err("[%s]: fail to mkdir /proc/mfb\n", __func__);
		return 0;
	}

	/* proc_entry = proc_create("pll_test", 0644, */
	/*	isp_mfb_dir, &pll_test_proc_fops); */

	proc_entry = proc_create("mfb_dump", 0444,
		isp_mfb_dir, &mfb_dump_proc_fops);

	proc_entry = proc_create("mfb_reg", 0644,
		isp_mfb_dir, &mfb_reg_proc_fops);


	/* isr log */
	if (PAGE_SIZE <
	    ((MFB_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN * ((
	    DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) *
	     LOG_PPNUM)) {
		i = 0;
		while (i <
		       ((MFB_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
			 ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
			i += PAGE_SIZE;
		}
	} else {
		i = PAGE_SIZE;
	}
	pLog_kmalloc = kmalloc(i, GFP_KERNEL);
	if (pLog_kmalloc == NULL) {
		log_err
			("log mem not enough\n");
		return -ENOMEM;
	}
	memset(pLog_kmalloc, 0x00, i);
	tmp = pLog_kmalloc;
	for (i = 0; i < LOG_PPNUM; i++) {
		for (j = 0; j < MFB_IRQ_TYPE_AMOUNT; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + */
			/*	(NORMAL_STR_LEN*DBG_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + */
			/*	(NORMAL_STR_LEN*INF_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			/* tmp = (void*) ((unsigned int)tmp + */
			/*	(NORMAL_STR_LEN*ERR_PAGE)); */
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * ERR_PAGE));
		}
		 //log buffer ,in case of overflow
		/* tmp = (void*) ((unsigned int)tmp + NORMAL_STR_LEN); */
		/* log buffer ,in case of overflow */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);
	}


#if 1
	/* Cmdq */
	/* Register MFB callback */
	log_dbg("register mfb callback for CMDQ");
	cmdqCoreRegisterCB(CMDQ_GROUP_MFB,
			   MFB_ClockOnCallback,
			   MFB_DumpCallback,
			   MFB_ResetCallback,
			   MFB_ClockOffCallback);
#endif

	log_dbg("- X. Ret: %d.", Ret);
	return Ret;
}

/******************************************************************************
 *
 ******************************************************************************/
static void __exit MFB_Exit(void)
{
	/*int i;*/

	log_dbg("- E.");
	/*  */
	platform_driver_unregister(&MFBDriver);
	/*  */
#if 1
	/* Cmdq */
	/* Unregister MFB callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_MFB, NULL, NULL, NULL, NULL);
#endif

	kfree(pLog_kmalloc);

	/*  */
}


/******************************************************************************
 *
 ******************************************************************************/
void MFB_ScheduleWork(struct work_struct *data)
{
	if (MFB_DBG_DBGLOG & MFBInfo.DebugMask)
		log_dbg("- E.");

#ifdef MFB_USE_GCE
#else
	ConfigMFB();
#endif
}


static irqreturn_t ISP_Irq_MFB(signed int Irq, void *DeviceId)
{
	unsigned int MfbStatus;
	bool bResulst = MFALSE;
	pid_t ProcessID;

	MfbStatus = MFB_RD32(MFB_INT_STATUS_REG);	/* MFB Status */

	spin_lock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]));

	if (MFB_INT_ST == (MFB_INT_ST & MfbStatus)) {
		/* Update the frame status. */
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("mfb_irq");
#endif

#ifndef MFB_USE_GCE
		MFB_WR32(MFB_START_REG, 0);
#endif
		bResulst = UpdateMFB(&ProcessID);
		/* ConfigMFB(); */
		if (bResulst == MTRUE) {
			schedule_work(&MFBInfo.ScheduleMfbWork);
#ifdef MFB_USE_GCE
			MFBInfo.IrqInfo.Status[
				MFB_IRQ_TYPE_INT_MFB_ST] |= MFB_INT_ST;
			MFBInfo.IrqInfo.ProcessID[
				MFB_PROCESS_ID_MFB] = ProcessID;
			MFBInfo.IrqInfo.MfbIrqCnt++;
			MFBInfo.ProcessID[
				MFBInfo.WriteReqIdx] = ProcessID;
			MFBInfo.WriteReqIdx =
			    (MFBInfo.WriteReqIdx + 1) %
			    _SUPPORT_MAX_MFB_FRAME_REQUEST_;
#ifdef MFB_MULTIPROCESS_TIMEING_ISSUE
			/* check the write value is equal to read value ? */
			/* actually, it doesn't happen!! */
			if (MFBInfo.WriteReqIdx == MFBInfo.ReadReqIdx) {
				IRQ_LOG_KEEPER(
				  MFB_IRQ_TYPE_INT_MFB_ST,
				  m_CurrentPPB, _LOG_ERR,
				  "%s Err!!, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
				  __func__,
				  MFBInfo.WriteReqIdx,
				  MFBInfo.ReadReqIdx);
			}
#endif

#else
			MFBInfo.IrqInfo.Status[
				MFB_IRQ_TYPE_INT_MFB_ST] |= MFB_INT_ST;
			MFBInfo.IrqInfo.ProcessID[
				MFB_PROCESS_ID_MFB] = ProcessID;
#endif
		}
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
		/* Config the Next frame */
	}
	spin_unlock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]));
	if (bResulst == MTRUE)
		wake_up_interruptible(&MFBInfo.WaitQueueHead);

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(MFB_IRQ_TYPE_INT_MFB_ST, m_CurrentPPB, _LOG_INF,
		       "Irq_MFB:%d, reg 0x%x : 0x%x, bResulst:%d, MfbHWSta:0x%x, MfbIrqCnt:0x%x, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
		       Irq, MFB_INT_STATUS_HW, MfbStatus, bResulst, MfbStatus,
		       MFBInfo.IrqInfo.MfbIrqCnt,
		       MFBInfo.WriteReqIdx, MFBInfo.ReadReqIdx);
	/* IRQ_LOG_KEEPER(MFB_IRQ_TYPE_INT_MFB_ST, m_CurrentPPB,
	 *	_LOG_INF, "MfbHWSta:0x%x, MfbHWSta:0x%x,
	 *	DpeDveSta0:0x%x\n", DveStatus, MfbStatus, DpeDveSta0);
	 */

	if (MfbStatus & MFB_INT_ST)
		tasklet_schedule(MFB_tasklet[MFB_IRQ_TYPE_INT_MFB_ST].pMFB_tkt);

	return IRQ_HANDLED;
}

static void ISP_TaskletFunc_MFB(unsigned long data)
{
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MFB_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MFB_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MFB_ST, m_CurrentPPB, _LOG_ERR);

}


/******************************************************************************
 *
 ******************************************************************************/
module_init(MFB_Init);
module_exit(MFB_Exit);
MODULE_DESCRIPTION("Camera MFB driver");
MODULE_AUTHOR("MM3SW5");
MODULE_LICENSE("GPL");
