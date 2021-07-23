/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

/*****************************************************************************
 * camera_fdvt.c - Linux FDVT Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers FDVT relative functions
 *
 *****************************************************************************/
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
#include <linux/vmalloc.h>
#include <linux/seq_file.h>

/*#include <linux/xlog.h>		 For xlog_printk(). */
/*  */
/*#include <mach/hardware.h>*/
/* #include <mach/mt6593_pll.h> */
#include "inc/camera_fdvt.h"
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
 * #define __FDVT_KERNEL_PERFORMANCE_MEASURE__
 */
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
#endif
#if 0
/* Another Performance Measure Usage */
#include <linux/kallsyms.h>
#include <linux/ftrace_event.h>
static unsigned long __read_mostly tracing_mark_write_addr;
#define _kernel_trace_begin(name) {\
	tracing_mark_write_addr = kallsyms_lookup_name("tracing_mark_write");\
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
//#include"../../../smi/smi_debug.h"

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*  #include "smi_common.h" */

//#include <linux/wakelock.h>
#ifdef CONFIG_PM_WAKELOCKS // modified by gasper for build pass
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

/* FDVT Command Queue */
/* #include "../../cmdq/mt6797/cmdq_record.h" */
/* #include "../../cmdq/mt6797/cmdq_core.h" */

/* CCF */
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#include <linux/clk.h>
struct FDVT_CLK_STRUCT {
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
	struct clk *CG_IMGSYS_FDVT;
};
struct FDVT_CLK_STRUCT fdvt_clk;
#endif
/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */

#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define FDVT_DEV_NAME                "camera-fdvt"
#define EP_NO_CLKMGR // GASPER ADD
#define BYPASS_REG         (0)
/* #define FDVT_WAITIRQ_LOG  */
#define FDVT_USE_GCE
/* #define FDVT_DEBUG_USE */
#define DUMMY_FDVT	   (0)
/* #define FDVT_MULTIPROCESS_TIMEING_ISSUE  */
/*I can' test the situation in FPGA due to slow FPGA. */
#define MyTag "[FDVT]"
#define IRQTag "KEEPER"

#define log_vrb(format, args...) \
pr_debug(MyTag "[%s] " format, __func__, ##args)

#ifdef FDVT_DEBUG_USE
#define log_dbg(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#else
#define log_dbg(format, args...)
#endif

#define log_inf(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#define log_notice(format, args...) \
pr_notice(MyTag "[%s] " format, __func__, ##args)
#define log_wrn(format, args...) \
pr_debug(MyTag "[%s] " format, __func__, ##args)
#define log_err(format, args...) \
pr_debug(MyTag "[%s] " format, __func__, ##args)
#define log_ast(format, args...) \
pr_debug(MyTag "[%s] " format, __func__, ##args)

/*****************************************************************************
 *
 *****************************************************************************/
// For other projects.
// #define FDVT_WR32(addr, data)    iowrite32(data, addr)
// For 89 Only.   // NEED_TUNING_BY_PROJECT
#define FDVT_WR32(addr, data)    mt_reg_sync_writel(data, addr)
#define FDVT_RD32(addr)          ioread32(addr)
/*****************************************************************************
 *
 *****************************************************************************/
/* dynamic log level */
#define FDVT_DBG_DBGLOG              (0x00000001)
#define FDVT_DBG_INFLOG              (0x00000002)
#define FDVT_DBG_INT                 (0x00000004)
#define FDVT_DBG_READ_REG            (0x00000008)
#define FDVT_DBG_WRITE_REG           (0x00000010)
#define FDVT_DBG_TASKLET             (0x00000020)

/*
 *    CAM interrupt status
 */

/* normal siganl : happens to be the same bit as register bit*/
/*#define FDVT_INT_ST           (1<<0)*/


/*
 *   IRQ signal mask
 */

#define INT_ST_MASK_FDVT     ( \
			FDVT_INT_ST)

#define CMDQ_REG_MASK 0xffffffff
#define FDVT_START_MASK      0x1

#define FDVT_IS_BUSY    0x1000000


/* static irqreturn_t FDVT_Irq_CAM_A(signed int  Irq,void *DeviceId); */
static irqreturn_t ISP_Irq_FDVT(signed int Irq, void *DeviceId);
static bool ConfigFDVT(void);
static signed int ConfigFDVTHW(FDVT_Config *pFdvtConfig);
static void FDVT_ScheduleWork(struct work_struct *data);
static signed int FDVT_DumpReg(void);


typedef irqreturn_t(*IRQ_CB) (signed int, void *);

struct ISR_TABLE {
	IRQ_CB isr_fp;
	unsigned int int_number;
	char device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE FDVT_IRQ_CB_TBL[FDVT_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_FDVT, FDVT_IRQ_BIT_ID, "fdvt"},
};

#else
/* int number is got from kernel api */
const struct ISR_TABLE FDVT_IRQ_CB_TBL[FDVT_IRQ_TYPE_AMOUNT] = {
#if DUMMY_FDVT
	{ISP_Irq_FDVT, 0, "fdvt-dummy"},
#else
	{ISP_Irq_FDVT, 0, "fdvt"},
#endif
};
#endif
/* ///////////////////////////////////////////////////////////////////////// */
/*  */
typedef void (*tasklet_cb) (unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct *pFDVT_tkt;
};

struct tasklet_struct Fdvttkt[FDVT_IRQ_TYPE_AMOUNT];

static void ISP_TaskletFunc_FDVT(unsigned long data);

static struct Tasklet_table FDVT_tasklet[FDVT_IRQ_TYPE_AMOUNT] = {
	{ISP_TaskletFunc_FDVT, &Fdvttkt[FDVT_IRQ_TYPE_INT_FDVT_ST]},
};

//struct wake_lock FDVT_wake_lock;
#ifdef CONFIG_PM_WAKELOCKS
struct wakeup_source FDVT_wake_lock;
#else
struct wake_lock FDVT_wake_lock;
#endif

static DEFINE_MUTEX(gFdvtMutex);
static DEFINE_MUTEX(gFdvtDequeMutex);

#ifdef CONFIG_OF

struct FDVT_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct FDVT_device *FDVT_devs;
static int nr_FDVT_devs;

/* Get HW modules' base address from device nodes */
#define FDVT_DEV_NODE_IDX 0
#define IMGSYS_DEV_MODE_IDX 1
/* static unsigned long gISPSYS_Reg[FDVT_IRQ_TYPE_AMOUNT]; */


#define ISP_FDVT_BASE                  (FDVT_devs[FDVT_DEV_NODE_IDX].regs)
#define ISP_IMGSYS_BASE               (FDVT_devs[IMGSYS_DEV_MODE_IDX].regs)

/* #define ISP_FDVT_BASE                  (gISPSYS_Reg[FDVT_DEV_NODE_IDX]) */



#else
#define ISP_FDVT_BASE                        (IMGSYS_BASE + 0x1000)

#endif


static unsigned int g_u4EnableClockCount;
static unsigned int g_u4FdvtCnt;

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32


enum FDVT_FRAME_STATUS_ENUM {
	FDVT_FRAME_STATUS_EMPTY,	/* 0 */
	FDVT_FRAME_STATUS_ENQUE,	/* 1 */
	FDVT_FRAME_STATUS_RUNNING,	/* 2 */
	FDVT_FRAME_STATUS_FINISHED,	/* 3 */
	FDVT_FRAME_STATUS_TOTAL
};


enum FDVT_REQUEST_STATE_ENUM {
	FDVT_REQUEST_STATE_EMPTY,	/* 0 */
	FDVT_REQUEST_STATE_PENDING,	/* 1 */
	FDVT_REQUEST_STATE_RUNNING,	/* 2 */
	FDVT_REQUEST_STATE_FINISHED,	/* 3 */
	FDVT_REQUEST_STATE_TOTAL
};

struct FDVT_REQUEST_STRUCT {
	enum FDVT_REQUEST_STATE_ENUM State;
	pid_t processID; /* caller process ID */
	unsigned int callerID; /* caller thread ID */
	/* to judge it belongs to which frame package */
	unsigned int enqueReqNum;
	signed int FrameWRIdx; /* Frame write Index */
	signed int RrameRDIdx; /* Frame read Index */
	enum FDVT_FRAME_STATUS_ENUM
	  FdvtFrameStatus[_SUPPORT_MAX_FDVT_FRAME_REQUEST_];
	FDVT_Config FdvtFrameConfig[_SUPPORT_MAX_FDVT_FRAME_REQUEST_];
};

struct FDVT_REQUEST_RING_STRUCT {
	signed int WriteIdx;	/* enque how many request  */
	signed int ReadIdx;		/* read which request index */
	signed int HWProcessIdx;	/* HWWriteIdx */
	struct FDVT_REQUEST_STRUCT
	  FDVTReq_Struct[_SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_];
};

struct  FDVT_CONFIG_STRUCT {
	FDVT_Config FdvtFrameConfig[_SUPPORT_MAX_FDVT_FRAME_REQUEST_];
};

static struct FDVT_REQUEST_RING_STRUCT g_FDVT_ReqRing;
static struct FDVT_CONFIG_STRUCT g_FdvtEnqueReq_Struct;
static struct FDVT_CONFIG_STRUCT g_FdvtDequeReq_Struct;


/*****************************************************************************
 *
 *****************************************************************************/
struct  FDVT_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};
enum FDVT_PROCESS_ID_ENUM {
	FDVT_PROCESS_ID_NONE,
	FDVT_PROCESS_ID_FDVT,
	FDVT_PROCESS_ID_AMOUNT
};


/*****************************************************************************
 *
 *****************************************************************************/
struct FDVT_IRQ_INFO_STRUCT {
	unsigned int Status[FDVT_IRQ_TYPE_AMOUNT];
	signed int FdvtIrqCnt;
	pid_t ProcessID[FDVT_PROCESS_ID_AMOUNT];
	unsigned int Mask[FDVT_IRQ_TYPE_AMOUNT];
};


struct FDVT_INFO_STRUCT {
	spinlock_t SpinLockFDVTRef;
	spinlock_t SpinLockFDVT;
	spinlock_t SpinLockIrq[FDVT_IRQ_TYPE_AMOUNT];
	wait_queue_head_t WaitQueueHead;
	struct work_struct ScheduleFdvtWork;
	unsigned int UserCount;	/* User Count */
	unsigned int DebugMask;	/* Debug Mask */
	signed int IrqNum;
	struct FDVT_IRQ_INFO_STRUCT IrqInfo;
	signed int WriteReqIdx;
	signed int ReadReqIdx;
	pid_t ProcessID[_SUPPORT_MAX_FDVT_FRAME_REQUEST_];
};


static struct FDVT_INFO_STRUCT FDVTInfo;

enum eLOG_TYPE {
	/* currently, only used at ipl_buf_ctrl for critical section */
	_LOG_DBG = 0,
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
} *FDVT_PSV_LOG_STR;

static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[FDVT_IRQ_TYPE_AMOUNT];

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
		str_leng = NORMAL_STR_LEN*ERR_PAGE;\
	} else if (logT == _LOG_DBG) {\
		str_leng = NORMAL_STR_LEN*DBG_PAGE;\
	} else if (logT == _LOG_INF) {\
		str_leng = NORMAL_STR_LEN*INF_PAGE;\
	} else {\
		str_leng = 0;\
	} \
	ptr = pDes = (char *)\
		&(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]);\
	sprintf((char *)(pDes), fmt, ##__VA_ARGS__);   \
	if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) {\
		log_err("log str over flow(%d)", irq);\
	} \
	while (*ptr++ != '\0') {        \
		(*ptr2)++;\
	}     \
} while (0)
#else
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...)  \
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

/* FDVT unmapped base address macro for GCE to access */
#define FDVT_START_HW                       (FDVT_BASE_HW)
#define FDVT_ENABLE_HW                      (FDVT_BASE_HW + 0x04)
#define FDVT_RS_HW                          (FDVT_BASE_HW + 0x08)
#define FDVT_RSCON_BASE_ADR_HW              (FDVT_BASE_HW + 0x0C)
#define FDVT_YUV2RGB_HW                     (FDVT_BASE_HW + 0x10)
#define FDVT_FD_PACK_MODE_HW                (FDVT_BASE_HW + 0x14)
#define FDVT_CONV0_HW                       (FDVT_BASE_HW + 0x18)
#define FDVT_CONV1_HW                       (FDVT_BASE_HW + 0x1C)
#define FDVT_CONV_WD_HT_HW                  (FDVT_BASE_HW + 0x20)
#define FDVT_SRC_WD_HT_HW                   (FDVT_BASE_HW + 0x24)
#define FDVT_DES_WD_HT_HW                   (FDVT_BASE_HW + 0x28)
#define FDVT_YUV2RGBCON_BASE_ADR_HW         (FDVT_BASE_HW + 0x2c)
#define FDVT_KERNEL_HW                      (FDVT_BASE_HW + 0x30)
#define FDVT_IN_SIZE_0_HW                   (FDVT_BASE_HW + 0x34)
#define FDVT_IN_STRIDE_0_HW                 (FDVT_BASE_HW + 0x38)
#define FDVT_IN_SIZE_1_HW                   (FDVT_BASE_HW + 0x3c)
#define FDVT_IN_STRIDE_1_HW                 (FDVT_BASE_HW + 0x40)
#define FDVT_IN_SIZE_2_HW                   (FDVT_BASE_HW + 0x44)
#define FDVT_IN_STRIDE_2_HW                 (FDVT_BASE_HW + 0x48)
#define FDVT_IN_SIZE_3_HW                   (FDVT_BASE_HW + 0x4C)
#define FDVT_IN_STRIDE_3_HW                 (FDVT_BASE_HW + 0x50)
#define FDVT_OUT_SIZE_0_HW                  (FDVT_BASE_HW + 0x54)
#define FDVT_OUT_STRIDE_0_HW                (FDVT_BASE_HW + 0x58)
#define FDVT_OUT_SIZE_1_HW                  (FDVT_BASE_HW + 0x5C)
#define FDVT_OUT_STRIDE_1_HW                (FDVT_BASE_HW + 0x60)
#define FDVT_OUT_SIZE_2_HW                  (FDVT_BASE_HW + 0x64)
#define FDVT_OUT_STRIDE_2_HW                (FDVT_BASE_HW + 0x68)
#define FDVT_OUT_SIZE_3_HW                  (FDVT_BASE_HW + 0x6C)
#define FDVT_OUT_STRIDE_3_HW                (FDVT_BASE_HW + 0x70)
#define FDVT_KERNEL_SIZE_HW                 (FDVT_BASE_HW + 0x74)
#define FDVT_KERNEL_STRIDE_HW               (FDVT_BASE_HW + 0x78)
#define FDVT_IN_BASE_ADR_0_HW               (FDVT_BASE_HW + 0x7C)
#define FDVT_IN_BASE_ADR_1_HW               (FDVT_BASE_HW + 0x80)
#define FDVT_IN_BASE_ADR_2_HW               (FDVT_BASE_HW + 0x84)
#define FDVT_IN_BASE_ADR_3_HW               (FDVT_BASE_HW + 0x88)
#define FDVT_OUT_BASE_ADR_0_HW              (FDVT_BASE_HW + 0x8C)
#define FDVT_OUT_BASE_ADR_1_HW              (FDVT_BASE_HW + 0x90)
#define FDVT_OUT_BASE_ADR_2_HW              (FDVT_BASE_HW + 0x94)
#define FDVT_OUT_BASE_ADR_3_HW              (FDVT_BASE_HW + 0x98)
#define FDVT_KERNEL_BASE_ADR_0_HW           (FDVT_BASE_HW + 0x9C)
#define FDVT_KERNEL_BASE_ADR_1_HW           (FDVT_BASE_HW + 0xA0)
#define FDVT_FD_HW                          (FDVT_BASE_HW + 0xA4)
#define FDVT_FD_CON_BASE_ADR_HW             (FDVT_BASE_HW + 0xA8)
#define FDVT_FD_RLT_BASE_ADR_HW             (FDVT_BASE_HW + 0xB0)
#define FDVT_RPN_HW                         (FDVT_BASE_HW + 0xB4)
#define FDVT_FD_ANCHOR0_INFO0_HW            (FDVT_BASE_HW + 0xB8)
#define FDVT_FD_ANCHOR1_INFO0_HW            (FDVT_BASE_HW + 0xC4)
#define FDVT_FD_ANCHOR2_INFO0_HW            (FDVT_BASE_HW + 0xD0)
#define FDVT_FD_ANCHOR3_INFO0_HW            (FDVT_BASE_HW + 0xDC)
#define FDVT_FD_ANCHOR4_INFO0_HW            (FDVT_BASE_HW + 0xE8)
#define FDVT_YUV_SRC_WD_HT_HW               (FDVT_BASE_HW + 0xF4)
#define FDVT_INT_EN_HW                      (FDVT_BASE_HW + 0x15C)
#define FDVT_INT_HW                         (FDVT_BASE_HW + 0x168)
#define FDVT_DEBUG_INFO_1_HW                (FDVT_BASE_HW + 0x16C)
#define FDVT_DEBUG_INFO_2_HW                (FDVT_BASE_HW + 0x170)
#define FDVT_DEBUG_INFO_3_HW                (FDVT_BASE_HW + 0x174)
#define FDVT_RESULT_HW                      (FDVT_BASE_HW + 0x178)
#define FDVT_RESULT_1_HW                    (FDVT_BASE_HW + 0x17C)
#define FDVT_DMA_CTL_HW                     (FDVT_BASE_HW + 0x180)
#define FDVT_RPN_PACK_MODE_HW               (FDVT_BASE_HW + 0x184)
#define FDVT_RSCON1_HW                      (FDVT_BASE_HW + 0x188)
#define FDVT_RSCON2_HW                      (FDVT_BASE_HW + 0x18C)
#define FDVT_CONV2_HW                       (FDVT_BASE_HW + 0x190)
#define FDVT_RPN_IMAGE_COORD_HW             (FDVT_BASE_HW + 0x194)
#define FDVT_SPARE_CELL_HW                  (FDVT_BASE_HW + 0x198)
#define FDVT_CTRL_HW                        (FDVT_BASE_HW + 0x19C)
#define FDVT_VERSION_HW                     (FDVT_BASE_HW + 0x1A0)
#define FDVT_ANCHOR_SHIFT0_HW               (FDVT_BASE_HW + 0x1A4)
#define FDVT_LANDMARK_SHIFT0_HW             (FDVT_BASE_HW + 0x1A8)
#define FDVT_ANCHOR_SHIFT1_HW               (FDVT_BASE_HW + 0x1AC)
#define FDVT_LANDMARK_SHIFT1_HW             (FDVT_BASE_HW + 0x1B0)
#define DMA_SOFT_RSTSTAT_HW                 (FDVT_BASE_HW + 0x200)
#define TDRI_BASE_ADDR_HW                   (FDVT_BASE_HW + 0x204)
#define TDRI_OFST_ADDR_HW                   (FDVT_BASE_HW + 0x208)
#define TDRI_XSIZE_HW                       (FDVT_BASE_HW + 0x20C)
#define VERTICAL_FLIP_EN_HW                 (FDVT_BASE_HW + 0x210)
#define DMA_SOFT_RESET_HW                   (FDVT_BASE_HW + 0x214)
#define LAST_ULTRA_EN_HW                    (FDVT_BASE_HW + 0x218)
#define SPECIAL_FUN_EN_HW                   (FDVT_BASE_HW + 0x21C)
#define FDVT_WRA_0_BASE_ADDR_HW             (FDVT_BASE_HW + 0x230)
#define FDVT_WRA_0_OFST_ADDR_HW             (FDVT_BASE_HW + 0x238)
#define FDVT_WRA_0_XSIZE_HW                 (FDVT_BASE_HW + 0x240)
#define FDVT_WRA_0_YSIZE_HW                 (FDVT_BASE_HW + 0x244)
#define FDVT_WRA_0_STRIDE_HW                (FDVT_BASE_HW + 0x248)
#define FDVT_WRA_0_CON_HW                   (FDVT_BASE_HW + 0x24C)
#define FDVT_WRA_0_CON2_HW                  (FDVT_BASE_HW + 0x250)
#define FDVT_WRA_0_CON3_HW                  (FDVT_BASE_HW + 0x254)
#define FDVT_WRA_0_CROP_HW                  (FDVT_BASE_HW + 0x258)
#define FDVT_WRA_1_BASE_ADDR_HW             (FDVT_BASE_HW + 0x260)
#define FDVT_WRA_1_OFST_ADDR_HW             (FDVT_BASE_HW + 0x268)
#define FDVT_WRA_1_XSIZE_HW                 (FDVT_BASE_HW + 0x270)
#define FDVT_WRA_1_YSIZE_HW                 (FDVT_BASE_HW + 0x274)
#define FDVT_WRA_1_STRIDE_HW                (FDVT_BASE_HW + 0x278)
#define FDVT_WRA_1_CON_HW                   (FDVT_BASE_HW + 0x27C)
#define FDVT_WRA_1_CON2_HW                  (FDVT_BASE_HW + 0x280)
#define FDVT_WRA_1_CON3_HW                  (FDVT_BASE_HW + 0x284)
#define FDVT_WRA_1_CROP_HW                  (FDVT_BASE_HW + 0x288)
#define FDVT_RDA_0_BASE_ADDR_HW             (FDVT_BASE_HW + 0x290)
#define FDVT_RDA_0_OFST_ADDR_HW             (FDVT_BASE_HW + 0x298)
#define FDVT_RDA_0_XSIZE_HW                 (FDVT_BASE_HW + 0x2A0)
#define FDVT_RDA_0_YSIZE_HW                 (FDVT_BASE_HW + 0x2A4)
#define FDVT_RDA_0_STRIDE_HW                (FDVT_BASE_HW + 0x2A8)
#define FDVT_RDA_0_CON_HW                   (FDVT_BASE_HW + 0x2AC)
#define FDVT_RDA_0_CON2_HW                  (FDVT_BASE_HW + 0x2B0)
#define FDVT_RDA_0_CON3_HW                  (FDVT_BASE_HW + 0x2B4)
#define FDVT_RDA_1_BASE_ADDR_HW             (FDVT_BASE_HW + 0x2C0)
#define FDVT_RDA_1_OFST_ADDR_HW             (FDVT_BASE_HW + 0x2C8)
#define FDVT_RDA_1_XSIZE_HW                 (FDVT_BASE_HW + 0x2D0)
#define FDVT_RDA_1_YSIZE_HW                 (FDVT_BASE_HW + 0x2D4)
#define FDVT_RDA_1_STRIDE_HW                (FDVT_BASE_HW + 0x2D8)
#define FDVT_RDA_1_CON_HW                   (FDVT_BASE_HW + 0x2DC)
#define FDVT_RDA_1_CON2_HW                  (FDVT_BASE_HW + 0x2E0)
#define FDVT_RDA_1_CON3_HW                  (FDVT_BASE_HW + 0x2E4)
#define FDVT_WRB_0_BASE_ADDR_HW             (FDVT_BASE_HW + 0x2F0)
#define FDVT_WRB_0_OFST_ADDR_HW             (FDVT_BASE_HW + 0x2F8)
#define FDVT_WRB_0_XSIZE_HW                 (FDVT_BASE_HW + 0x300)
#define FDVT_WRB_0_YSIZE_HW                 (FDVT_BASE_HW + 0x304)
#define FDVT_WRB_0_STRIDE_HW                (FDVT_BASE_HW + 0x308)
#define FDVT_WRB_0_CON_HW                   (FDVT_BASE_HW + 0x30C)
#define FDVT_WRB_0_CON2_HW                  (FDVT_BASE_HW + 0x310)
#define FDVT_WRB_0_CON3_HW                  (FDVT_BASE_HW + 0x314)
#define FDVT_WRB_0_CROP_HW                  (FDVT_BASE_HW + 0x318)
#define FDVT_WRB_1_BASE_ADDR_HW             (FDVT_BASE_HW + 0x320)
#define FDVT_WRB_1_OFST_ADDR_HW             (FDVT_BASE_HW + 0x328)
#define FDVT_WRB_1_XSIZE_HW                 (FDVT_BASE_HW + 0x330)
#define FDVT_WRB_1_YSIZE_HW                 (FDVT_BASE_HW + 0x334)
#define FDVT_WRB_1_STRIDE_HW                (FDVT_BASE_HW + 0x338)
#define FDVT_WRB_1_CON_HW                   (FDVT_BASE_HW + 0x33C)
#define FDVT_WRB_1_CON2_HW                  (FDVT_BASE_HW + 0x340)
#define FDVT_WRB_1_CON3_HW                  (FDVT_BASE_HW + 0x344)
#define FDVT_WRB_1_CROP_HW                  (FDVT_BASE_HW + 0x348)
#define FDVT_RDB_0_BASE_ADDR_HW             (FDVT_BASE_HW + 0x350)
#define FDVT_RDB_0_OFST_ADDR_HW             (FDVT_BASE_HW + 0x358)
#define FDVT_RDB_0_XSIZE_HW                 (FDVT_BASE_HW + 0x360)
#define FDVT_RDB_0_YSIZE_HW                 (FDVT_BASE_HW + 0x364)
#define FDVT_RDB_0_STRIDE_HW                (FDVT_BASE_HW + 0x368)
#define FDVT_RDB_0_CON_HW                   (FDVT_BASE_HW + 0x36C)
#define FDVT_RDB_0_CON2_HW                  (FDVT_BASE_HW + 0x370)
#define FDVT_RDB_0_CON3_HW                  (FDVT_BASE_HW + 0x374)
#define FDVT_RDB_1_BASE_ADDR_HW             (FDVT_BASE_HW + 0x380)
#define FDVT_RDB_1_OFST_ADDR_HW             (FDVT_BASE_HW + 0x388)
#define FDVT_RDB_1_XSIZE_HW                 (FDVT_BASE_HW + 0x390)
#define FDVT_RDB_1_YSIZE_HW                 (FDVT_BASE_HW + 0x394)
#define FDVT_RDB_1_STRIDE_HW                (FDVT_BASE_HW + 0x398)
#define FDVT_RDB_1_CON_HW                   (FDVT_BASE_HW + 0x39c)
#define FDVT_RDB_1_CON2_HW                  (FDVT_BASE_HW + 0x3A0)
#define FDVT_RDB_1_CON3_HW                  (FDVT_BASE_HW + 0x3A4)
#define DMA_ERR_CTRL_HW                     (FDVT_BASE_HW + 0x3B0)
#define FDVT_WRA_0_ERR_STAT_HW              (FDVT_BASE_HW + 0x3B4)
#define FDVT_WRA_1_ERR_STAT_HW              (FDVT_BASE_HW + 0x3B8)
#define FDVT_WRB_0_ERR_STAT_HW              (FDVT_BASE_HW + 0x3BC)
#define FDVT_WRB_1_ERR_STAT_HW              (FDVT_BASE_HW + 0x3C0)
#define FDVT_RDA_0_ERR_STAT_HW              (FDVT_BASE_HW + 0x3C4)
#define FDVT_RDA_1_ERR_STAT_HW              (FDVT_BASE_HW + 0x3C8)
#define FDVT_RDB_0_ERR_STAT_HW              (FDVT_BASE_HW + 0x3CC)
#define FDVT_RDB_1_ERR_STAT_HW              (FDVT_BASE_HW + 0x3D0)
#define DMA_DEBUG_ADDR_HW                   (FDVT_BASE_HW + 0x3E0)
#define DMA_RSV1_HW                         (FDVT_BASE_HW + 0x3E4)
#define DMA_RSV2_HW                         (FDVT_BASE_HW + 0x3E8)
#define DMA_RSV3_HW                         (FDVT_BASE_HW + 0x3EC)
#define DMA_RSV4_HW                         (FDVT_BASE_HW + 0x3F0)
#define DMA_DEBUG_SEL_HW                    (FDVT_BASE_HW + 0x3F4)
#define DMA_BW_SELF_TEST_HW                 (FDVT_BASE_HW + 0x3F8)


/*SW Access Registers : using mapped base address from DTS*/
#define FDVT_START_REG                       (ISP_FDVT_BASE)
#define FDVT_ENABLE_REG                      (ISP_FDVT_BASE + 0x04)
#define FDVT_RS_REG                          (ISP_FDVT_BASE + 0x08)
#define FDVT_RSCON_BASE_ADR_REG              (ISP_FDVT_BASE + 0x0C)
#define FDVT_YUV2RGB_REG                     (ISP_FDVT_BASE + 0x10)
#define FDVT_FD_PACK_MODE_REG                (ISP_FDVT_BASE + 0x14)
#define FDVT_CONV0_REG                       (ISP_FDVT_BASE + 0x18)
#define FDVT_CONV1_REG                       (ISP_FDVT_BASE + 0x1C)
#define FDVT_CONV_WD_HT_REG                  (ISP_FDVT_BASE + 0x20)
#define FDVT_SRC_WD_HT_REG                   (ISP_FDVT_BASE + 0x24)
#define FDVT_DES_WD_HT_REG                   (ISP_FDVT_BASE + 0x28)
#define FDVT_YUV2RGBCON_BASE_ADR_REG         (ISP_FDVT_BASE + 0x2c)
#define FDVT_KERNEL_REG                      (ISP_FDVT_BASE + 0x30)
#define FDVT_IN_SIZE_0_REG                   (ISP_FDVT_BASE + 0x34)
#define FDVT_IN_STRIDE_0_REG                 (ISP_FDVT_BASE + 0x38)
#define FDVT_IN_SIZE_1_REG                   (ISP_FDVT_BASE + 0x3c)
#define FDVT_IN_STRIDE_1_REG                 (ISP_FDVT_BASE + 0x40)
#define FDVT_IN_SIZE_2_REG                   (ISP_FDVT_BASE + 0x44)
#define FDVT_IN_STRIDE_2_REG                 (ISP_FDVT_BASE + 0x48)
#define FDVT_IN_SIZE_3_REG                   (ISP_FDVT_BASE + 0x4C)
#define FDVT_IN_STRIDE_3_REG                 (ISP_FDVT_BASE + 0x50)
#define FDVT_OUT_SIZE_0_REG                  (ISP_FDVT_BASE + 0x54)
#define FDVT_OUT_STRIDE_0_REG                (ISP_FDVT_BASE + 0x58)
#define FDVT_OUT_SIZE_1_REG                  (ISP_FDVT_BASE + 0x5C)
#define FDVT_OUT_STRIDE_1_REG                (ISP_FDVT_BASE + 0x60)
#define FDVT_OUT_SIZE_2_REG                  (ISP_FDVT_BASE + 0x64)
#define FDVT_OUT_STRIDE_2_REG                (ISP_FDVT_BASE + 0x68)
#define FDVT_OUT_SIZE_3_REG                  (ISP_FDVT_BASE + 0x6C)
#define FDVT_OUT_STRIDE_3_REG                (ISP_FDVT_BASE + 0x70)
#define FDVT_KERNEL_SIZE_REG                 (ISP_FDVT_BASE + 0x74)
#define FDVT_KERNEL_STRIDE_REG               (ISP_FDVT_BASE + 0x78)
#define FDVT_IN_BASE_ADR_0_REG               (ISP_FDVT_BASE + 0x7C)
#define FDVT_IN_BASE_ADR_1_REG               (ISP_FDVT_BASE + 0x80)
#define FDVT_IN_BASE_ADR_2_REG               (ISP_FDVT_BASE + 0x84)
#define FDVT_IN_BASE_ADR_3_REG               (ISP_FDVT_BASE + 0x88)
#define FDVT_OUT_BASE_ADR_0_REG              (ISP_FDVT_BASE + 0x8C)
#define FDVT_OUT_BASE_ADR_1_REG              (ISP_FDVT_BASE + 0x90)
#define FDVT_OUT_BASE_ADR_2_REG              (ISP_FDVT_BASE + 0x94)
#define FDVT_OUT_BASE_ADR_3_REG              (ISP_FDVT_BASE + 0x98)
#define FDVT_KERNEL_BASE_ADR_0_REG           (ISP_FDVT_BASE + 0x9C)
#define FDVT_KERNEL_BASE_ADR_1_REG           (ISP_FDVT_BASE + 0xA0)
#define FDVT_FD_REG                          (ISP_FDVT_BASE + 0xA4)
#define FDVT_FD_CON_BASE_ADR_REG             (ISP_FDVT_BASE + 0xA8)
#define FDVT_FD_RLT_BASE_ADR_REG             (ISP_FDVT_BASE + 0xB0)
#define FDVT_RPN_REG                         (ISP_FDVT_BASE + 0xB4)
#define FDVT_FD_ANCHOR0_INFO0_REG            (ISP_FDVT_BASE + 0xB8)
#define FDVT_FD_ANCHOR1_INFO0_REG            (ISP_FDVT_BASE + 0xC4)
#define FDVT_FD_ANCHOR2_INFO0_REG            (ISP_FDVT_BASE + 0xD0)
#define FDVT_FD_ANCHOR3_INFO0_REG            (ISP_FDVT_BASE + 0xDC)
#define FDVT_FD_ANCHOR4_INFO0_REG            (ISP_FDVT_BASE + 0xE8)
#define FDVT_YUV_SRC_WD_HT_REG               (ISP_FDVT_BASE + 0xF4)
#define FDVT_INT_EN_REG                      (ISP_FDVT_BASE + 0x15C)
#define FDVT_INT_REG                         (ISP_FDVT_BASE + 0x168)
#define FDVT_DEBUG_INFO_1_REG                (ISP_FDVT_BASE + 0x16C)
#define FDVT_DEBUG_INFO_2_REG                (ISP_FDVT_BASE + 0x170)
#define FDVT_DEBUG_INFO_3_REG                (ISP_FDVT_BASE + 0x174)
#define FDVT_RESULT_REG                      (ISP_FDVT_BASE + 0x178)
#define FDVT_RESULT_1_REG                    (ISP_FDVT_BASE + 0x17C)
#define FDVT_DMA_CTL_REG                     (ISP_FDVT_BASE + 0x180)
#define FDVT_RPN_PACK_MODE_REG               (ISP_FDVT_BASE + 0x184)
#define FDVT_RSCON1_REG                      (ISP_FDVT_BASE + 0x188)
#define FDVT_RSCON2_REG                      (ISP_FDVT_BASE + 0x18C)
#define FDVT_CONV2_REG                       (ISP_FDVT_BASE + 0x190)
#define FDVT_RPN_IMAGE_COORD_REG             (ISP_FDVT_BASE + 0x194)
#define FDVT_SPARE_CELL_REG                  (ISP_FDVT_BASE + 0x198)
#define FDVT_CTRL_REG                        (ISP_FDVT_BASE + 0x19C)
#define FDVT_VERSION_REG                     (ISP_FDVT_BASE + 0x1A0)
#define FDVT_ANCHOR_SHIFT0_REG               (ISP_FDVT_BASE + 0x1A4)
#define FDVT_LANDMARK_SHIFT0_REG             (ISP_FDVT_BASE + 0x1A8)
#define FDVT_ANCHOR_SHIFT1_REG               (ISP_FDVT_BASE + 0x1AC)
#define FDVT_LANDMARK_SHIFT1_REG             (ISP_FDVT_BASE + 0x1B0)
#define DMA_SOFT_RSTSTAT_REG                 (ISP_FDVT_BASE + 0x200)
#define TDRI_BASE_ADDR_REG                   (ISP_FDVT_BASE + 0x204)
#define TDRI_OFST_ADDR_REG                   (ISP_FDVT_BASE + 0x208)
#define TDRI_XSIZE_REG                       (ISP_FDVT_BASE + 0x20C)
#define VERTICAL_FLIP_EN_REG                 (ISP_FDVT_BASE + 0x210)
#define DMA_SOFT_RESET_REG                   (ISP_FDVT_BASE + 0x214)
#define LAST_ULTRA_EN_REG                    (ISP_FDVT_BASE + 0x218)
#define SPECIAL_FUN_EN_REG                   (ISP_FDVT_BASE + 0x21C)
#define FDVT_WRA_0_BASE_ADDR_REG             (ISP_FDVT_BASE + 0x230)
#define FDVT_WRA_0_OFST_ADDR_REG             (ISP_FDVT_BASE + 0x238)
#define FDVT_WRA_0_XSIZE_REG                 (ISP_FDVT_BASE + 0x240)
#define FDVT_WRA_0_YSIZE_REG                 (ISP_FDVT_BASE + 0x244)
#define FDVT_WRA_0_STRIDE_REG                (ISP_FDVT_BASE + 0x248)
#define FDVT_WRA_0_CON_REG                   (ISP_FDVT_BASE + 0x24C)
#define FDVT_WRA_0_CON2_REG                  (ISP_FDVT_BASE + 0x250)
#define FDVT_WRA_0_CON3_REG                  (ISP_FDVT_BASE + 0x254)
#define FDVT_WRA_0_CROP_REG                  (ISP_FDVT_BASE + 0x258)
#define FDVT_WRA_1_BASE_ADDR_REG             (ISP_FDVT_BASE + 0x260)
#define FDVT_WRA_1_OFST_ADDR_REG             (ISP_FDVT_BASE + 0x268)
#define FDVT_WRA_1_XSIZE_REG                 (ISP_FDVT_BASE + 0x270)
#define FDVT_WRA_1_YSIZE_REG                 (ISP_FDVT_BASE + 0x274)
#define FDVT_WRA_1_STRIDE_REG                (ISP_FDVT_BASE + 0x278)
#define FDVT_WRA_1_CON_REG                   (ISP_FDVT_BASE + 0x27C)
#define FDVT_WRA_1_CON2_REG                  (ISP_FDVT_BASE + 0x280)
#define FDVT_WRA_1_CON3_REG                  (ISP_FDVT_BASE + 0x284)
#define FDVT_WRA_1_CROP_REG                  (ISP_FDVT_BASE + 0x288)
#define FDVT_RDA_0_BASE_ADDR_REG             (ISP_FDVT_BASE + 0x290)
#define FDVT_RDA_0_OFST_ADDR_REG             (ISP_FDVT_BASE + 0x298)
#define FDVT_RDA_0_XSIZE_REG                 (ISP_FDVT_BASE + 0x2A0)
#define FDVT_RDA_0_YSIZE_REG                 (ISP_FDVT_BASE + 0x2A4)
#define FDVT_RDA_0_STRIDE_REG                (ISP_FDVT_BASE + 0x2A8)
#define FDVT_RDA_0_CON_REG                   (ISP_FDVT_BASE + 0x2AC)
#define FDVT_RDA_0_CON2_REG                  (ISP_FDVT_BASE + 0x2B0)
#define FDVT_RDA_0_CON3_REG                  (ISP_FDVT_BASE + 0x2B4)
#define FDVT_RDA_1_BASE_ADDR_REG             (ISP_FDVT_BASE + 0x2C0)
#define FDVT_RDA_1_OFST_ADDR_REG             (ISP_FDVT_BASE + 0x2C8)
#define FDVT_RDA_1_XSIZE_REG                 (ISP_FDVT_BASE + 0x2D0)
#define FDVT_RDA_1_YSIZE_REG                 (ISP_FDVT_BASE + 0x2D4)
#define FDVT_RDA_1_STRIDE_REG                (ISP_FDVT_BASE + 0x2D8)
#define FDVT_RDA_1_CON_REG                   (ISP_FDVT_BASE + 0x2DC)
#define FDVT_RDA_1_CON2_REG                  (ISP_FDVT_BASE + 0x2E0)
#define FDVT_RDA_1_CON3_REG                  (ISP_FDVT_BASE + 0x2E4)
#define FDVT_WRB_0_BASE_ADDR_REG             (ISP_FDVT_BASE + 0x2F0)
#define FDVT_WRB_0_OFST_ADDR_REG             (ISP_FDVT_BASE + 0x2F8)
#define FDVT_WRB_0_XSIZE_REG                 (ISP_FDVT_BASE + 0x300)
#define FDVT_WRB_0_YSIZE_REG                 (ISP_FDVT_BASE + 0x304)
#define FDVT_WRB_0_STRIDE_REG                (ISP_FDVT_BASE + 0x308)
#define FDVT_WRB_0_CON_REG                   (ISP_FDVT_BASE + 0x30C)
#define FDVT_WRB_0_CON2_REG                  (ISP_FDVT_BASE + 0x310)
#define FDVT_WRB_0_CON3_REG                  (ISP_FDVT_BASE + 0x314)
#define FDVT_WRB_0_CROP_REG                  (ISP_FDVT_BASE + 0x318)
#define FDVT_WRB_1_BASE_ADDR_REG             (ISP_FDVT_BASE + 0x320)
#define FDVT_WRB_1_OFST_ADDR_REG             (ISP_FDVT_BASE + 0x328)
#define FDVT_WRB_1_XSIZE_REG                 (ISP_FDVT_BASE + 0x330)
#define FDVT_WRB_1_YSIZE_REG                 (ISP_FDVT_BASE + 0x334)
#define FDVT_WRB_1_STRIDE_REG                (ISP_FDVT_BASE + 0x338)
#define FDVT_WRB_1_CON_REG                   (ISP_FDVT_BASE + 0x33C)
#define FDVT_WRB_1_CON2_REG                  (ISP_FDVT_BASE + 0x340)
#define FDVT_WRB_1_CON3_REG                  (ISP_FDVT_BASE + 0x344)
#define FDVT_WRB_1_CROP_REG                  (ISP_FDVT_BASE + 0x348)
#define FDVT_RDB_0_BASE_ADDR_REG             (ISP_FDVT_BASE + 0x350)
#define FDVT_RDB_0_OFST_ADDR_REG             (ISP_FDVT_BASE + 0x358)
#define FDVT_RDB_0_XSIZE_REG                 (ISP_FDVT_BASE + 0x360)
#define FDVT_RDB_0_YSIZE_REG                 (ISP_FDVT_BASE + 0x364)
#define FDVT_RDB_0_STRIDE_REG                (ISP_FDVT_BASE + 0x368)
#define FDVT_RDB_0_CON_REG                   (ISP_FDVT_BASE + 0x36C)
#define FDVT_RDB_0_CON2_REG                  (ISP_FDVT_BASE + 0x370)
#define FDVT_RDB_0_CON3_REG                  (ISP_FDVT_BASE + 0x374)
#define FDVT_RDB_1_BASE_ADDR_REG             (ISP_FDVT_BASE + 0x380)
#define FDVT_RDB_1_OFST_ADDR_REG             (ISP_FDVT_BASE + 0x388)
#define FDVT_RDB_1_XSIZE_REG                 (ISP_FDVT_BASE + 0x390)
#define FDVT_RDB_1_YSIZE_REG                 (ISP_FDVT_BASE + 0x394)
#define FDVT_RDB_1_STRIDE_REG                (ISP_FDVT_BASE + 0x398)
#define FDVT_RDB_1_CON_REG                   (ISP_FDVT_BASE + 0x39c)
#define FDVT_RDB_1_CON2_REG                  (ISP_FDVT_BASE + 0x3A0)
#define FDVT_RDB_1_CON3_REG                  (ISP_FDVT_BASE + 0x3A4)
#define DMA_ERR_CTRL_REG                     (ISP_FDVT_BASE + 0x3B0)
#define FDVT_WRA_0_ERR_STAT_REG              (ISP_FDVT_BASE + 0x3B4)
#define FDVT_WRA_1_ERR_STAT_REG              (ISP_FDVT_BASE + 0x3B8)
#define FDVT_WRB_0_ERR_STAT_REG              (ISP_FDVT_BASE + 0x3BC)
#define FDVT_WRB_1_ERR_STAT_REG              (ISP_FDVT_BASE + 0x3C0)
#define FDVT_RDA_0_ERR_STAT_REG              (ISP_FDVT_BASE + 0x3C4)
#define FDVT_RDA_1_ERR_STAT_REG              (ISP_FDVT_BASE + 0x3C8)
#define FDVT_RDB_0_ERR_STAT_REG              (ISP_FDVT_BASE + 0x3CC)
#define FDVT_RDB_1_ERR_STAT_REG              (ISP_FDVT_BASE + 0x3D0)
#define DMA_DEBUG_ADDR_REG                   (ISP_FDVT_BASE + 0x3E0)
#define DMA_RSV1_REG                         (ISP_FDVT_BASE + 0x3E4)
#define DMA_RSV2_REG                         (ISP_FDVT_BASE + 0x3E8)
#define DMA_RSV3_REG                         (ISP_FDVT_BASE + 0x3EC)
#define DMA_RSV4_REG                         (ISP_FDVT_BASE + 0x3F0)
#define DMA_DEBUG_SEL_REG                    (ISP_FDVT_BASE + 0x3F4)
#define DMA_BW_SELF_TEST_REG                 (ISP_FDVT_BASE + 0x3F8)

/*****************************************************************************
 *
 *****************************************************************************/
static inline unsigned int FDVT_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline unsigned int FDVT_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline unsigned int FDVT_GetIRQState(unsigned int type,
					    unsigned int userNumber,
					    unsigned int stus,
					    enum FDVT_PROCESS_ID_ENUM whichReq,
					    int ProcessID)
{
	unsigned int ret = 0;
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;

	/*  */
	spin_lock_irqsave(&(FDVTInfo.SpinLockIrq[type]), flags);
#ifdef FDVT_USE_GCE

#ifdef FDVT_MULTIPROCESS_TIMEING_ISSUE
	if (stus & FDVT_INT_ST) {
		ret = ((FDVTInfo.IrqInfo.FdvtIrqCnt > 0)
		       && (FDVTInfo.ProcessID[FDVTInfo.ReadReqIdx] ==
		       ProcessID));
	} else {
		log_err(
		" WaitIRQ StatusErr, type:%d, userNum:%d, status:%d, whichReq:%d,ProcessID:0x%x, ReadReqIdx:%d\n",
		type, userNumber, stus,
		whichReq, ProcessID,
		FDVTInfo.ReadReqIdx);
	}

#else
	if (stus & FDVT_INT_ST) {
		ret = ((FDVTInfo.IrqInfo.FdvtIrqCnt > 0)
		       && (FDVTInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
	} else {
		log_err(
		"WaitIRQ Status Error, type:%d, userNumber:%d, status:%d, whichReq:%d, ProcessID:0x%x\n",
		type, userNumber, stus, whichReq, ProcessID);
	}
#endif
#else
	ret = ((FDVTInfo.IrqInfo.Status[type] & stus)
	       && (FDVTInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
#endif
	spin_unlock_irqrestore(&(FDVTInfo.SpinLockIrq[type]), flags);
	/*  */
	return ret;
}


/*****************************************************************************
 *
 *****************************************************************************/
static inline unsigned int FDVT_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}

#define RegDump(start, end) {\
	unsigned int i;\
	for (i = start; i <= end; i += 0x10) {\
		log_dbg(\
		"[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]",\
		(unsigned int)(ISP_FDVT_BASE + i),\
		(unsigned int)FDVT_RD32(ISP_FDVT_BASE + i),\
		(unsigned int)(ISP_FDVT_BASE + i+0x4),\
		(unsigned int)FDVT_RD32(ISP_FDVT_BASE + i+0x4),\
		(unsigned int)(ISP_FDVT_BASE + i+0x8),\
		(unsigned int)FDVT_RD32(ISP_FDVT_BASE + i+0x8),\
		(unsigned int)(ISP_FDVT_BASE + i+0xc),\
		(unsigned int)FDVT_RD32(ISP_FDVT_BASE + i+0xc));\
	} \
}

static bool ConfigFDVTRequest(signed int ReqIdx)
{
#ifdef FDVT_USE_GCE
	unsigned int j;
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;


	spin_lock_irqsave(&(FDVTInfo
			  .SpinLockIrq
			  [FDVT_IRQ_TYPE_INT_FDVT_ST]),
			  flags);
	if (g_FDVT_ReqRing.FDVTReq_Struct[ReqIdx].State ==
	    FDVT_REQUEST_STATE_PENDING) {
		g_FDVT_ReqRing.FDVTReq_Struct[ReqIdx].State =
		FDVT_REQUEST_STATE_RUNNING;
		for (j = 0; j < _SUPPORT_MAX_FDVT_FRAME_REQUEST_; j++) {
			if (FDVT_FRAME_STATUS_ENQUE ==
			    g_FDVT_ReqRing
			    .FDVTReq_Struct
			    [ReqIdx]
			    .FdvtFrameStatus[j]) {
				g_FDVT_ReqRing
				.FDVTReq_Struct
				[ReqIdx]
				.FdvtFrameStatus[j] =
				    FDVT_FRAME_STATUS_RUNNING;
				spin_unlock_irqrestore(&
						(FDVTInfo
						.SpinLockIrq
						[FDVT_IRQ_TYPE_INT_FDVT_ST]),
						flags);
				ConfigFDVTHW(&g_FDVT_ReqRing
					     .FDVTReq_Struct[ReqIdx]
					     .FdvtFrameConfig[j]);
				spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
						  [FDVT_IRQ_TYPE_INT_FDVT_ST]),
						  flags);
			}
		}
	} else {
		log_err(
		"FDVT state machine error!!, ReqIdx:%d, State:%d\n",
		ReqIdx, g_FDVT_ReqRing.FDVTReq_Struct[ReqIdx].State);
	}
	spin_unlock_irqrestore(&(FDVTInfo.SpinLockIrq
			       [FDVT_IRQ_TYPE_INT_FDVT_ST]),
			       flags);


	return MTRUE;
#else
	log_err("[%s] don't support this mode.!!\n", __func__);
	return MFALSE;
#endif
}


static bool ConfigFDVT(void)
{
#ifdef FDVT_USE_GCE

	unsigned int i, j, k;
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;


	spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
			  [FDVT_IRQ_TYPE_INT_FDVT_ST]),
			  flags);
	for (k = 0;
	     k < _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
	     k++) {
		i = (g_FDVT_ReqRing.HWProcessIdx + k) %
		    _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
		if (g_FDVT_ReqRing.FDVTReq_Struct[i].State ==
		    FDVT_REQUEST_STATE_PENDING) {
			g_FDVT_ReqRing.FDVTReq_Struct[i].State =
			    FDVT_REQUEST_STATE_RUNNING;
			for (j = 0; j < _SUPPORT_MAX_FDVT_FRAME_REQUEST_; j++) {
				if (FDVT_FRAME_STATUS_ENQUE ==
				    g_FDVT_ReqRing
				    .FDVTReq_Struct[i]
				    .FdvtFrameStatus[j]) {
					/* break; */
					g_FDVT_ReqRing
					.FDVTReq_Struct[i]
					.FdvtFrameStatus[j] =
					    FDVT_FRAME_STATUS_RUNNING;
					spin_unlock_irqrestore(&
						(FDVTInfo
						.SpinLockIrq
						[FDVT_IRQ_TYPE_INT_FDVT_ST]),
						flags);
					ConfigFDVTHW(&g_FDVT_ReqRing
						     .FDVTReq_Struct[i]
						     .FdvtFrameConfig[j]);
					spin_lock_irqsave(&
						  (FDVTInfo
						  .SpinLockIrq
						  [FDVT_IRQ_TYPE_INT_FDVT_ST]),
						  flags);
				}
			}
			/* log_dbg("ConfigFDVT idx j:%d\n",j); */
			if (j != _SUPPORT_MAX_FDVT_FRAME_REQUEST_) {
				log_err(
				"FDVT Config State is wrong!  idx j(%d), HWProcessIdx(%d), State(%d)\n",
				j, g_FDVT_ReqRing.HWProcessIdx,
				g_FDVT_ReqRing.FDVTReq_Struct[i].State);
				/* g_FDVT_ReqRing.FDVTReq_Struct[i].
				 * FdvtFrameStatus[j] =
				 * FDVT_FRAME_STATUS_RUNNING;
				 * spin_unlock_irqrestore(&
				 * (FDVTInfo.
				 * SpinLockIrq[FDVT_IRQ_TYPE_INT_FDVT_ST]),
				 *  flags);
				 * ConfigFDVTHW(&g_FDVT_ReqRing.
				 * FDVTReq_Struct[i].FdvtFrameConfig[j]);
				 * return MTRUE;
				 */
				return MFALSE;
			}
			/* else {
			 * g_FDVT_ReqRing.FDVTReq_Struct[i].State =
			 * FDVT_REQUEST_STATE_RUNNING;
			 * log_err(
			 * "FDVT Config State is wrong!
			 * HWProcessIdx(%d), State(%d)\n",
			 * g_FDVT_ReqRing.HWProcessIdx,
			 * g_FDVT_ReqRing.FDVTReq_Struct[i].State);
			 * g_FDVT_ReqRing.HWProcessIdx =
			 * (g_FDVT_ReqRing.HWProcessIdx+1)
			 * %_SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
			 * }
			 */
		}
	}
	spin_unlock_irqrestore(&(FDVTInfo.SpinLockIrq
			       [FDVT_IRQ_TYPE_INT_FDVT_ST]),
			       flags);
	if (k == _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_)
		log_dbg("No any FDVT Request in Ring!!\n");

	return MTRUE;


#else				/* #ifdef FDVT_USE_GCE */

	unsigned int i, j, k;
	unsigned int flags;

	for (k = 0; k < _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_; k++) {
		i = (g_FDVT_ReqRing.HWProcessIdx + k) %
		_SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
		if (g_FDVT_ReqRing.FDVTReq_Struct[i].State ==
		    FDVT_REQUEST_STATE_PENDING) {
			for (j = 0; j < _SUPPORT_MAX_FDVT_FRAME_REQUEST_; j++) {
				if (FDVT_FRAME_STATUS_ENQUE ==
				    g_FDVT_ReqRing
				    .FDVTReq_Struct[i]
				    .FdvtFrameStatus[j]) {
					break;
				}
			}
			log_dbg(
			"%s idx j:%d\n",
			__func__, j);
			if (j != _SUPPORT_MAX_FDVT_FRAME_REQUEST_) {
				g_FDVT_ReqRing
				.FDVTReq_Struct[i]
				.FdvtFrameStatus[j] =
				    FDVT_FRAME_STATUS_RUNNING;
				ConfigFDVTHW(&g_FDVT_ReqRing.FDVTReq_Struct[i]
					     .FdvtFrameConfig[j]);
				return MTRUE;
			}
			/*else {*/
			log_err(
			"FDVT Config State is wrong! HWProcessIdx(%d), State(%d)\n",
			g_FDVT_ReqRing.HWProcessIdx,
			g_FDVT_ReqRing.FDVTReq_Struct[i].State);
			g_FDVT_ReqRing.HWProcessIdx =
			    (g_FDVT_ReqRing.HWProcessIdx +
			     1) % _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
			/*}*/
		}
	}
	if (k == _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_)
		log_dbg("No any FDVT Request in Ring!!\n");

	return MFALSE;

#endif				/* #ifdef FDVT_USE_GCE */



}


static bool UpdateFDVT(pid_t *ProcessID)
{
#ifdef FDVT_USE_GCE
	unsigned int i, j, next_idx;
	bool bFinishRequest = MFALSE;

	for (i = g_FDVT_ReqRing.HWProcessIdx;
	     i < _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
	     i++) {
		if (g_FDVT_ReqRing.FDVTReq_Struct[i].State ==
		    FDVT_REQUEST_STATE_RUNNING) {
			for (j = 0;
			     j < _SUPPORT_MAX_FDVT_FRAME_REQUEST_;
			     j++) {
				if (FDVT_FRAME_STATUS_RUNNING ==
				    g_FDVT_ReqRing
				    .FDVTReq_Struct[i]
				    .FdvtFrameStatus[j]) {
					break;
				}
			}
			IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST,
				       m_CurrentPPB,
				       _LOG_DBG,
				       "%s idx j:%d\n",
				       __func__, j);
			if (j != _SUPPORT_MAX_FDVT_FRAME_REQUEST_) {
				next_idx = j + 1;
				g_FDVT_ReqRing
				.FDVTReq_Struct[i]
				.FdvtFrameStatus[j] =
				    FDVT_FRAME_STATUS_FINISHED;
				if ((_SUPPORT_MAX_FDVT_FRAME_REQUEST_ ==
				    (next_idx))
				    || ((_SUPPORT_MAX_FDVT_FRAME_REQUEST_ >
				    (next_idx))
				    && (FDVT_FRAME_STATUS_EMPTY ==
				    g_FDVT_ReqRing.FDVTReq_Struct[i]
				    .FdvtFrameStatus[next_idx]))) {
					bFinishRequest = MTRUE;
					(*ProcessID) =
					    g_FDVT_ReqRing
					    .FDVTReq_Struct[i].processID;
					g_FDVT_ReqRing
					.FDVTReq_Struct[i].State =
					    FDVT_REQUEST_STATE_FINISHED;
					g_FDVT_ReqRing
					.HWProcessIdx =
					(g_FDVT_ReqRing.HWProcessIdx +
					1) %
					_SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
					IRQ_LOG_KEEPER(
						FDVT_IRQ_TYPE_INT_FDVT_ST,
						m_CurrentPPB,
						_LOG_INF,
						"Finish FDVT Request i:%d, j:%d, HWProcessIdx:%d\n",
						i, j,
						g_FDVT_ReqRing
						.HWProcessIdx);
				} else {
					IRQ_LOG_KEEPER(
						FDVT_IRQ_TYPE_INT_FDVT_ST,
						m_CurrentPPB,
						_LOG_DBG,
						"Finish FDVT Frame i:%d, j:%d, HWProcessIdx:%d\n",
						i, j,
						g_FDVT_ReqRing
						.HWProcessIdx);
				}
				break;
			}
			/*else {*/
			IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST,
				       m_CurrentPPB,
				       _LOG_ERR,
				       "FDVT State Machine is wrong! HWProcessIdx(%d), State(%d)\n",
				       g_FDVT_ReqRing.HWProcessIdx,
				       g_FDVT_ReqRing.FDVTReq_Struct[i].State);
			g_FDVT_ReqRing.FDVTReq_Struct[i].State =
			    FDVT_REQUEST_STATE_FINISHED;
			g_FDVT_ReqRing.HWProcessIdx =
			    (g_FDVT_ReqRing.HWProcessIdx +
			     1) % _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
			break;
			/*}*/
		}
	}

	return bFinishRequest;


#else /* #ifdef FDVT_USE_GCE */
	unsigned int i, j, next_idx;
	bool bFinishRequest = MFALSE;

	for (i = g_FDVT_ReqRing.HWProcessIdx;
	     i < _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
	     i++) {
		if (g_FDVT_ReqRing.FDVTReq_Struct[i].State ==
		    FDVT_REQUEST_STATE_PENDING) {
			for (j = 0; j < _SUPPORT_MAX_FDVT_FRAME_REQUEST_; j++) {
				if (FDVT_FRAME_STATUS_RUNNING ==
				    g_FDVT_ReqRing
				    .FDVTReq_Struct[i]
				    .FdvtFrameStatus[j]) {
					break;
				}
			}
			IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST,
				       m_CurrentPPB,
				       _LOG_DBG,
				       "%s idx j:%d\n",
				       __func__, j);
			if (j != _SUPPORT_MAX_FDVT_FRAME_REQUEST_) {
				next_idx = j + 1;
				g_FDVT_ReqRing.FDVTReq_Struct[i]
				.FdvtFrameStatus[j] =
				    FDVT_FRAME_STATUS_FINISHED;
				if ((_SUPPORT_MAX_FDVT_FRAME_REQUEST_ ==
				    (next_idx))
				    || ((_SUPPORT_MAX_FDVT_FRAME_REQUEST_ >
				    (next_idx))
					&& (FDVT_FRAME_STATUS_EMPTY ==
					    g_FDVT_ReqRing
					    .FDVTReq_Struct[i]
					    .FdvtFrameStatus[next_idx]))) {
					bFinishRequest = MTRUE;
					(*ProcessID) =
					    g_FDVT_ReqRing
					    .FDVTReq_Struct[i]
					    .processID;
					g_FDVT_ReqRing
					.FDVTReq_Struct[i]
					.State =
					    FDVT_REQUEST_STATE_FINISHED;
					g_FDVT_ReqRing
					.HWProcessIdx =
					(g_FDVT_ReqRing.HWProcessIdx +
					1) %
					_SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
					IRQ_LOG_KEEPER(
						FDVT_IRQ_TYPE_INT_FDVT_ST,
						m_CurrentPPB,
						_LOG_INF,
						"Finish FDVT Request i:%d, j:%d, HWProcessIdx:%d\n",
						i, j,
						g_FDVT_ReqRing
						.HWProcessIdx);
				} else {
					IRQ_LOG_KEEPER(
						FDVT_IRQ_TYPE_INT_FDVT_ST,
						m_CurrentPPB,
						_LOG_DBG,
						"Finish FDVT Frame i:%d, j:%d, HWProcessIdx:%d\n",
						i, j,
						g_FDVT_ReqRing
						.HWProcessIdx);
				}
				break;
			}
			/*else {*/
			IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST,
				       m_CurrentPPB,
				       _LOG_ERR,
				       "FDVT State Machine is wrong! HWProcessIdx(%d), State(%d)\n",
				       g_FDVT_ReqRing.HWProcessIdx,
				       g_FDVT_ReqRing.FDVTReq_Struct[i].State);
			g_FDVT_ReqRing.FDVTReq_Struct[i].State =
			    FDVT_REQUEST_STATE_FINISHED;
			g_FDVT_ReqRing.HWProcessIdx =
			    (g_FDVT_ReqRing.HWProcessIdx +
			     1) % _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
			break;
			/*}*/
		}
	}

	return bFinishRequest;

#endif				/* #ifdef FDVT_USE_GCE */


}

static signed int ConfigFDVTHW(FDVT_Config *pFdvtConfig)
#if !BYPASS_REG
{
#ifdef FDVT_USE_GCE
		struct cmdqRecStruct *handle;
		uint64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_FDVT);
#endif
	if (FDVT_DBG_DBGLOG == (FDVT_DBG_DBGLOG & FDVTInfo.DebugMask)) {

		log_dbg("ConfigFDVTHW Start!\n");
		log_dbg("FDVT_YUV2RGB:0x%x!\n",
			(unsigned int)pFdvtConfig->FDVT_YUV2RGB);
		log_dbg("FDVT_YUV_SRC_WD_HT:0x%x!\n",
			(unsigned int)pFdvtConfig->FDVT_YUV_SRC_WD_HT);
		log_dbg("FDVT_RSCON_BASE_ADR:0x%x!\n",
			(unsigned int)pFdvtConfig->FDVT_RSCON_BASE_ADR);
		log_dbg("FDVT_FD_CON_BASE_ADR:0x%x!\n",
			(unsigned int)pFdvtConfig->FDVT_FD_CON_BASE_ADR);
		log_dbg("FDVT_YUV2RGBCON_BASE_ADR:0x%x!\n",
			(unsigned int)pFdvtConfig->FDVT_YUV2RGBCON_BASE_ADR);
		log_dbg("FD_MODE:0x%x!\n",
			(unsigned int)pFdvtConfig->FD_MODE);

	}

#ifdef FDVT_USE_GCE

#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("ConfigFDVTHW");
#endif

	cmdqRecCreate(CMDQ_SCENARIO_KERNEL_CONFIG_GENERAL, &handle);
	/* CMDQ driver dispatches CMDQ HW thread
	 * and HW thread's priority according to scenario
	 */

	cmdqRecSetEngine(handle, engineFlag);

	cmdqRecReset(handle);

#if 1
	/* Use command queue to write register */
	/* BIT0 for INT_EN */
	cmdqRecWrite(handle, FDVT_ENABLE_HW, 0x00000111, CMDQ_REG_MASK);
	if (pFdvtConfig->FD_MODE == 0) {
		cmdqRecWrite(handle, FDVT_RS_HW, 0x00000409, CMDQ_REG_MASK);
		cmdqRecWrite(handle, FDVT_FD_HW, 0x04000042, CMDQ_REG_MASK);
	} else if (pFdvtConfig->FD_MODE == 1) {
		cmdqRecWrite(handle, FDVT_RS_HW, 0x00000403, CMDQ_REG_MASK);
		cmdqRecWrite(handle, FDVT_FD_HW, 0x04000012, CMDQ_REG_MASK);
	} else if (pFdvtConfig->FD_MODE == 2) {
		cmdqRecWrite(handle, FDVT_RS_HW, 0x00000403, CMDQ_REG_MASK);
		cmdqRecWrite(handle, FDVT_FD_HW, 0x04000012, CMDQ_REG_MASK);
	}
	cmdqRecWrite(handle, FDVT_YUV2RGB_HW,
		     pFdvtConfig->FDVT_YUV2RGB, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_YUV_SRC_WD_HT_HW,
		     pFdvtConfig->FDVT_YUV_SRC_WD_HT, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_INT_EN_HW, 0x1, CMDQ_REG_MASK);

	cmdqRecWrite(handle, FDVT_RSCON_BASE_ADR_HW,
		     pFdvtConfig->FDVT_RSCON_BASE_ADR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_FD_CON_BASE_ADR_HW,
		     pFdvtConfig->FDVT_FD_CON_BASE_ADR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_YUV2RGBCON_BASE_ADR_HW,
		     pFdvtConfig->FDVT_YUV2RGBCON_BASE_ADR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, FDVT_FD_RLT_BASE_ADR_HW, 0x0, CMDQ_REG_MASK);

	cmdqRecWrite(handle, FDVT_START_HW, 0x1, CMDQ_REG_MASK);
	cmdqRecWait(handle, CMDQ_EVENT_FDVT_DONE);
	cmdqRecWrite(handle, FDVT_START_HW, 0x0, CMDQ_REG_MASK);
#endif
	/* non-blocking API, Please  use cmdqRecFlushAsync() */
	cmdqRecFlushAsync(handle);
	/* if you want to re-use the handle, please reset the handle */
	cmdqRecReset(handle);
	cmdqRecDestroy(handle); /* recycle the memory */
	FDVT_DumpReg(); // ADD by gasper
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif

#else

#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("ConfigFDVTHW");
#endif
#if 0
	/* FDVT Interrupt enabled in read-clear mode */
	FDVT_WR32(FDVT_INT_EN_REG, 0x1);

	FDVT_WR32(FDVT_ENABLE_REG, 0x00000111);
	FDVT_WR32(FDVT_RS_REG, 0x00000409);

	FDVT_WR32(FDVT_YUV2RGB_REG, pFdvtConfig->FDVT_YUV2RGB);
	FDVT_WR32(FDVT_FD_REG, 0x04000042);
	FDVT_WR32(FDVT_YUV_SRC_WD_HT_REG, pFdvtConfig->FDVT_YUV_SRC_WD_HT);
	FDVT_WR32(FDVT_RSCON_BASE_ADR_REG, pFdvtConfig->FDVT_RSCON_BASE_ADR);
	FDVT_WR32(FDVT_FD_CON_BASE_ADR_REG, pFdvtConfig->FDVT_FD_CON_BASE_ADR);
	FDVT_WR32(FDVT_YUV2RGBCON_BASE_ADR_REG,
		  pFdvtConfig->FDVT_YUV2RGBCON_BASE_ADR);
	FDVT_WR32(FDVT_FD_RLT_BASE_ADR_REG, NULL);

	FDVT_WR32(FDVT_START_REG, 0x1);	/* FDVT Interrupt read-clear mode */
#endif
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
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

#ifndef FDVT_USE_GCE

static bool Check_FDVT_Is_Busy(void)
#if !BYPASS_REG
{
	unsigned int Ctrl _Fsm;
	unsigned int Fdvt_Start;

	Ctrl_Fsm = FDVT_RD32(FDVT_INT_EN_REG);
	Fdvt_Start = FDVT_RD32(FDVT_START_REG);
	if ((FDVT_IS_BUSY == (Ctrl_Fsm & FDVT_IS_BUSY)) ||
	    (FDVT_START_MASK == (Fdvt_Start & FDVT_START_MASK)))
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
static signed int FDVT_DumpReg(void)
{
	signed int Ret = 0;
	//unsigned int i, j;

	log_inf("- E.");

	log_inf("FDVT Config Info\n");

	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_START_HW),
		(unsigned int)FDVT_RD32(FDVT_START_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_ENABLE_HW),
		(unsigned int)FDVT_RD32(FDVT_ENABLE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_RS_HW),
		(unsigned int)FDVT_RD32(FDVT_RS_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_INT_EN_HW),
		(unsigned int)FDVT_RD32(FDVT_INT_EN_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_YUV2RGB_HW),
		(unsigned int)FDVT_RD32(FDVT_YUV2RGB_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_FD_HW),
		(unsigned int)FDVT_RD32(FDVT_FD_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_YUV_SRC_WD_HT_HW),
		(unsigned int)FDVT_RD32(FDVT_YUV_SRC_WD_HT_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_RSCON_BASE_ADR_HW),
		(unsigned int)FDVT_RD32(FDVT_RSCON_BASE_ADR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_FD_CON_BASE_ADR_HW),
		(unsigned int)FDVT_RD32(FDVT_FD_CON_BASE_ADR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_YUV2RGBCON_BASE_ADR_HW),
		(unsigned int)FDVT_RD32(FDVT_YUV2RGBCON_BASE_ADR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(FDVT_FD_RLT_BASE_ADR_HW),
		(unsigned int)FDVT_RD32(FDVT_FD_RLT_BASE_ADR_REG));
#if 0
	log_inf("FDVT:HWProcessIdx:%d, WriteIdx:%d, ReadIdx:%d\n",
		g_FDVT_ReqRing.HWProcessIdx,
		g_FDVT_ReqRing.WriteIdx,
		g_FDVT_ReqRing.ReadIdx);

	for (i = 0; i < _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_; i++) {
		log_inf(
		"FDVT Req:State:%d, procID:0x%08X, callerID:0x%08X, enqueReqNum:%d, FrameWRIdx:%d, RrameRDIdx:%d\n",
		g_FDVT_ReqRing.FDVTReq_Struct[i].State,
		g_FDVT_ReqRing.FDVTReq_Struct[i].processID,
		g_FDVT_ReqRing.FDVTReq_Struct[i].callerID,
		g_FDVT_ReqRing.FDVTReq_Struct[i].enqueReqNum,
		g_FDVT_ReqRing.FDVTReq_Struct[i].FrameWRIdx,
		g_FDVT_ReqRing.FDVTReq_Struct[i].RrameRDIdx);

		for (j = 0; j < _SUPPORT_MAX_FDVT_FRAME_REQUEST_;) {
			log_inf(
			"FDVT:FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d\n",
			j,
			g_FDVT_ReqRing
			.FDVTReq_Struct[i]
			.FdvtFrameStatus[j],
			j + 1,
			g_FDVT_ReqRing
			.FDVTReq_Struct[i]
			.FdvtFrameStatus[j + 1],
			j + 2,
			g_FDVT_ReqRing
			.FDVTReq_Struct[i]
			.FdvtFrameStatus[j + 2],
			j + 3,
			g_FDVT_ReqRing
			.FDVTReq_Struct[i]
			.FdvtFrameStatus[j + 3]);
			j = j + 4;
		}

	}

#endif

	log_inf("- X.\n");
	/*  */
	return Ret;
}

#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
static inline void FDVT_Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order:
	 * CG_SCP_SYS_MM0-> CG_MM_SMI_COMMON ->
	 * CG_SCP_SYS_ISP -> FDVT clk
	 */
#ifndef SMI_CLK
	ret = clk_prepare_enable(fdvt_clk.CG_SCP_SYS_MM0);
	if (ret)
		log_err("cannot prepare and enable CG_SCP_SYS_MM0 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_2X);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_2X clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_GALS_M0_2X);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_GALS_M0_2X clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_GALS_M1_2X);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_GALS_M1_2X clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_UPSZ0);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_UPSZ0 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_UPSZ1);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_UPSZ1 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_FIFO0);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_FIFO0 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_SMI_COMMON_FIFO1);
	if (ret)
		log_err("cannot prepare and enable CG_MM_SMI_COMMON_FIFO1 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_MM_LARB5);
	if (ret)
		log_err("cannot prepare and enable CG_MM_LARB5 clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_SCP_SYS_ISP);
	if (ret)
		log_err("cannot prepare and enable CG_SCP_SYS_ISP clock\n");

	ret = clk_prepare_enable(fdvt_clk.CG_IMGSYS_LARB);
	if (ret)
		log_err("cannot prepare and enable CG_IMGSYS_LARB clock\n");
#else
	//smi_bus_enable(SMI_LARB_IMGSYS1, "camera_fdvt"); //modified by Gasper
	// tony
	//smi_bus_prepare_enable(SMI_LARB5_REG_INDX, "camera-fdvt", true);
#endif

	ret = clk_prepare_enable(fdvt_clk.CG_IMGSYS_FDVT);
	if (ret)
		log_err("cannot prepare and enable CG_IMGSYS_FDVT clock\n");

}

static inline void FDVT_Disable_Unprepare_ccf_clock(void)
{
	/* must keep this clk close order:
	 * FDVT clk -> CG_SCP_SYS_ISP ->
	 * CG_MM_SMI_COMMON -> CG_SCP_SYS_MM0
	 */
	clk_disable_unprepare(fdvt_clk.CG_IMGSYS_FDVT);
#ifndef SMI_CLK
	clk_disable_unprepare(fdvt_clk.CG_IMGSYS_LARB);
	clk_disable_unprepare(fdvt_clk.CG_SCP_SYS_ISP);
	clk_disable_unprepare(fdvt_clk.CG_MM_LARB5);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_FIFO1);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_FIFO0);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_UPSZ1);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_UPSZ0);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_GALS_M1_2X);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_GALS_M0_2X);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON_2X);
	clk_disable_unprepare(fdvt_clk.CG_MM_SMI_COMMON);
	clk_disable_unprepare(fdvt_clk.CG_SCP_SYS_MM0);
#else
	//smi_bus_disable(SMI_LARB_IMGSYS1, "camera_fdvt"); // marked by gasper
	// tony
	//smi_bus_disable_unprepare(SMI_LARB5_REG_INDX, "camera-fdvt", true);
#endif
}
#endif

/*****************************************************************************
 *
 *****************************************************************************/
static void FDVT_EnableClock(bool En)
{
#if defined(EP_NO_CLKMGR)
	unsigned int setReg;
#endif

	if (En) { /* Enable clock. */
		/* log_dbg("Dpe clock enbled. g_u4EnableClockCount: %d.",
		 * g_u4EnableClockCount);
		 */
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			FDVT_Prepare_Enable_ccf_clock();
#else
			/* Enable clock by hardcode:
			 * 1. CAMSYS_CG_CLR (0x1A000008) = 0xffffffff;
			 * 2. IMG_CG_CLR (0x15000008) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			FDVT_WR32(IMGSYS_REG_CG_CLR, setReg);

#endif
#else
			enable_clock(MT_CG_DMFB0_SMI_COMMON, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			enable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			enable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			enable_clock(MT_CG_IMAGE_FD, "CAMERA");
			enable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
#endif /* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
			break;
		default:
			break;
		}
		spin_lock(&(FDVTInfo.SpinLockFDVT));
		g_u4EnableClockCount++;
		spin_unlock(&(FDVTInfo.SpinLockFDVT));
	} else { /* Disable clock. */

		/* log_dbg("Dpe clock disabled. g_u4EnableClockCount: %d.",
		 * g_u4EnableClockCount);
		 */
		spin_lock(&(FDVTInfo.SpinLockFDVT));
		g_u4EnableClockCount--;
		spin_unlock(&(FDVTInfo.SpinLockFDVT));
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			FDVT_Disable_Unprepare_ccf_clock();
#else
			/* Disable clock by hardcode:
			 *  1. CAMSYS_CG_SET (0x1A000004) = 0xffffffff;
			 *  2. IMG_CG_SET (0x15000004) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			FDVT_WR32(IMGSYS_REG_CG_SET, setReg);

#endif
#else
			/* do disable clock */
			disable_clock(MT_CG_IMAGE_CAM_SMI, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_TG, "CAMERA");
			disable_clock(MT_CG_IMAGE_SEN_CAM, "CAMERA");
			disable_clock(MT_CG_IMAGE_CAM_SV, "CAMERA");
			disable_clock(MT_CG_IMAGE_FD, "CAMERA");
			disable_clock(MT_CG_IMAGE_LARB2_SMI, "CAMERA");
			disable_clock(MT_CG_DMFB0_SMI_COMMON, "CAMERA");
#endif /* #if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) */
			break;
		default:
			break;
		}
	}
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline void FDVT_Reset(void)
{
	log_dbg("- E.");

	log_dbg(" FDVT Reset start!\n");
	spin_lock(&(FDVTInfo.SpinLockFDVTRef));

	if (FDVTInfo.UserCount > 1) {
		spin_unlock(&(FDVTInfo.SpinLockFDVTRef));
		log_dbg("Curr UserCount(%d) users exist", FDVTInfo.UserCount);
	} else {
		spin_unlock(&(FDVTInfo.SpinLockFDVTRef));

		/* Reset FDVT flow */
		FDVT_WR32(FDVT_START_REG,
			 (FDVT_RD32(FDVT_START_REG) |
			 0x20000));
		while ((FDVT_RD32(FDVT_START_REG) && 0x20000 != 0x0))
			log_dbg("FDVT resetting...\n");
		FDVT_WR32(FDVT_START_REG, 0x10000);
		FDVT_WR32(FDVT_START_REG, 0x0);
		log_dbg(" FDVT Reset end!\n");
	}

}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_ReadReg(FDVT_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;
	/*  */
	FDVT_REG_STRUCT reg;
	/* unsigned int* pData = (unsigned int*)pRegIo->Data; */
	FDVT_REG_STRUCT *pData = (FDVT_REG_STRUCT *) pRegIo->pData;

	for (i = 0; i < pRegIo->Count; i++) {
		if (get_user(reg.Addr, (unsigned int *) &pData->Addr) != 0) {
			log_err("get_user failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		/* pData++; */
		/*  */
		if ((ISP_FDVT_BASE + reg.Addr >= ISP_FDVT_BASE)
		    && (reg.Addr < FDVT_REG_RANGE)
			&& ((reg.Addr & 0x3) == 0)) {
			reg.Val = FDVT_RD32(ISP_FDVT_BASE + reg.Addr);
		} else {
			log_err("Wrong address(0x%p), FDVT_BASE(0x%p), Addr(0x%lx)",
				(ISP_FDVT_BASE + reg.Addr),
				ISP_FDVT_BASE,
				(unsigned long)reg.Addr);
			reg.Val = 0;
		}
		/*  */

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


/*****************************************************************************
 *
 *****************************************************************************/
/* Can write sensor's test model only,
 * if need write to other modules, need modify current code flow
 */
static signed int FDVT_WriteRegToHw(FDVT_REG_STRUCT *pReg, unsigned int Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;

	/* Use local variable to store FDVTInfo.DebugMask &
	 * FDVT_DBG_WRITE_REG for saving lock time
	 */
	spin_lock(&(FDVTInfo.SpinLockFDVT));
	dbgWriteReg = FDVTInfo.DebugMask & FDVT_DBG_WRITE_REG;
	spin_unlock(&(FDVTInfo.SpinLockFDVT));

	/*  */
	if (dbgWriteReg)
		log_dbg("- E.\n");

	/*  */
	for (i = 0; i < Count; i++) {
		if (dbgWriteReg) {
			log_dbg("Addr(0x%lx), Val(0x%x)\n",
				(unsigned long)(ISP_FDVT_BASE + pReg[i].Addr),
				(unsigned int) (pReg[i].Val));
		}

		if ((pReg[i].Addr < FDVT_REG_RANGE) &&
		    ((pReg[i].Addr & 0x3) == 0)) {
			FDVT_WR32(ISP_FDVT_BASE + pReg[i].Addr, pReg[i].Val);
		} else {
			log_err("wrong address(0x%p), FDVT_BASE(0x%p), Addr(0x%lx)\n",
				(ISP_FDVT_BASE + pReg[i].Addr),
				ISP_FDVT_BASE,
				(unsigned long)pReg[i].Addr);
		}
	}

	/*  */
	return Ret;
}



/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_WriteReg(FDVT_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	/* unsigned char* pData = NULL; */
	FDVT_REG_STRUCT *pData = NULL;
	/* */
	if (FDVTInfo.DebugMask & FDVT_DBG_WRITE_REG)
		log_dbg(
		"Data(0x%p), Count(%d)\n",
		(pRegIo->pData),
		(pRegIo->Count));

	if ((pRegIo->pData == NULL) || (pRegIo->Count == 0) ||
	    (pRegIo->Count > (FDVT_REG_RANGE>>2))) {
		log_err(
		"ERROR: pRegIo->pData is NULL or Count:%d\n", pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	/* pData = (unsigned char*)kmalloc(
	 * (pRegIo->Count)*sizeof(FDVT_REG_STRUCT), GFP_ATOMIC);
	 */
	pData = kmalloc((pRegIo->Count) * sizeof(FDVT_REG_STRUCT), GFP_KERNEL);
	/* if (pData == NULL) {
	 * log_err(
	 * "ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)\n",
	 * current->comm,
	 * current->pid,
	 * current->tgid);
	 * Ret = -ENOMEM;
	 * goto EXIT;
	 * }
	 */
	if (copy_from_user
		(pData, (void __user *)(pRegIo->pData),
		pRegIo->Count * sizeof(FDVT_REG_STRUCT)) != 0) {
		log_err("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	Ret = FDVT_WriteRegToHw(pData, pRegIo->Count);
	/*  */
EXIT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return Ret;
}



/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_WaitIrq(FDVT_WAIT_IRQ_STRUCT *WaitIrq)
{

	signed int Ret = 0;
	signed int Timeout = WaitIrq->Timeout;
	enum FDVT_PROCESS_ID_ENUM whichReq = FDVT_PROCESS_ID_NONE;

	/*unsigned int i;*/
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;
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
	if (FDVTInfo.DebugMask & FDVT_DBG_INT) {
		if (WaitIrq->Status & FDVTInfo.IrqInfo.Mask[WaitIrq->Type]) {
			if (WaitIrq->UserKey > 0) {
				log_dbg("+WaitIrq clr(%d), Type(%d), Stat(0x%08X), Timeout(%d),usr(%d), ProcID(%d)\n",
				WaitIrq->Clear, WaitIrq->Type,
				WaitIrq->Status, WaitIrq->Timeout,
				WaitIrq->UserKey, WaitIrq->ProcessID);
			}
		}
	}


	/* 1. wait type update */
	if (WaitIrq->Clear == FDVT_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
				  [WaitIrq->Type]),
				  flags);
		/* log_dbg("WARNING: Clear(%d), Type(%d):
		 * IrqStatus(0x%08X) has been cleared"
		 * ,WaitIrq->EventInfo.Clear,WaitIrq->Type,
		 * FDVTInfo.IrqInfo.Status[WaitIrq->Type]);
		 * FDVTInfo.IrqInfo.Status[WaitIrq->Type]
		 * [WaitIrq->EventInfo.UserKey] &=
		 * (~WaitIrq->EventInfo.Status);
		 */
		FDVTInfo.IrqInfo.Status[WaitIrq->Type] &=
		(~WaitIrq->Status);
		spin_unlock_irqrestore(&(FDVTInfo.SpinLockIrq
				       [WaitIrq->Type]),
				       flags);
		return Ret;
	}

	if (WaitIrq->Clear == FDVT_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
				  [WaitIrq->Type]),
				  flags);
		if (FDVTInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			FDVTInfo.IrqInfo.Status[WaitIrq->Type] &=
			(~WaitIrq->Status);

		spin_unlock_irqrestore(&(FDVTInfo.SpinLockIrq
				       [WaitIrq->Type]),
				       flags);
	} else if (WaitIrq->Clear == FDVT_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
				  [WaitIrq->Type]),
				  flags);

		FDVTInfo.IrqInfo.Status[WaitIrq->Type] = 0;
		spin_unlock_irqrestore(&(FDVTInfo.SpinLockIrq
				       [WaitIrq->Type]),
				       flags);
	}
	/* FDVT_IRQ_WAIT_CLEAR ==> do nothing */


	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(FDVTInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = FDVTInfo.IrqInfo.Status[WaitIrq->Type];
	spin_unlock_irqrestore(&(FDVTInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (WaitIrq->Status & FDVT_INT_ST) {
		whichReq = FDVT_PROCESS_ID_FDVT;
	} else {
		log_err("No Such Stats can be waited!! irq Type/User/Sts/Pid(0x%x/%d/0x%x/%d)\n",
			WaitIrq->Type, WaitIrq->UserKey,
			WaitIrq->Status, WaitIrq->ProcessID);
	}


#ifdef FDVT_WAITIRQ_LOG
	log_inf(
	"before wait_event:Tout(%d), Clear(%d), Type(%d), IrqStat(0x%08X), WaitStat(0x%08X), usrKey(%d)\n",
	WaitIrq->Timeout, WaitIrq->Clear, WaitIrq->Type,
	irqStatus, WaitIrq->Status, WaitIrq->UserKey);
	log_inf(
	"before wait_event:ProcID(%d), FdvtIrq(0x%08X), WriteReq(0x%08X), ReadReq(0x%08X), whichReq(%d)\n",
	WaitIrq->ProcessID, FDVTInfo.IrqInfo.FdvtIrqCnt,
	FDVTInfo.WriteReqIdx, FDVTInfo.ReadReqIdx, whichReq);
#endif

	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(FDVTInfo.WaitQueueHead,
						   FDVT_GetIRQState
						   (WaitIrq->Type,
						   WaitIrq->UserKey,
						   WaitIrq->Status,
						   whichReq,
						   WaitIrq->ProcessID),
						   FDVT_MsToJiffies
						   (WaitIrq->Timeout));

	/* check if user is interrupted by system signal */
	if ((Timeout != 0) &&
		(!FDVT_GetIRQState(WaitIrq->Type, WaitIrq->UserKey,
				   WaitIrq->Status, whichReq,
				   WaitIrq->ProcessID))) {
		log_dbg("interrupted by system, timeout(%d),irq Type/User/Sts/whichReq/Pid(0x%x/%d/0x%x/%d/%d)\n",
		Timeout, WaitIrq->Type, WaitIrq->UserKey,
		WaitIrq->Status, whichReq, WaitIrq->ProcessID);
		/* actually it should be -ERESTARTSYS */
		Ret = -ERESTARTSYS;
		goto EXIT;
	}
	/* timeout */
	if (Timeout == 0) {
		/* Store irqinfo status in here
		 * to redeuce time of spin_lock_irqsave
		 */
		spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
				  [WaitIrq->Type]),
				  flags);
		irqStatus = FDVTInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(&(FDVTInfo
				       .SpinLockIrq
				       [WaitIrq->Type]),
				       flags);

		log_err("WaitIrq Timeout:Tout(%d) clr(%d) Type(%d) IrqStat(0x%08X) WaitStat(0x%08X) usrKey(%d)\n",
		     WaitIrq->Timeout, WaitIrq->Clear,
		     WaitIrq->Type, irqStatus,
		     WaitIrq->Status, WaitIrq->UserKey);
		log_err("WaitIrq Timeout:whichReq(%d),ProcID(%d) FdvtIrqCnt(0x%08X) WriteReq(0x%08X) ReadReq(0x%08X)\n",
		     whichReq, WaitIrq->ProcessID,
		     FDVTInfo.IrqInfo.FdvtIrqCnt,
		     FDVTInfo.WriteReqIdx, FDVTInfo.ReadReqIdx);

		if (WaitIrq->bDumpReg)
			FDVT_DumpReg();

		Ret = -EFAULT;
		goto EXIT;
	} else {
/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("[FDVT]WaitIrq");
#endif

		spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
				  [WaitIrq->Type]),
				  flags);
		irqStatus = FDVTInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(&(FDVTInfo.SpinLockIrq
				       [WaitIrq->Type]),
				       flags);

		if (WaitIrq->Clear == FDVT_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
					  [WaitIrq->Type]),
					  flags);
#ifdef FDVT_USE_GCE

#ifdef FDVT_MULTIPROCESS_TIMEING_ISSUE
			FDVTInfo.ReadReqIdx =
			    (FDVTInfo.ReadReqIdx + 1) %
			    _SUPPORT_MAX_FDVT_FRAME_REQUEST_;
			/* actually, it doesn't happen the timging issue!! */
			/* wake_up_interruptible(&FDVTInfo.WaitQueueHead); */
#endif
			if (WaitIrq->Status & FDVT_INT_ST) {
				FDVTInfo.IrqInfo.FdvtIrqCnt--;
				if (FDVTInfo.IrqInfo.FdvtIrqCnt == 0)
					FDVTInfo.IrqInfo
					.Status[WaitIrq->Type] &=
					(~WaitIrq->Status);
			} else {
				log_err("FDVT_IRQ_WAIT_CLEAR Error, Type(%d), WaitStatus(0x%08X)",
					WaitIrq->Type, WaitIrq->Status);
			}
#else
			if (FDVTInfo.IrqInfo.Status[WaitIrq->Type] &
			    WaitIrq->Status)
				FDVTInfo.IrqInfo.Status[WaitIrq->Type] &=
				(~WaitIrq->Status);
#endif
			spin_unlock_irqrestore(&(FDVTInfo
					       .SpinLockIrq
					       [WaitIrq->Type]),
					       flags);
		}

#ifdef FDVT_WAITIRQ_LOG
		log_inf(
		"no Timeout:Tout(%d), clr(%d), Type(%d), IrqStat(0x%08X), WaitStat(0x%08X), usrKey(%d)\n",
		WaitIrq->Timeout, WaitIrq->Clear,
		WaitIrq->Type, irqStatus, WaitIrq->Status,
		WaitIrq->UserKey);
		log_inf(
		"no Timeout:ProcID(%d),FdvtIrq(0x%08X), WriteReq(0x%08X), ReadReq(0x%08X),whichReq(%d)\n",
		WaitIrq->ProcessID, FDVTInfo.IrqInfo.FdvtIrqCnt,
		FDVTInfo.WriteReqIdx, FDVTInfo.ReadReqIdx, whichReq);
#endif

#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif

	}


EXIT:


	return Ret;
}


/*****************************************************************************
 *
 *****************************************************************************/
static long FDVT_ioctl(struct file *pFile,
		       unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;

	/*unsigned int pid = 0;*/
	FDVT_REG_IO_STRUCT RegIo;
	FDVT_WAIT_IRQ_STRUCT IrqInfo;
	FDVT_CLEAR_IRQ_STRUCT ClearIrq;
	FDVT_Config fdvt_FdvtConfig;
	FDVT_Request fdvt_FdvtReq;
	signed int FdvtWriteIdx = 0;
	int idx;
	struct FDVT_USER_INFO_STRUCT *pUserInfo;
	int enqueNum;
	int dequeNum;
	/* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned long flags;



	/*  */
	if (pFile->private_data == NULL) {
		log_wrn(
		"private_data is NULL,(process, pid, tgid)=(%s, %d, %d)",
		current->comm,
		current->pid, current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct FDVT_USER_INFO_STRUCT *) (pFile->private_data);
	/*  */
	switch (Cmd) {
	case FDVT_RESET:
		{
			spin_lock(&(FDVTInfo.SpinLockFDVT));
			FDVT_Reset();
			spin_unlock(&(FDVTInfo.SpinLockFDVT));
			break;
		}

		/*  */
	case FDVT_DUMP_REG:
		{
			Ret = FDVT_DumpReg();
			break;
		}
	case FDVT_DUMP_ISR_LOG:
		{
			unsigned int currentPPB = m_CurrentPPB;

			spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
					  [FDVT_IRQ_TYPE_INT_FDVT_ST]),
					  flags);
			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(&(FDVTInfo.SpinLockIrq
					       [FDVT_IRQ_TYPE_INT_FDVT_ST]),
					       flags);

			IRQ_LOG_PRINTER(FDVT_IRQ_TYPE_INT_FDVT_ST,
					currentPPB,
					_LOG_INF);
			IRQ_LOG_PRINTER(FDVT_IRQ_TYPE_INT_FDVT_ST,
					currentPPB,
					_LOG_ERR);
			break;
		}
	case FDVT_READ_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
			    sizeof(FDVT_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir
				 * of copy from user
				 * is implemented in
				 * FDVT_ReadReg(...)
				 */
				Ret = FDVT_ReadReg(&RegIo);
			} else {
				log_err("FDVT_READ_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case FDVT_WRITE_REGISTER:
		{
			if (copy_from_user(&RegIo, (void *)Param,
			    sizeof(FDVT_REG_IO_STRUCT)) == 0) {
				/* 2nd layer behavoir
				 * of copy from user
				 * is implemented in
				 * FDVT_WriteReg(...)
				 */
				Ret = FDVT_WriteReg(&RegIo);
			} else {
				log_err("FDVT_WRITE_REGISTER copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case FDVT_WAIT_IRQ:
		{
			if (copy_from_user(&IrqInfo, (void *)Param,
			    sizeof(FDVT_WAIT_IRQ_STRUCT)) ==
			    0) {
				/*  */
				if ((IrqInfo.Type >= FDVT_IRQ_TYPE_AMOUNT) ||
				    (IrqInfo.Type < 0)) {
					Ret = -EFAULT;
					log_err(
					"invalid type(%d)",
					IrqInfo.Type);
					goto EXIT;
				}

				if ((IrqInfo.UserKey >= IRQ_USER_NUM_MAX) ||
				    (IrqInfo.UserKey < 0)) {
					log_err("invalid userKey(%d), max(%d), force userkey = 0\n",
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
				Ret = FDVT_WaitIrq(&IrqInfo);

				if (copy_to_user
				    ((void *)Param, &IrqInfo,
				    sizeof(FDVT_WAIT_IRQ_STRUCT)) != 0) {
					log_err("copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				log_err("FDVT_WAIT_IRQ copy_from_user failed");
				Ret = -EFAULT;
			}
			break;
		}
	case FDVT_CLEAR_IRQ:
		{
			if (copy_from_user(&ClearIrq,
					   (void *)Param,
					   sizeof(FDVT_CLEAR_IRQ_STRUCT))
			    == 0) {
				log_dbg(
				"FDVT_CLEAR_IRQ Type(%d)",
				ClearIrq.Type);

				if ((ClearIrq.Type >= FDVT_IRQ_TYPE_AMOUNT) ||
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

				log_dbg("FDVT_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)\n",
					ClearIrq.Type, ClearIrq.Status,
					FDVTInfo.IrqInfo.Status[ClearIrq.Type]);
				spin_lock_irqsave(&
						(FDVTInfo
						.SpinLockIrq
						[ClearIrq.Type]),
						flags);
				FDVTInfo.IrqInfo.Status[ClearIrq.Type] &=
				(~ClearIrq.Status);
				spin_unlock_irqrestore(&
						   (FDVTInfo
						   .SpinLockIrq
						   [ClearIrq.Type]),
						   flags);
			} else {
				log_err("FDVT_CLEAR_IRQ copy_from_user failed\n");
				Ret = -EFAULT;
			}
			break;
		}
	case FDVT_ENQNUE_NUM:
		{
			/* enqueNum */
			if (copy_from_user(&enqueNum,
					   (void *)Param,
					   sizeof(int)) == 0) {
				if (FDVT_REQUEST_STATE_EMPTY ==
				    g_FDVT_ReqRing
				    .FDVTReq_Struct
				    [g_FDVT_ReqRing.WriteIdx]
				    .State) {
					spin_lock_irqsave(&
						(FDVTInfo
						.SpinLockIrq
						[FDVT_IRQ_TYPE_INT_FDVT_ST]),
						flags);
					g_FDVT_ReqRing
					.FDVTReq_Struct
					[g_FDVT_ReqRing
					.WriteIdx].processID =
					    pUserInfo->Pid;
					g_FDVT_ReqRing
					.FDVTReq_Struct
					[g_FDVT_ReqRing
					.WriteIdx]
					.enqueReqNum =
					    enqueNum;
					spin_unlock_irqrestore(&
						(FDVTInfo
						.SpinLockIrq
						[FDVT_IRQ_TYPE_INT_FDVT_ST]),
						flags);
					if (enqueNum >
					    _SUPPORT_MAX_FDVT_FRAME_REQUEST_) {
						log_err(
						"FDVT Enque Num is bigger than enqueNum:%d\n",
						enqueNum);
					}
					log_dbg("FDVT_ENQNUE_NUM:%d\n",
						enqueNum);
				} else {
					log_err(
					"WFME Enque request state is not empty:%d, writeIdx:%d, readIdx:%d\n",
					g_FDVT_ReqRing
					.FDVTReq_Struct
					[g_FDVT_ReqRing
					.WriteIdx]
					.State,
					g_FDVT_ReqRing.WriteIdx,
					g_FDVT_ReqRing.ReadIdx);
				}
			} else {
				log_err("FDVT_EQNUE_NUM copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
		/* FDVT_Config */
	case FDVT_ENQUE:
		{
			if (copy_from_user(&fdvt_FdvtConfig,
			    (void *)Param, sizeof(FDVT_Config))
			    == 0) {
		/* log_dbg("FDVT_CLEAR_IRQ:Type(%d),
		 * Status(0x%08X),IrqStatus(0x%08X)",
		 * ClearIrq.Type, ClearIrq.Status,
		 * FDVTInfo.IrqInfo.Status[ClearIrq.Type]);
		 */
				spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
						  [FDVT_IRQ_TYPE_INT_FDVT_ST]),
						  flags);
				if ((FDVT_REQUEST_STATE_EMPTY ==
				     g_FDVT_ReqRing
				     .FDVTReq_Struct
				     [g_FDVT_ReqRing.WriteIdx]
				     .State)
				    && (g_FDVT_ReqRing
					.FDVTReq_Struct
					[g_FDVT_ReqRing.WriteIdx]
					.FrameWRIdx <
					g_FDVT_ReqRing
					.FDVTReq_Struct
					[g_FDVT_ReqRing
					.WriteIdx]
					.enqueReqNum)) {
					g_FDVT_ReqRing
					.FDVTReq_Struct
					[g_FDVT_ReqRing
					.WriteIdx]
					    .FdvtFrameStatus[g_FDVT_ReqRing
							    .FDVTReq_Struct
							    [g_FDVT_ReqRing
							    .WriteIdx]
							    .FrameWRIdx] =
					    FDVT_FRAME_STATUS_ENQUE;
					memcpy(&g_FDVT_ReqRing
					       .FDVTReq_Struct
					       [g_FDVT_ReqRing.WriteIdx]
					       .FdvtFrameConfig
					       [g_FDVT_ReqRing
					       .FDVTReq_Struct[g_FDVT_ReqRing
					       .WriteIdx]
					       .FrameWRIdx++],
					       &fdvt_FdvtConfig,
					       sizeof(FDVT_Config));
					if (g_FDVT_ReqRing
					    .FDVTReq_Struct
					    [g_FDVT_ReqRing.WriteIdx]
					    .FrameWRIdx ==
					    g_FDVT_ReqRing
					    .FDVTReq_Struct
					    [g_FDVT_ReqRing
					    .WriteIdx]
					    .enqueReqNum) {
						g_FDVT_ReqRing
						    .FDVTReq_Struct
						    [g_FDVT_ReqRing.WriteIdx]
						    .State =
						    FDVT_REQUEST_STATE_PENDING;
						g_FDVT_ReqRing.WriteIdx =
						(g_FDVT_ReqRing.WriteIdx +
						 1) %
					_SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
						log_dbg("FDVT enque done!!\n");
					} else {
						log_dbg("FDVT enque frame!!\n");
					}
				} else {
					log_err("No Buffer! WriteIdx(%d), Stat(%d), FrameWRIdx(%d), enqueReqNum(%d)\n",
					     g_FDVT_ReqRing
					     .WriteIdx,
					     g_FDVT_ReqRing
					     .FDVTReq_Struct
					     [g_FDVT_ReqRing.WriteIdx]
					     .State,
					     g_FDVT_ReqRing
					     .FDVTReq_Struct
					     [g_FDVT_ReqRing.WriteIdx]
					     .FrameWRIdx,
					     g_FDVT_ReqRing
					     .FDVTReq_Struct
					     [g_FDVT_ReqRing.WriteIdx]
					     .enqueReqNum);
				}
#ifdef FDVT_USE_GCE
				spin_unlock_irqrestore(&
						(FDVTInfo
						.SpinLockIrq
						[FDVT_IRQ_TYPE_INT_FDVT_ST]),
						flags);
				log_dbg("ConfigFDVT!!\n");
				ConfigFDVT();
#else
				/* check the hw is running or not ? */
				if (Check_FDVT_Is_Busy() == MFALSE) {
					/* config the fdvt hw and run */
					log_dbg("ConfigFDVT\n");
					ConfigFDVT();
				} else {
					log_inf("FDVT HW is busy!!\n");
				}
				spin_unlock_irqrestore(&
						(FDVTInfo
						.SpinLockIrq
						[FDVT_IRQ_TYPE_INT_FDVT_ST]),
						flags);
#endif


			} else {
				log_err("FDVT_ENQUE copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
	case FDVT_ENQUE_REQ:
		{
			if (copy_from_user(&fdvt_FdvtReq,
			    (void *)Param,
			    sizeof(FDVT_Request)) ==
			    0) {
				log_dbg("FDVT_ENQNUE_NUM:%d, pid:%d\n",
					fdvt_FdvtReq.m_ReqNum,
					pUserInfo->Pid);
				if (fdvt_FdvtReq.m_ReqNum >
				    _SUPPORT_MAX_FDVT_FRAME_REQUEST_) {
					log_err("FDVT Enque Num is bigger than enqueNum:%d\n",
						fdvt_FdvtReq.m_ReqNum);
					Ret = -EFAULT;
					goto EXIT;
				}
				if (copy_from_user
				    (g_FdvtEnqueReq_Struct.FdvtFrameConfig,
				     (void *)fdvt_FdvtReq.m_pFdvtConfig,
				     fdvt_FdvtReq.m_ReqNum *
				     sizeof(FDVT_Config)) != 0) {
					log_err("copy FDVTConfig from request is fail!!\n");
					Ret = -EFAULT;
					goto EXIT;
				}

				/* Protect the Multi Process */
				mutex_lock(&gFdvtMutex);

				spin_lock_irqsave(&(FDVTInfo.SpinLockIrq
						  [FDVT_IRQ_TYPE_INT_FDVT_ST]),
						  flags);
				if (FDVT_REQUEST_STATE_EMPTY ==
				    g_FDVT_ReqRing
				    .FDVTReq_Struct
				    [g_FDVT_ReqRing.WriteIdx]
				    .State) {
					g_FDVT_ReqRing
					.FDVTReq_Struct
					[g_FDVT_ReqRing
					.WriteIdx]
					.processID =
					    pUserInfo->Pid;
					g_FDVT_ReqRing
					.FDVTReq_Struct
					[g_FDVT_ReqRing
					.WriteIdx]
					.enqueReqNum =
					    fdvt_FdvtReq.m_ReqNum;

					for (idx = 0;
					     idx < fdvt_FdvtReq.m_ReqNum;
					     idx++) {
						g_FDVT_ReqRing
						.FDVTReq_Struct
						[g_FDVT_ReqRing.WriteIdx]
						.FdvtFrameStatus
						[g_FDVT_ReqRing
						.FDVTReq_Struct
						[g_FDVT_ReqRing.WriteIdx]
						.FrameWRIdx] =
						    FDVT_FRAME_STATUS_ENQUE;
						memcpy(&g_FDVT_ReqRing
						       .FDVTReq_Struct
						       [g_FDVT_ReqRing
						       .WriteIdx]
						       .FdvtFrameConfig
						       [g_FDVT_ReqRing
						       .FDVTReq_Struct
						       [g_FDVT_ReqRing
						       .WriteIdx].FrameWRIdx++],
						       &g_FdvtEnqueReq_Struct
						       .FdvtFrameConfig[idx],
						       sizeof(FDVT_Config));
					}
					g_FDVT_ReqRing
					.FDVTReq_Struct
					[g_FDVT_ReqRing
					.WriteIdx].State =
					    FDVT_REQUEST_STATE_PENDING;
					FdvtWriteIdx = g_FDVT_ReqRing.WriteIdx;
					g_FDVT_ReqRing.WriteIdx =
					(g_FDVT_ReqRing.WriteIdx +
					1) %
					_SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
					log_dbg("FDVT request enque done!!\n");
				} else {
					log_err("Enque req NG: WriteIdx(%d) Stat(%d) FrameWRIdx(%d) enqueReqNum(%d)\n",
					     g_FDVT_ReqRing.WriteIdx,
					     g_FDVT_ReqRing
					     .FDVTReq_Struct
					     [g_FDVT_ReqRing.WriteIdx]
					     .State,
					     g_FDVT_ReqRing
					     .FDVTReq_Struct
					     [g_FDVT_ReqRing.WriteIdx]
					     .FrameWRIdx,
					     g_FDVT_ReqRing
					     .FDVTReq_Struct
					     [g_FDVT_ReqRing.WriteIdx]
					     .enqueReqNum);
				}
				spin_unlock_irqrestore(&
						(FDVTInfo
						.SpinLockIrq
						[FDVT_IRQ_TYPE_INT_FDVT_ST]),
						flags);
				log_dbg("ConfigFDVT Request!!\n");
				ConfigFDVTRequest(FdvtWriteIdx);

				mutex_unlock(&gFdvtMutex);
			} else {
				log_err("FDVT_ENQUE_REQ copy_from_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}
	case FDVT_DEQUE_NUM:
		{
			if (FDVT_REQUEST_STATE_FINISHED ==
			    g_FDVT_ReqRing
			    .FDVTReq_Struct
			    [g_FDVT_ReqRing.ReadIdx]
			    .State) {
				dequeNum =
				    g_FDVT_ReqRing
				    .FDVTReq_Struct
				    [g_FDVT_ReqRing.ReadIdx]
				    .enqueReqNum;
				log_dbg("FDVT_DEQUE_NUM(%d)\n", dequeNum);
			} else {
				dequeNum = 0;
				log_err("DEQUE_NUM:No Buffer: ReadIdx(%d) State(%d) RrameRDIdx(%d) enqueReqNum(%d)\n",
				     g_FDVT_ReqRing.ReadIdx,
				     g_FDVT_ReqRing
				     .FDVTReq_Struct
				     [g_FDVT_ReqRing.ReadIdx]
				     .State,
				     g_FDVT_ReqRing
				     .FDVTReq_Struct
				     [g_FDVT_ReqRing.ReadIdx]
				     .RrameRDIdx,
				     g_FDVT_ReqRing
				     .FDVTReq_Struct
				     [g_FDVT_ReqRing.ReadIdx]
				     .enqueReqNum);
			}
			if (copy_to_user((void *)Param,
			    &dequeNum, sizeof(unsigned int)) != 0) {
				log_err("FDVT_DEQUE_NUM copy_to_user failed\n");
				Ret = -EFAULT;
			}

			break;
		}

	case FDVT_DEQUE:
		{
			spin_lock_irqsave(&
					  (FDVTInfo.SpinLockIrq
					  [FDVT_IRQ_TYPE_INT_FDVT_ST]),
					  flags);
			if ((FDVT_REQUEST_STATE_FINISHED ==
			     g_FDVT_ReqRing
			     .FDVTReq_Struct
			     [g_FDVT_ReqRing.ReadIdx]
			     .State)
			    && (g_FDVT_ReqRing
				.FDVTReq_Struct
				[g_FDVT_ReqRing.ReadIdx]
				.RrameRDIdx <
				g_FDVT_ReqRing
				.FDVTReq_Struct
				[g_FDVT_ReqRing.ReadIdx]
				.enqueReqNum)) {
				/* dequeNum = g_DVE_RequestRing.
				 * DVEReq_Struct[g_DVE_RequestRing.ReadIdx]
				 *.enqueReqNum;
				 */
				if (FDVT_FRAME_STATUS_FINISHED ==
				    g_FDVT_ReqRing.FDVTReq_Struct
				    [g_FDVT_ReqRing.ReadIdx]
				    .FdvtFrameStatus[g_FDVT_ReqRing
						    .FDVTReq_Struct
						    [g_FDVT_ReqRing.ReadIdx]
						    .RrameRDIdx]) {

					memcpy(&fdvt_FdvtConfig,
					       &g_FDVT_ReqRing
					       .FDVTReq_Struct
					       [g_FDVT_ReqRing.ReadIdx]
					       .FdvtFrameConfig
					       [g_FDVT_ReqRing
					       .FDVTReq_Struct
					       [g_FDVT_ReqRing
					       .ReadIdx].RrameRDIdx],
					       sizeof(FDVT_Config));
					g_FDVT_ReqRing
					.FDVTReq_Struct[g_FDVT_ReqRing
						       .ReadIdx]
						       .FdvtFrameStatus
						       [g_FDVT_ReqRing
						       .FDVTReq_Struct
						       [g_FDVT_ReqRing
						       .ReadIdx]
						       .RrameRDIdx++] =
						       FDVT_FRAME_STATUS_EMPTY;
				}
				if (g_FDVT_ReqRing.FDVTReq_Struct
				    [g_FDVT_ReqRing.ReadIdx]
				    .RrameRDIdx ==
				    g_FDVT_ReqRing
				    .FDVTReq_Struct
				    [g_FDVT_ReqRing.ReadIdx]
				    .enqueReqNum) {
					g_FDVT_ReqRing
						.FDVTReq_Struct[g_FDVT_ReqRing
						.ReadIdx].State =
						FDVT_REQUEST_STATE_EMPTY;
					g_FDVT_ReqRing
						.FDVTReq_Struct[g_FDVT_ReqRing
						.ReadIdx].FrameWRIdx = 0;
					g_FDVT_ReqRing
						.FDVTReq_Struct[g_FDVT_ReqRing
						.ReadIdx].RrameRDIdx = 0;
					g_FDVT_ReqRing
						.FDVTReq_Struct[g_FDVT_ReqRing
						.ReadIdx].enqueReqNum = 0;
					g_FDVT_ReqRing.ReadIdx =
					(g_FDVT_ReqRing.ReadIdx +
					 1) %
					 _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
					log_dbg(
					"FDVT ReadIdx(%d)\n",
					g_FDVT_ReqRing.ReadIdx);
				}
				spin_unlock_irqrestore(&
						(FDVTInfo
						.SpinLockIrq
						[FDVT_IRQ_TYPE_INT_FDVT_ST]),
						flags);
				if (copy_to_user
				    ((void *)Param,
				     &g_FDVT_ReqRing
				     .FDVTReq_Struct
				     [g_FDVT_ReqRing.ReadIdx]
				     .FdvtFrameConfig[g_FDVT_ReqRing
						     .FDVTReq_Struct
						     [g_FDVT_ReqRing.ReadIdx]
						     .RrameRDIdx],
				     sizeof(FDVT_Config)) != 0) {
					log_err("FDVT_DEQUE copy_to_user failed\n");
					Ret = -EFAULT;
				}

			} else {
				spin_unlock_irqrestore(&
						(FDVTInfo
						.SpinLockIrq
						[FDVT_IRQ_TYPE_INT_FDVT_ST]),
						flags);
				log_err("FDVT_DEQUE No Buffer: ReadIdx(%d)State(%d) RrameRDIdx(%d), enqueReqNum(%d)\n",
				     g_FDVT_ReqRing.ReadIdx,
				     g_FDVT_ReqRing.FDVTReq_Struct
				     [g_FDVT_ReqRing.ReadIdx].State,
				     g_FDVT_ReqRing
				     .FDVTReq_Struct[g_FDVT_ReqRing.ReadIdx]
				     .RrameRDIdx,
				     g_FDVT_ReqRing
				     .FDVTReq_Struct[g_FDVT_ReqRing.ReadIdx]
				     .enqueReqNum);
			}

			break;
		}
	case FDVT_DEQUE_REQ:
		{
			if (copy_from_user(&fdvt_FdvtReq, (void *)Param,
			    sizeof(FDVT_Request)) == 0) {
				/* Protect the Multi Process */
				mutex_lock(&gFdvtDequeMutex);

				spin_lock_irqsave(&(FDVTInfo
						  .SpinLockIrq
						  [FDVT_IRQ_TYPE_INT_FDVT_ST]),
						  flags);

				if (FDVT_REQUEST_STATE_FINISHED ==
				    g_FDVT_ReqRing
				    .FDVTReq_Struct
				    [g_FDVT_ReqRing.ReadIdx]
				    .State) {
					dequeNum =
					    g_FDVT_ReqRing
					    .FDVTReq_Struct
					    [g_FDVT_ReqRing
					    .ReadIdx]
					    .enqueReqNum;
					log_dbg(
					"FDVT_DEQUE_REQ(%d)\n", dequeNum);
				} else {
					dequeNum = 0;
					log_err("DEQUE_REQ no buf:RIdx(%d) Stat(%d) RrameRDIdx(%d) enqueReqNum(%d)\n",
					     g_FDVT_ReqRing.ReadIdx,
					     g_FDVT_ReqRing
					     .FDVTReq_Struct
					     [g_FDVT_ReqRing.ReadIdx]
					     .State,
					     g_FDVT_ReqRing
					     .FDVTReq_Struct
					     [g_FDVT_ReqRing.ReadIdx]
					     .RrameRDIdx,
					     g_FDVT_ReqRing
					     .FDVTReq_Struct
					     [g_FDVT_ReqRing.ReadIdx]
					     .enqueReqNum);
				}
				fdvt_FdvtReq.m_ReqNum = dequeNum;

				for (idx = 0; idx < dequeNum; idx++) {
					if (FDVT_FRAME_STATUS_FINISHED ==
					    g_FDVT_ReqRing
					    .FDVTReq_Struct
					    [g_FDVT_ReqRing.ReadIdx]
					    .FdvtFrameStatus[g_FDVT_ReqRing
							   .FDVTReq_Struct
							   [g_FDVT_ReqRing
							   .ReadIdx]
							   .RrameRDIdx]) {

						memcpy(&g_FdvtDequeReq_Struct
						       .FdvtFrameConfig[idx],
						       &g_FDVT_ReqRing
						       .FDVTReq_Struct
						       [g_FDVT_ReqRing
						       .ReadIdx]
						       .FdvtFrameConfig
						       [g_FDVT_ReqRing
						       .FDVTReq_Struct
						       [g_FDVT_ReqRing.ReadIdx]
						       .RrameRDIdx],
						       sizeof(FDVT_Config));
						g_FDVT_ReqRing
						.FDVTReq_Struct
						[g_FDVT_ReqRing.ReadIdx]
						.FdvtFrameStatus[g_FDVT_ReqRing
						.FDVTReq_Struct
						[g_FDVT_ReqRing.ReadIdx]
						.RrameRDIdx++] =
						    FDVT_FRAME_STATUS_EMPTY;
					} else {
						log_err("deq err idx(%d) dequNum(%d) Rd(%d) RrameRD(%d) FrmStat(%d)\n",
						idx, dequeNum,
						g_FDVT_ReqRing.ReadIdx,
						g_FDVT_ReqRing
						.FDVTReq_Struct
						[g_FDVT_ReqRing.ReadIdx]
						.RrameRDIdx,
						g_FDVT_ReqRing
						.FDVTReq_Struct
						[g_FDVT_ReqRing.ReadIdx]
						.FdvtFrameStatus
						[g_FDVT_ReqRing
						.FDVTReq_Struct
						[g_FDVT_ReqRing.ReadIdx]
						.RrameRDIdx]);
					}
				}
				g_FDVT_ReqRing
				    .FDVTReq_Struct[g_FDVT_ReqRing.ReadIdx]
				    .State = FDVT_REQUEST_STATE_EMPTY;
				g_FDVT_ReqRing
				    .FDVTReq_Struct[g_FDVT_ReqRing.ReadIdx]
				    .FrameWRIdx = 0;
				g_FDVT_ReqRing
				    .FDVTReq_Struct[g_FDVT_ReqRing.ReadIdx]
				    .RrameRDIdx = 0;
				g_FDVT_ReqRing
				    .FDVTReq_Struct[g_FDVT_ReqRing.ReadIdx]
				    .enqueReqNum = 0;
				g_FDVT_ReqRing.ReadIdx =
				    (g_FDVT_ReqRing.ReadIdx +
				     1) % _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_;
				log_dbg("FDVT Request ReadIdx(%d)\n",
					 g_FDVT_ReqRing.ReadIdx);

				spin_unlock_irqrestore(&
						(FDVTInfo
						.SpinLockIrq
						[FDVT_IRQ_TYPE_INT_FDVT_ST]),
						flags);

				mutex_unlock(&gFdvtDequeMutex);

				if (fdvt_FdvtReq.m_pFdvtConfig == NULL) {
					log_err("NULL pointer:fdvt_FdvtReq.m_pFdvtConfig");
					Ret = -EFAULT;
					goto EXIT;
				}

				if (copy_to_user
				    ((void *)fdvt_FdvtReq.m_pFdvtConfig,
				     &g_FdvtDequeReq_Struct.FdvtFrameConfig[0],
				     dequeNum * sizeof(FDVT_Config)) != 0) {
					log_err(
					"FDVT_DEQUE_REQ copy_to_user frameconfig failed\n");
					Ret = -EFAULT;
				}
				if (copy_to_user
				    ((void *)Param, &fdvt_FdvtReq,
				    sizeof(FDVT_Request)) != 0) {
					log_err("FDVT_DEQUE_REQ copy_to_user failed\n");
					Ret = -EFAULT;
				}
			} else {
				log_err("FDVT_CMD_FDVT_DEQUE_REQ copy_from_user failed\n");
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

/*****************************************************************************
 *
 *****************************************************************************/
static int compat_get_FDVT_read_register_data(compat_FDVT_REG_IO_STRUCT
					     __user *data32,
					     FDVT_REG_IO_STRUCT __user *data)
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

static int compat_put_FDVT_read_register_data(compat_FDVT_REG_IO_STRUCT
					     __user *data32,
					     FDVT_REG_IO_STRUCT __user *data)
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

static int compat_get_FDVT_enque_req_data(compat_FDVT_Request __user *data32,
					      FDVT_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pFdvtConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pFdvtConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_FDVT_enque_req_data(compat_FDVT_Request __user *data32,
					      FDVT_Request __user *data)
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


static int compat_get_FDVT_deque_req_data(compat_FDVT_Request __user *data32,
					      FDVT_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pFdvtConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pFdvtConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_FDVT_deque_req_data(compat_FDVT_Request __user *data32,
					      FDVT_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pFdvtConfig); */
	/* err |= put_user(uptr, &data32->m_pFdvtConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static long FDVT_ioctl_compat(struct file *filp,
			      unsigned int cmd, unsigned long arg)
{
	long ret;


	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		log_err("no f_op !!!\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case COMPAT_FDVT_READ_REGISTER:
		{
			compat_FDVT_REG_IO_STRUCT __user *data32;
			FDVT_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_FDVT_read_register_data(data32, data);
			if (err) {
				log_inf(
				"compat_get_FDVT_read_register_data error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, FDVT_READ_REGISTER,
						       (unsigned long)data);
			err = compat_put_FDVT_read_register_data(data32, data);
			if (err) {
				log_inf(
				"compat_put_FDVT_read_register_data error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_FDVT_WRITE_REGISTER:
		{
			compat_FDVT_REG_IO_STRUCT __user *data32;
			FDVT_REG_IO_STRUCT __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_FDVT_read_register_data(data32, data);
			if (err) {
				log_inf("COMPAT_FDVT_WRITE_REGISTER error!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp,
						       FDVT_WRITE_REGISTER,
						       (unsigned long)data);
			return ret;
		}
	case COMPAT_FDVT_ENQUE_REQ:
		{
			compat_FDVT_Request __user *data32;
			FDVT_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_FDVT_enque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_FDVT_ENQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, FDVT_ENQUE_REQ,
						       (unsigned long)data);
			err = compat_put_FDVT_enque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_FDVT_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_FDVT_DEQUE_REQ:
		{
			compat_FDVT_Request __user *data32;
			FDVT_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_FDVT_deque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_FDVT_DEQUE_REQ error!!!\n");
				return err;
			}
			ret =
			    filp->f_op->unlocked_ioctl(filp, FDVT_DEQUE_REQ,
						       (unsigned long)data);
			err = compat_put_FDVT_deque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_FDVT_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}

	case FDVT_WAIT_IRQ:
	case FDVT_CLEAR_IRQ:	/* structure (no pointer) */
	case FDVT_ENQNUE_NUM:
	case FDVT_ENQUE:
	case FDVT_DEQUE_NUM:
	case FDVT_DEQUE:
	case FDVT_RESET:
	case FDVT_DUMP_REG:
	case FDVT_DUMP_ISR_LOG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return FDVT_ioctl(filep, cmd, arg); */
	}
}

#endif

/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i, j;
	/*int q = 0, p = 0;*/
	struct FDVT_USER_INFO_STRUCT *pUserInfo;

	log_dbg("- E. UserCount: %d.", FDVTInfo.UserCount);


	/*  */
	spin_lock(&(FDVTInfo.SpinLockFDVTRef));

	pFile->private_data = NULL;
	pFile->private_data =
	    kmalloc(sizeof(struct FDVT_USER_INFO_STRUCT), GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		log_dbg("ERROR: kmalloc failed, (process, pid, tgid)=(%s, %d, %d)",
			current->comm,
			current->pid,
			current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo =
		    (struct FDVT_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (FDVTInfo.UserCount > 0) {
		FDVTInfo.UserCount++;
		spin_unlock(&(FDVTInfo.SpinLockFDVTRef));
		log_dbg("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			FDVTInfo.UserCount, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else {
		FDVTInfo.UserCount++;
		spin_unlock(&(FDVTInfo.SpinLockFDVTRef));
		log_dbg("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), first user",
			FDVTInfo.UserCount, current->comm,
			current->pid, current->tgid);
	}

	/* do wait queue head init when re-enter in camera */
	/*  */
	for (i = 0; i < _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_; i++) {
		/* FDVT */
		g_FDVT_ReqRing.FDVTReq_Struct[i].processID = 0x0;
		g_FDVT_ReqRing.FDVTReq_Struct[i].callerID = 0x0;
		g_FDVT_ReqRing.FDVTReq_Struct[i].enqueReqNum = 0x0;
		/* g_FDVT_ReqRing.FDVTReq_Struct[i].enqueIdx = 0x0; */
		g_FDVT_ReqRing.FDVTReq_Struct[i].State =
		    FDVT_REQUEST_STATE_EMPTY;
		g_FDVT_ReqRing.FDVTReq_Struct[i].FrameWRIdx = 0x0;
		g_FDVT_ReqRing.FDVTReq_Struct[i].RrameRDIdx = 0x0;
		for (j = 0; j < _SUPPORT_MAX_FDVT_FRAME_REQUEST_; j++) {
			g_FDVT_ReqRing.FDVTReq_Struct[i].FdvtFrameStatus[j] =
			    FDVT_FRAME_STATUS_EMPTY;
		}

	}
	g_FDVT_ReqRing.WriteIdx = 0x0;
	g_FDVT_ReqRing.ReadIdx = 0x0;
	g_FDVT_ReqRing.HWProcessIdx = 0x0;

	/* Enable clock */
	FDVT_EnableClock(MTRUE);

	g_u4FdvtCnt = 0;
	log_dbg("FDVT open g_u4EnableClockCount: %d", g_u4EnableClockCount);
	/*  */

	for (i = 0; i < FDVT_IRQ_TYPE_AMOUNT; i++)
		FDVTInfo.IrqInfo.Status[i] = 0;

	for (i = 0; i < _SUPPORT_MAX_FDVT_FRAME_REQUEST_; i++)
		FDVTInfo.ProcessID[i] = 0;

	FDVTInfo.WriteReqIdx = 0;
	FDVTInfo.ReadReqIdx = 0;
	FDVTInfo.IrqInfo.FdvtIrqCnt = 0;

/*#define KERNEL_LOG*/
#ifdef KERNEL_LOG
    /* In EP, Add FDVT_DBG_WRITE_REG for debug. Should remove it after EP */
	FDVTInfo.DebugMask =
	(FDVT_DBG_INT | FDVT_DBG_DBGLOG | FDVT_DBG_WRITE_REG);
#endif
	/*  */
EXIT:



	log_dbg("- X. Ret: %d. UserCount: %d.", Ret, FDVTInfo.UserCount);
	return Ret;

}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_release(struct inode *pInode, struct file *pFile)
{
	struct FDVT_USER_INFO_STRUCT *pUserInfo;
	/*unsigned int Reg;*/

	log_dbg("- E. UserCount: %d.", FDVTInfo.UserCount);

	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo =
		(struct  FDVT_USER_INFO_STRUCT *) pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&(FDVTInfo.SpinLockFDVTRef));
	FDVTInfo.UserCount--;

	if (FDVTInfo.UserCount > 0) {
		spin_unlock(&(FDVTInfo.SpinLockFDVTRef));
		log_dbg("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), users exist",
			FDVTInfo.UserCount, current->comm,
			current->pid, current->tgid);
		goto EXIT;
	} else
		spin_unlock(&(FDVTInfo.SpinLockFDVTRef));
	/*  */
	log_dbg("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d), last user",
		FDVTInfo.UserCount, current->comm, current->pid, current->tgid);


	/* Disable clock. */
	FDVT_EnableClock(MFALSE);
	log_dbg("FDVT release g_u4EnableClockCount: %d", g_u4EnableClockCount);

	/*  */
EXIT:


	log_dbg("- X. UserCount: %d.", FDVTInfo.UserCount);
	return 0;
}


/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	length = pVma->vm_end - pVma->vm_start;
	/*  */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;

	log_inf(
	"[%s] mmap:vm_pgoff(0x%lx) pfn(0x%x) phy(0x%lx) vm_start(0x%lx) vm_end(0x%lx) length(0x%lx)",
	__func__, pVma->vm_pgoff, pfn, pVma->vm_pgoff << PAGE_SHIFT,
	pVma->vm_start, pVma->vm_end, length);

	switch (pfn) {
	case FDVT_BASE_HW:
		if (length > FDVT_REG_RANGE) {
			log_err("mmap range error :module:0x%x length(0x%lx),FDVT_REG_RANGE(0x%x)!",
				pfn, length, FDVT_REG_RANGE);
			return -EAGAIN;
		}
		break;
	default:
		log_err("Illegal starting HW addr for mmap!");
		return -EAGAIN;
	}
	if (remap_pfn_range
	    (pVma, pVma->vm_start, pVma->vm_pgoff,
	     pVma->vm_end - pVma->vm_start,
	     pVma->vm_page_prot)) {
		return -EAGAIN;
	}
	/*  */
	return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/

static dev_t FDVTDevNo;
static struct cdev *pFDVTCharDrv;
static struct class *pFDVTClass;

static const struct file_operations FDVTFileOper = {
	.owner = THIS_MODULE,
	.open = FDVT_open,
	.release = FDVT_release,
	/* .flush   = mt_FDVT_flush, */
	.mmap = FDVT_mmap,
	.unlocked_ioctl = FDVT_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = FDVT_ioctl_compat,
#endif
};

/*****************************************************************************
 *
 *****************************************************************************/
static inline void FDVT_UnregCharDev(void)
{
	log_dbg("- E.");
	/*  */
	/* Release char driver */
	if (pFDVTCharDrv != NULL) {
		cdev_del(pFDVTCharDrv);
		pFDVTCharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(FDVTDevNo, 1);
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline signed int FDVT_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	log_dbg("- E.");
	/*  */
	Ret = alloc_chrdev_region(&FDVTDevNo, 0, 1, FDVT_DEV_NAME);
	if (Ret < 0) {
		log_err("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pFDVTCharDrv = cdev_alloc();
	if (pFDVTCharDrv == NULL) {
		log_err("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pFDVTCharDrv, &FDVTFileOper);
	/*  */
	pFDVTCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pFDVTCharDrv, FDVTDevNo, 1);
	if (Ret < 0) {
		log_err("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		FDVT_UnregCharDev();

	/*  */

	log_dbg("- X.");
	return Ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*struct resource *pRes = NULL;*/
	signed int i = 0;
	unsigned char n;
	unsigned int irq_info[3]; /* Record interrupts info from device tree */
	struct device *dev = NULL;
	struct FDVT_device *_fdvt_dev;


#ifdef CONFIG_OF
	struct FDVT_device *FDVT_dev;
#endif

	log_inf("- E. FDVT driver probe.\n");

	/* Check platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		dev_dbg(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_FDVT_devs += 1;
	_fdvt_dev = krealloc(FDVT_devs, sizeof(struct FDVT_device) *
			     nr_FDVT_devs, GFP_KERNEL);
	if (!_fdvt_dev) {
		dev_dbg(&pDev->dev, "Unable to allocate FDVT_devs\n");
		return -ENOMEM;
	}
	FDVT_devs = _fdvt_dev;

	FDVT_dev = &(FDVT_devs[nr_FDVT_devs - 1]);
	FDVT_dev->dev = &pDev->dev;

	/* iomap registers */
	FDVT_dev->regs = of_iomap(pDev->dev.of_node, 0);
	/* gISPSYS_Reg[nr_FDVT_devs - 1] = FDVT_dev->regs; */

	if (!FDVT_dev->regs) {
		dev_dbg(&pDev->dev,
			"Unable to ioremap registers, of_iomap fail, nr_FDVT_devs=%d, devnode(%s).\n",
			nr_FDVT_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

	log_inf("nr_FDVT_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_FDVT_devs,
		pDev->dev.of_node->name, (unsigned long)FDVT_dev->regs);

	/* get IRQ ID and request IRQ */
	FDVT_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (FDVT_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array
		    (pDev->dev.of_node, "interrupts",
		    irq_info, ARRAY_SIZE(irq_info))) {
			dev_dbg(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < FDVT_IRQ_TYPE_AMOUNT; i++) {
			if (strcmp(pDev->dev.of_node->name,
			    FDVT_IRQ_CB_TBL[i].device_name)
			    == 0) {
				Ret =
				    request_irq(FDVT_dev->irq,
						(irq_handler_t)
						FDVT_IRQ_CB_TBL[i]
						.isr_fp,
						irq_info[2],
						(const char *)
						FDVT_IRQ_CB_TBL[i]
						.device_name,
						NULL);
				if (Ret) {
					dev_dbg(&pDev->dev,
						"Unable to request IRQ, request_irq fail, nr_FDVT_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
						nr_FDVT_devs,
						pDev->dev.of_node->name,
						FDVT_dev->irq,
						FDVT_IRQ_CB_TBL[i].device_name);
					return Ret;
				}

				log_inf(
				"nr_FDVT_devs=%d, devnode(%s), irq=%d, ISR: %s\n",
				nr_FDVT_devs,
				pDev->dev.of_node->name,
				FDVT_dev->irq,
				FDVT_IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= FDVT_IRQ_TYPE_AMOUNT) {
			log_inf(
			"No corresponding ISR!!: nr_FDVT_devs=%d, devnode(%s), irq=%d\n",
			nr_FDVT_devs,
			pDev->dev.of_node->name,
			FDVT_dev->irq);
		}


	} else {
		log_inf("No IRQ!!: nr_FDVT_devs=%d, devnode(%s), irq=%d\n",
			nr_FDVT_devs,
			pDev->dev.of_node->name, FDVT_dev->irq);
	}


#endif

	/* Only register char driver in the 1st time */
	if (nr_FDVT_devs == 1) {

		/* Register char driver */
		Ret = FDVT_RegCharDev();
		if (Ret) {
			dev_dbg(&pDev->dev, "register char failed");
			return Ret;
		}
#ifndef EP_NO_CLKMGR
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
		    /*CCF: Grab clock pointer (struct clk*) */
#ifndef SMI_CLK
		fdvt_clk.CG_SCP_SYS_MM0 =
		    devm_clk_get(&pDev->dev, "FDVT_SCP_SYS_MM0");
		fdvt_clk.CG_MM_SMI_COMMON =
		    devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG2_B11");
		fdvt_clk.CG_MM_SMI_COMMON_2X =
		    devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG2_B12");
		fdvt_clk.CG_MM_SMI_COMMON_GALS_M0_2X =
		    devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B12");
		fdvt_clk.CG_MM_SMI_COMMON_GALS_M1_2X =
		    devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B13");
		fdvt_clk.CG_MM_SMI_COMMON_UPSZ0 =
		    devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B14");
		fdvt_clk.CG_MM_SMI_COMMON_UPSZ1 =
		    devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B15");
		fdvt_clk.CG_MM_SMI_COMMON_FIFO0 =
		    devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B16");
		fdvt_clk.CG_MM_SMI_COMMON_FIFO1 =
		    devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B17");
		fdvt_clk.CG_MM_LARB5 =
		    devm_clk_get(&pDev->dev, "FDVT_CLK_MM_CG1_B10");
		fdvt_clk.CG_SCP_SYS_ISP =
		    devm_clk_get(&pDev->dev, "FDVT_SCP_SYS_ISP");
		fdvt_clk.CG_IMGSYS_LARB =
		    devm_clk_get(&pDev->dev, "FDVT_CLK_IMG_LARB");
#endif
		fdvt_clk.CG_IMGSYS_FDVT =
		    devm_clk_get(&pDev->dev, "FD_CLK_IMG_FDVT");

#ifndef SMI_CLK
		if (IS_ERR(fdvt_clk.CG_SCP_SYS_MM0)) {
			log_err("cannot get CG_SCP_SYS_MM0 clock\n");
			return PTR_ERR(fdvt_clk.CG_SCP_SYS_MM0);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON)) {
			log_err("cannot get CG_MM_SMI_COMMON clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_2X)) {
			log_err("cannot get CG_MM_SMI_COMMON_2X clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_2X);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_GALS_M0_2X)) {
			log_err("cannot get CG_MM_SMI_COMMON_GALS_M0_2X clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_GALS_M0_2X);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_GALS_M1_2X)) {
			log_err("cannot get CG_MM_SMI_COMMON_GALS_M1_2X clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_GALS_M1_2X);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_UPSZ0)) {
			log_err("cannot get CG_MM_SMI_COMMON_UPSZ0 clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_UPSZ0);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_UPSZ1)) {
			log_err("cannot get CG_MM_SMI_COMMON_UPSZ1 clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_UPSZ1);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_FIFO0)) {
			log_err("cannot get CG_MM_SMI_COMMON_FIFO0 clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_FIFO0);
		}
		if (IS_ERR(fdvt_clk.CG_MM_SMI_COMMON_FIFO1)) {
			log_err("cannot get CG_MM_SMI_COMMON_FIFO1 clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_SMI_COMMON_FIFO1);
		}
		if (IS_ERR(fdvt_clk.CG_MM_LARB5)) {
			log_err("cannot get CG_MM_LARB5 clock\n");
			return PTR_ERR(fdvt_clk.CG_MM_LARB5);
		}
		if (IS_ERR(fdvt_clk.CG_SCP_SYS_ISP)) {
			log_err("cannot get CG_SCP_SYS_ISP clock\n");
			return PTR_ERR(fdvt_clk.CG_SCP_SYS_ISP);
		}
		if (IS_ERR(fdvt_clk.CG_IMGSYS_LARB)) {
			log_err("cannot get CG_IMGSYS_LARB clock\n");
			return PTR_ERR(fdvt_clk.CG_IMGSYS_LARB);
		}
#endif
		if (IS_ERR(fdvt_clk.CG_IMGSYS_FDVT)) {
			log_err("cannot get CG_IMGSYS_FDVT clock\n");
			return PTR_ERR(fdvt_clk.CG_IMGSYS_FDVT);
		}
#endif	/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */
#endif

		/* Create class register */
		pFDVTClass = class_create(THIS_MODULE, "FDVTdrv");
		if (IS_ERR(pFDVTClass)) {
			Ret = PTR_ERR(pFDVTClass);
			log_err("Unable to create class, err = %d", Ret);
			goto EXIT;
		}

		dev = device_create(pFDVTClass, NULL,
				    FDVTDevNo, NULL, FDVT_DEV_NAME);
		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			dev_dbg(&pDev->dev, "Failed to create device: /dev/%s, err = %d",
				FDVT_DEV_NAME, Ret);
			goto EXIT;
		}

		/* Init spinlocks */
		spin_lock_init(&(FDVTInfo.SpinLockFDVTRef));
		spin_lock_init(&(FDVTInfo.SpinLockFDVT));
		for (n = 0; n < FDVT_IRQ_TYPE_AMOUNT; n++)
			spin_lock_init(&(FDVTInfo.SpinLockIrq[n]));

		/*  */
		init_waitqueue_head(&FDVTInfo.WaitQueueHead);
		INIT_WORK(&FDVTInfo.ScheduleFdvtWork, FDVT_ScheduleWork);


#ifdef CONFIG_PM_WAKELOCKS
		wakeup_source_init(&FDVT_wake_lock, "fdvt_lock_wakelock");
#else
		wake_lock_init(&FDVT_wake_lock,
			WAKE_LOCK_SUSPEND,
			"fdvt_lock_wakelock");
#endif

		// wake_lock_init(
		// &FDVT_wake_lock, WAKE_LOCK_SUSPEND, "fdvt_lock_wakelock");

		for (i = 0; i < FDVT_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(FDVT_tasklet[i].pFDVT_tkt,
				     FDVT_tasklet[i].tkt_cb, 0);




		/* Init FDVTInfo */
		spin_lock(&(FDVTInfo.SpinLockFDVTRef));
		FDVTInfo.UserCount = 0;
		spin_unlock(&(FDVTInfo.SpinLockFDVTRef));
		/*  */
		FDVTInfo.IrqInfo
		.Mask[FDVT_IRQ_TYPE_INT_FDVT_ST] = INT_ST_MASK_FDVT;

	}

EXIT:
	if (Ret < 0)
		FDVT_UnregCharDev();


	log_inf("- X. FDVT driver probe.");

	return Ret;
}

/*****************************************************************************
 * Called when the device is being detached from the driver
 *****************************************************************************/
static signed int FDVT_remove(struct platform_device *pDev)
{
	/*struct resource *pRes;*/
	signed int IrqNum;
	int i;
	/*  */
	log_dbg("- E.");
	/* unregister char driver. */
	FDVT_UnregCharDev();

	/* Release IRQ */
	disable_irq(FDVTInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < FDVT_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(FDVT_tasklet[i].pFDVT_tkt);
#if 0
	/* free all registered irq(child nodes) */
	FDVT_UnRegister_AllregIrq();
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
	device_destroy(pFDVTClass, FDVTDevNo);
	/*  */
	class_destroy(pFDVTClass);
	pFDVTClass = NULL;
	/*  */
	return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int bPass1_On_In_Resume_TG1;

static signed int FDVT_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	/*signed int ret = 0;*/

	log_dbg("bPass1_On_In_Resume_TG1(%d)\n", bPass1_On_In_Resume_TG1);

	bPass1_On_In_Resume_TG1 = 0;

	if (g_u4EnableClockCount > 0) {
		FDVT_EnableClock(MFALSE);
		g_u4FdvtCnt++;
	}
	return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static signed int FDVT_resume(struct platform_device *pDev)
{
	log_dbg("bPass1_On_In_Resume_TG1(%d).\n", bPass1_On_In_Resume_TG1);

	if (g_u4FdvtCnt > 0) {
		FDVT_EnableClock(MTRUE);
		g_u4FdvtCnt--;
	}
	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int FDVT_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/* pr_debug("calling %s()\n", __func__); */
	log_inf("FDVT suspend g_u4EnableClockCount: %d, g_u4FdvtCnt: %d",
		g_u4EnableClockCount, g_u4FdvtCnt);

	return FDVT_suspend(pdev, PMSG_SUSPEND);
}

int FDVT_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/* pr_debug("calling %s()\n", __func__); */
	log_inf("FDVT resume g_u4EnableClockCount: %d, g_u4FdvtCnt: %d",
		g_u4EnableClockCount, g_u4FdvtCnt);

	return FDVT_resume(pdev);
}
#ifndef CONFIG_OF
/*extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);*/
/*extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);*/
#endif
int FDVT_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
/*	mt_irq_set_sens(FDVT_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);*/
/*	mt_irq_set_polarity(FDVT_IRQ_BIT_ID, MT_POLARITY_LOW);*/
#endif
	return 0;

}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define FDVT_pm_suspend NULL
#define FDVT_pm_resume  NULL
#define FDVT_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_OF
/*
 * Note!!! The order and member of .compatible
 * must be the same with FDVT_DEV_NODE_IDX
 */
static const struct of_device_id FDVT_of_ids[] = {
/*	{.compatible = "mediatek,imgsyscq",},*/
	{.compatible = "mediatek,fdvt",},
	{}
};
#endif

const struct dev_pm_ops FDVT_pm_ops = {
	.suspend = FDVT_pm_suspend,
	.resume = FDVT_pm_resume,
	.freeze = FDVT_pm_suspend,
	.thaw = FDVT_pm_resume,
	.poweroff = FDVT_pm_suspend,
	.restore = FDVT_pm_resume,
	.restore_noirq = FDVT_pm_restore_noirq,
};


/*****************************************************************************
 *
 *****************************************************************************/
static struct platform_driver FDVTDriver = {
	.probe = FDVT_probe,
	.remove = FDVT_remove,
	.suspend = FDVT_suspend,
	.resume = FDVT_resume,
	.driver = {
		   .name = FDVT_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = FDVT_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &FDVT_pm_ops,
#endif
	}
};


static int fdvt_dump_read(struct seq_file *m, void *v)
{
	int i, j;

	seq_puts(m, "\n============ fdvt dump register============\n");
	seq_puts(m, "FDVT Config Info\n");

	if (FDVTInfo.UserCount > 0) {
		seq_puts(m, "FDVT Config Info\n");
		for (i = 0x4; i < 0x168; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				   (unsigned int)(FDVT_BASE_HW + i),
				   (unsigned int)FDVT_RD32(ISP_FDVT_BASE + i));
		}
		seq_puts(m, "FDVT Debug Info\n");
		for (i = 0x16C; i < 0x174; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				   (unsigned int)(FDVT_BASE_HW + i),
				   (unsigned int)FDVT_RD32(ISP_FDVT_BASE + i));
		}
		seq_puts(m, "FDVT DMA Info\n");
		for (i = 0x200; i < 0x3F8; i = i + 4) {
			seq_printf(m, "[0x%08X %08X]\n",
				   (unsigned int)(FDVT_BASE_HW + i),
				   (unsigned int)FDVT_RD32(ISP_FDVT_BASE + i));
		}
	}
	seq_puts(m, "\n");
	seq_printf(m, "FDVT Clock Count:%d\n", g_u4EnableClockCount);

	seq_printf(m, "FDVT:HWProcessIdx:%d, WriteIdx:%d, ReadIdx:%d\n",
		   g_FDVT_ReqRing.HWProcessIdx, g_FDVT_ReqRing.WriteIdx,
		   g_FDVT_ReqRing.ReadIdx);

	for (i = 0; i < _SUPPORT_MAX_FDVT_REQUEST_RING_SIZE_; i++) {
		seq_printf(m,
			   "FDVT:State:%d, processID:0x%08X, callerID:0x%08X, enqueReqNum:%d, FrameWRIdx:%d, RrameRDIdx:%d\n",
			   g_FDVT_ReqRing.FDVTReq_Struct[i].State,
			   g_FDVT_ReqRing.FDVTReq_Struct[i].processID,
			   g_FDVT_ReqRing.FDVTReq_Struct[i].callerID,
			   g_FDVT_ReqRing.FDVTReq_Struct[i].enqueReqNum,
			   g_FDVT_ReqRing.FDVTReq_Struct[i].FrameWRIdx,
			   g_FDVT_ReqRing.FDVTReq_Struct[i].RrameRDIdx);

		for (j = 0; j < _SUPPORT_MAX_FDVT_FRAME_REQUEST_;) {
			seq_printf(m,
				   "FDVT:FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d, FrameStatus[%d]:%d\n",
				   j,
				   g_FDVT_ReqRing
				   .FDVTReq_Struct[i]
				   .FdvtFrameStatus[j],
				   j + 1,
				   g_FDVT_ReqRing
				   .FDVTReq_Struct[i]
				   .FdvtFrameStatus[j + 1],
				   j + 2,
				   g_FDVT_ReqRing
				   .FDVTReq_Struct[i]
				   .FdvtFrameStatus[j + 2]
				   , j + 3,
				   g_FDVT_ReqRing
				   .FDVTReq_Struct[i]
				   .FdvtFrameStatus[j + 3]);
			j = j + 4;
		}
	}

	seq_puts(m, "\n============ fdvt dump debug ============\n");

	return 0;
}


static int proc_fdvt_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, fdvt_dump_read, NULL);
}

static const struct file_operations fdvt_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_fdvt_dump_open,
	.read = seq_read,
};


static int fdvt_reg_read(struct seq_file *m, void *v)
{
	unsigned int i;

	seq_puts(m, "======== read fdvt register ========\n");

	if (FDVTInfo.UserCount > 0) {
		for (i = 0x0; i <= 0x3f8; i = i + 4) {
			seq_printf(m, "[0x%08X 0x%08X]\n",
				   (unsigned int)(FDVT_BASE_HW + i),
				   (unsigned int)FDVT_RD32(ISP_FDVT_BASE + i));
		}
	}
	seq_printf(m, "[0x%08X 0x%08X]\n", (unsigned int)(DMA_DEBUG_ADDR_HW),
		   (unsigned int)FDVT_RD32(DMA_DEBUG_ADDR_REG));


	return 0;
}

// static int fdvt_reg_write(struct file *file, const char __user *buffer,
//                            size_t count, loff_t *data)

static ssize_t fdvt_reg_write(struct file *file, const char __user *buffer,
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

	if (FDVTInfo.UserCount <= 0)
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%23s %23s", addrSzBuf, valSzBuf) == 2) {
		pszTmp = strstr(addrSzBuf, "0x");
		if (pszTmp == NULL) {
			if (kstrtol(addrSzBuf, 10, (long int *)&tempval) != 0)
				log_err("scan decimal addr is wrong !!:%s",
					addrSzBuf);
		} else {
			if (strlen(addrSzBuf) > 2) {
				if (sscanf(addrSzBuf + 2, "%x", &addr) != 1)
					log_err("scan hexadecimal addr is wrong !!:%s",
						addrSzBuf);
			} else {
				log_inf("FDVT Write Addr Error!!:%s",
					addrSzBuf);
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
					log_err("scan hexadecimal value is wrong !!:%s",
						valSzBuf);
			} else {
				log_inf("FDVT Write Value Error!:%s\n",
					valSzBuf);
			}
		}

		if ((addr >= FDVT_START_HW) && (addr <= DMA_BW_SELF_TEST_HW)
			&& ((addr & 0x3) == 0)) {
			log_inf("Write Request - addr:0x%x, value:0x%x\n",
				addr, val);
			FDVT_WR32((ISP_FDVT_BASE + (addr - FDVT_START_HW)),
				  val);
		} else {
			log_inf(
			"Write-Address Range exceeds the size of hw fdvt!! addr:0x%x, value:0x%x\n",
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
				log_inf("FDVT Read Addr Error!!:%s", addrSzBuf);
			}
		}

		if ((addr >= FDVT_START_HW) && (addr <= DMA_BW_SELF_TEST_HW)
			&& ((addr & 0x3) == 0)) {
			val =
			FDVT_RD32((ISP_FDVT_BASE + (addr - FDVT_BASE_HW)));
			log_inf("Read Request - addr:0x%x,value:0x%x\n",
				addr, val);
		} else {
			log_inf(
			"Read-Address Range exceeds the size of hw fdvt!! addr:0x%x, value:0x%x\n",
			addr, val);
		}

	}


	return count;
}

static int proc_fdvt_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, fdvt_reg_read, NULL);
}

static const struct file_operations fdvt_reg_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_fdvt_reg_open,
	.read = seq_read,
	.write = fdvt_reg_write,
};


/*****************************************************************************
 *
 *****************************************************************************/

int32_t FDVT_ClockOnCallback(uint64_t engineFlag)
{
	/* log_dbg("FDVT_ClockOnCallback"); */
	/* log_dbg("+CmdqEn:%d", g_u4EnableClockCount); */
	/* FDVT_EnableClock(MTRUE); */

	return 0;
}

int32_t FDVT_DumpCallback(uint64_t engineFlag, int level)
{
	log_dbg("[FDVT]DumpCallback");

	FDVT_DumpReg();

	return 0;
}

int32_t FDVT_ResetCallback(uint64_t engineFlag)
{
	log_dbg("[FDVT]ResetCallback");
	FDVT_Reset();

	return 0;
}

int32_t FDVT_ClockOffCallback(uint64_t engineFlag)
{
	/* log_dbg("FDVT_ClockOffCallback"); */
	/* FDVT_EnableClock(MFALSE); */
	/* log_dbg("-CmdqEn:%d", g_u4EnableClockCount); */
	return 0;
}


static signed int __init FDVT_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	/* FIX-ME: linux-3.10 procfs API changed */
	/* use proc_create */
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *isp_fdvt_dir;


	int i;
	/*  */
	log_dbg("- E.");
	/*  */
	Ret = platform_driver_register(&FDVTDriver);
	if (Ret < 0) {
		log_err("platform_driver_register fail");
		return Ret;
	}

#if 0
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,FDVT");
	if (!node) {
		log_err("find mediatek,FDVT node failed!!!\n");
		return -ENODEV;
	}
	ISP_FDVT_BASE = of_iomap(node, 0);
	if (!ISP_FDVT_BASE) {
		log_err("unable to map ISP_FDVT_BASE registers!!!\n");
		return -ENODEV;
	}
	log_dbg("ISP_FDVT_BASE: %lx\n", ISP_FDVT_BASE);
#endif

	isp_fdvt_dir = proc_mkdir("fdvt", NULL);
	if (!isp_fdvt_dir) {
		log_err("[%s]: fail to mkdir /proc/fdvt\n", __func__);
		return 0;
	}

	// proc_entry = proc_create("pll_test", S_IRUGO | S_IWUSR,
	// isp_fdvt_dir, &pll_test_proc_fops);

	proc_entry = proc_create("fdvt_dump", 0444,
				 isp_fdvt_dir, &fdvt_dump_proc_fops);

	proc_entry = proc_create("fdvt_reg", 0644,
				 isp_fdvt_dir, &fdvt_reg_proc_fops);


	/* isr log */
	if (PAGE_SIZE <
	    ((FDVT_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
	     ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) *
	     LOG_PPNUM)) {
		i = 0;
		while (i <
		       ((FDVT_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
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
		for (j = 0; j < FDVT_IRQ_TYPE_AMOUNT; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			// tmp = (void*) ((unsigned int)tmp +
			// (NORMAL_STR_LEN*DBG_PAGE));
			tmp = (void *)((char *)tmp +
			       (NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			// tmp = (void*) ((unsigned int)tmp +
			// (NORMAL_STR_LEN*INF_PAGE));
			tmp = (void *)((char *)tmp +
			       (NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			// tmp =
			// (void*) ((unsigned int)tmp +
			// (NORMAL_STR_LEN*ERR_PAGE));
			tmp = (void *)((char *)tmp +
			       (NORMAL_STR_LEN * ERR_PAGE));
		}
		/* log buffer ,in case of overflow */
		/* tmp = (void*) ((unsigned int)tmp + NORMAL_STR_LEN); */
		/* log buffer ,in case of overflow */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);
	}


#if 0
	/* Cmdq */
	/* Register FDVT callback */
	log_dbg("register fdvt callback for CMDQ");
	cmdqCoreRegisterCB(CMDQ_GROUP_FDVT,
			   FDVT_ClockOnCallback,
			   FDVT_DumpCallback,
			   FDVT_ResetCallback,
			   FDVT_ClockOffCallback);
#endif

	log_dbg("- X. Ret: %d.", Ret);
	return Ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void __exit FDVT_Exit(void)
{
	/*int i;*/

	log_dbg("- E.");
	/*  */
	platform_driver_unregister(&FDVTDriver);
	/*  */
#if 0
	/* Cmdq */
	/* Unregister FDVT callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_FDVT, NULL, NULL, NULL, NULL);
#endif

	kfree(pLog_kmalloc);

	/*  */
}


/*****************************************************************************
 *
 *****************************************************************************/
void FDVT_ScheduleWork(struct work_struct *data)
{
	if (FDVT_DBG_DBGLOG & FDVTInfo.DebugMask)
		log_dbg("- E.");

#ifdef FDVT_USE_GCE
#else
	ConfigFDVT();
#endif
}


static irqreturn_t ISP_Irq_FDVT(signed int Irq, void *DeviceId)
{
	unsigned int FdvtStatus;
	bool bResulst = MFALSE;
	pid_t ProcessID;

	FdvtStatus = FDVT_RD32(FDVT_INT_REG);	/* FDVT Status */

	spin_lock(&(FDVTInfo.SpinLockIrq[FDVT_IRQ_TYPE_INT_FDVT_ST]));

	if (FDVT_INT_ST == (FDVT_INT_ST & FdvtStatus)) {
		/* Update the frame status. */
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("fdvt_irq");
#endif

#ifndef FDVT_USE_GCE
		FDVT_WR32(FDVT_START_REG, 0);
#endif
		bResulst = UpdateFDVT(&ProcessID);
		/* ConfigFDVT(); */
		if (bResulst == MTRUE) {
			schedule_work(&FDVTInfo.ScheduleFdvtWork);
#ifdef FDVT_USE_GCE
			FDVTInfo.IrqInfo
			.Status[FDVT_IRQ_TYPE_INT_FDVT_ST] |= FDVT_INT_ST;
			FDVTInfo.IrqInfo
			.ProcessID[FDVT_PROCESS_ID_FDVT] = ProcessID;
			FDVTInfo.IrqInfo.FdvtIrqCnt++;
			FDVTInfo.ProcessID[FDVTInfo.WriteReqIdx] = ProcessID;
			FDVTInfo.WriteReqIdx =
			    (FDVTInfo.WriteReqIdx + 1) %
			    _SUPPORT_MAX_FDVT_FRAME_REQUEST_;
#ifdef FDVT_MULTIPROCESS_TIMEING_ISSUE
			/* check the write value is equal to read value ? */
			/* actually, it doesn't happen!! */
			if (FDVTInfo.WriteReqIdx == FDVTInfo.ReadReqIdx) {
				IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST,
					       m_CurrentPPB, _LOG_ERR,
					       "Irq_FDVT Err!!, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
					       FDVTInfo.WriteReqIdx,
					       FDVTInfo.ReadReqIdx);
			}
#endif

#else
			FDVTInfo.IrqInfo
			.Status[FDVT_IRQ_TYPE_INT_FDVT_ST] |= FDVT_INT_ST;
			FDVTInfo.IrqInfo
			.ProcessID[FDVT_PROCESS_ID_FDVT] = ProcessID;
#endif
		}
#ifdef __FDVT_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
		/* Config the Next frame */
	}
	spin_unlock(&(FDVTInfo.SpinLockIrq[FDVT_IRQ_TYPE_INT_FDVT_ST]));

	if (bResulst == MTRUE)
		wake_up_interruptible(&FDVTInfo.WaitQueueHead);

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB, _LOG_INF,
		       "Irq_FDVT:%d, reg 0x%x : 0x%x, bResulst:%d, FdvtHWSta:0x%x, FdvtIrqCnt:0x%x, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
		       Irq, FDVT_INT_HW, FdvtStatus, bResulst, FdvtStatus,
		       FDVTInfo.IrqInfo.FdvtIrqCnt,
		       FDVTInfo.WriteReqIdx, FDVTInfo.ReadReqIdx);
	/* IRQ_LOG_KEEPER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB, _LOG_INF,
	 *  "FdvtHWSta:0x%x, FdvtHWSta:0x%x,
	 *  DpeDveSta0:0x%x\n", DveStatus, FdvtStatus, DpeDveSta0);
	 */

	if (FdvtStatus & FDVT_INT_ST)
		tasklet_schedule(FDVT_tasklet[FDVT_IRQ_TYPE_INT_FDVT_ST]
				.pFDVT_tkt);

	return IRQ_HANDLED;
}

static void ISP_TaskletFunc_FDVT(unsigned long data)
{
	IRQ_LOG_PRINTER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(FDVT_IRQ_TYPE_INT_FDVT_ST, m_CurrentPPB, _LOG_ERR);

}


/*****************************************************************************
 *
 *****************************************************************************/
module_init(FDVT_Init);
module_exit(FDVT_Exit);
MODULE_DESCRIPTION("Camera FDVT driver");
MODULE_AUTHOR("MM3SW5");
MODULE_LICENSE("GPL");
